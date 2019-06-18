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

/*!
 * \file   intf_main.c
 * \brief  Interface Mgmt main file
 */

#include "ds_common_types.h"
#include "cps_api_events.h"
#include "cps_api_operation.h"
#include "cps_api_object_key.h"
#include "cps_class_map.h"
#include "nas_int_physical_cps.h"
#include "dell-base-if-linux.h"
#include "dell-base-if.h"
#include "dell-base-if-vlan.h"
#include "dell-base-if-mgmt.h"
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

#include "nas_int_port.h"
#include "nas_stats.h"
#include "nas_fc_stats.h"
#include "nas_int_utils.h"
#include "std_utils.h"
#include "nas_vrf.h"
#include "bridge/nas_interface_bridge_cps.h"
#include "bridge/nas_vlan_bridge_cps.h"
#include "interface/nas_interface_cps.h"
#include "interface/nas_interface_vxlan_cps.h"
#include "nas_vrf_utils.h"

#include "nas_os_interface.h"
#include "nas_ndi_port.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <set>

#define NUM_INT_CPS_API_THREAD 1

static cps_api_operation_handle_t nas_if_handle;
extern t_std_error mgmt_intf_init (void);

void hal_interface_send_event(cps_api_object_t obj) {
    if (cps_api_event_thread_publish(obj)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INTF-EVENT","Failed to send event.  Service issue");
    }
}

static auto oper_state_handlers = new std::set <oper_state_handler_t> ;

void nas_int_oper_state_register_cb(oper_state_handler_t oper_state_cb) {
    if (oper_state_cb != NULL) {
        oper_state_handlers->insert(oper_state_cb);
    }
}

static void hw_link_state_cb(npu_id_t npu, npu_port_t port,
        ndi_intf_link_state_t *data) {

    IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t status = ndi_to_cps_oper_type(data->oper_status);
    for (auto it = oper_state_handlers->begin(); it != oper_state_handlers->end(); ++it) {
        (*it)(npu, port, status);
    }
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

    if (nas_vxlan_remote_endpoint_handler_register() != STD_ERR_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (mgmt_intf_init() != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR,"NAS-MGMT-INTF", "Management interface model initialization failed.");
    }

    //Create a handle for CPS objects
    if (cps_api_operation_subsystem_init(&nas_if_handle,NUM_INT_CPS_API_THREAD)!=cps_api_ret_code_OK) {
        return STD_ERR(CPSNAS,FAIL,0);
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

    if((rc = nas_vlan_bridge_cps_init(nas_if_handle)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INTF-CPS-VLAN-SWERR", "Initializing CPS for VLAN failed");
        return rc;
    }

    if((rc = nas_vxlan_bridge_cps_init(nas_if_handle)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INTF-CPS-VXLAN-SWERR", "Initializing CPS for VXLAN failed");
        return rc;
    }

    if ( (rc=nas_stats_if_init(nas_if_handle))!= STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-SWERR", "Initializing interface statistic failed");
        return rc;
    }

    if (nas_switch_get_fc_supported()) {
        if ( (rc=nas_stats_fc_if_init(nas_if_handle))!= STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-FC-INT-SWERR", "Initializing interface FC statistics failed");
            return rc;
        }
    }

    if ( (rc=nas_interface_generic_init(nas_if_handle))!= STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-SWERR", "Initializing vlan and vxlan interface failed");
        return rc;
    }

    if ( (rc=nas_stats_vlan_init(nas_if_handle))!= STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-SWERR", "Initializing vlan statistics failed");
        return rc;
    }

    if ( (rc=nas_stats_bridge_init(nas_if_handle))!= STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-SWERR", "Initializing vlan statistics failed");
        return rc;
    }

    if ( (rc=nas_stats_vlan_sub_intf_init(nas_if_handle))!= STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-SWERR", "Initializing vlan statistics failed");
        return rc;
    }

    if ( (rc=nas_stats_vxlan_init(nas_if_handle))!= STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-SWERR", "Initializing vlan statistics failed");
        return rc;
    }

    if ( (rc=nas_stats_tunnel_init(nas_if_handle))!= STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-SWERR", "Initializing tunnel statistics failed");
        return rc;
    }

    if ( (rc=nas_stats_virt_network_init(nas_if_handle))!= STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-SWERR", "Initializing virt network statistics failed");
        return rc;
    }


    if ((rc=nas_eee_stats_if_init(nas_if_handle)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-EEE-INT-SWERR",
                   "Initializing interface EEE statistics failed");
        return rc;
    }

    if ( (rc=interface_obj_init(nas_if_handle))!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT-IF", "Failed to initialize common interface handler");
        return rc;
    }
    if ( (rc=nas_bridge_cps_obj_init(nas_if_handle))!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT-IF", "Failed to initialize Bridge handler");
        return rc;
    }

    if((rc = nas_vxlan_init(nas_if_handle)) != STD_ERR_OK){
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-VXLAN-INIT","Failed to initialize VxLAN handler");
        return rc;
    }

    nas_default_vlan_cache_init();
    nas_shell_command_init();
    return (rc);
}

