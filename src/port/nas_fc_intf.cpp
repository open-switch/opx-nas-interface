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

/*
 * filename: nas_fc_intf.c
 */


#include "nas_ndi_fc.h"
#include "nas_ndi_port.h"
#include "nas_int_utils.h"
#include "cps_api_events.h"
#include "cps_api_object_attr.h"
#include "std_mac_utils.h"
#include "event_log.h"
#include "dell-interface.h"
#include "dell-base-if-phy.h"
#include "iana-if-type.h"
#include "cps_api_operation.h"
#include <unordered_map>
#define MAX_NO_OF_SUPP_SPEED 10
#define FC_MTU_SIZE 2188

static
t_std_error _set_generic_mac (npu_id_t npu, port_t port, cps_api_attr_id_t attr, cps_api_object_t obj) {

    char mac_str[18];
    memset(mac_str, 0 , sizeof(mac_str));
    nas_fc_param_t param;
    cps_api_object_attr_t mac_attr = cps_api_object_attr_get(obj,
                                           attr);
    STD_ASSERT(mac_attr != nullptr);
    void *addr = cps_api_object_attr_data_bin(mac_attr);
    int addr_len = strlen(static_cast<char *>(addr));
    if (!std_string_to_mac(&param.mac, static_cast<const char *>(addr), addr_len)) {
       EV_LOGGING(INTERFACE, ERR, "NAS-FCOE-MAP", "Error set dest mac");
       return STD_ERR(INTERFACE,FAIL,0);

    }
    EV_LOGGING(INTERFACE, DEBUG, "NAS-FCOE-MAP", "Setting mac address %s, actual string %s, len %d",
                std_mac_to_string(&param.mac, mac_str,sizeof(mac_str)),  static_cast<char *>(addr), addr_len);

    return ndi_set_fc_attr(npu, port, &param, attr);

}

/* Stores cps varied len data in u32, before sending to NDI */
static
t_std_error _set_generic_u32(npu_id_t npu, port_t port, cps_api_attr_id_t attr,
         cps_api_object_t obj) {

    nas_fc_param_t param;
    cps_api_object_attr_t _attr = cps_api_object_attr_get(obj,attr);
    if (_attr==NULL)  return STD_ERR(INTERFACE,FAIL,0);
    param.u32 = cps_api_object_attr_data_uint(_attr);
    return ndi_set_fc_attr(npu, port, &param, attr);
}

static
t_std_error _set_generic_bool(npu_id_t npu, port_t port, cps_api_attr_id_t attr, cps_api_object_t obj) {
    nas_fc_param_t param;
    cps_api_object_attr_t _attr = cps_api_object_attr_get(obj,attr);
    if (_attr==NULL) return STD_ERR(INTERFACE,FAIL,0);
    param.ty_bool = (bool) cps_api_object_attr_data_uint(_attr);
    return ndi_set_fc_attr(npu, port, &param, attr);

}

static
t_std_error _set_fc_speed(npu_id_t npu, port_t port, cps_api_attr_id_t attr,
         cps_api_object_t obj) {

    cps_api_object_attr_t _attr = cps_api_object_attr_get(obj,attr);
    if (_attr == NULL)  return STD_ERR(INTERFACE,FAIL,0);
    uint32_t speed =  cps_api_object_attr_data_uint(_attr);
    if (speed == BASE_IF_SPEED_AUTO) {
        EV_LOGGING(INTERFACE,DEBUG ,"NAS-FCOE-MAP","Speed auto is not supported for FC interfaces");
        return STD_ERR_OK;
    }
    return _set_generic_u32(npu, port, attr, obj);
}


/* Will come as interface object */

static const std::unordered_map<cps_api_attr_id_t,
    t_std_error (*)(npu_id_t, port_t, cps_api_attr_id_t, cps_api_object_t )> _set_attr_handlers = {
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_SRC_MAC_MODE, _set_generic_u32 },
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_SRC_MAP_PREFIX, _set_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_INGRESS_SRC_MAC, _set_generic_mac},

        { BASE_IF_FC_IF_INTERFACES_INTERFACE_DEST_MAC_MODE, _set_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_DEST_MAP_PREFIX, _set_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_INGRESS_DEST_MAC, _set_generic_mac},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_VFT_HEADER,      _set_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_BB_CREDIT,      _set_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_BB_CREDIT_RECOVERY,      _set_generic_u32},
        { DELL_IF_IF_INTERFACES_INTERFACE_AUTO_NEGOTIATION,  _set_generic_bool},
        { IF_INTERFACES_INTERFACE_ENABLED,                   _set_generic_bool},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_FCOE_PKT_VLANID, _set_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_PRIORITY,       _set_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_MTU,            _set_generic_u32},
        { DELL_IF_IF_INTERFACES_INTERFACE_SPEED,             _set_fc_speed},
        { BASE_IF_PHY_IF_INTERFACES_INTERFACE_PHY_MEDIA,    _set_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_STATE_INTERFACE_BB_CREDIT_RECEIVE, _set_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_FLOW_CONTROL_ENABLE, _set_generic_bool},
};


cps_api_return_code_t nas_cps_create_fc_port(npu_id_t npu_id, port_t port, BASE_IF_SPEED_t speed, uint32_t *hw_port_list, size_t count)
{
    if (ndi_create_fc_port(npu_id, port, speed, hw_port_list, count) != STD_ERR_OK) {
        return cps_api_ret_code_ERR;
    }

    nas_fc_param_t param;
    cps_api_attr_id_t attr =  BASE_IF_FC_IF_INTERFACES_INTERFACE_MTU;
    param.u32 = FC_MTU_SIZE;
    if (ndi_set_fc_attr(npu_id, port, &param, attr) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-FCOE-MAP"," FC create: set mtu failed  npu %d, port  %u",
                   npu_id, port);
    }

    EV_LOGGING(INTERFACE,DEBUG ,"NAS-FCOE-MAP"," FC create  %d for port  %u", npu_id, port);
    return cps_api_ret_code_OK;
}

cps_api_return_code_t nas_cps_set_fc_attr(npu_id_t npu_id, port_t port, cps_api_object_t obj, cps_api_object_t prev)
{

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));
    cps_api_object_it_t it_req;
    cps_api_object_it_begin(obj, &it_req);

    EV_LOGGING(INTERFACE,DEBUG,"NAS-FCOE-MAP","nas_cps_set_fc_port  %d for port  %u", npu_id, port);

    for ( ; cps_api_object_it_valid(&it_req); cps_api_object_it_next(&it_req)) {
        cps_api_attr_id_t id = cps_api_object_attr_id(it_req.attr);
        auto func = _set_attr_handlers.find(id);
        if (func ==_set_attr_handlers.end()) continue;

        EV_LOGGING(INTERFACE, DEBUG ,"NAS-FCOE-MAP"," Set attribute for npu %u, port %u, attr %lu ",
              npu_id, port, id);
        t_std_error ret = func->second(npu_id, port, id, obj );
        if (ret != STD_ERR_OK) {
            if (op == cps_api_oper_SET) {
                EV_LOGGING(INTERFACE,ERR,"NAS-FCOE-MAP","Failed to set Attribute  %lu for port  %u", id, port);
                /* TODO: Roll back */
                return cps_api_ret_code_ERR;
            } else {
                // report error messages and continue with create operation
                EV_LOGGING(INTERFACE,ERR,"NAS-FCOE-MAP","Failed to set Attribute  %lu for port  %u for create op", id, port);
            }
        }

    }
    return cps_api_ret_code_OK;

}

cps_api_return_code_t nas_cps_delete_fc_port(npu_id_t npu_id, port_t port)
{

   EV_LOGGING(INTERFACE,DEBUG,"NAS-FCOE-MAP","nas_cps_delete_fc_port  %d for port  %u", npu_id, port);
   return ndi_delete_fc_port(npu_id, port);


}

void nas_fc_fill_speed_autoneg_state(npu_id_t npu, port_t port, cps_api_object_t obj)
{
    nas_fc_id_value_t param;
    uint64_t speed = 0;
    t_std_error rc;
    ndi_intf_link_state_t state;

    EV_LOGGING(INTERFACE,DEBUG,"NAS-FCOE-MAP", "nas_fc_fill_speed_autoneg  %d for port  %u", npu, port);

    memset(&param, 0, sizeof(nas_fc_id_value_t));
    param.attr_id = IF_INTERFACES_STATE_INTERFACE_SPEED;

    rc = ndi_port_link_state_get(npu, port,&state);
    if (rc == STD_ERR_OK) {
        if (state.oper_status != ndi_port_OPER_UP) {
            speed = 0;
            EV_LOGGING(INTERFACE,DEBUG,"NAS-FCOE-MAP", "nas_fc_fill_speed_autoneg speed set to zero");
            cps_api_object_attr_delete(obj, IF_INTERFACES_STATE_INTERFACE_SPEED);
            cps_api_object_attr_add_u64(obj, IF_INTERFACES_STATE_INTERFACE_SPEED, speed);
        } else {
            if (ndi_get_fc_attr(npu, port, &param, 1) == STD_ERR_OK) {
                speed = param.value.u64;
                cps_api_object_attr_delete(obj, IF_INTERFACES_STATE_INTERFACE_SPEED);
                cps_api_object_attr_add_u64(obj, IF_INTERFACES_STATE_INTERFACE_SPEED, speed);
            }
        }
    }

    memset(&param, 0, sizeof(nas_fc_id_value_t));
    param.attr_id = DELL_IF_IF_INTERFACES_STATE_INTERFACE_AUTO_NEGOTIATION;
    if ((ndi_get_fc_attr(npu, port, &param, 1)) == STD_ERR_OK) {
        cps_api_object_attr_delete(obj, DELL_IF_IF_INTERFACES_STATE_INTERFACE_AUTO_NEGOTIATION);
        cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_STATE_INTERFACE_AUTO_NEGOTIATION, param.value.ty_bool);
    }
    cps_api_object_attr_add(obj, IF_INTERFACES_STATE_INTERFACE_TYPE,
                            (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_FIBRECHANNEL,
                            sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_FIBRECHANNEL));
    return;
}

static BASE_IF_SPEED_t fc_supp_speed_arr_32gfc[MAX_NO_OF_SUPP_SPEED]= 
                         {BASE_IF_SPEED_32GFC, BASE_IF_SPEED_16GFC, BASE_IF_SPEED_8GFC};
static BASE_IF_SPEED_t fc_supp_speed_arr_16gfc[MAX_NO_OF_SUPP_SPEED]= 
                         {BASE_IF_SPEED_16GFC, BASE_IF_SPEED_8GFC};
static BASE_IF_SPEED_t fc_supp_speed_arr_8gfc[MAX_NO_OF_SUPP_SPEED]= 
                         {BASE_IF_SPEED_8GFC};

static std::unordered_map<BASE_IF_SPEED_t, BASE_IF_SPEED_t*> _fc_port_speed_to_supp_speed_map = { 
    { BASE_IF_SPEED_32GFC, fc_supp_speed_arr_32gfc},
    { BASE_IF_SPEED_16GFC, fc_supp_speed_arr_16gfc},
    { BASE_IF_SPEED_8GFC,  fc_supp_speed_arr_8gfc}
};

void nas_fc_fill_supported_speed(npu_id_t npu, port_t port, cps_api_object_t obj)
{
    uint32_t sup_speeds[MAX_NO_OF_SUPP_SPEED] = {0};
    BASE_IF_SPEED_t max_port_speed = BASE_IF_SPEED_0MBPS;

    /* Obtain the max port-speed for the port from the cache */
    if (nas_int_get_phy_speed(npu, port, &max_port_speed) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-FCOE-MAP", "Err obtaining max port speed from cache for port %d",port);
        return;
    } 

    cps_api_object_attr_delete(obj, DELL_IF_IF_INTERFACES_STATE_INTERFACE_SUPPORTED_SPEED);

    EV_LOGGING(INTERFACE,DEBUG,"NAS-FCOE-MAP","Filling INTERFACE_SUPPORTED_SPEED using "
               "max_port_speed: %d for port %u", max_port_speed, port);

    auto it = _fc_port_speed_to_supp_speed_map.find(max_port_speed);
    if (it == _fc_port_speed_to_supp_speed_map.end()) {
        return;  
    }

    for (int ix =0;ix < MAX_NO_OF_SUPP_SPEED; ix++) {
        sup_speeds[ix] = (uint32_t) (it->second)[ix];
        if (sup_speeds[ix] == 0) {
            break;
        }
        cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_STATE_INTERFACE_SUPPORTED_SPEED, 
                                    sup_speeds[ix]);
    }

    return;
}

void nas_fc_phy_fill_supported_speed(npu_id_t npu, port_t port, cps_api_object_t obj)
{
    uint32_t sup_speeds[MAX_NO_OF_SUPP_SPEED] = {0};
    BASE_IF_SPEED_t max_port_speed = BASE_IF_SPEED_0MBPS;
    cps_api_object_attr_t _speed_attr;

    _speed_attr = cps_api_object_attr_get(obj, BASE_IF_PHY_PHYSICAL_SPEED);
    if (_speed_attr != nullptr) {
        max_port_speed = (BASE_IF_SPEED_t) cps_api_object_attr_data_u32(_speed_attr);
        EV_LOGGING(INTERFACE, DEBUG, "NAS-FCOE-MAP", "max_port_speed for FC: %d from phy obj for port:%d ",
                   max_port_speed, port);
    } else {
        EV_LOGGING(INTERFACE, ERR, "NAS-FCOE-MAP", "NULL _speed_attr for port %d",port);
        return;
    }

    cps_api_object_attr_delete(obj, BASE_IF_PHY_PHYSICAL_SUPPORTED_SPEED);

    EV_LOGGING(INTERFACE,DEBUG,"NAS-FCOE-MAP","Filling PHYSICAL_SUPPORTED_SPEED for npu %d and port %u", 
               npu, port);

    auto it = _fc_port_speed_to_supp_speed_map.find(max_port_speed);
    if (it == _fc_port_speed_to_supp_speed_map.end()) {
        return;  
    }

    for (int ix =0;ix < MAX_NO_OF_SUPP_SPEED; ix++) {
        sup_speeds[ix] = (uint32_t) (it->second)[ix];
        if (sup_speeds[ix] == 0) {
            break;
        }
        cps_api_object_attr_add_u32(obj, BASE_IF_PHY_PHYSICAL_SUPPORTED_SPEED, 
                                    sup_speeds[ix]);
    }

    return;
}

static
t_std_error _get_generic_u32(npu_id_t npu, port_t port, cps_api_attr_id_t attr,
         cps_api_object_t obj) {

    nas_fc_id_value_t param;
    memset(&param, 0, sizeof(nas_fc_id_value_t));
    param.attr_id = attr;

    if ((ndi_get_fc_attr(npu, port, &param, 1)) != STD_ERR_OK) {
       EV_LOGGING(INTERFACE,ERR,"NAS-FCOE-MAP"," failed get_generic_u32 for npu  %d for port  %u attr %lu", npu, port, attr);
       return STD_ERR(INTERFACE,FAIL,0);
    }
    cps_api_object_attr_add_u32(obj, attr, param.value.u32);
    return STD_ERR_OK;
}

static
t_std_error _get_generic_mac (npu_id_t npu, port_t port, cps_api_attr_id_t attr, cps_api_object_t obj) {

    nas_fc_id_value_t param;
    memset(&param, 0, sizeof(nas_fc_id_value_t));
    param.attr_id = attr;
    char mac_str[18];
    memset(mac_str, 0, sizeof(mac_str));

    if ((ndi_get_fc_attr(npu, port, &param, 1)) != STD_ERR_OK) {
       EV_LOGGING(INTERFACE,ERR,"NAS-FCOE-MAP"," failed get_generic_mac for npu  %d for port  %u attr %lu", npu, port, attr);
       return STD_ERR(INTERFACE,FAIL,0);
    }

    std_mac_to_string(&param.value.mac, mac_str,sizeof(mac_str));
    cps_api_object_attr_add(obj, BASE_IF_FC_IF_INTERFACES_INTERFACE_INGRESS_SRC_MAC,
            mac_str, strlen(mac_str)+1);

    EV_LOGGING(INTERFACE, DEBUG, "NAS-FCOE-MAP", "_get_generic_mac  mac address %s,  len %lu",
                mac_str, strlen(mac_str));
    return STD_ERR_OK;

}


static
t_std_error _get_generic_bool(npu_id_t npu, port_t port, cps_api_attr_id_t attr, cps_api_object_t obj) {

    nas_fc_id_value_t param;
    memset(&param, 0, sizeof(nas_fc_id_value_t));
    param.attr_id = attr;
    if ((ndi_get_fc_attr(npu, port, &param, 1)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-FCOE-MAP"," failed get_generic_bool for npu  %d for port  %u attr %lu", npu, port, attr);
        return STD_ERR(INTERFACE,FAIL,0);
    }
    cps_api_object_attr_add_u32(obj, attr, param.value.ty_bool);
    return STD_ERR_OK;

}


static const std::unordered_map<cps_api_attr_id_t,
    t_std_error (*)(npu_id_t, port_t, cps_api_attr_id_t, cps_api_object_t )> _get_attr_handlers = {
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_SRC_MAC_MODE, _get_generic_u32 },
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_SRC_MAP_PREFIX, _get_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_INGRESS_SRC_MAC, _get_generic_mac},

        { BASE_IF_FC_IF_INTERFACES_INTERFACE_DEST_MAC_MODE, _get_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_DEST_MAP_PREFIX, _get_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_INGRESS_DEST_MAC, _get_generic_mac},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_VFT_HEADER,      _get_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_BB_CREDIT,      _get_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_BB_CREDIT_RECOVERY,      _get_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_FCOE_PKT_VLANID, _get_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_PRIORITY,       _get_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_MTU,            _get_generic_u32},
        { DELL_IF_IF_INTERFACES_INTERFACE_SPEED,             _get_generic_u32},
        { IF_INTERFACES_STATE_INTERFACE_OPER_STATUS,         _get_generic_u32},
        { BASE_IF_PHY_IF_INTERFACES_INTERFACE_PHY_MEDIA,     _get_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_STATE_INTERFACE_BB_CREDIT_RECEIVE, _get_generic_u32},
        { BASE_IF_FC_IF_INTERFACES_INTERFACE_FLOW_CONTROL_ENABLE, _get_generic_bool},

};



void nas_fc_fill_intf_attr(npu_id_t npu, port_t port, cps_api_object_t obj)
{
    for (auto it = _get_attr_handlers.begin(); it != _get_attr_handlers.end() ; ++it) {
        EV_LOGGING(INTERFACE, ERR ,"NAS-FCOE-MAP"," Get attribute for npu %u, port %u, attr %lu ",
              npu, port, it->first);
        it->second(npu, port, it->first, obj);

    }

}


void nas_fc_fill_misc_state(npu_id_t npu, port_t port, cps_api_object_t obj)
{

    _get_generic_u32(npu, port, DELL_IF_IF_INTERFACES_STATE_INTERFACE_FC_MTU, obj);
    _get_generic_u32(npu, port, DELL_IF_IF_INTERFACES_STATE_INTERFACE_BB_CREDIT, obj);

}
