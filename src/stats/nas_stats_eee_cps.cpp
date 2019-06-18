/*
 * Copyright (c) 2019 Dell Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 * LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 * FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 * See the Apache Version 2.0 License for specific language governing
 * permissions and limitations under the License.
 */

/*
 * filename: nas_stats_eee_cps.cpp
 */



#include "dell-base-if.h"
#include "dell-interface.h"
#include "interface_obj.h"
#include "hal_if_mapping.h"

#include "cps_api_key.h"
#include "cps_api_object_key.h"
#include "cps_class_map.h"
#include "cps_api_operation.h"
#include "event_log.h"
#include "nas_ndi_plat_stat.h"
#include "nas_ndi_port.h"

static bool nas_get_ifindex (cps_api_object_t obj,
                             hal_ifindex_t *index,
                             bool clear)
{
    cps_api_attr_id_t attr_id;

    if (clear) {
        attr_id = DELL_IF_CLEAR_EEE_COUNTERS_INPUT_INTF_CHOICE_IFNAME_CASE_IFNAME;
    } else {
        attr_id = IF_INTERFACES_STATE_INTERFACE_NAME;
    }

    cps_api_object_attr_t if_name_attr = cps_api_get_key_data(obj,attr_id);

    if (clear) {
        attr_id = DELL_IF_CLEAR_EEE_COUNTERS_INPUT_INTF_CHOICE_IFINDEX_CASE_IFINDEX;
    } else {
        attr_id = DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX;
    }

    cps_api_object_attr_t if_index_attr = cps_api_object_attr_get(obj, attr_id);


    if (if_index_attr == NULL && if_name_attr == NULL) {
        EV_LOGGING (NAS_INT_STATS, ERR, "NAS-STAT",
                   "Missing Name/ifindex attribute for STAT Get");
        return false;
    }

    if (if_index_attr){
        *index = (hal_ifindex_t) cps_api_object_attr_data_u32(if_index_attr);
    } else {
        const char * name = (const char *)cps_api_object_attr_data_bin(if_name_attr);
        interface_ctrl_t i;

        memset(&i,0,sizeof(i));
        strncpy(i.if_name,name,sizeof(i.if_name)-1);
        i.q_type = HAL_INTF_INFO_FROM_IF_NAME;
        if (dn_hal_get_interface_info(&i)!=STD_ERR_OK){
            EV_LOGGING (NAS_INT_STATS, ERR, "NAS-STAT",
                       "Can't get interface control information for %s", name);
            return false;
        }
        *index = i.if_index;
    }
    return true;
}

static cps_api_return_code_t if_eee_stats_clear (void *context,
                                                 cps_api_transaction_params_t *param,
                                                 size_t ix)
{

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    cps_api_operation_types_t op
        = cps_api_object_type_operation(cps_api_object_key(obj));

    if (op != cps_api_oper_ACTION) {
        EV_LOGGING (NAS_INT_STATS, ERR, "NAS-EEE-STATS",
                   "Invalid operation %d for clearing stat", op);
        return (cps_api_ret_code_ERR);
    }

    hal_ifindex_t ifindex = 0;
    if (!nas_get_ifindex(obj, &ifindex, true)) {
        return (cps_api_ret_code_ERR);
    }

    interface_ctrl_t intf_ctrl;
    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
    intf_ctrl.if_index = ifindex;

    /*
     * Sanity
     */
    if (dn_hal_get_interface_info(&intf_ctrl) != STD_ERR_OK) {
        EV_LOGGING (NAS_INT_STATS, ERR, "NAS-EEE-STATS",
                   "Interface %d has NO slot %d, port %d",
                   intf_ctrl.if_index, intf_ctrl.npu_id, intf_ctrl.port_id);
        return (cps_api_ret_code_ERR);
    }

    /*
     * Clear the EEE stats
     */
    if (ndi_port_clear_eee_stats(intf_ctrl.npu_id, intf_ctrl.port_id)
        != STD_ERR_OK) {
        EV_LOGGING (NAS_INT_STATS, ERR, "NAS-EEE-STATS", "NDI failed");

        return (cps_api_ret_code_ERR);
    }

    EV_LOGGING (NAS_INT_STATS, DEBUG, "NAS-EEE-STATS", "leaving");

    return cps_api_ret_code_OK;
}

/*
 * Register if_eee_stats_clear() with CPS
 */
extern "C" t_std_error nas_eee_stats_if_init (cps_api_operation_handle_t handle) {

    cps_api_registration_functions_t f;
    char buff[CPS_API_KEY_STR_MAX];

    memset(&f, 0, sizeof(f));

    memset(buff, 0, sizeof(buff));


    if (!cps_api_key_from_attr_with_qual(&f.key, DELL_BASE_IF_CMN_DELL_IF_CLEAR_EEE_COUNTERS_INPUT_OBJ,
                                         cps_api_qualifier_TARGET)) {
        EV_LOGGING (NAS_INT_STATS, ERR, "NAS-EEE-STATS",
                   "Could not translate %d to key %s",
                   DELL_BASE_IF_CMN_DELL_IF_CLEAR_EEE_COUNTERS_INPUT_OBJ,
                   cps_api_key_print(&f.key, buff, sizeof(buff) - 1));
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    f.handle = handle;
    f._write_function = if_eee_stats_clear;

    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;
}
