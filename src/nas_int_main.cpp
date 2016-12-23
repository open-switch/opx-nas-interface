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

/*!
 * \file   intf_main.c
 * \brief  Interface Mgmt main file
 */

#include "ds_common_types.h"
#include "cps_api_events.h"
#include "cps_api_operation.h"
#include "cps_api_object_key.h"
#include "cps_class_map.h"
#include "nas_int_bridge.h"
#include "nas_int_physical_cps.h"
#include "dell-base-if-linux.h"
#include "dell-base-if.h"
#include "dell-base-if-vlan.h"
#include "dell-interface.h"
#include "nas_int_lag_cps.h"
#include "interface_obj.h"
#include "iana-if-type.h"

#include "hal_interface.h"
#include "hal_if_mapping.h"
#include "nas_switch.h"
#include "std_mac_utils.h"
#include "hal_interface_defaults.h"
#include "hal_interface_common.h"
#include "event_log_types.h"
#include "event_log.h"
#include "nas_ndi_port.h"
#include "std_assert.h"

#include "std_config_file.h"
#include "nas_int_vlan.h"
#include "nas_int_port.h"
#include "nas_stats.h"
#include "std_utils.h"

#include "nas_ndi_port.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <set>


#define NUM_INT_CPS_API_THREAD 1

static cps_api_operation_handle_t nas_if_handle;

static char if_mac_base[MAC_STRING_SZ];

void hal_interface_send_event(cps_api_object_t obj) {
    if (cps_api_event_thread_publish(obj)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INTF-EVENT","Failed to send event.  Service issue");
    }
}

static std::set <oper_state_handler_t> oper_state_handlers;

void nas_int_oper_state_register_cb(oper_state_handler_t oper_state_cb) {
    if (oper_state_cb != NULL) {
        oper_state_handlers.insert(oper_state_cb);
    }
}

static void hw_link_state_cb(npu_id_t npu, npu_port_t port,
        ndi_intf_link_state_t *data) {

    IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t status = ndi_to_cps_oper_type(data->oper_status);
    for (auto it = oper_state_handlers.begin(); it != oper_state_handlers.end(); ++it) {
        (*it)(npu, port, status);
    }
}

static void nas_if_get_base_mac_addr(void) {
    hal_mac_addr_t mac_addr;

    while(1) {
        if (nas_get_platform_base_mac_address(&mac_addr) == STD_ERR_OK)  {
            std_mac_to_string((const hal_mac_addr_t *)&mac_addr, if_mac_base, sizeof(if_mac_base));
            break;
        }
        EV_LOGGING(INTERFACE, ERR ,"NAS-INTF-MAC","waiting for base mac address from PAS");
        sleep(1);
    }
}

t_std_error nas_if_get_base_mac(char *mac_base, size_t mac_buf_size) {

    t_std_error rc = STD_ERR(INTERFACE,FAIL,0);

    if (mac_base != NULL) {
        safestrncpy(mac_base, if_mac_base, mac_buf_size);
        rc = STD_ERR_OK;
    }
    return rc;
}

t_std_error nas_if_get_assigned_mac(const char *if_type,
                                    const char *if_name,
                                    hal_vlan_id_t vlan_id,
                                    char *mac_addr, size_t mac_buf_size)
{
    if (mac_buf_size == 0) {
        return STD_ERR_OK;
    }
    cps_api_object_t obj = cps_api_object_create();
    if (obj == NULL) {
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    if (!cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                         DELL_BASE_IF_CMN_GET_MAC_ADDRESS_OBJ,
                                         cps_api_qualifier_TARGET)) {
        cps_api_object_delete(obj);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_TYPE, if_type, strlen(if_type) + 1);

    if (strncmp(if_type,
                IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
                sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN)) == 0) {
        if (vlan_id == 0) {
            return STD_ERR(INTERFACE, FAIL, 0);
        }
        cps_api_object_attr_add_u16(obj, BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, vlan_id);
    } else {
        if (if_name == NULL) {
            return STD_ERR(INTERFACE, FAIL, 0);
        }
        cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_NAME, if_name, strlen(if_name) + 1);
    }

    cps_api_transaction_params_t tr;
    if (cps_api_transaction_init(&tr) != cps_api_ret_code_OK) {
        cps_api_object_delete(obj);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    cps_api_action(&tr, obj);
    t_std_error rc = STD_ERR(INTERFACE, FAIL, 0);
    do {
        if (cps_api_commit(&tr) != cps_api_ret_code_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-INTF-MAC", "Failed to commit");
            break;
        }
        size_t mx = cps_api_object_list_size(tr.change_list);
        if (mx == 0) {
            EV_LOGGING(INTERFACE, ERR, "NAS-INTF-MAC", "No object returned");
            break;
        }
        obj = cps_api_object_list_get(tr.change_list, 0);
        cps_api_object_attr_t attr = cps_api_object_attr_get(obj,
                                        DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS);
        if (attr == NULL) {
            EV_LOGGING(INTERFACE, ERR, "NAS-INTF-MAC", "No mac address in returned object");
            break;
        }
        char *addr_str = (char *)cps_api_object_attr_data_bin(attr);
        if (strlen(addr_str) > MAC_STRING_SZ) {
            EV_LOGGING(INTERFACE, ERR, "NAS-INTF-MAC", "Invalid mac address format: %s",
                       addr_str);
            break;
        }
        safestrncpy(mac_addr, addr_str, mac_buf_size);
        rc = STD_ERR_OK;
    } while(0);
    cps_api_transaction_close(&tr);

    return rc;
}

/*
 * Initialize the interface management module
 */
t_std_error hal_interface_init(void) {
    t_std_error rc;

    // register for events
    cps_api_event_reg_t reg;
    memset(&reg,0,sizeof(reg));
    const uint_t NUM_EVENTS=1;

    cps_api_key_t keys[NUM_EVENTS];

    char buff[CPS_API_KEY_STR_MAX];

    nas_if_get_base_mac_addr();
    if (!cps_api_key_from_attr_with_qual(&keys[0],
                BASE_IF_LINUX_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_OBSERVED)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Could not translate %d to key %s",
            (int)(BASE_IF_LINUX_IF_INTERFACES_INTERFACE_OBJ),cps_api_key_print(&keys[0],buff,sizeof(buff)-1));
        return STD_ERR(INTERFACE,FAIL,0);
    }
    EV_LOG(INFO, INTERFACE,0,"NAS-IF-REG", "Registered for interface events with key %s",
                    cps_api_key_print(&keys[0],buff,sizeof(buff)-1));

    reg.number_of_objects = NUM_EVENTS;
    reg.objects = keys;

    if (cps_api_event_thread_reg(&reg,nas_int_ev_handler_cb,NULL)!=cps_api_ret_code_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }

    //Create a handle for CPS objects
    if (cps_api_operation_subsystem_init(&nas_if_handle,NUM_INT_CPS_API_THREAD)!=cps_api_ret_code_OK) {
        return STD_ERR(CPSNAS,FAIL,0);
    }

    if ( (rc=interface_obj_init(nas_if_handle))!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT-IF", "Failed to initialize common interface handler");
        return rc;
    }

    if (ndi_port_oper_state_notify_register(hw_link_state_cb)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR,"NAS-INT-INIT","Initializing Interface callback failed");
        return STD_ERR(INTERFACE,FAIL,0);
    }
    if ( (rc=nas_int_cps_init(nas_if_handle))!= STD_ERR_OK) {
        EV_LOG_ERR(ev_log_t_INTERFACE, 3,"NAS-INT-SWERR", "Initializing interface management failed");
        return rc;
    }

    if((rc = nas_cps_lag_init(nas_if_handle)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INTF-CPS-LAG-SWERR", "Initializing CPS for LAG failed");
        return rc;
    }

    if((rc = nas_cps_vlan_init(nas_if_handle)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INTF-CPS-VLAN-SWERR", "Initializing CPS for VLAN failed");
        return rc;
    }

    if ( (rc=nas_stats_if_init(nas_if_handle))!= STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-SWERR", "Initializing interface statistic failed");
        return rc;
    }

    if ( (rc=nas_stats_vlan_init(nas_if_handle))!= STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-SWERR", "Initializing vlan statistics failed");
        return rc;
    }

    return (rc);
}

