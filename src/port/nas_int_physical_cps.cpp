/*
 * Copyright (c) 2019 Dell Inc.
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
#include "nas_int_utils.h"

#include "cps_class_map.h"
#include "cps_api_object_key.h"
#include "cps_api_operation.h"
#include "event_log.h"
#include "nas_switch.h"
#include "nas_interface_fc.h"

#include <vector>
#include <unordered_map>

struct _port_cache {
     uint32_t front_panel_port;
     uint32_t loopback;
     BASE_IF_SPEED_t speed;
     BASE_IF_PHY_MODE_TYPE_t phy_mode;
};

using NasPhyPortMap = std::unordered_map<uint_t, _port_cache>;
static NasPhyPortMap _phy_port;

#define MAX_HWPORT_PER_PORT 10

static  cps_api_return_code_t nas_fc_to_eth_speed(BASE_IF_SPEED_t speed, size_t hwp_count, BASE_IF_SPEED_t *npu_speed) {
    switch (speed) {
        case BASE_IF_SPEED_8GFC:
            *npu_speed = BASE_IF_SPEED_10GIGE;  // 4x8G mode  40G
            break;
        case BASE_IF_SPEED_16GFC:
            if (hwp_count == 1)    // 4x16G mode 100G
                *npu_speed = BASE_IF_SPEED_25GIGE;
            else if (hwp_count == 2) // 2x16G mode  40G
                *npu_speed = BASE_IF_SPEED_20GIGE;
            else
                return cps_api_ret_code_ERR;
            break;
        case BASE_IF_SPEED_32GFC:
            if (hwp_count == 1)  // 4x32G mode 25G
                *npu_speed = BASE_IF_SPEED_25GIGE;
            if (hwp_count == 2)  // 2x32G mode 100G
                *npu_speed = BASE_IF_SPEED_50GIGE;
            if (hwp_count == 4)  // 1x32G Mode 40G
                *npu_speed = BASE_IF_SPEED_40GIGE;
            break;
        default:
                return cps_api_ret_code_ERR;
    }
    return cps_api_ret_code_OK;

}
static bool _is_cpu_port(npu_id_t npu, port_t port) {
    npu_port_t cpu_port = 0;
    if (ndi_cpu_port_get(npu, &cpu_port)==STD_ERR_OK && port==cpu_port) {
        return true;
    }
    return false;
}

static bool get_hw_port(npu_id_t npu, port_t port, uint32_t& hwport) {
    return ndi_hwport_list_get(npu, port, &hwport) == STD_ERR_OK;
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
    cps_api_object_attr_t _phy_mode_attr = cps_api_object_attr_get(obj, BASE_IF_PHY_PHYSICAL_PHY_MODE);
    BASE_IF_PHY_MODE_TYPE_t phy_mode = BASE_IF_PHY_MODE_TYPE_ETHERNET;

    if (_phy_mode_attr != nullptr) {
        phy_mode = (BASE_IF_PHY_MODE_TYPE_t) cps_api_object_attr_data_u32(_phy_mode_attr);
    } else {
        EV_LOGGING(INTERFACE, ERR, "NAS-PHY", "NULL phy_mode_attr from obj for port:%d ", port);
    }

    if (phy_mode == BASE_IF_PHY_MODE_TYPE_FC) {
        nas_fc_phy_fill_supported_speed(npu, port, obj);
        return;
    } 
    
    if (ndi_port_supported_speed_get(npu,port,&speed_count, speed_list) == STD_ERR_OK) {
        size_t mx = speed_count;
        for (size_t ix =0;ix < mx; ix++) {
            cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_SUPPORTED_SPEED, speed_list[ix]);
        }
    }
}

static _port_cache* get_phy_port_cache(npu_id_t npu, uint_t port)
{
    uint32_t hwport;
    if (!get_hw_port(npu, port, hwport)) {
        return nullptr;
    }
    auto it = _phy_port.find(hwport);
    if (it == _phy_port.end()) {
        _phy_port[hwport].front_panel_port = 0;
        _phy_port[hwport].loopback = 0;
        _phy_port[hwport].phy_mode = BASE_IF_PHY_MODE_TYPE_ETHERNET;

        BASE_IF_SPEED_t speed;
        if (ndi_port_speed_get_nocheck(npu, port, &speed) == STD_ERR_OK) {
            _phy_port[hwport].speed = speed;
        } else {
            EV_LOGGING(INTERFACE, ERR, "NAS-PHY", "Failed to get speed of physical port %d",
                       port);
            _phy_port[hwport].speed = BASE_IF_SPEED_0MBPS;
        }
    }

    return &_phy_port[hwport];
}

t_std_error nas_int_get_phy_speed(npu_id_t npu, port_t port, BASE_IF_SPEED_t* speed)
{
    if (speed == nullptr) {
        return STD_ERR(INTERFACE, PARAM, 0);
    }

    _port_cache* port_info = get_phy_port_cache(npu, port);
    if (port_info == nullptr) {
        EV_LOGGING(INTERFACE, ERR,
                   "NAS-PHY", "Failed to get physical port info from cache for npu %d port %d",
                   npu, port);
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    *speed = port_info->speed;
    return STD_ERR_OK;
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

    uint32_t hwport_list[MAX_HWPORT_PER_PORT] = {0};
    size_t count = MAX_HWPORT_PER_PORT;
    if (ndi_hwport_list_get_list(npu, port, hwport_list, &count) == STD_ERR_OK) {
        for (size_t idx = 0; idx < count; idx ++) {
            cps_api_object_attr_add_u32(obj, BASE_IF_PHY_PHYSICAL_HARDWARE_PORT_LIST,
                                        hwport_list[idx]);
        }
    }

    _if_fill_in_supported_speeds_attrs(npu,port,obj);

    auto phy_port = get_phy_port_cache(npu, port);
    if(phy_port != nullptr) {
        cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_FRONT_PANEL_NUMBER,
                                    phy_port->front_panel_port);
        cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_LOOPBACK, phy_port->loopback);
        cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_SPEED, phy_port->speed);
        cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_PHY_MODE, phy_port->phy_mode);
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

    auto phy_port = get_phy_port_cache(npu, port);
    if (phy_port == nullptr) {
        EV_LOGGING(INTERFACE, ERR, "NAS-PHY", "Can't find phy port cache");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_t _fp = cps_api_get_key_data(req,BASE_IF_PHY_PHYSICAL_FRONT_PANEL_NUMBER);
    cps_api_object_attr_t _loopback = cps_api_get_key_data(req,BASE_IF_PHY_PHYSICAL_LOOPBACK);

    if (_fp!=nullptr) {
        phy_port->front_panel_port = cps_api_object_attr_data_u32(_fp);
    }

    if (_loopback != nullptr) {
        if (ndi_port_loopback_set(npu, port,
                    (BASE_CMN_LOOPBACK_TYPE_t) cps_api_object_attr_data_u32(_loopback)) != STD_ERR_OK)  {
            EV_LOGGING(INTERFACE,ERR,"NAS-PHY","Failed to set loopback for %d:%d",npu,port);
            return cps_api_ret_code_ERR;
        }
        phy_port->loopback = cps_api_object_attr_data_u32(_loopback);
    }

    cps_api_key_set(cps_api_object_key(req),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_OBSERVED);
    hal_interface_send_event(req);
    cps_api_key_set(cps_api_object_key(req),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_TARGET);

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _phy_create(cps_api_object_t cur, cps_api_object_t pref)
{
    cps_api_object_attr_t npu_attr = cps_api_object_attr_get(cur, BASE_IF_PHY_PHYSICAL_NPU_ID);
    cps_api_object_attr_t speed_attr = cps_api_object_attr_get(cur, BASE_IF_PHY_PHYSICAL_SPEED);
    BASE_IF_PHY_MODE_TYPE_t phy_mode = BASE_IF_PHY_MODE_TYPE_ETHERNET;

    if (npu_attr == nullptr || speed_attr == nullptr) {
        EV_LOGGING(INTERFACE, ERR, "NAS-PHY-CREATE", "Required attribute not present");
        return cps_api_ret_code_ERR;
    }

    npu_id_t npu_id = cps_api_object_attr_data_u32(npu_attr);
    BASE_IF_SPEED_t speed =
            (BASE_IF_SPEED_t)cps_api_object_attr_data_u32(speed_attr);
    BASE_IF_SPEED_t npu_speed = speed;

    std::vector<uint32_t> hw_ports;
    cps_api_object_it_t it;
    cps_api_object_it_begin(cur, &it);
    while(cps_api_object_it_attr_walk(&it, BASE_IF_PHY_PHYSICAL_HARDWARE_PORT_LIST)) {
        uint32_t port_id = cps_api_object_attr_data_u32(it.attr);
        hw_ports.push_back(port_id);
        cps_api_object_it_next(&it);
    }
    if (hw_ports.size() == 0) {
        EV_LOGGING(INTERFACE, ERR, "NAS-PHY-CREATE", "No hardware port present");
        return cps_api_ret_code_ERR;
    }
     cps_api_object_attr_t _phy_mode_attr = cps_api_object_attr_get(cur, BASE_IF_PHY_PHYSICAL_PHY_MODE);
    if (_phy_mode_attr != nullptr) {
        phy_mode = (BASE_IF_PHY_MODE_TYPE_t) cps_api_object_attr_data_u32(_phy_mode_attr);
    }
    if (phy_mode == BASE_IF_PHY_MODE_TYPE_FC) {
        if (nas_fc_to_eth_speed(speed, hw_ports.size(), &npu_speed) == cps_api_ret_code_ERR) {
            EV_LOGGING(INTERFACE, ERR, "NAS-PHY-CREATE", "Wrong FC Speed ");
            return cps_api_ret_code_ERR;
        }
    }

    npu_port_t phy_port_id;
    t_std_error rc = ndi_phy_port_create(npu_id, npu_speed,
                                         hw_ports.data(), hw_ports.size(),
                                         &phy_port_id);
    if (rc != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-PHY-CREATE", "Failure on NDI port creation");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_add_u32(cur, BASE_IF_PHY_PHYSICAL_PORT_ID, phy_port_id);

    if (phy_mode == BASE_IF_PHY_MODE_TYPE_FC) {

        if (nas_cps_create_fc_port(npu_id, phy_port_id, speed, hw_ports.data(), hw_ports.size()) != cps_api_ret_code_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-PHY-CREATE", "Failure on NDI FC port creation");
            return cps_api_ret_code_ERR;
         }
    }

    uint32_t hwport;
    if (!get_hw_port(npu_id, phy_port_id, hwport)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-PHY-CREATE", "Failed to get hw port for port %d",
                   phy_port_id);
        return cps_api_ret_code_ERR;
    }

    auto phy_it = _phy_port.find(hwport);
    if (phy_it == _phy_port.end()) {
        _phy_port[hwport].front_panel_port = 0;
        _phy_port[hwport].loopback = 0;
    }
    cps_api_object_attr_t fp_attr = cps_api_object_attr_get(cur,
                                            BASE_IF_PHY_PHYSICAL_FRONT_PANEL_NUMBER);
    if (fp_attr != nullptr) {
        _phy_port[hwport].front_panel_port = cps_api_object_attr_data_u32(fp_attr);
    }
    _phy_port[hwport].speed = speed;
    _phy_port[hwport].phy_mode = phy_mode;

    make_phy_port_details(npu_id, phy_port_id, cur);

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _phy_delete(cps_api_object_t cur, cps_api_object_t pref)
{
    cps_api_return_code_t rc = cps_api_ret_code_ERR;
    cps_api_object_attr_t npu_attr = cps_api_object_attr_get(cur, BASE_IF_PHY_PHYSICAL_NPU_ID);
    cps_api_object_attr_t port_attr = cps_api_object_attr_get(cur, BASE_IF_PHY_PHYSICAL_PORT_ID);

    if (npu_attr == nullptr || port_attr == nullptr) {
        EV_LOGGING(INTERFACE, ERR, "NAS-PHY-DELETE", "Required attribute not present");
        return cps_api_ret_code_ERR;
    }

    npu_id_t npu_id = cps_api_object_attr_data_u32(npu_attr);
    npu_port_t port_id = cps_api_object_attr_data_u32(port_attr);

    uint32_t hwport;
    if (!get_hw_port(npu_id, port_id, hwport)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-PHY-DELETE", "Failed to get hw port for port %d",
                   port_id);
        return cps_api_ret_code_ERR;
    }

    auto phy_it = _phy_port.find(hwport);
    if (phy_it != _phy_port.end() &&  (_phy_port[hwport].phy_mode == BASE_IF_PHY_MODE_TYPE_FC)) {
        if ((rc = nas_cps_delete_fc_port(npu_id, port_id)) !=cps_api_ret_code_OK ){
            EV_LOGGING(INTERFACE, ERR, "NAS-PHY-DELETE", "Failure on NDI FC port delete");
            return rc;

        }
    }


    t_std_error ret = ndi_phy_port_delete(npu_id, port_id);
    if (ret != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-PHY-DELETE", "Failure on NDI port delete");
        return cps_api_ret_code_ERR;
    }

    auto it = _phy_port.find(hwport);
    if (it != _phy_port.end()) {
        _phy_port.erase(it);
    }

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _phy_int_set(void * context, cps_api_transaction_params_t * param,size_t ix) {
    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    if (obj==NULL) return cps_api_ret_code_ERR;

    cps_api_object_t prev = cps_api_object_list_create_obj_and_append(param->prev);
    if (prev==nullptr) return cps_api_ret_code_ERR;

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if (op==cps_api_oper_CREATE) return _phy_create(obj,prev);
    if (op==cps_api_oper_SET) return _phy_set(obj,prev);
    if (op==cps_api_oper_DELETE) return _phy_delete(obj,prev);

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

    return nas_int_logical_init(handle);
}
