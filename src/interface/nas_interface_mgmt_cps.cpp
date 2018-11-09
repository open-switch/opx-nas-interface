/*
 * Copyright (c) 2018 Dell EMC.
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
 * nas_interface_mgmt_cps.cpp
 *
 */

#include "nas_os_interface.h"
#include "hal_if_mapping.h"
#include "interface_obj.h"

#include "cps_api_object_attr.h"
#include "dell-base-if-linux.h"
#include "dell-interface.h"

#include "interface/nas_interface_map.h"
#include "interface/nas_interface_utils.h"
#include "vrf-mgmt.h"
#include "event_log.h"
#include "std_utils.h"

#include <inttypes.h>
#include <unordered_map>
#include <list>


extern bool if_data_from_obj(obj_intf_cat_t obj_cat, cps_api_object_t o, interface_ctrl_t& i);

static cps_api_return_code_t _set_attr_mtu (cps_api_object_t obj)
{
    cps_api_return_code_t ret = cps_api_ret_code_OK;
    do {
        cps_api_object_attr_t attr = cps_api_object_attr_get(obj,DELL_IF_IF_INTERFACES_INTERFACE_MTU);
        if (attr == nullptr) {
            ret = cps_api_ret_code_ERR;
            break;
        }

        if (nas_os_mgmt_interface_set_attribute(obj,DELL_IF_IF_INTERFACES_INTERFACE_MTU)!=STD_ERR_OK) {
            ret = cps_api_ret_code_ERR;
            break;
        }
    } while (0);

    return ret;
}

static cps_api_return_code_t _set_attr_mac (cps_api_object_t obj)
{
    cps_api_return_code_t ret = cps_api_ret_code_OK;
    do {
        cps_api_object_attr_t attr = cps_api_object_attr_get(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS);
        if (attr == nullptr) {
            ret = cps_api_ret_code_ERR;
            break;
        }

        if (nas_os_mgmt_interface_set_attribute(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS)!=STD_ERR_OK) {
            ret = cps_api_ret_code_ERR;
            break;
        }
    } while (0);

    return ret;
}
static cps_api_return_code_t _set_attr_up (cps_api_object_t obj)
{
    cps_api_return_code_t ret = cps_api_ret_code_OK;
    do {
        cps_api_object_attr_t attr = cps_api_object_attr_get(obj,IF_INTERFACES_INTERFACE_ENABLED);
        cps_api_object_attr_t _ifix = cps_api_object_attr_get(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
        if ((attr == nullptr) || (_ifix == nullptr)) {
            ret = cps_api_ret_code_ERR;    break;
        }

        if (nas_os_mgmt_interface_set_attribute(obj,IF_INTERFACES_INTERFACE_ENABLED) != STD_ERR_OK) {
            ret = cps_api_ret_code_ERR;
            break;
        }
    } while (0);

    return ret;
}
static cps_api_return_code_t _set_attr_ethtool_cmd_data (const char * if_name, cps_api_object_t obj)
{
    cps_api_return_code_t ret = cps_api_ret_code_OK;
    do {
        if ((if_name == nullptr) || (obj == nullptr)) {
            ret = cps_api_ret_code_ERR;
            break;
        }

        if (nas_os_set_interface_ethtool_cmd_data(if_name, obj) != STD_ERR_OK) {
            ret = cps_api_ret_code_ERR;
            break;
        }
    } while (0);

    return ret;
}
using mgmt_intf_set_attr_handler_t = cps_api_return_code_t (*)(cps_api_object_t);
static const std::unordered_map<cps_api_attr_id_t,
    std::pair<mgmt_intf_set_attr_handler_t, const char*>> _set_attr_handlers = {
        { IF_INTERFACES_INTERFACE_ENABLED, {_set_attr_up, "enabled"} },
        { DELL_IF_IF_INTERFACES_INTERFACE_MTU, {_set_attr_mtu, "mtu"} },
        { DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS, {_set_attr_mac, "mac_addr"} }
};

void nas_interface_mgmt_attr_up(cps_api_object_t obj, bool enabled)
{
    cps_api_object_t sobj = cps_api_object_create();
    hal_ifindex_t ifidx = 0;
    char vrf_name[NAS_VRF_NAME_SZ] = "";
    char name[HAL_IF_NAME_SZ] = "";

    do {
        cps_api_object_attr_t attr_id = cps_api_object_attr_get(obj,
                DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
        if (attr_id == NULL) break;

        ifidx = cps_api_object_attr_data_u32(attr_id);
        attr_id = cps_api_object_attr_get(obj, IF_INTERFACES_INTERFACE_NAME);
        if (attr_id == NULL) break;
        safestrncpy(name, (const char*)cps_api_object_attr_data_bin(attr_id), sizeof(name));

        attr_id = cps_api_object_attr_get(obj, NI_IF_INTERFACES_INTERFACE_BIND_NI_NAME);
        if (attr_id == NULL) break;
        safestrncpy(vrf_name, (const char*)cps_api_object_attr_data_bin(attr_id), sizeof(vrf_name));

        cps_api_object_attr_add(sobj, IF_INTERFACES_INTERFACE_ENABLED, &enabled, sizeof(enabled));
        cps_api_object_attr_add_u32(sobj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, ifidx);
        cps_api_object_attr_add(sobj, IF_INTERFACES_INTERFACE_NAME, name, strlen(name) + 1);
        cps_api_object_attr_add(sobj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
                vrf_name, strlen(vrf_name) + 1);

        _set_attr_up(sobj);

    } while (0);

    cps_api_object_delete(sobj);
    return;
}


cps_api_return_code_t nas_interface_mgmt_if_set (void* context, cps_api_transaction_params_t * param,
                                                 size_t ix)
{
    cps_api_object_t req_if = cps_api_object_list_get(param->change_list,ix);
    if (req_if == nullptr) return cps_api_ret_code_ERR;

    cps_api_object_t prev = cps_api_object_list_create_obj_and_append(param->prev);
    if (prev == nullptr) return cps_api_ret_code_ERR;

    cps_api_object_attr_t _ifix = cps_api_object_attr_get(req_if,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
    cps_api_object_attr_t _name = cps_api_get_key_data(req_if,IF_INTERFACES_INTERFACE_NAME);

    if ((_ifix == NULL) && (_name==NULL)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-MGMT-INTF", "Requested Intf Object is Null for Interface update ");
        return cps_api_ret_code_ERR;
    }
    interface_ctrl_t _port;
    if (!if_data_from_obj(obj_INTF, req_if,_port)) {
        return cps_api_ret_code_ERR;
    }

    std::string st_name = std::string(_port.if_name);
    cps_api_object_attr_t attr_up = cps_api_object_attr_get(req_if,IF_INTERFACES_INTERFACE_ENABLED);
    if (attr_up != NULL) {
        bool state = (bool) cps_api_object_attr_data_uint(attr_up);
        nas_interface_util_mgmt_enabled_set(st_name, state);
    }
    cps_api_object_attr_add_u32(req_if, VRF_MGMT_NI_IF_INTERFACES_INTERFACE_VRF_ID, _port.vrf_id);
    cps_api_object_attr_add_u32(prev,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,_port.if_index);
    if(_ifix == NULL) {
        cps_api_object_attr_add_u32(req_if,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,_port.if_index);
    }

    cps_api_object_attr_t an_attr = cps_api_object_attr_get(req_if,
            DELL_IF_IF_INTERFACES_INTERFACE_AUTO_NEGOTIATION);
    cps_api_object_attr_t dp_attr = cps_api_object_attr_get(req_if,
            DELL_IF_IF_INTERFACES_INTERFACE_DUPLEX);
    cps_api_object_attr_t sp_attr = cps_api_object_attr_get(req_if,
            DELL_IF_IF_INTERFACES_INTERFACE_SPEED);

    if ((an_attr != NULL) || (dp_attr != NULL) || (sp_attr != NULL)) {
        if (dp_attr != NULL) {
            BASE_CMN_DUPLEX_TYPE_t dp = (BASE_CMN_DUPLEX_TYPE_t)cps_api_object_attr_data_uint(dp_attr);
            nas_interface_util_mgmt_duplex_set(st_name, dp);
        } else {
            BASE_CMN_DUPLEX_TYPE_t dp = BASE_CMN_DUPLEX_TYPE_AUTO;
            nas_interface_util_mgmt_duplex_get(st_name, &dp);
            cps_api_object_attr_add_u32(req_if, DELL_IF_IF_INTERFACES_INTERFACE_DUPLEX, dp);
        }
        if (sp_attr != NULL) {
            BASE_IF_SPEED_t sp = (BASE_IF_SPEED_t)cps_api_object_attr_data_uint(sp_attr);
            nas_interface_util_mgmt_speed_set(st_name, sp);
        } else {
            BASE_IF_SPEED_t sp = BASE_IF_SPEED_AUTO;
            nas_interface_util_mgmt_speed_get(st_name, &sp);
            cps_api_object_attr_add_u32(req_if, DELL_IF_IF_INTERFACES_INTERFACE_SPEED, sp);
        }
        if (_set_attr_ethtool_cmd_data (_port.if_name, req_if) != cps_api_ret_code_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-MGMT-INTF", "Management interface ethtool cmd data set failed.");
            return cps_api_ret_code_ERR;
        }
    }

    cps_api_object_it_t it_req;
    cps_api_object_it_begin(req_if,&it_req);
    cps_api_return_code_t ret = cps_api_ret_code_OK;

    for ( ; cps_api_object_it_valid(&it_req); cps_api_object_it_next(&it_req)) {
        cps_api_attr_id_t id = cps_api_object_attr_id(it_req.attr);
        auto func = _set_attr_handlers.find(id);
        if (func ==_set_attr_handlers.end()) continue;

        EV_LOGGING(INTERFACE, INFO, "NAS-MGMT-INTF", "Set attribute %s (%" PRId64 ") for interface %s",
                func->second.second, id, _port.if_name);

        ret = func->second.first(req_if);

        if (ret!=cps_api_ret_code_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-MGMT-INTF","Failed to set Attribute %s (%" PRId64 ") for interface  %s",
                    func->second.second, id, _port.if_name);
            return ret;
        }
    }

    cps_api_key_set(cps_api_object_key(req_if),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_OBSERVED);
    cps_api_event_thread_publish(req_if);
    cps_api_key_set(cps_api_object_key(req_if),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_TARGET);
    return cps_api_ret_code_OK;
}

cps_api_return_code_t nas_interface_mgmt_if_state_set (void * context, cps_api_transaction_params_t * param,
                                                       size_t key_ix)
{
    EV_LOGGING(INTERFACE, ERR, "NAS-MGMT-INTF", "Invalid request, MGMT IF_STATE set ");
    return cps_api_ret_code_ERR;
}
