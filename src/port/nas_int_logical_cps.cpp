/*
 * Copyright (c) 2016 Dell Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED ON AN *AS IS* BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 * LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 * FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 * See the Apache Version 2.0 License for specific language governing
 * permissions and limitations under the License.
 */

/*
 * nas_int_logical_cps.cpp
 *
 *  Created on: Jun 5, 2015
 */

#include "std_rw_lock.h"
#include "nas_os_interface.h"
#include "hal_if_mapping.h"
#include "nas_if_utils.h"

#include "nas_int_port.h"
#include "nas_int_utils.h"
#include "interface_obj.h"
#include "nas_interface_fc.h"

#include "hal_interface_common.h"
#include "hal_interface_defaults.h"
#include "nas_ndi_port.h"

#include "cps_api_events.h"
#include "cps_api_object_attr.h"
#include "dell-base-common.h"
#include "dell-base-if-phy.h"
#include "dell-base-if.h"
#include "iana-if-type.h"
#include "dell-base-if-linux.h"
#include "dell-interface.h"
#include "nas_linux_l2.h"

#include "cps_class_map.h"
#include "cps_api_object_key.h"
#include "cps_api_operation.h"
#include "cps_api_db_interface.h"

#include "event_log.h"
#include "std_utils.h"
#include "nas_ndi_port.h"
#include "nas_switch.h"
#include "nas_int_com_utils.h"

#include <inttypes.h>
#include <unordered_map>
#include <list>

struct _npu_port_t {
    uint_t npu_id;
    uint_t npu_port;

    bool operator== (const _npu_port_t& p) const {
        return npu_id == p.npu_id && npu_port == p.npu_port;
    }
};

struct _npu_port_hash_t {
    std::size_t operator() (const _npu_port_t& p) const {
        return std::hash<int>()(p.npu_id) ^ (std::hash<int>()(p.npu_port) << 1);
    }
};

struct _port_cache {
    uint32_t mode;
    uint32_t media_type;
    BASE_IF_SUPPORTED_AUTONEG_t supported_autoneg;
    BASE_IF_SPEED_t configured_speed = BASE_IF_SPEED_0MBPS;

    _port_cache() : mode( BASE_IF_MODE_MODE_NONE),
                    media_type(PLATFORM_MEDIA_TYPE_AR_POPTICS_NOTPRESENT),
                    supported_autoneg(BASE_IF_SUPPORTED_AUTONEG_NOT_SUPPORTED){}
};

using NasLogicalPortMap = std::unordered_map<_npu_port_t, _port_cache, _npu_port_hash_t>;
static NasLogicalPortMap _logical_port_tbl;
static std_rw_lock_t _logical_port_lock;


// TODO move this into common interface utility file
static bool if_data_from_obj(obj_intf_cat_t obj_cat, cps_api_object_t o, interface_ctrl_t& i) {
    cps_api_object_attr_t _name = cps_api_get_key_data(o,(obj_cat == obj_INTF) ?
                                                  (uint)IF_INTERFACES_INTERFACE_NAME:
                                                  (uint)IF_INTERFACES_STATE_INTERFACE_NAME);
    cps_api_object_attr_t _ifix = cps_api_object_attr_get(o,(obj_cat == obj_INTF) ?
                                (uint)DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX:
                                (uint)IF_INTERFACES_STATE_INTERFACE_IF_INDEX);


    cps_api_object_attr_t _npu = nullptr;
    cps_api_object_attr_t _port = nullptr;

    if (obj_cat == obj_INTF) {
        _npu = cps_api_object_attr_get(o,BASE_IF_PHY_IF_INTERFACES_INTERFACE_NPU_ID);
        _port = cps_api_object_attr_get(o,BASE_IF_PHY_IF_INTERFACES_INTERFACE_PORT_ID);
    }

    if (_ifix!=nullptr) {
        memset(&i,0,sizeof(i));
        i.if_index = cps_api_object_attr_data_u32(_ifix);
        i.q_type = HAL_INTF_INFO_FROM_IF;
        if (dn_hal_get_interface_info(&i)==STD_ERR_OK) return true;
    }

    if (_name!=nullptr) {
        memset(&i,0,sizeof(i));
        strncpy(i.if_name,(const char *)cps_api_object_attr_data_bin(_name),sizeof(i.if_name)-1);
        i.q_type = HAL_INTF_INFO_FROM_IF_NAME;
        if (dn_hal_get_interface_info(&i)==STD_ERR_OK) return true;
    }

    if (_npu!=nullptr && _port!=nullptr &&
        cps_api_object_attr_len(_npu) > 0 && cps_api_object_attr_len(_port) > 0) {
        memset(&i,0,sizeof(i));
        i.npu_id = cps_api_object_attr_data_u32(_npu);
        i.port_id = cps_api_object_attr_data_u32(_port);
        i.q_type = HAL_INTF_INFO_FROM_PORT;
        if (dn_hal_get_interface_info(&i)==STD_ERR_OK) return true;
    }

    EV_LOGGING(INTERFACE,ERR,"IF-CPS-CREATE","Invalid fields - can't locate specified port");
    return false;
}

static hal_ifindex_t nas_int_ifindex_from_port(npu_id_t npu,port_t port) {
    interface_ctrl_t info;
    memset(&info,0,sizeof(info));
    info.npu_id = npu;
    info.port_id = port;
    info.q_type = HAL_INTF_INFO_FROM_PORT;
    if (dn_hal_get_interface_info(&info)!=STD_ERR_OK) {
        return -1;
    }
    return info.if_index;
}

static hal_ifindex_t nas_int_ifindex_from_name(const char *name)
{
    interface_ctrl_t info;
    memset(&info, 0, sizeof(info));
    strncpy(info.if_name, name, sizeof(info.if_name) - 1);
    info.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    if (dn_hal_get_interface_info(&info)!=STD_ERR_OK) {
        return -1;
    }
    return info.if_index;
}

static void _if_fill_in_supported_autoneg_attr(npu_id_t npu, port_t port, cps_api_object_t obj) {
    _npu_port_t npu_port  = {(uint_t)npu, (uint_t)port};
    cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_STATE_INTERFACE_SUPPORTED_AUTONEG, _logical_port_tbl[npu_port].supported_autoneg);
}

static void _if_fill_in_npu_speed_attr(npu_id_t npu, port_t port, nas_int_type_t int_type,
        cps_api_object_t obj) {

    BASE_IF_SPEED_t speed;
    if (int_type == nas_int_type_FC) {
        if (ndi_port_speed_get_nocheck(npu,port,&speed) == STD_ERR_OK) {
            cps_api_object_attr_add_u32(obj,DELL_IF_IF_INTERFACES_STATE_INTERFACE_NPU_SPEED, speed);
        }
    }
}

static void _if_fill_in_supported_speeds_attrs(npu_id_t npu, port_t port, nas_int_type_t int_type,
        cps_api_object_t obj) {

    size_t speed_count = NDI_PORT_SUPPORTED_SPEED_MAX;
    BASE_IF_SPEED_t speed_list[NDI_PORT_SUPPORTED_SPEED_MAX];
    if (int_type == nas_int_type_FC) {
        nas_fc_fill_supported_speed(npu, port, obj);
        return;
    }
    if (ndi_port_supported_speed_get(npu,port,&speed_count, speed_list) == STD_ERR_OK) {
        size_t mx = speed_count;
        for (size_t ix =0;ix < mx; ix++) {
            cps_api_object_attr_add_u32(obj,DELL_IF_IF_INTERFACES_STATE_INTERFACE_SUPPORTED_SPEED, speed_list[ix]);
        }
    }
}

/*
 * This routine will get the EEE state (enabled/disabled) for this
 * interface
 */
void _if_fill_in_eee_attrs (npu_id_t npu, port_t port, cps_api_object_t obj)
{
    uint_t state;
    uint16_t wake_time, idle_time;

    if ((ndi_port_eee_get(npu, port, &state) == STD_ERR_OK)
        && (ndi_port_eee_get_wake_time(npu, port, &wake_time) == STD_ERR_OK)
        && (ndi_port_eee_get_idle_time(npu, port, &idle_time) == STD_ERR_OK)) {
        cps_api_object_attr_add_u32(obj,
                                    DELL_IF_IF_INTERFACES_INTERFACE_EEE, state);
        cps_api_object_attr_add_u16(obj,
                                    DELL_IF_IF_INTERFACES_INTERFACE_TX_WAKE_TIME,
                                    wake_time);
        cps_api_object_attr_add_u16(obj,
                                    DELL_IF_IF_INTERFACES_INTERFACE_TX_IDLE_TIME,
                                    idle_time);
    }
}

#define SPEED_1MBPS (1000*1000)
#define SPEED_1GIGE (uint64_t)(1000*1000*1000)
static std::unordered_map<BASE_IF_SPEED_t, uint64_t ,std::hash<int>>
_base_to_ietf64bit_speed = {
    {BASE_IF_SPEED_0MBPS,                     0},
    {BASE_IF_SPEED_10MBPS,       10*SPEED_1MBPS},
    {BASE_IF_SPEED_100MBPS,     100*SPEED_1MBPS},
    {BASE_IF_SPEED_1GIGE,         1*SPEED_1GIGE},
    {BASE_IF_SPEED_10GIGE,       10*SPEED_1GIGE},
    {BASE_IF_SPEED_20GIGE,       20*SPEED_1GIGE},
    {BASE_IF_SPEED_25GIGE,       25*SPEED_1GIGE},
    {BASE_IF_SPEED_40GIGE,       40*SPEED_1GIGE},
    {BASE_IF_SPEED_50GIGE,       50*SPEED_1GIGE},
    {BASE_IF_SPEED_100GIGE,     100*SPEED_1GIGE},
};

static bool nas_base_to_ietf_state_speed(BASE_IF_SPEED_t speed, uint64_t *ietf_speed) {
    auto it = _base_to_ietf64bit_speed.find(speed);
    if (it != _base_to_ietf64bit_speed.end()) {
        *ietf_speed = it->second;
        return true;
    }
    return false;
}

static void _if_fill_in_speed_duplex_autoneg_state_attrs(npu_id_t npu, port_t port, cps_api_object_t obj) {
    BASE_IF_SPEED_t speed;
    if (ndi_port_speed_get(npu,port,&speed)==STD_ERR_OK) {
        uint64_t ietf_speed;
        if (nas_base_to_ietf_state_speed(speed, &ietf_speed)) {
            cps_api_object_attr_add_u64(obj, IF_INTERFACES_STATE_INTERFACE_SPEED,ietf_speed);
        } else {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", "speed Read error <%d %d> speed %d ",npu, port, speed);
        }
    }
    BASE_CMN_DUPLEX_TYPE_t duplex;
    if (ndi_port_duplex_get(npu,port,&duplex)==STD_ERR_OK) {
        cps_api_object_attr_add_u32(obj,DELL_IF_IF_INTERFACES_STATE_INTERFACE_DUPLEX, duplex);
    }
    bool auto_neg;
    if (ndi_port_auto_neg_get(npu, port, &auto_neg) == STD_ERR_OK) {
        cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_STATE_INTERFACE_AUTO_NEGOTIATION, auto_neg);
    }

    cps_api_object_attr_add(obj, IF_INTERFACES_STATE_INTERFACE_TYPE,
                            (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_ETHERNETCSMACD,
                            sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_ETHERNETCSMACD));
}

static void _if_fill_in_speed_duplex_attrs(npu_id_t npu, port_t port, cps_api_object_t obj) {

    _npu_port_t npu_port  = {(uint_t)npu, (uint_t)port};
    cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_SPEED,
        _logical_port_tbl[npu_port].configured_speed);

    BASE_CMN_DUPLEX_TYPE_t duplex;
    if (ndi_port_duplex_get(npu,port,&duplex)==STD_ERR_OK) {
        cps_api_object_attr_add_u32(obj,DELL_IF_IF_INTERFACES_INTERFACE_DUPLEX, duplex);
    }
}


static cps_api_return_code_t _if_fill_in_npu_attrs(npu_id_t npu, port_t port,
                                    nas_int_type_t int_type, cps_api_object_t obj) {
    IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS_t state;
    if (ndi_port_admin_state_get(npu,port,&state)==STD_ERR_OK) {
        cps_api_object_attr_delete(obj,IF_INTERFACES_INTERFACE_ENABLED);
        cps_api_object_attr_add_u32(obj,IF_INTERFACES_INTERFACE_ENABLED,
                ((state == IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS_UP) ? true : false));
    }

    BASE_IF_PHY_IF_INTERFACES_INTERFACE_TAGGING_MODE_t mode;
    if (ndi_port_get_untagged_port_attrib(npu,port,&mode)==STD_ERR_OK) {
        cps_api_object_attr_add_u32(obj,BASE_IF_PHY_IF_INTERFACES_INTERFACE_TAGGING_MODE,mode);
    }

    bool auto_neg;
    if (ndi_port_auto_neg_get(npu, port, &auto_neg) == STD_ERR_OK) {
        cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_AUTO_NEGOTIATION, auto_neg);
    }

    BASE_IF_PHY_MAC_LEARN_MODE_t learn_mode;
    if (ndi_port_mac_learn_mode_get(npu, port, &learn_mode) == STD_ERR_OK) {
        cps_api_object_attr_add_u32(obj, BASE_IF_PHY_IF_INTERFACES_INTERFACE_LEARN_MODE, learn_mode);
    }

    BASE_CMN_FEC_TYPE_t fec_mode;
    if (ndi_port_fec_get(npu, port, &fec_mode) == STD_ERR_OK) {
        cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_FEC, fec_mode);
    }

    uint32_t oui;
    if (ndi_port_oui_get(npu, port, &oui) == STD_ERR_OK) {
        cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_OUI, oui);
    }

    _if_fill_in_speed_duplex_attrs(npu,port, obj);
    _if_fill_in_supported_speeds_attrs(npu, port, int_type, obj);
    _if_fill_in_eee_attrs(npu, port, obj);

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _if_get_prev_from_cache(npu_id_t npu, port_t port,
        cps_api_object_t obj) {

    cps_api_object_list_guard lg(cps_api_object_list_create());
    if (lg.get()==nullptr) {
        return cps_api_ret_code_ERR;
    }

    if (nas_os_get_interface(obj,lg.get())!=STD_ERR_OK) {
        return cps_api_ret_code_ERR;
    }

    cps_api_object_t os_if = cps_api_object_list_get(lg.get(),0);

    if (cps_api_object_list_size(lg.get())!=1 || os_if==nullptr) {
        EV_LOGGING(INTERFACE,INFO,"NAS-INT-GET", "Failed to get interface info from OS for npu %d port %d ", npu, port);
    } else {
        cps_api_object_clone(obj,os_if);
    }
    return _if_fill_in_npu_attrs(npu,port,nas_int_type_PORT, obj);
}


static cps_api_return_code_t if_cpu_port_get (void * context, cps_api_get_params_t * param,
        size_t key_ix) {

    npu_id_t npu = 0;
    npu_id_t npu_max = (npu_id_t)nas_switch_get_max_npus();
    interface_ctrl_t _port;

    EV_LOGGING(INTERFACE,INFO,"NAS-INT-GET", "GET request received for CPU port interface");
    for ( ; npu < npu_max ; ++npu ) {
        memset(&_port,0,sizeof(_port));
        _port.q_type = HAL_INTF_INFO_FROM_IF_NAME;
        snprintf(_port.if_name,sizeof(_port.if_name),"npu-%d",npu); // cpu name is hard coded while creation
        if(dn_hal_get_interface_info(&_port) !=STD_ERR_OK) {
            continue;
        }
        cps_api_object_t obj = cps_api_object_list_create_obj_and_append(param->list);
        if (obj==NULL) return cps_api_ret_code_ERR;

        cps_api_key_from_attr_with_qual(cps_api_object_key(obj), DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                        cps_api_qualifier_TARGET);
        cps_api_set_key_data(obj, IF_INTERFACES_INTERFACE_NAME,
                       cps_api_object_ATTR_T_BIN, _port.if_name, strlen(_port.if_name)+1);
        cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
                                    _port.if_index);
        cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
                                    (const void *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_BASE_IF_CPU,
                                    strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_BASE_IF_CPU)+1);
    }
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t if_cpu_port_set(void * context, cps_api_transaction_params_t * param,size_t ix) {

    EV_LOGGING(INTERFACE,ERR,"NAS_CPU","Set command for CPU port is not supported");
    return cps_api_ret_code_ERR;
}

static cps_api_return_code_t if_get (void * context, cps_api_get_params_t * param,
        size_t key_ix) {

    cps_api_object_t filt = cps_api_object_list_get(param->filters,key_ix);
    cps_api_object_attr_t type_attr = cps_api_object_attr_get(filt, IF_INTERFACES_INTERFACE_TYPE);
    EV_LOG(INFO,INTERFACE,3,"NAS-INT-GET", "GET request received for physical interface");

    char *req_if_type = NULL;
    bool have_type_filter = false;
    if (type_attr != nullptr) {
        req_if_type = (char *)cps_api_object_attr_data_bin(type_attr);
        have_type_filter = true;
    }

    cps_api_object_attr_t ifix = cps_api_object_attr_get(filt,
                                      DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
    cps_api_object_attr_t name = cps_api_get_key_data(filt,IF_INTERFACES_INTERFACE_NAME);

    if (ifix == nullptr && name != NULL) {
        interface_ctrl_t _port;
        if (!if_data_from_obj(obj_INTF, filt,_port)) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT-GET", "Wrong interface name or interface not present %s",_port.if_name);
            return cps_api_ret_code_ERR;
        }
        cps_api_object_attr_add_u32(filt, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
                                    _port.if_index);
    }

    if (nas_os_get_interface(filt,param->list)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Failed to get interfaces from OS");
        return cps_api_ret_code_ERR;
    }
    size_t mx = cps_api_object_list_size(param->list);
    char if_type[256];
    size_t ix = 0;
    while (ix < mx) {
        cps_api_object_t object = cps_api_object_list_get(param->list,ix);
        cps_api_object_attr_t ifix = cps_api_object_attr_get(object,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
        cps_api_key_from_attr_with_qual(cps_api_object_key(object), DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                cps_api_qualifier_TARGET);
        interface_ctrl_t _port;
        if (ifix != nullptr) {
            memset(&_port,0,sizeof(_port));
            _port.if_index = cps_api_object_attr_data_u32(ifix);
            _port.q_type = HAL_INTF_INFO_FROM_IF;
            if(dn_hal_get_interface_info(&_port)==STD_ERR_OK) {
                if (!nas_to_ietf_if_type_get(_port.int_type, if_type, sizeof(if_type))) {
                    EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Failed to get IETF interface type for type id %d",
                               _port.int_type);
                    return cps_api_ret_code_ERR;
                }
                // TODO revisit the logic since this handler is always expecting interface type in the  get call
                if (have_type_filter) {
                    if(strncmp(if_type,req_if_type, sizeof(if_type)) != 0) {
                        cps_api_object_list_remove(param->list,ix);
                        cps_api_object_delete(object);
                        --mx;
                        continue;
                    }
                }
                cps_api_object_attr_add_u32(object,BASE_IF_PHY_IF_INTERFACES_INTERFACE_NPU_ID,_port.npu_id);
                cps_api_object_attr_add_u32(object,BASE_IF_PHY_IF_INTERFACES_INTERFACE_PORT_ID,_port.port_id);

                cps_api_object_attr_add(object,IF_INTERFACES_INTERFACE_TYPE,
                                                (const void *)if_type, strlen(if_type)+1);

                if (_port.port_mapped) {
                    if (_port.int_type == nas_int_type_FC) {
                        nas_fc_fill_intf_attr(_port.npu_id, _port.port_id, object);
                    } else {
                        _if_fill_in_npu_attrs(_port.npu_id, _port.port_id, _port.int_type, object);
                    }
                }

                if (_port.desc) {
                    cps_api_object_attr_add(object, IF_INTERFACES_INTERFACE_DESCRIPTION,
                                                (const void*)_port.desc, strlen(_port.desc) + 1);
                }
            } else {
                ifix = nullptr;    //use to indicate that we want to erase this entry
            }
        } else {
            EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Ifindex not found in object");
        }
        if (ifix != nullptr && _port.port_mapped) {
            _npu_port_t npu_port = {(uint_t)_port.npu_id, (uint_t)_port.port_id};
            std_rw_lock_read_guard g(&_logical_port_lock);
            auto it = _logical_port_tbl.find(npu_port);
            if (it != _logical_port_tbl.end()) {
                cps_api_object_attr_add_u32(object, DELL_IF_IF_INTERFACES_INTERFACE_MODE,
                        it->second.mode);
                cps_api_object_attr_add_u32(object, BASE_IF_PHY_IF_INTERFACES_INTERFACE_PHY_MEDIA,
                        it->second.media_type);
            }
        }
        if (ifix==nullptr) {
            cps_api_object_list_remove(param->list,ix);
            cps_api_object_delete(object);
            --mx;
            continue;
        }
        ++ix;
    }

    return cps_api_ret_code_OK;
}

static void _if_fill_in_npu_intf_state(npu_id_t npu_id, npu_port_t port_id, nas_int_type_t int_type,
                                        cps_api_object_t obj)
{
    t_std_error rc = STD_ERR_OK;
    IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS_t state;
    if (ndi_port_admin_state_get(npu_id,port_id,&state)==STD_ERR_OK) {
        cps_api_object_attr_add_u32(obj,IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS,state);
    }
    IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t oper_state;
    ndi_intf_link_state_t link_state;
    rc = ndi_port_link_state_get(npu_id,port_id,&link_state);
    if (rc==STD_ERR_OK) {
        oper_state = ndi_to_cps_oper_type(link_state.oper_status);
        cps_api_object_attr_add_u32(obj,IF_INTERFACES_STATE_INTERFACE_OPER_STATUS,oper_state);
    }

    BASE_CMN_FEC_TYPE_t fec_mode;
    if (ndi_port_fec_get(npu_id, port_id, &fec_mode) == STD_ERR_OK) {
        cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_STATE_INTERFACE_FEC, fec_mode);
    }

    uint32_t oui;
    if (ndi_port_oui_get(npu_id, port_id, &oui) == STD_ERR_OK) {
        cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_STATE_INTERFACE_OUI, oui);
    }

    if (int_type == nas_int_type_FC) {
        nas_fc_fill_speed_autoneg_state(npu_id, port_id, obj);
        nas_fc_fill_misc_state(npu_id, port_id, obj);
    } else {
        _if_fill_in_speed_duplex_autoneg_state_attrs(npu_id,port_id,obj);
    }
}

//TODO merge if_get and if_state_get in a common implementation
static cps_api_return_code_t if_state_get (void * context, cps_api_get_params_t * param,
        size_t key_ix) {

    cps_api_object_t filt = cps_api_object_list_get(param->filters,key_ix);
    cps_api_object_attr_t ifix = cps_api_object_attr_get(filt, IF_INTERFACES_STATE_INTERFACE_IF_INDEX);
    cps_api_object_attr_t name = cps_api_get_key_data(filt,IF_INTERFACES_STATE_INTERFACE_NAME);

    if (ifix != nullptr) {
        /*  Call os API with interface object  */
        cps_api_object_attr_add_u32(filt,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
                                 cps_api_object_attr_data_u32(ifix));
        cps_api_object_attr_delete(filt,IF_INTERFACES_STATE_INTERFACE_IF_INDEX);
    }
    if (ifix == nullptr && name != NULL) {
        interface_ctrl_t _port;
        if (!if_data_from_obj(obj_INTF_STATE, filt,_port)) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT-GET", "Wrong interface name or interface not present %s",_port.if_name);
            return cps_api_ret_code_ERR;
        }
        cps_api_object_attr_add_u32(filt, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
                                    _port.if_index);
    }
    cps_api_object_attr_t type_attr = cps_api_object_attr_get(filt, IF_INTERFACES_STATE_INTERFACE_TYPE);
    char *req_if_type = NULL;
    bool have_type_filter = false;
    if (type_attr != nullptr) {
        // Taking the pointer to if type once input filt object is done with all modifications
        req_if_type = (char *)cps_api_object_attr_data_bin(type_attr);
        have_type_filter = true;
    }

    if (nas_os_get_interface(filt,param->list)!=STD_ERR_OK) {
        return cps_api_ret_code_ERR;
    }
    size_t mx = cps_api_object_list_size(param->list);
    char if_type[256];
    size_t ix = 0;
    while (ix < mx) {
        cps_api_object_t object = cps_api_object_list_get(param->list,ix);
        cps_api_object_attr_t ifix = cps_api_object_attr_get(object, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
        cps_api_key_from_attr_with_qual(cps_api_object_key(object), DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_OBJ,
                cps_api_qualifier_OBSERVED);
        /*  TODO If if_index not present then extract from name */
        interface_ctrl_t _port;
        memset(&_port,0,sizeof(_port));
        if (ifix!=NULL) {
            _port.if_index = cps_api_object_attr_data_u32(ifix);
            _port.q_type = HAL_INTF_INFO_FROM_IF;
            if(dn_hal_get_interface_info(&_port)==STD_ERR_OK) {
                if (!nas_to_ietf_if_type_get(_port.int_type, if_type, sizeof(if_type))) {
                    EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Failed to get IETF interface type for type id %d",
                               _port.int_type);
                    return cps_api_ret_code_ERR;
                }
                // TODO revisit the logic since this handler is always expecting interface type in the  get call
                if (have_type_filter) {
                    if(strncmp(if_type,req_if_type, sizeof(if_type)) != 0) {
                        cps_api_object_list_remove(param->list,ix);
                        cps_api_object_delete(object);
                        --mx;
                        continue;
                    }
                }
                cps_api_set_key_data(object,IF_INTERFACES_STATE_INTERFACE_NAME,
                               cps_api_object_ATTR_T_BIN, _port.if_name, strlen(_port.if_name)+1);
                _if_fill_in_npu_intf_state(_port.npu_id, _port.port_id, _port.int_type, object);
                _if_fill_in_supported_speeds_attrs(_port.npu_id,_port.port_id, _port.int_type, object);
                _if_fill_in_eee_attrs(_port.npu_id,_port.port_id,object);
                _if_fill_in_npu_speed_attr(_port.npu_id,_port.port_id, _port.int_type, object);
                _if_fill_in_supported_autoneg_attr(_port.npu_id,_port.port_id, object);
                /*  Add if index with right key */
                cps_api_object_attr_add_u32(object,IF_INTERFACES_STATE_INTERFACE_IF_INDEX,
                                 _port.if_index);
                cps_api_object_attr_delete(object, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
                cps_api_object_attr_delete(object, IF_INTERFACES_INTERFACE_NAME);
                cps_api_object_attr_add(object,IF_INTERFACES_STATE_INTERFACE_TYPE,
                                                (const void *)if_type, strlen(if_type)+1);

            } else {
                ifix = nullptr;    //use to indicate that we want to erase this entry
            }
        }
        if (ifix==nullptr) {
            cps_api_object_list_remove(param->list,ix);
            cps_api_object_delete(object);
            --mx;
            continue;
        }
        ++ix;
    }

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t loopback_if_state_set(void * context, cps_api_transaction_params_t * param,size_t ix) {

    // not supposed to be called. return error
    return cps_api_ret_code_ERR;
}

static cps_api_return_code_t _set_attr_fail (npu_id_t npu, port_t port, cps_api_object_t obj) {
    //ignore these..
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_attr_name (npu_id_t npu, port_t port, cps_api_object_t obj) {
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_attr_tagging_mode (npu_id_t npu, port_t port, cps_api_object_t obj) {
    cps_api_object_attr_t attr = cps_api_object_attr_get(obj,BASE_IF_PHY_IF_INTERFACES_INTERFACE_TAGGING_MODE);
    if (attr == nullptr) {
        return cps_api_ret_code_ERR;
    }

    if (ndi_port_set_untagged_port_attrib(npu,port,
            (BASE_IF_PHY_IF_INTERFACES_INTERFACE_TAGGING_MODE_t)cps_api_object_attr_data_u32(attr))==STD_ERR_OK) {
        return cps_api_ret_code_OK;
    }
    return cps_api_ret_code_ERR;
}

static cps_api_return_code_t _set_attr_mac(npu_id_t npu, port_t port, cps_api_object_t obj) {
    cps_api_object_attr_t attr = cps_api_object_attr_get(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS);
    if (attr == nullptr) {
        return cps_api_ret_code_ERR;
    }

    if (nas_os_interface_set_attribute(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","OS update for MAC npu %d port %d",
                npu,port);
        return cps_api_ret_code_ERR;
    }
    /*  TODO Save MAC address in the intf control block */
    cps_api_object_attr_t _ifix = cps_api_object_attr_get(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
    if ((_ifix == nullptr)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-SET","update MAC failure for npu %d port %d", npu,port);
        return cps_api_ret_code_ERR;
    }
    hal_ifindex_t if_index = cps_api_object_attr_data_u32(_ifix);
    char *mac_addr = (char *)cps_api_object_attr_data_bin(attr);
    return (dn_hal_update_intf_mac(if_index, mac_addr));
}

static cps_api_return_code_t _set_attr_desc(npu_id_t npu, port_t port, cps_api_object_t obj) {
    cps_api_object_attr_t attr = cps_api_object_attr_get(obj, IF_INTERFACES_INTERFACE_DESCRIPTION);
    if (attr == nullptr) {
        return cps_api_ret_code_ERR;
    }

    /* TODO OS Update for interface description? */

    cps_api_object_attr_t _ifix = cps_api_object_attr_get(obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
    if ((_ifix == nullptr)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-SET","update intf desc failure for npu %d port %d", npu,port);
        return cps_api_ret_code_ERR;
    }
    hal_ifindex_t if_index = cps_api_object_attr_data_u32(_ifix);
    char *desc = (char *)cps_api_object_attr_data_bin(attr);
    return (dn_hal_update_intf_desc(if_index, desc));
}

static cps_api_return_code_t _set_attr_mtu (npu_id_t npu, port_t port, cps_api_object_t obj) {
    cps_api_object_attr_t attr = cps_api_object_attr_get(obj,DELL_IF_IF_INTERFACES_INTERFACE_MTU);
    if (attr == nullptr) {
        return cps_api_ret_code_ERR;
    }

    if (nas_os_interface_set_attribute(obj,DELL_IF_IF_INTERFACES_INTERFACE_MTU)!=STD_ERR_OK) {
        return cps_api_ret_code_ERR;
    }

    /* Adjust LINK HDR size while setting MTU in the NPU */
    uint_t mtu = cps_api_object_attr_data_u32(attr);
    if (ndi_port_mtu_set(npu,port, mtu)!=STD_ERR_OK) {
        if (ndi_port_mtu_get(npu,port,&mtu)==STD_ERR_OK) {
            cps_api_object_attr_delete(obj,DELL_IF_IF_INTERFACES_INTERFACE_MTU);
            cps_api_object_attr_add_u32(obj,DELL_IF_IF_INTERFACES_INTERFACE_MTU,mtu);
            nas_os_interface_set_attribute(obj,DELL_IF_IF_INTERFACES_INTERFACE_MTU);
        }
        return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_attr_eee (npu_id_t npu, port_t port, cps_api_object_t obj)
{
    cps_api_object_attr_t attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_EEE);
    if (attr == nullptr) {
        return cps_api_ret_code_ERR;
    }

    uint32_t state = cps_api_object_attr_data_u32(attr);

    /* Enable or disable the EEE (802.3az) feature */

    if ((state == 0) || (state == 1)) {
        if (ndi_port_eee_set(npu, port, state) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-EEE", "Failed to set %d:%d to state %d", npu, port, (int) state);
            return cps_api_ret_code_ERR;
        }
    } else {
        EV_LOGGING(INTERFACE, ERR, "NAS-EEE", "Invalid state %d", (int) state);
        return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_attr_up (npu_id_t npu, port_t port, cps_api_object_t obj) {
    cps_api_object_attr_t attr = cps_api_object_attr_get(obj,IF_INTERFACES_INTERFACE_ENABLED);
    cps_api_object_attr_t _ifix = cps_api_object_attr_get(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
    if ((attr == nullptr) || (_ifix == nullptr)) {
        return cps_api_ret_code_ERR;
    }

    bool state;
    bool revert;
    state = (bool) cps_api_object_attr_data_uint(attr); // TRUE is ADMIN UP and false ADMIN DOWN
    revert = (state == true) ? false : true;

    if (nas_os_interface_set_attribute(obj,IF_INTERFACES_INTERFACE_ENABLED)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","OS update for npu %d port %d to %d failed",
                npu,port,state);
        return cps_api_ret_code_ERR;
    }

    t_std_error rc = STD_ERR_OK;
    if ((rc=ndi_port_admin_state_set(npu,port,state))!=STD_ERR_OK) {
        cps_api_object_attr_delete(obj,IF_INTERFACES_INTERFACE_ENABLED);
        cps_api_object_attr_add_u32(obj,IF_INTERFACES_INTERFACE_ENABLED,revert);
        nas_os_interface_set_attribute(obj,IF_INTERFACES_INTERFACE_ENABLED);
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","NPU update for npu %d port %d to %d failed (%d)",
                npu,port,state,rc);
    }
    /*  Send admin state change event. some processes may be waiting on it */
    hal_ifindex_t if_index = cps_api_object_attr_data_u32(_ifix);
    nas_send_admin_state_event(if_index,
        (state == true) ? IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS_UP: IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS_DOWN);

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_mac_learn_mode(npu_id_t npu, port_t port, cps_api_object_t obj) {
    cps_api_object_attr_t mac_learn_mode = cps_api_object_attr_get(obj,
                                           BASE_IF_PHY_IF_INTERFACES_INTERFACE_LEARN_MODE);
    if (mac_learn_mode == nullptr) {
        return cps_api_ret_code_ERR;
    }
    BASE_IF_PHY_MAC_LEARN_MODE_t mode = (BASE_IF_PHY_MAC_LEARN_MODE_t)
                                       cps_api_object_attr_data_u32(mac_learn_mode);


    if (ndi_port_mac_learn_mode_set(npu,port,mode)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to update MAC Learn mode to %d for "
               "npu %d port %d",mode,npu,port);
        return cps_api_ret_code_ERR;
    }
    cps_api_object_attr_t _ifix = cps_api_object_attr_get(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
    if(_ifix){
        hal_ifindex_t ifindex = cps_api_object_attr_data_u32(_ifix);
        bool enable = true;
        if(mode == BASE_IF_PHY_MAC_LEARN_MODE_DROP || mode == BASE_IF_PHY_MAC_LEARN_MODE_DISABLE ){
            enable = false;
        }
        if(nas_os_mac_change_learning(ifindex,enable) != STD_ERR_OK){
            EV_LOG(INFO,INTERFACE,3,"NAS-IF-SET","Failed to update MAC learn mode in Linux kernel");
        }
    }else{
        EV_LOG(INFO,INTERFACE,3,"NAS-IF-SET","No ifindex in the object to update mac learn mode");
    }
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_phy_media(npu_id_t npu, port_t port, cps_api_object_t obj) {

    cps_api_object_attr_t _phy_media = cps_api_object_attr_get(obj,
                                           BASE_IF_PHY_IF_INTERFACES_INTERFACE_PHY_MEDIA);
    if (_phy_media == nullptr) {
        return cps_api_ret_code_ERR;
    }
    PLATFORM_MEDIA_TYPE_t media_type = (PLATFORM_MEDIA_TYPE_t)cps_api_object_attr_data_u32(_phy_media);

    if (ndi_port_media_type_set(npu, port, media_type)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to set media_type %d for "
               "npu %d port %d",media_type,npu,port);
        return cps_api_ret_code_ERR;
    }

    _npu_port_t npu_port  = {(uint_t)npu, (uint_t)port};
    std_rw_lock_write_guard g(&_logical_port_lock);
    _logical_port_tbl[npu_port].media_type = media_type;

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_hw_profile (npu_id_t npu, port_t port, cps_api_object_t obj) {

    cps_api_object_attr_t _hw_profile = cps_api_object_attr_get(obj,
                                           BASE_IF_PHY_IF_INTERFACES_INTERFACE_HW_PROFILE);
    if(_hw_profile == nullptr) {
        return cps_api_ret_code_ERR;
    }

    uint32_t hw_profile = cps_api_object_attr_data_uint(_hw_profile);

    if (ndi_port_hw_profile_set(npu, port, hw_profile)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to set hw_profile %d for "
               "npu %d port %d", hw_profile, npu, port);
        return cps_api_ret_code_ERR;
    }
    EV_LOGGING(INTERFACE,INFO,"NAS-IF-REG"," set hw profile to %d for "
               "npu %d port %d",hw_profile,npu,port);
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_identification_led (npu_id_t npu, port_t port, cps_api_object_t obj) {

    cps_api_object_attr_t _ident_led = cps_api_object_attr_get(obj,
                                           BASE_IF_PHY_IF_INTERFACES_INTERFACE_IDENTIFICATION_LED);
    if(_ident_led == nullptr) {
        return cps_api_ret_code_ERR;
    }

    bool state = (bool) cps_api_object_attr_data_uint(_ident_led);

    if (ndi_port_identification_led_set(npu, port, state)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to set identification led state %d for "
               "npu %d port %d", state, npu, port);
        return cps_api_ret_code_ERR;
    }
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_speed(npu_id_t npu, port_t port, cps_api_object_t obj) {

    cps_api_object_attr_t _speed = cps_api_object_attr_get(obj,
                                           DELL_IF_IF_INTERFACES_INTERFACE_SPEED);
    _npu_port_t npu_port  = {(uint_t)npu, (uint_t)port};

    if(_speed == nullptr) {
        return cps_api_ret_code_ERR;
    }
    BASE_IF_SPEED_t speed = (BASE_IF_SPEED_t)cps_api_object_attr_data_u32(_speed);

    if (ndi_port_speed_set(npu,port,speed)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to set speed %d for "
               "npu %d port %d",speed,npu,port);
        return cps_api_ret_code_ERR;
    }
    _logical_port_tbl[npu_port].configured_speed = speed;
    EV_LOGGING(INTERFACE,INFO,"NAS-IF-REG","set speed %d for npu %d port %d",speed,npu,port);
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_supported_autoneg(npu_id_t npu, port_t port, cps_api_object_t obj) {

    _npu_port_t npu_port  = {(uint_t)npu, (uint_t)port};
    BASE_IF_SUPPORTED_AUTONEG_t supported_autoneg = BASE_IF_SUPPORTED_AUTONEG_BOTH_SUPPORTED;
    cps_api_object_attr_t supported_autoneg_attr = cps_api_object_attr_get(obj,
                                           BASE_IF_PHY_IF_INTERFACES_INTERFACE_SUPPORTED_AUTONEG);
    if(supported_autoneg_attr == nullptr) {
        return cps_api_ret_code_ERR;
    }
    supported_autoneg = (BASE_IF_SUPPORTED_AUTONEG_t) cps_api_object_attr_data_uint(supported_autoneg_attr);
    _logical_port_tbl[npu_port].supported_autoneg = supported_autoneg;
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_auto_neg(npu_id_t npu, port_t port, cps_api_object_t obj) {

    cps_api_object_attr_t autoneg_attr = cps_api_object_attr_get(obj,
                                           DELL_IF_IF_INTERFACES_INTERFACE_AUTO_NEGOTIATION);
    if(autoneg_attr == nullptr) {
        return cps_api_ret_code_ERR;

    }
    bool enable = (bool) cps_api_object_attr_data_uint(autoneg_attr);

    if (ndi_port_auto_neg_set(npu,port,enable)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to set auto negotiation to %d for "
               "npu %d port %d",enable,npu,port);
        return cps_api_ret_code_ERR;
    }
    EV_LOGGING(INTERFACE,INFO,"NAS-IF-REG"," set auto negotiation to %d for "
               "npu %d port %d",enable,npu,port);
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_duplex_mode(npu_id_t npu, port_t port, cps_api_object_t obj) {

    cps_api_object_attr_t duplex_mode = cps_api_object_attr_get(obj,
                                           DELL_IF_IF_INTERFACES_INTERFACE_DUPLEX);
    if(duplex_mode == nullptr) {
        return cps_api_ret_code_ERR;
    }
    BASE_CMN_DUPLEX_TYPE_t mode = (BASE_CMN_DUPLEX_TYPE_t)
                                       cps_api_object_attr_data_u32(duplex_mode);

    if (ndi_port_duplex_set(npu,port,mode)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to set duplex mode to %d for "
               "npu %d port %d",mode,npu,port);
        return cps_api_ret_code_ERR;
    }
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_attr_mode(npu_id_t npu, port_t port, cps_api_object_t obj) {
    _npu_port_t npu_port  = {(uint_t)npu, (uint_t)port};
    cps_api_object_attr_t if_mode_attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_MODE);

    if(if_mode_attr == nullptr) {
        return cps_api_ret_code_ERR;
    }
    BASE_IF_MODE_t mode = (BASE_IF_MODE_t)cps_api_object_attr_data_u32(if_mode_attr);
    std_rw_lock_write_guard g(&_logical_port_lock);
    _logical_port_tbl[npu_port].mode = mode;

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_attr_fec(npu_id_t npu, port_t port, cps_api_object_t obj) {
    cps_api_object_attr_t fec_attr = cps_api_object_attr_get(obj,
                                        DELL_IF_IF_INTERFACES_INTERFACE_FEC);
    if (fec_attr == nullptr) {
        EV_LOGGING(INTERFACE, ERR, "NAS-IF-UPDATE", "No FEC mode attribute");
        return cps_api_ret_code_ERR;
    }
    BASE_CMN_FEC_TYPE_t fec_mode = (BASE_CMN_FEC_TYPE_t)
                                       cps_api_object_attr_data_u32(fec_attr);

    if (ndi_port_fec_set(npu, port, fec_mode) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-UPDATE","Failed to set FEC mode to %d for "
               "npu %d port %d", fec_mode, npu, port);
        return cps_api_ret_code_ERR;
    }
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_attr_oui(npu_id_t npu, port_t port, cps_api_object_t obj) {
    cps_api_object_attr_t oui_attr = cps_api_object_attr_get(obj,
                                        DELL_IF_IF_INTERFACES_INTERFACE_OUI);
    if (oui_attr == nullptr) {
        EV_LOGGING(INTERFACE, ERR, "NAS-IF-UPDATE", "No OUI number attribute");
        return cps_api_ret_code_ERR;
    }
    uint32_t oui = cps_api_object_attr_data_u32(oui_attr);

    if (ndi_port_oui_set(npu, port, oui) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-UPDATE","Failed to set OUI to %d for "
               "npu %d port %d", oui, npu, port);
        return cps_api_ret_code_ERR;
    }
    return cps_api_ret_code_OK;
}

using intf_set_attr_handler_t = cps_api_return_code_t (*)(npu_id_t, port_t,cps_api_object_t);
static const std::unordered_map<cps_api_attr_id_t,
    std::pair<intf_set_attr_handler_t, const char*>> _set_attr_handlers = {
        { IF_INTERFACES_INTERFACE_NAME, {_set_attr_name, "name"} },
        { IF_INTERFACES_INTERFACE_ENABLED, {_set_attr_up, "enabled"} },
        { DELL_IF_IF_INTERFACES_INTERFACE_MTU, {_set_attr_mtu, "mtu"} },
        { IF_INTERFACES_STATE_INTERFACE_OPER_STATUS, {_set_attr_fail, "oper_status"} },
        { BASE_IF_PHY_IF_INTERFACES_INTERFACE_TAGGING_MODE, {_set_attr_tagging_mode, "tagging_mode"} },
        { DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS, {_set_attr_mac, "mac_addr"} },
        { BASE_IF_PHY_IF_INTERFACES_INTERFACE_LEARN_MODE, {_set_mac_learn_mode, "mac_learn_mode"} },
        { BASE_IF_PHY_IF_INTERFACES_INTERFACE_PHY_MEDIA, {_set_phy_media, "phy_media"} },
        { BASE_IF_PHY_IF_INTERFACES_INTERFACE_IDENTIFICATION_LED, {_set_identification_led, "identification_led"} },
        { BASE_IF_PHY_IF_INTERFACES_INTERFACE_HW_PROFILE, {_set_hw_profile, "hw_profile"} },
        { BASE_IF_PHY_IF_INTERFACES_INTERFACE_SUPPORTED_AUTONEG, {_set_supported_autoneg, "supported_autoneg"} },
        { DELL_IF_IF_INTERFACES_INTERFACE_SPEED, {_set_speed, "speed"} },
        { DELL_IF_IF_INTERFACES_INTERFACE_AUTO_NEGOTIATION, {_set_auto_neg, "autoneg"} },
        { DELL_IF_IF_INTERFACES_INTERFACE_DUPLEX, {_set_duplex_mode, "duplex_mode"} },
        { DELL_IF_IF_INTERFACES_INTERFACE_MODE, {_set_attr_mode, "mode"} },
        { DELL_IF_IF_INTERFACES_INTERFACE_EEE, {_set_attr_eee, "eee"} },
        { DELL_IF_IF_INTERFACES_INTERFACE_FEC, {_set_attr_fec, "fec"} },
        { DELL_IF_IF_INTERFACES_INTERFACE_OUI, {_set_attr_oui, "oui"} },
        { IF_INTERFACES_INTERFACE_DESCRIPTION, {_set_attr_desc, "description"}}
};

static void remove_same_values(cps_api_object_t now, cps_api_object_t req) {
    cps_api_object_it_t it;
    cps_api_object_it_begin(now,&it);
    for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it)) {
        cps_api_attr_id_t id = cps_api_object_attr_id(it.attr);

        if (_set_attr_handlers.find(id)==_set_attr_handlers.end()) continue;

        cps_api_object_attr_t new_val = cps_api_object_e_get(req,&id,1);
        if (new_val==nullptr) continue;
        if (cps_api_object_attrs_compare(it.attr,new_val)!=0) continue;
        if (id == IF_INTERFACES_INTERFACE_ENABLED) continue; // temp fix AR3331
        if (id == IF_INTERFACES_INTERFACE_NAME) continue;
        if (id == DELL_IF_IF_INTERFACES_INTERFACE_SPEED) continue; // configuring speed when port is in negotiated mode
        if (id == DELL_IF_IF_INTERFACES_INTERFACE_FEC) {
            BASE_CMN_FEC_TYPE_t fec_mode = (BASE_CMN_FEC_TYPE_t)
                                       cps_api_object_attr_data_u32(new_val);
            if (fec_mode == BASE_CMN_FEC_TYPE_OFF) {
                // if link is down, SAI always returns FEC off without giving actual FEC configuration
                // so we need to actually set it to off to make link up if peer is FEC off
                continue;
            }
        }
        cps_api_object_attr_delete(req,id);
    }
}

static cps_api_return_code_t _if_delete(cps_api_object_t req_if, cps_api_object_t prev) {
    if (( prev == nullptr) ||  (req_if == nullptr)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-INT-SET", "Request Intf Object is Null for Delete Interface");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_t _ifix = cps_api_object_attr_get(req_if,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
    cps_api_object_attr_t _name = cps_api_get_key_data(req_if,IF_INTERFACES_INTERFACE_NAME);

    if (_ifix==NULL && _name==NULL) {    //requires this at a minimum
        return cps_api_ret_code_ERR;
    }

    interface_ctrl_t _port;
    if (!if_data_from_obj(obj_INTF, req_if,_port)) {
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_add_u32(prev,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,_port.if_index);
    if(_ifix == NULL) {
        cps_api_object_attr_add_u32(req_if,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,_port.if_index);
    }

    if(nas_int_port_delete(_port.if_name)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-IF-REG", "Failed to delete logical interface %s",
                   _port.if_name);
        return cps_api_ret_code_ERR;
    }
    //force the state to be down at the time of interface delete since there will
    //be no way to do this after the interface is deleted
    ndi_port_admin_state_set(_port.npu_id,_port.port_id, false);

    if(cps_api_db_commit_one(cps_api_oper_DELETE,req_if,nullptr,false)!= cps_api_ret_code_OK){
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-SET","Failed to write physical interface object to db");
    }

    cps_api_key_set(cps_api_object_key(req_if),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_OBSERVED);
    cps_api_event_thread_publish(req_if);
    cps_api_key_set(cps_api_object_key(req_if),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_TARGET);

    return cps_api_ret_code_OK;
}

static void if_rollback(cps_api_object_t rollback,interface_ctrl_t _port) {
    cps_api_object_it_t it_rollback;
    cps_api_object_it_begin(rollback,&it_rollback);
    for ( ; cps_api_object_it_valid(&it_rollback) ; cps_api_object_it_next(&it_rollback)) {
        cps_api_attr_id_t id_rollback = cps_api_object_attr_id(it_rollback.attr);
        auto func = _set_attr_handlers.find(id_rollback);
        if (func ==_set_attr_handlers.end()) continue;
        EV_LOGGING(INTERFACE, INFO, "NAS-IF-REG", "Calling rollback attribute %s for interface %s",
                   func->second.second, _port.if_name);
        cps_api_return_code_t ret = func->second.first(_port.npu_id,_port.port_id,rollback);
        if (ret!=cps_api_ret_code_OK)
            EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to rollback Attribute %s for interface %s",
                       func->second.second, _port.if_name);
    }
}

static cps_api_return_code_t _if_update(cps_api_object_t req_if, cps_api_object_t prev, bool if_set)
{
    if (( prev == nullptr) ||  (req_if == nullptr)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-INT-SET", "Requested Intf Object is Null for Interface update ");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_t _ifix = cps_api_object_attr_get(req_if,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
    cps_api_object_attr_t _name = cps_api_get_key_data(req_if,IF_INTERFACES_INTERFACE_NAME);
    if (_ifix==NULL && _name==NULL) {    //requires this at a minimum
        return cps_api_ret_code_ERR;
    }

    interface_ctrl_t _port;
    if (!if_data_from_obj(obj_INTF, req_if,_port)) {
        return cps_api_ret_code_ERR;
    }

    bool connect = false;
    cps_api_object_attr_t npu_attr = cps_api_object_attr_get(req_if,BASE_IF_PHY_IF_INTERFACES_INTERFACE_NPU_ID);
    cps_api_object_attr_t port_attr = cps_api_object_attr_get(req_if,BASE_IF_PHY_IF_INTERFACES_INTERFACE_PORT_ID);
    bool rpt_disconn_port = false;
    port_t disconn_port = 0;
    npu_id_t disconn_npu = 0;

    if (if_set && npu_attr != nullptr && port_attr != nullptr) {
        npu_id_t npu;
        port_t port;
        if (cps_api_object_attr_len(npu_attr) > 0 && cps_api_object_attr_len(port_attr) > 0) {
            npu = cps_api_object_attr_data_u32(npu_attr);
            port = cps_api_object_attr_data_u32(port_attr);
            connect = true;
        } else {
            disconn_npu = npu = _port.npu_id;
            /* record npu port to be disconnected. the port id will be published for related modules
               to do cleanup on interface disassociated from physical port */
            disconn_port = port = _port.port_id;
            rpt_disconn_port = true;
            /*
             * In case of dis-association port might have been moved from all VLANs, in that case
             * nas-l3 might have created the RIF. Triggering the mode change to L2 here so nas-l3 can
             * cleanup the L3 configuration of port being dis-associated.
             */
            EV_LOGGING(INTERFACE,DEBUG,"IF-DISCONN","Port %d is going to be disconncted changing its mode"
                    " to L2",_port.if_index);
            if(!nas_intf_handle_intf_mode_change(_port.if_index,BASE_IF_MODE_MODE_L2)){
                EV_LOGGING(INTERFACE,ERR,"IF-DISCONN","Failed to change the mode to L2 for port %d"
                                    "to L2",_port.if_index);
            }

        }

        if (nas_int_update_npu_port(_port.if_name, npu, port, connect) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to update npu port for interface  %s",
                       _port.if_name);
            return cps_api_ret_code_ERR;
        }
        std::string tmp_name{_port.if_name};
        memset(&_port, 0, sizeof(_port));
        strncpy(_port.if_name, tmp_name.c_str(), sizeof(_port.if_name) - 1);
        _port.q_type = HAL_INTF_INFO_FROM_IF_NAME;
        if (dn_hal_get_interface_info(&_port) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to get info of interface  %s",_port.if_name);
            return cps_api_ret_code_ERR;
        }

        if(connect){

            cps_api_object_guard _og(cps_api_object_create());
            if(!_og.valid()){
                EV_LOGGING(INTERFACE,ERR,"INT-DB-GET","Failed to create object for db get");
                return cps_api_ret_code_ERR;
            }
            if(nas_intf_db_obj_get(_port.if_name,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                    cps_api_qualifier_TARGET,_og.get()) != STD_ERR_OK ){
                EV_LOGGING(INTERFACE,ERR,
                        "INT-DB-GET","Failed to get object from DB for interface %s",
                        _port.if_name);
                return cps_api_ret_code_ERR;
            }
            if (!cps_api_object_attr_merge(_og.get(), req_if, true)) {
                EV_LOGGING(INTERFACE,ERR,"INT-DB-GET","Failure, object merge");
                return cps_api_ret_code_ERR;
            }
            if (!cps_api_object_clone(req_if, _og.get())) {
                EV_LOGGING(INTERFACE,ERR,"INT-DB-GET","Failure, object clone");
                return cps_api_ret_code_ERR;
            }
            cps_api_key_set_attr(cps_api_object_key(req_if), cps_api_oper_SET);
        }
    }

    cps_api_object_attr_add_u32(prev,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,_port.if_index);
    if(_ifix == NULL) {
        cps_api_object_attr_add_u32(req_if,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,_port.if_index);
    }

    if (_port.port_mapped) {
        if(!connect){
            if (_if_get_prev_from_cache(_port.npu_id,_port.port_id,prev)) {
                EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to get configured attributes for interface  %s",
                           _port.if_name);
                return cps_api_ret_code_ERR;
            }
            remove_same_values(prev,req_if);
        }
        /* Save the configured attributes in rollback object and Apply
         * rollback object in case if any attribute config fails.
         * */
        cps_api_object_t rollback = cps_api_object_create();
        cps_api_object_attr_add_u32(rollback,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,_port.if_index);
        if (rollback == NULL) return cps_api_ret_code_ERR;
        cps_api_object_it_t it_req;
        cps_api_object_it_begin(req_if,&it_req);
        cps_api_return_code_t ret;

        for ( ; cps_api_object_it_valid(&it_req); cps_api_object_it_next(&it_req)) {
            cps_api_attr_id_t id = cps_api_object_attr_id(it_req.attr);
            auto func = _set_attr_handlers.find(id);
            if (func ==_set_attr_handlers.end()) continue;
            if ( _port.int_type == nas_int_type_FC &&
                (id  == DELL_IF_IF_INTERFACES_INTERFACE_AUTO_NEGOTIATION ||
                 id  == DELL_IF_IF_INTERFACES_INTERFACE_SPEED ||
                 id  == DELL_IF_IF_INTERFACES_STATE_INTERFACE_DUPLEX)) {
                continue; /*  AN/Speed/Duplex should not be set in the NPU for FC interface */
            }
            EV_LOGGING(INTERFACE, INFO, "NAS-IF-REG", "Set attribute %s (%" PRId64 ") for interface %s",
                       func->second.second, id, _port.if_name);
            ret = func->second.first(_port.npu_id,_port.port_id,req_if);
            if (ret!=cps_api_ret_code_OK) {
                EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to set Attribute %s (%" PRId64 ") for interface  %s",
                           func->second.second, id, _port.if_name);
                if_rollback(rollback,_port);
                cps_api_object_delete(rollback);
                return ret;
            } else {
                const void *data = cps_api_object_get_data(prev, id);
                if (NULL == data)
                        continue;
                cps_api_object_attr_add(rollback, id, data, cps_api_object_attr_len(it_req.attr));
            }
        }
        cps_api_object_delete(rollback);
    }

    /*  Update CPS DB with the interface object for persistency */
    if(cps_api_db_commit_one(cps_api_oper_SET,req_if,nullptr,false)!= cps_api_ret_code_OK){
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-SET","Failed to write physical interface object to db");
    }

    if (_port.int_type == nas_int_type_FC) {
        if (nas_cps_set_fc_attr(_port.npu_id,_port.port_id, req_if,  prev) != cps_api_ret_code_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-IF","Failed to set Attribute  for FC interface  %s",_port.if_name);
            return cps_api_ret_code_ERR;
        }
    }

    /*  Publish the INTF object before returning from SET request */
    if (rpt_disconn_port) {
        cps_api_object_attr_delete(req_if, BASE_IF_PHY_IF_INTERFACES_INTERFACE_PORT_ID);
        cps_api_object_attr_add_u32(req_if, BASE_IF_PHY_IF_INTERFACES_INTERFACE_PORT_ID,
                                    disconn_port);
        cps_api_object_attr_delete(req_if, BASE_IF_PHY_IF_INTERFACES_INTERFACE_NPU_ID);
        cps_api_object_attr_add_u32(req_if, BASE_IF_PHY_IF_INTERFACES_INTERFACE_NPU_ID,
                                    disconn_npu);
    }
    cps_api_key_set(cps_api_object_key(req_if),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_OBSERVED);
    cps_api_event_thread_publish(req_if);
    cps_api_key_set(cps_api_object_key(req_if),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_TARGET);
    if (rpt_disconn_port) {
        cps_api_object_attr_delete(req_if, BASE_IF_PHY_IF_INTERFACES_INTERFACE_PORT_ID);
        cps_api_object_attr_delete(req_if, BASE_IF_PHY_IF_INTERFACES_INTERFACE_NPU_ID);
    }

    return cps_api_ret_code_OK;
}

static void if_add_tracker_for_reload(cps_api_object_t cur, bool mapped, npu_id_t npu, port_t port) {
    char alias[100]; //enough for npu and port
    if (mapped) {
        snprintf(alias,sizeof(alias),"NAS## %d %d",npu,port);
    } else {
        snprintf(alias,sizeof(alias),"NAS##");
    }
    cps_api_object_attr_add(cur,NAS_OS_IF_ALIAS,alias,strlen(alias)+1);
    nas_os_interface_set_attribute(cur,NAS_OS_IF_ALIAS);
}

t_std_error update_if_tracker(const char *name, npu_id_t npu, port_t port, bool connect)
{
    hal_ifindex_t if_index = 0;
    if (nas_int_name_to_if_index(&if_index, name) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "INTF-UPDATE", "Failed to get ifindex for interface %s",
                   name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    cps_api_object_t obj = cps_api_object_create();
    if (obj == nullptr) {
        EV_LOGGING(INTERFACE, ERR, "INTF-UPDATE", "Failed to create cps object");
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
                                if_index);
    if (!connect) {
        npu = port = 0;
    }
    if_add_tracker_for_reload(obj, connect, npu, port);
    nas_os_interface_set_attribute(obj, NAS_OS_IF_ALIAS);
    return STD_ERR_OK;
}

static bool if_get_tracker_details(cps_api_object_t cur, bool& mapped, npu_id_t &npu,port_t &port) {
    cps_api_object_attr_t attr = cps_api_object_attr_get(cur,NAS_OS_IF_ALIAS);
    if (attr==NULL) return false;

    std_parsed_string_t handle;
    if (!std_parse_string(&handle,(const char *)cps_api_object_attr_data_bin(attr)," ")) {
        return false;
    }

    bool rc = false;
    const char *_hash= std_parse_string_at(handle,0); //from token
    size_t tok_num = std_parse_string_num_tokens(handle);
    do {
        if (_hash == nullptr || strcmp("NAS##", _hash) != 0) {
            break;
        }
        if (tok_num >= 3) {
            mapped = true;
            const char *_npu = std_parse_string_at(handle, 1);
            const char *_port = std_parse_string_at(handle, 2);
            if (_npu == nullptr || _port == nullptr) {
                break;
            }
            npu = (npu_id_t)atoi(_npu);
            port = (port_t)atoi(_port);
        } else {
            mapped = false;
        }
        rc = true;
    } while(0);

    std_parse_string_free(handle);
    return rc;
}


static cps_api_return_code_t _if_create(cps_api_object_t cur, cps_api_object_t prev) {

    cps_api_object_attr_t _name = cps_api_get_key_data(cur,IF_INTERFACES_INTERFACE_NAME);
    cps_api_object_attr_t _npu = cps_api_object_attr_get(cur,BASE_IF_PHY_IF_INTERFACES_INTERFACE_NPU_ID);
    cps_api_object_attr_t _port = cps_api_object_attr_get(cur,BASE_IF_PHY_IF_INTERFACES_INTERFACE_PORT_ID);
    cps_api_object_attr_t _ietf_type = cps_api_object_attr_get(cur, IF_INTERFACES_INTERFACE_TYPE);
    cps_api_object_attr_t _ietf_desc = cps_api_object_attr_get(cur, IF_INTERFACES_INTERFACE_DESCRIPTION);

    if (_name == nullptr) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-CREATE", "Interface create fail: Name not present ");
        return cps_api_ret_code_ERR;
    }

    nas_int_type_t  _type;
    if  ( _ietf_type != nullptr) {
        const char *ietf_intf_type = (const char *)cps_api_object_attr_data_bin(_ietf_type);
        if (ietf_to_nas_if_type_get(ietf_intf_type, &_type) == false) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT-CREATE","Could not convert the %s type to nas if type",ietf_intf_type);
            return cps_api_ret_code_ERR;
        }
    } else {
        _type = nas_int_type_PORT;

    }

    const char *name = (const char*)cps_api_object_attr_data_bin(_name);
    EV_LOGGING(INTERFACE,INFO,"NAS-INT-CREATE", "Interface create received for  %s", name);

    char *desc = NULL;
    if (_ietf_desc != nullptr) {
        desc = (char*)cps_api_object_attr_data_bin(_ietf_desc);
    }

    t_std_error rc;
    bool npu_port_present = false;
    npu_id_t npu = 0, port = 0;
    if (_npu != nullptr && _port != nullptr) {
        npu = cps_api_object_attr_data_u32(_npu);
        port = cps_api_object_attr_data_u32(_port);
        if (nas_int_port_id_used(npu, port)) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT-CREATE", "Interface already exists at %d:%d",
                       (int)npu,(int)port);
            return (cps_api_return_code_t)STD_ERR(INTERFACE,PARAM,0);
        }
        npu_port_present = true;
    }

    if (nas_int_port_name_used(name)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-CREATE", "Interface %s already exists",
                   name);
        return (cps_api_return_code_t)STD_ERR(INTERFACE,PARAM,0);
    }

    EV_LOGGING(INTERFACE, INFO, "NAS-INT-CREATE", "Create interface %s for npu %d port %d",
               name, (int)npu, (int)port);
    if (npu_port_present) {
        rc = nas_int_port_create_mapped(npu, port, name, desc, _type);
    } else {
        rc = nas_int_port_create_unmapped(name, desc, _type);
    }
    if (rc != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-INT-CREATE", "Failed to create interface");
        return (cps_api_return_code_t)STD_ERR(INTERFACE, FAIL, 0);
    }

    if (npu_port_present) {
        ndi_intf_link_state_t link_state;
        IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t state = IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_DOWN;

        rc = ndi_port_link_state_get(npu,port,&link_state);
        if (rc==STD_ERR_OK) {
            state = ndi_to_cps_oper_type(link_state.oper_status);
            EV_LOGGING(INTERFACE, DEBUG, "NAS-INT-CREATE", "Interface %s initial link state is %d",name,state);
            nas_int_port_link_change(npu,port,state);
        }
    }

    hal_ifindex_t ifix = npu_port_present ? nas_int_ifindex_from_port(npu,port) :
                                            nas_int_ifindex_from_name(name);
    if (ifix != -1) {
        EV_LOGGING(INTERFACE,NOTICE,"NAS-INT-CREATE", "Interface created name %s and index %d ",
                   name, ifix);
        cps_api_object_attr_add_u32(cur, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
                                    ifix);

        if_add_tracker_for_reload(cur, npu_port_present, npu, port);

        if(cps_api_db_commit_one(cps_api_oper_CREATE,cur,nullptr,false)!= cps_api_ret_code_OK){
            EV_LOGGING(INTERFACE,ERR,"NAS-INT-SET","Failed to write physical interface object to db");
        }

        cps_api_return_code_t ret = cps_api_ret_code_ERR;
        if ((ret = _if_update(cur,prev,false)) != cps_api_ret_code_OK) {
            return ret;
        }

        return cps_api_ret_code_OK;
    }
    return cps_api_ret_code_ERR;
}

static cps_api_return_code_t if_set(void * context, cps_api_transaction_params_t * param,size_t ix) {
    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    if (obj==nullptr) return cps_api_ret_code_ERR;

    cps_api_object_t prev = cps_api_object_list_create_obj_and_append(param->prev);
    if (prev==nullptr) return cps_api_ret_code_ERR;

    EV_LOGGING(INTERFACE,INFO,"NAS-INT-SET", "SET request received for physical interface");
    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if (op==cps_api_oper_CREATE) return _if_create(obj,prev);
    if (op==cps_api_oper_SET) return _if_update(obj,prev,true);
    if (op==cps_api_oper_DELETE) return _if_delete(obj,prev);

    return cps_api_ret_code_ERR;
}


static void nas_int_oper_state_cb(npu_id_t npu, npu_port_t port,
                                  IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t status)
{

    char buff[CPS_API_MIN_OBJ_LEN];
    cps_api_object_t obj = cps_api_object_init(buff,sizeof(buff));

    interface_ctrl_t _port;
    memset(&_port,0,sizeof(_port));
    _port.npu_id = npu;
    _port.port_id = port;
    _port.q_type = HAL_INTF_INFO_FROM_PORT;
    EV_LOGGING(INTERFACE,INFO,
               "NAS-INTF-EVENT","Entering interface state change callback: npu %d port %d status %d",
               npu, port, status);
    if (dn_hal_get_interface_info(&_port)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE, INFO, "NAS-INTF-EVENT", "Interface info not found for npu %d port %d",
                   npu, port);
        return;
    }

    nas_int_port_link_change(npu,port,status);

    if (!cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_OBJ,
                cps_api_qualifier_OBSERVED)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG",
                   "Could not translate to logical interface key for npu %d port %d",npu,port);
        return;
    }

    cps_api_set_key_data(obj,IF_INTERFACES_STATE_INTERFACE_NAME,
                               cps_api_object_ATTR_T_BIN, _port.if_name, strlen(_port.if_name)+1);
    cps_api_object_attr_add_u32(obj,IF_INTERFACES_STATE_INTERFACE_IF_INDEX,_port.if_index);
    cps_api_object_attr_add_u32(obj,IF_INTERFACES_STATE_INTERFACE_OPER_STATUS,
            status);

    if (_port.int_type == nas_int_type_FC) {
        nas_fc_fill_speed_autoneg_state(npu, port, obj);
    } else {
        _if_fill_in_speed_duplex_autoneg_state_attrs(npu,port,obj);
    }

    BASE_CMN_FEC_TYPE_t fec_mode;
    if (ndi_port_fec_get(npu, port, &fec_mode) == STD_ERR_OK) {
        cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_STATE_INTERFACE_FEC, fec_mode);
    }

    EV_LOGGING(INTERFACE,INFO,"NAS-INTF-EVENT",
               "Sending oper event notification for interface %s: oper_status %d",
               _port.if_name, status);
    hal_interface_send_event(obj);
}

static void resync_with_os() {
    cps_api_object_list_guard lg(cps_api_object_list_create());
    cps_api_object_guard og(cps_api_object_create());

    if(lg.get()==nullptr || og.get()==nullptr) {
        return ;
    }

    if (nas_os_get_interface(og.get(),lg.get())!=STD_ERR_OK) {
        return ;
    }

    size_t ix = 0;
    size_t mx = cps_api_object_list_size(lg.get());
    for ( ; ix < mx ; ++ix ) {
        cps_api_object_t cur = cps_api_object_list_get(lg.get(),ix);

        cps_api_object_attr_t _name = cps_api_object_attr_get(cur,IF_INTERFACES_INTERFACE_NAME);
        if (_name==nullptr) continue;

        EV_LOGGING(INTERFACE,INFO,"NAS-INT-CREATE", "Looking at interface %s",
                (const char *)cps_api_object_attr_data_bin(_name));

        npu_id_t npu = 0;
        port_t port = 0;
        bool mapped = true;
        if (!if_get_tracker_details(cur, mapped, npu, port)) {
            continue;
        }
        cps_api_object_guard prev(cps_api_object_create());
        if(!prev.valid()) continue;

        if (mapped) {
            cps_api_object_attr_add_u32(cur,BASE_IF_PHY_IF_INTERFACES_INTERFACE_NPU_ID, npu);
            cps_api_object_attr_add_u32(cur,BASE_IF_PHY_IF_INTERFACES_INTERFACE_PORT_ID,port);
        }

        if (_if_create(cur,prev.get())!=cps_api_ret_code_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT-RELOAD", "Reload failed for %d:%d",(int)npu,(int)port);
        }
    }
}

static t_std_error _nas_int_npu_port_init(void) {
    npu_id_t npu = 0;
    npu_id_t npu_max = (npu_id_t)nas_switch_get_max_npus();

    for ( ; npu < npu_max ; ++npu ) {
        npu_port_t cpu_port = 0;
        if (ndi_cpu_port_get(npu, &cpu_port)==STD_ERR_OK) {
            char buff[100]; //plenty of space for a name
            snprintf(buff,sizeof(buff),"npu-%d",npu);
            t_std_error rc = nas_int_port_create_mapped(npu, cpu_port, buff, NULL, nas_int_type_CPU);
            if (rc==STD_ERR_OK) {
                nas_int_port_link_change(npu,cpu_port,IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_UP);
                cps_api_object_guard og(cps_api_object_create());
                if (!og.valid()) {
                    EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT","Memory allocation failure");
                    return STD_ERR(INTERFACE,FAIL,0);
                }
                cps_api_object_attr_add_u32(og.get(),IF_INTERFACES_INTERFACE_ENABLED,
                        true);

                cps_api_object_attr_add_u32(og.get(),IF_INTERFACES_STATE_INTERFACE_IF_INDEX,
                        nas_int_ifindex_from_port(npu,cpu_port));

                //will not effect the system if down or up
                nas_os_interface_set_attribute(og.get(),IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS);

                cps_api_key_set(cps_api_object_key(og.get()),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_OBSERVED);
                cps_api_object_set_type_operation(cps_api_object_key(og.get()),
                                                  cps_api_oper_CREATE );
                cps_api_event_thread_publish(og.get());

            }
        }
    }
    return STD_ERR_OK;
}

static cps_api_return_code_t loopback_if_get(void* context, cps_api_get_params_t* param,
                                             size_t key_ix)
{
    cps_api_object_t filt = cps_api_object_list_get(param->filters,key_ix);

    cps_api_object_attr_t ifix = cps_api_object_attr_get(filt,
                                      DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
    cps_api_object_attr_t name = cps_api_get_key_data(filt,IF_INTERFACES_INTERFACE_NAME);

    bool if_type_added = false;
    if (ifix == nullptr && name != nullptr) {
        interface_ctrl_t _port;
        if (!if_data_from_obj(obj_INTF, filt,_port)) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT-GET", "Wrong interface name or interface not present %s",_port.if_name);
            return cps_api_ret_code_ERR;
        }
        cps_api_object_attr_add_u32(filt, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
                                    _port.if_index);
    } else if (ifix == nullptr && name == nullptr) {
        cps_api_object_attr_add_u32(filt, BASE_IF_LINUX_IF_INTERFACES_INTERFACE_DELL_TYPE,
                                    BASE_CMN_INTERFACE_TYPE_LOOPBACK);
        if_type_added = true;
    }

    t_std_error rc = nas_os_get_interface(filt,param->list);

    if (if_type_added) {
        cps_api_object_attr_delete(filt, BASE_IF_LINUX_IF_INTERFACES_INTERFACE_DELL_TYPE);
    }

    if (rc != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Failed to get interfaces from OS");
        return cps_api_ret_code_ERR;
    }

    size_t mx = cps_api_object_list_size(param->list);
    size_t ix = 0;
    while (ix < mx) {
        cps_api_object_t object = cps_api_object_list_get(param->list, ix);
        cps_api_object_attr_t ifix = cps_api_object_attr_get(object, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
        if (ifix == nullptr) {
            EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Could not find ifindex in object %d", ix);
            cps_api_object_list_remove(param->list, ix);
            cps_api_object_delete(object);
            --mx;
            continue;
        }
        cps_api_key_from_attr_with_qual(cps_api_object_key(object), DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                cps_api_qualifier_TARGET);
        interface_ctrl_t _port;
        memset(&_port,0,sizeof(_port));
        _port.if_index = cps_api_object_attr_data_u32(ifix);
        _port.q_type = HAL_INTF_INFO_FROM_IF;
        if(dn_hal_get_interface_info(&_port) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Failed to get registered info for ifindex %d", _port.if_index);
            cps_api_object_list_remove(param->list, ix);
            cps_api_object_delete(object);
            --mx;
            continue;
        }

        cps_api_object_attr_add(object,IF_INTERFACES_INTERFACE_TYPE,
                        (const void *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_SOFTWARELOOPBACK,
                        strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_SOFTWARELOOPBACK)+1);

        /** TODO implement description for loopback **/
        cps_api_object_attr_add(object, IF_INTERFACES_INTERFACE_DESCRIPTION,
                        (const void*)"", 1);
        ++ ix;
    }

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t loopback_if_set(void* context, cps_api_transaction_params_t* param,
                                             size_t ix)
{
    // not supposed to be called, return error.
    return cps_api_ret_code_ERR;
}

static cps_api_return_code_t loopback_if_state_get (void * context, cps_api_get_params_t * param,
        size_t key_ix) {

    cps_api_object_t filt = cps_api_object_list_get(param->filters,key_ix);
    cps_api_object_attr_t ifix = cps_api_object_attr_get(filt, IF_INTERFACES_STATE_INTERFACE_IF_INDEX);
    cps_api_object_attr_t name = cps_api_get_key_data(filt,IF_INTERFACES_STATE_INTERFACE_NAME);
    bool if_type_added = false;

    if (ifix != nullptr) {
        /*  Call os API with interface object  */
        cps_api_object_attr_add_u32(filt,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
                                 cps_api_object_attr_data_u32(ifix));
        cps_api_object_attr_delete(filt,IF_INTERFACES_STATE_INTERFACE_IF_INDEX);
    }

    if (ifix == nullptr && name != NULL) {
        interface_ctrl_t _port;
        if (!if_data_from_obj(obj_INTF_STATE, filt,_port)) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT-GET", "Wrong interface name or interface not present %s",_port.if_name);
            return cps_api_ret_code_ERR;
        }
        cps_api_object_attr_add_u32(filt, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
                                    _port.if_index);
    } else if (ifix == nullptr && name == nullptr) {
        cps_api_object_attr_add_u32(filt, BASE_IF_LINUX_IF_INTERFACES_INTERFACE_DELL_TYPE,
                                    BASE_CMN_INTERFACE_TYPE_LOOPBACK);
        if_type_added = true;
    }

    t_std_error rc = nas_os_get_interface(filt,param->list);

    if (if_type_added) {
        cps_api_object_attr_delete(filt, BASE_IF_LINUX_IF_INTERFACES_INTERFACE_DELL_TYPE);
    }

    if (rc != STD_ERR_OK) {
        return cps_api_ret_code_ERR;
    }

    size_t mx = cps_api_object_list_size(param->list);
    size_t ix = 0;

    while (ix < mx) {
        cps_api_object_t object = cps_api_object_list_get(param->list,ix);
        cps_api_object_attr_t ifix = cps_api_object_attr_get(object, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);

        if (ifix == nullptr) {
            EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Could not find ifindex in object %d", ix);
            cps_api_object_list_remove(param->list, ix);
            cps_api_object_delete(object);
            --mx;
            continue;
        }

        cps_api_key_from_attr_with_qual(cps_api_object_key(object), DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                cps_api_qualifier_OBSERVED);
        interface_ctrl_t _port;
        memset(&_port,0,sizeof(_port));
        _port.if_index = cps_api_object_attr_data_u32(ifix);
        _port.q_type = HAL_INTF_INFO_FROM_IF;

        if(dn_hal_get_interface_info(&_port) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Failed to get registered info for ifindex %d", _port.if_index);
            cps_api_object_list_remove(param->list, ix);
            cps_api_object_delete(object);
            --mx;
            continue;
        }

        cps_api_set_key_data(object,IF_INTERFACES_STATE_INTERFACE_NAME,
                       cps_api_object_ATTR_T_BIN, _port.if_name, strlen(_port.if_name)+1);
        _if_fill_in_npu_intf_state(_port.npu_id, _port.port_id, _port.int_type, object);
        //_if_fill_in_supported_speeds_attrs(_port.npu_id,_port.port_id, _port.int_type, object);
        //_if_fill_in_eee_attrs(_port.npu_id,_port.port_id,object);
        _if_fill_in_npu_speed_attr(_port.npu_id,_port.port_id, _port.int_type, object);
        //_if_fill_in_supported_autoneg_attr(_port.npu_id,_port.port_id, object);
        /*  Add if index with right key */
        cps_api_object_attr_add_u32(object,IF_INTERFACES_STATE_INTERFACE_IF_INDEX,
                         _port.if_index);
        cps_api_object_attr_delete(object, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
        cps_api_object_attr_delete(object, IF_INTERFACES_INTERFACE_NAME);
        cps_api_object_attr_add(object,IF_INTERFACES_STATE_INTERFACE_TYPE,
                        (const void *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_SOFTWARELOOPBACK,
                        strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_SOFTWARELOOPBACK)+1);
        ++ ix;
    }

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t if_state_set(void * context, cps_api_transaction_params_t * param,size_t ix) {

    // not supposed to be called. return error
    return cps_api_ret_code_ERR;
}

t_std_error nas_int_logical_init(cps_api_operation_handle_t handle)  {

    std_rw_lock_create_default(&_logical_port_lock);

    if (nas_int_port_init()!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT", "Faild to init tab subsystem");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    resync_with_os();
    // Register phy
    if (intf_obj_handler_registration(obj_INTF, nas_int_type_PORT, if_get, if_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT", "Failed to register PHY interface CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (intf_obj_handler_registration(obj_INTF, nas_int_type_FC, if_get, if_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT", "Failed to register PHY interface CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (intf_obj_handler_registration(obj_INTF_STATE, nas_int_type_PORT, if_state_get, if_state_set)
                                                                      != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT", "Failed to register PHY interface state CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (intf_obj_handler_registration(obj_INTF_STATE, nas_int_type_FC, if_state_get, if_state_set)
                                                                      != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT", "Failed to register FC PHY interface state CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }
    /*  register interface get set for CPU port */
    if (intf_obj_handler_registration(obj_INTF, nas_int_type_CPU, if_cpu_port_get, if_cpu_port_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT", "Failed to register CPU interface CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (intf_obj_handler_registration(obj_INTF, nas_int_type_LPBK, loopback_if_get, loopback_if_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT", "Failed to register Loopback interface CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (intf_obj_handler_registration(obj_INTF_STATE, nas_int_type_LPBK, loopback_if_state_get, loopback_if_state_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT", "Failed to register Loopback interface state CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    nas_int_oper_state_register_cb(nas_int_oper_state_cb);

    t_std_error rc;
    if ((rc=_nas_int_npu_port_init())!=STD_ERR_OK) {
        return rc;
    }

    return STD_ERR_OK;
}

