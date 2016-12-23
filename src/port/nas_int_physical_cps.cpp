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

#include "nas_int_physical_cps.h"

#include "dell-base-platform-common.h"
#include "dell-base-media.h"
#include "dell-base-if-phy.h"
#include "dell-base-if.h"

#include "nas_os_interface.h"
#include "hal_if_mapping.h"

#include "hal_interface_common.h"
#include "hal_interface_defaults.h"
#include "nas_ndi_port.h"
#include "nas_int_logical.h"

#include "cps_class_map.h"
#include "cps_api_object_key.h"
#include "cps_api_operation.h"
#include "event_log.h"
#include "nas_switch.h"

#include <unordered_map>

struct _port_cache {
     uint32_t front_panel_port;
     uint32_t sub_port;
     uint32_t media_type;
     uint32_t loopback;
};

using NasPhyPortMap = std::unordered_map<uint_t, _port_cache>;
static NasPhyPortMap _phy_port;


static bool _is_cpu_port(npu_id_t npu, port_t port) {
    npu_port_t cpu_port = 0;
    if (ndi_cpu_port_get(npu, &cpu_port)==STD_ERR_OK && port==cpu_port) {
        return true;
    }
    return false;
}

static bool get_hw_port(npu_id_t npu, port_t port, uint32_t &hwport) {
    return ndi_hwport_list_get(npu,port,&hwport)==STD_ERR_OK;
}

static void init_phy_port_obj(npu_id_t npu, port_t port, cps_api_object_t obj) {
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
            BASE_IF_PHY_PHYSICAL_OBJ,cps_api_qualifier_TARGET);

    cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_NPU_ID,npu);
    cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_PORT_ID,port);
}

static void _if_fill_in_supported_speeds_attrs(npu_id_t npu, port_t port,
        cps_api_object_t obj) {
    size_t speed_count = NDI_PORT_SUPPORTED_SPEED_MAX;
    BASE_IF_SPEED_t speed_list[NDI_PORT_SUPPORTED_SPEED_MAX];
    if (ndi_port_supported_speed_get(npu,port,&speed_count, speed_list) == STD_ERR_OK) {
        size_t mx = speed_count;
        for (size_t ix =0;ix < mx; ix++) {
            cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_SUPPORTED_SPEED, speed_list[ix]);
        }
    }
}

static void make_phy_port_details(npu_id_t npu, port_t port, cps_api_object_t obj) {
    init_phy_port_obj(npu,port,obj);

    if (_is_cpu_port(npu, port)) {
        return;
    }

    uint32_t hwport;
    if (get_hw_port(npu,port,hwport)) {
        cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_HARDWARE_PORT_ID,hwport);
    }

    //should be size_t
    int mode_count = BASE_IF_PHY_BREAKOUT_MODE_BREAKOUT_1X1+1;
    BASE_IF_PHY_BREAKOUT_MODE_t mode_list[BASE_IF_PHY_BREAKOUT_MODE_BREAKOUT_1X1+1];

    if (ndi_port_supported_breakout_mode_get(npu,port,
            &mode_count,mode_list)==STD_ERR_OK) {
        size_t ix = 0;
        size_t mx = mode_count;
        for ( ; ix < mx ; ++ix ) {
            cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_BREAKOUT_CAPABILITIES,mode_list[ix]);
        }
    }
    BASE_IF_PHY_BREAKOUT_MODE_t cur_mode;
    if (ndi_port_breakout_mode_get(npu,port,&cur_mode)==STD_ERR_OK) {
        cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_FANOUT_MODE,cur_mode);
    }

    _if_fill_in_supported_speeds_attrs(npu,port,obj);

    auto it = _phy_port.find(hwport);

    if (it!=_phy_port.end()) {
        cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_FRONT_PANEL_NUMBER, it->second.front_panel_port);
        cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_PHY_MEDIA, it->second.media_type);
        cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_LOOPBACK, it->second.loopback);
    }
}

static cps_api_return_code_t _phy_int_get (void * context, cps_api_get_params_t * param,
        size_t key_ix) {

    cps_api_object_t filt = cps_api_object_list_get(param->filters,key_ix);

    npu_id_t npu = 0;
    npu_id_t npu_max = (npu_id_t)nas_switch_get_max_npus();

    cps_api_object_attr_t _npu = cps_api_get_key_data(filt,BASE_IF_PHY_PHYSICAL_NPU_ID);
    cps_api_object_attr_t _port = cps_api_get_key_data(filt,BASE_IF_PHY_PHYSICAL_PORT_ID);
    cps_api_object_attr_t _hw_port = cps_api_get_key_data(filt,BASE_IF_PHY_PHYSICAL_HARDWARE_PORT_ID);

    for ( npu = 0; npu<  npu_max ; ++npu){
        if (_npu!=NULL && cps_api_object_attr_data_u32(_npu)!=(uint32_t)npu) continue;

        unsigned int intf_max_ports = ndi_max_npu_port_get(npu);
        for (port_t port = 0; port < intf_max_ports ; ++port ) {
            if (_port!=NULL && cps_api_object_attr_data_u32(_port)!=port) continue;

            if (_hw_port!=NULL) {
                uint32_t hwport;
                if (get_hw_port(npu,port,hwport)) {
                    if (cps_api_object_attr_data_u32(_hw_port)!=hwport) continue;
                }
            }

            // skip invalid port or cpu port
            if (!ndi_port_is_valid(npu, port) || _is_cpu_port(npu, port)) {
                continue;
            }

            cps_api_object_t o = cps_api_object_list_create_obj_and_append(param->list);
            if (o==NULL) return cps_api_ret_code_ERR;

            make_phy_port_details(npu,port,o);
        }
    }
    return cps_api_ret_code_OK;
}

static t_std_error dn_nas_get_phy_media_default_setting(PLATFORM_MEDIA_TYPE_t media_type, cps_api_object_t obj)
{
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);
    cps_api_get_request_guard rg(&gp);

    cps_api_object_t media_obj = cps_api_object_list_create_obj_and_append(gp.filters);
    t_std_error rc = STD_ERR_MK(e_std_err_INTERFACE, e_std_err_code_FAIL, 0);

    do {
        if (!cps_api_key_from_attr_with_qual(cps_api_object_key(media_obj),
                                BASE_MEDIA_MEDIA_INFO_OBJ, cps_api_qualifier_OBSERVED)) {
            break;
        }
        cps_api_set_key_data_uint(media_obj, BASE_MEDIA_MEDIA_INFO_MEDIA_TYPE, &media_type, sizeof(media_type));
        cps_api_object_attr_add_u32(media_obj, BASE_MEDIA_MEDIA_INFO_MEDIA_TYPE, media_type);
        if (cps_api_get(&gp) != cps_api_ret_code_OK)
            break;

        if (0 == cps_api_object_list_size(gp.list))
            break;

        media_obj = cps_api_object_list_get(gp.list,0);
        if (!cps_api_object_clone(obj, media_obj)) {
            break;
        }
        rc = STD_ERR_OK;
    } while(0);
    return rc;
}
static t_std_error _phy_media_type_with_default_config_set(npu_id_t npu, port_t port,
                                        PLATFORM_MEDIA_TYPE_t media_type)
{
    t_std_error rc;
    if ((rc = ndi_port_media_type_set(npu, port, media_type)) != STD_ERR_OK) {
        return rc;
    }
    /*  set default speed and autoneg setting  corresponding to the media type */
    cps_api_object_t obj = cps_api_object_create();
    cps_api_object_guard og(obj);
    if ((rc = dn_nas_get_phy_media_default_setting(media_type, obj)) != STD_ERR_OK) {
        return rc;
    }

    /* First set AN and then speed */
    cps_api_object_attr_t autoneg_attr = cps_api_object_attr_get(obj, BASE_MEDIA_MEDIA_INFO_AUTONEG);
    if (autoneg_attr != nullptr) {
        bool autoneg = (bool)cps_api_object_attr_data_u32(autoneg_attr);
        if (ndi_port_auto_neg_set(npu,port,autoneg)!=STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to set autoneg %d for "
                   "npu %d port %d return Error 0x%x",autoneg,npu,port,rc);
            return STD_ERR(INTERFACE,FAIL,0);
        }
    }

    BASE_IF_PHY_BREAKOUT_MODE_t cur_mode;
    if ((rc = ndi_port_breakout_mode_get(npu, port, &cur_mode)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to get breakout mode for "
                   "npu %d port %d return error 0x%x",npu,port, rc);
            return STD_ERR(INTERFACE,FAIL,0);
    }
    /* if the port is fanout skip setting the default speed say 40G */
    if (BASE_IF_PHY_BREAKOUT_MODE_BREAKOUT_1X1 == cur_mode) {
        cps_api_object_attr_t speed_attr = cps_api_object_attr_get(obj, BASE_MEDIA_MEDIA_INFO_SPEED);
        if (speed_attr != nullptr) {
            BASE_IF_SPEED_t speed = (BASE_IF_SPEED_t)cps_api_object_attr_data_u32(speed_attr);
            if ((rc = ndi_port_speed_set(npu,port,speed))!=STD_ERR_OK) {
                EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to set speed %d for "
                       "npu %d port %d return error 0x%x",speed,npu,port, rc);
                return STD_ERR(INTERFACE,FAIL,0);
            }
        }
    }
    cps_api_object_attr_t duplex_attr = cps_api_object_attr_get(obj, BASE_MEDIA_MEDIA_INFO_DUPLEX);
    if (duplex_attr != nullptr) {
        BASE_CMN_DUPLEX_TYPE_t duplex = (BASE_CMN_DUPLEX_TYPE_t)cps_api_object_attr_data_u32(duplex_attr);
        if ((rc = ndi_port_duplex_set(npu,port,duplex))!=STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Failed to set duplex %d for "
                   "npu %d port %d return error 0x%x",duplex,npu,port, rc);
            return STD_ERR(INTERFACE,FAIL,0);
        }
    }
    return STD_ERR_OK;
}

static cps_api_return_code_t _phy_set(cps_api_object_t req, cps_api_object_t prev) {
    STD_ASSERT(prev!=nullptr && req!=nullptr);

    cps_api_object_attr_t _npu = cps_api_get_key_data(req,BASE_IF_PHY_PHYSICAL_NPU_ID);
    cps_api_object_attr_t _port = cps_api_get_key_data(req,BASE_IF_PHY_PHYSICAL_PORT_ID);

    if (_npu==nullptr || _port==nullptr) {
        EV_LOGGING(INTERFACE,ERR,"NAS-PHY","Set req missing key instances");
        return cps_api_ret_code_ERR;
    }

    npu_id_t npu = cps_api_object_attr_data_u32(_npu);
    port_t port = cps_api_object_attr_data_u32(_port);

    if (_is_cpu_port(npu, port)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-PHY","Can't modify the CPU port %d:%d",npu,port);
        return cps_api_ret_code_ERR;
    }

    uint32_t hwport;
    if (!get_hw_port(npu,port,hwport)) {
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_t _fp = cps_api_get_key_data(req,BASE_IF_PHY_PHYSICAL_FRONT_PANEL_NUMBER);
    cps_api_object_attr_t _media_type = cps_api_get_key_data(req,BASE_IF_PHY_PHYSICAL_PHY_MEDIA);
    cps_api_object_attr_t _loopback = cps_api_get_key_data(req,BASE_IF_PHY_PHYSICAL_LOOPBACK);

    auto it = _phy_port.find(hwport);
    if (it == _phy_port.end()) {
        _phy_port[hwport].front_panel_port = 0;
        _phy_port[hwport].sub_port = 0;
        _phy_port[hwport].media_type = 0;
        _phy_port[hwport].loopback = 0;
    }

    if (_fp!=nullptr) {
        _phy_port[hwport].front_panel_port = cps_api_object_attr_data_u32(_fp);
    }

    if (_media_type != nullptr) {
        PLATFORM_MEDIA_TYPE_t media_type = (PLATFORM_MEDIA_TYPE_t) cps_api_object_attr_data_u32(_media_type);
		if (_phy_media_type_with_default_config_set(npu, port, media_type) != STD_ERR_OK)  {
			EV_LOGGING(INTERFACE,ERR,"NAS-PHY","Failed to set phy media type for %d:%d",npu,port);
			return cps_api_ret_code_ERR;
		}
    }

    if (_loopback != nullptr) {
        if (ndi_port_loopback_set(npu, port,
                    (BASE_CMN_LOOPBACK_TYPE_t) cps_api_object_attr_data_u32(_loopback)) != STD_ERR_OK)  {
            EV_LOGGING(INTERFACE,ERR,"NAS-PHY","Failed to set loopback for %d:%d",npu,port);
            return cps_api_ret_code_ERR;
        }
        _phy_port[hwport].loopback = cps_api_object_attr_data_u32(_loopback);
    }
    cps_api_key_set(cps_api_object_key(req),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_OBSERVED);
    hal_interface_send_event(req);
    cps_api_key_set(cps_api_object_key(req),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_TARGET);

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _phy_int_set(void * context, cps_api_transaction_params_t * param,size_t ix) {
    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    if (obj==NULL) return cps_api_ret_code_ERR;

    cps_api_object_t prev = cps_api_object_list_create_obj_and_append(param->prev);
    if (prev==nullptr) return cps_api_ret_code_ERR;

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if (op==cps_api_oper_SET) return _phy_set(obj,prev);

    EV_LOGGING(INTERFACE,ERR,"NAS-PHY","Invalid operation");

    return cps_api_ret_code_ERR;
}

static void _ndi_port_event_update_ (ndi_port_t  *ndi_port, ndi_port_event_t event, uint32_t hwport) {
    cps_api_object_guard og(cps_api_object_create());

    init_phy_port_obj(ndi_port->npu_id,ndi_port->npu_port,og.get());

    cps_api_object_attr_add_u32(og.get(),BASE_IF_PHY_PHYSICAL_HARDWARE_PORT_ID,hwport);

    if (!(event == ndi_port_ADD || event==ndi_port_DELETE)) return ;

    cps_api_object_set_type_operation(cps_api_object_key(og.get()),event == ndi_port_ADD ?
        cps_api_oper_CREATE : cps_api_oper_DELETE );

    hal_interface_send_event(og.get());
}

t_std_error nas_int_cps_init(cps_api_operation_handle_t handle) {

    cps_api_registration_functions_t f;
    char buff[CPS_API_KEY_STR_MAX];
    memset(&f,0,sizeof(f));

    if (!cps_api_key_from_attr_with_qual(&f.key,BASE_IF_PHY_PHYSICAL_OBJ,cps_api_qualifier_TARGET)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Could not translate %d to key %s",
                            (int)(DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ),
                            cps_api_key_print(&f.key,buff,sizeof(buff)-1));
        return STD_ERR(INTERFACE,FAIL,0);
    }
    EV_LOGGING(INTERFACE,INFO,"NAS-IF-REG","Registering for %s",
                        cps_api_key_print(&f.key,buff,sizeof(buff)-1));
    f.handle = handle;
    f._read_function = _phy_int_get;
    f._write_function = _phy_int_set;
    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }

    t_std_error rc = STD_ERR_OK;
    npu_id_t npu = 0;
    npu_id_t npu_max = (npu_id_t)nas_switch_get_max_npus();
    for ( ; npu < npu_max ; ++npu ) {
        rc = ndi_port_event_cb_register(npu,_ndi_port_event_update_);
        if (rc!=STD_ERR_OK) return rc;
    }

    if ((rc=nas_int_breakout_init(handle))!=STD_ERR_OK) return rc;

    return nas_int_logical_init(handle);
}
