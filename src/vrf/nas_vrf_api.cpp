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
 * nas_vrf_api.cpp
 *
 */

#include "event_log.h"
#include "nas_os_l3.h"
#include "nas_vrf.h"
#include "dell-base-interface-common.h"
#include "dell-base-common.h"
#include "ietf-network-instance.h"
#include "vrf-mgmt.h"
#include "cps_api_events.h"
#include "cps_class_map.h"
#include "nas_switch.h"
#include "nas_ndi_router_interface.h"
#include "std_utils.h"
#include "nas_vrf_utils.h"
#include "nas_int_com_utils.h"

static cps_api_event_service_handle_t         _handle;

t_std_error nas_vrf_create_publish_handle() {
    if (cps_api_event_client_connect(&_handle) != cps_api_ret_code_OK) {
        NAS_VRF_LOG_ERR("VRF-PUB", "Failed to create the handle for event publish!");
        return STD_ERR(ROUTE,FAIL,0);
    }
    return STD_ERR_OK;
}

cps_api_return_code_t nas_vrf_publish_event(cps_api_object_t msg) {
    cps_api_return_code_t rc = cps_api_ret_code_OK;

    rc = cps_api_event_publish(_handle,msg);
    return rc;
}

/* This function provides parent interface to router interface mapping */
cps_api_return_code_t nas_vrf_get_intf_info(cps_api_object_list_t list, const char *vrf_name,
                                            const char *if_name) {
    interface_ctrl_t intf_ctrl;

    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));
    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    safestrncpy(intf_ctrl.if_name, (const char *)if_name,
                sizeof(intf_ctrl.if_name));

    if ((dn_hal_get_interface_info(&intf_ctrl)) != STD_ERR_OK) {
        NAS_VRF_LOG_ERR("VRF-INTF-GET", "Invalid interface %s interface get failed ",
                        if_name);
        return cps_api_ret_code_ERR;
    }
    /* See if we have router interface on the parent interface,
     * if present, return interface info for that interface. */
    if (intf_ctrl.l3_intf_info.if_index != 0) {
        hal_vrf_id_t rt_vrf_id = intf_ctrl.l3_intf_info.vrf_id;
        hal_ifindex_t rt_if_index = intf_ctrl.l3_intf_info.if_index;
        memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));
        intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
        intf_ctrl.vrf_id = rt_vrf_id;
        intf_ctrl.if_index = rt_if_index;

        if ((dn_hal_get_interface_info(&intf_ctrl)) != STD_ERR_OK) {
            NAS_VRF_LOG_ERR("VRF-INTF-GET", "Invalid interface VRF-id:%d if-index:%d.",
                            rt_vrf_id, rt_if_index);
            return cps_api_ret_code_ERR;
        }
    }

    if (vrf_name) {
        hal_vrf_id_t vrf_id = 0;
        if (nas_get_vrf_internal_id_from_vrf_name(vrf_name, &vrf_id) != STD_ERR_OK) {
            NAS_VRF_LOG_ERR("VRF-INTF-GET", "Invalid VRF name:%s ",
                            vrf_name);
            return cps_api_ret_code_ERR;
        }

        if (intf_ctrl.vrf_id != vrf_id) {
            /* Invalid VRF-id, return failure! */
            return cps_api_ret_code_ERR;
        }
    }

    cps_api_object_t obj = cps_api_object_create();
    if(obj == NULL){
        NAS_VRF_LOG_ERR("VRF-INTF-GET", "Failed to allocate memory to cps object");
        return cps_api_ret_code_ERR;
    }

    cps_api_key_t key;
    cps_api_key_from_attr_with_qual(&key, NI_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_set_key(obj,&key);

    if (intf_ctrl.vrf_id == NAS_DEFAULT_VRF_ID) {
        cps_api_object_attr_add(obj,NI_IF_INTERFACES_INTERFACE_BIND_NI_NAME, NAS_DEFAULT_VRF_NAME,
                                strlen(NAS_DEFAULT_VRF_NAME)+1);
    } else {
        cps_api_object_attr_add(obj,NI_IF_INTERFACES_INTERFACE_BIND_NI_NAME, intf_ctrl.vrf_name,
                                strlen(intf_ctrl.vrf_name)+1);
    }
    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_NAME, if_name, strlen(if_name)+1);

    cps_api_object_attr_add(obj, VRF_MGMT_NI_IF_INTERFACES_INTERFACE_IFNAME, (const void *)intf_ctrl.if_name,
                            strlen(intf_ctrl.if_name)+1);

    cps_api_object_attr_add_u32(obj,VRF_MGMT_NI_IF_INTERFACES_INTERFACE_IFINDEX, intf_ctrl.if_index);
    cps_api_object_attr_add(obj, VRF_MGMT_NI_IF_INTERFACES_INTERFACE_MAC_ADDR, (const void *)intf_ctrl.mac_addr,
                            strlen(intf_ctrl.mac_addr)+1);

    if (!cps_api_object_list_append(list,obj)) {
        cps_api_object_delete(obj);
        NAS_VRF_LOG_ERR("VRF-INTF-GET", "Object append to list is failed for %s", if_name);
        return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}

/* This function provides router interface to parent interface mapping */
cps_api_return_code_t nas_vrf_get_router_intf_info(cps_api_object_list_t list, const char *if_name) {
    interface_ctrl_t intf_ctrl;

    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));
    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    safestrncpy(intf_ctrl.if_name, (const char *)if_name,
                sizeof(intf_ctrl.if_name));

    if ((dn_hal_get_interface_info(&intf_ctrl)) != STD_ERR_OK) {
        NAS_VRF_LOG_ERR("VRF-INTF-GET", "Invalid interface %s interface get failed ",
                        if_name);
        return cps_api_ret_code_ERR;
    }
    /* See if we have router interface on the parent interface,
     * if present, return interface info for that interface. */
    if (intf_ctrl.l3_intf_info.if_index != 0) {
        hal_vrf_id_t rt_vrf_id = intf_ctrl.l3_intf_info.vrf_id;
        hal_ifindex_t rt_if_index = intf_ctrl.l3_intf_info.if_index;
        memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));
        intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
        intf_ctrl.vrf_id = rt_vrf_id;
        intf_ctrl.if_index = rt_if_index;

        if ((dn_hal_get_interface_info(&intf_ctrl)) != STD_ERR_OK) {
            NAS_VRF_LOG_ERR("VRF-INTF-GET", "Invalid interface VRF-id:%d if-index:%d.",
                            rt_vrf_id, rt_if_index);
            return cps_api_ret_code_ERR;
        }
    }

    cps_api_object_t obj = cps_api_object_create();
    if(obj == NULL){
        NAS_VRF_LOG_ERR("VRF-INTF-GET", "Failed to allocate memory to cps object");
        return cps_api_ret_code_ERR;
    }

    cps_api_key_t key;
    cps_api_key_from_attr_with_qual(&key, VRF_MGMT_ROUTER_INTF_ENTRY_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_set_key(obj,&key);

    /* @@TODO Fill vrf-name also?
    if (intf_ctrl.vrf_id == NAS_DEFAULT_VRF_ID) {
        cps_api_object_attr_add(obj,NI_IF_INTERFACES_INTERFACE_BIND_NI_NAME, NAS_DEFAULT_VRF_NAME,
                                strlen(NAS_DEFAULT_VRF_NAME)+1);
    } else {
        cps_api_object_attr_add(obj,NI_IF_INTERFACES_INTERFACE_BIND_NI_NAME, intf_ctrl.vrf_name,
                                strlen(intf_ctrl.vrf_name)+1);
    }
    */
    cps_api_object_attr_add(obj,VRF_MGMT_ROUTER_INTF_ENTRY_NAME, if_name, strlen(if_name)+1);

    cps_api_object_attr_add(obj, VRF_MGMT_ROUTER_INTF_ENTRY_IFNAME, (const void *)intf_ctrl.if_name,
                            strlen(intf_ctrl.if_name)+1);

    cps_api_object_attr_add_u32(obj, VRF_MGMT_ROUTER_INTF_ENTRY_IFINDEX, intf_ctrl.if_index);

    if (!cps_api_object_list_append(list,obj)) {
        cps_api_object_delete(obj);
        NAS_VRF_LOG_ERR("VRF-INTF-GET", "Object append to list is failed for %s", if_name);
        return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}


/* @@TODO The assumption here for VRF oid delete is all the dependent modules
 * have cleaned-up the VRF oid dependent data */
bool nas_vrf_update_vrf_id(const char *vrf_name, bool is_add) {
    t_std_error rc;

    nas_vrf_ctrl_t vrf_info;
    memset(&vrf_info, 0, sizeof(vrf_info));

    /* Dont create VRF object for out of band management VRF */
    if (strncmp(vrf_name, NAS_MGMT_VRF_NAME, sizeof(NAS_MGMT_VRF_NAME)) != 0) {
        if (is_add) {
            ndi_vr_entry_t  vr_entry;
            /* Create a virtual router entry and get vr_id (maps to fib vrf id) */
            memset (&vr_entry, 0, sizeof (ndi_vr_entry_t));

            nas_switch_wait_for_sys_base_mac(&vr_entry.src_mac);

            /*
             * Set system MAC address for the VRs
             */
            vr_entry.flags = NDI_VR_ATTR_SRC_MAC_ADDRESS;

            if ((rc = ndi_route_vr_create(&vr_entry, &vrf_info.vrf_id))!= STD_ERR_OK) {
                NAS_VRF_LOG_ERR("VRF-ID", "VRF oid creation failed for VRF:%s", vrf_name);
                return false;
            }
        } else {
            nas_obj_id_t ndi_vr_id = 0;
            if (nas_get_vrf_obj_id_from_vrf_name(vrf_name, &ndi_vr_id) == STD_ERR_OK) {
                if ((rc = ndi_route_vr_delete(0, ndi_vr_id))!= STD_ERR_OK) {
                    NAS_VRF_LOG_ERR("VRF-ID", "VRF oid deletion failed for VRF:%s VRFF id 0x%lx", vrf_name, ndi_vr_id);
                    return false;
                }
                NAS_VRF_LOG_INFO("VRF-ID", "VRF oid deletion successful for VRF:%s id 0x%lx", vrf_name, ndi_vr_id);
            }
        }
    }
    safestrncpy(vrf_info.vrf_name, vrf_name, sizeof(vrf_info.vrf_name));
    if (nas_update_vrf_info((is_add ? NAS_VRF_OP_ADD : NAS_VRF_OP_DEL), &vrf_info) != STD_ERR_OK) {
        NAS_VRF_LOG_ERR("VRF-ID-GET", "VRF oid update failed for VRF:%s is_add:%d", vrf_name, is_add);
        return false;
    }
    return true;
}

cps_api_return_code_t nas_vrf_process_cps_vrf_msg(cps_api_transaction_params_t * param, size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    cps_api_return_code_t rc = cps_api_ret_code_OK;

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    cps_api_object_t cloned = cps_api_object_create();
    if (!cloned) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "CPS malloc error");
        return cps_api_ret_code_ERR;
    }
    cps_api_object_clone(cloned,obj);
    cps_api_object_list_append(param->prev,cloned);

    const char *vrf_name = (const char *)cps_api_object_get_data(obj,
                                                                 NI_NETWORK_INSTANCES_NETWORK_INSTANCE_NAME);
    if (vrf_name == nullptr) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "VRF name is not present");
        return cps_api_ret_code_ERR;
    }

    if (strlen(vrf_name) > NAS_VRF_NAME_SZ) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "Error - VRF name:%s is more than the allowed length:%d",
                        vrf_name, NAS_VRF_NAME_SZ);
        return cps_api_ret_code_ERR;
    }

    if (strncmp(vrf_name, NAS_DEFAULT_VRF_NAME, sizeof(NAS_DEFAULT_VRF_NAME)) == 0) {
        NAS_VRF_LOG_INFO("NAS-RT-CPS-SET", "Default VRF name:%s op:%d", vrf_name, op);
        return cps_api_ret_code_OK;
    }
    if (op == cps_api_oper_CREATE) {
        NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "In VRF CREATE ");
        if (nas_vrf_update_vrf_id(vrf_name, true) == false) {
            NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "VRF id handling failed for VRF:%s!", vrf_name);
            return cps_api_ret_code_ERR;
        }
        if(nas_os_add_vrf(obj) != STD_ERR_OK){
            NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "OS VRF add failed for VRF:%s", vrf_name);
            rc = cps_api_ret_code_ERR;
        }
    } else if (op == cps_api_oper_SET) {
        NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "In VRF SET");
        if(nas_os_set_vrf(obj) != STD_ERR_OK){
            NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "OS VRF update failed");
            rc = cps_api_ret_code_ERR;
        }
    } else if (op == cps_api_oper_DELETE) {
        NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "In VRF del ");
        if(nas_os_del_vrf(obj) != STD_ERR_OK){
            NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "OS VRF del failed");
            rc = cps_api_ret_code_ERR;
        }
        /* Call L3 multicast synchronous cleanup */
        if (nas_l3mc_vrf_cleanup(vrf_name) == false)
        {
            NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "Synchornous L3 multicast cleanup failed for VRF:%s", vrf_name);
        }
        if (nas_vrf_update_vrf_id(vrf_name, false) == false) {
            NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "VRF id delete failed for VRF:%s!", vrf_name);
            return cps_api_ret_code_ERR;
        }
    }

    if (rc == cps_api_ret_code_OK) {
        cps_api_key_set(cps_api_object_key(obj),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_OBSERVED);
        if (nas_vrf_publish_event(obj) != cps_api_ret_code_OK) {
            NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "VRF publish failed!");
        }
        cps_api_key_set(cps_api_object_key(obj),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_TARGET);
    }
    return rc;
}

/* This function is used only mgmt VRF, will migrate to nas_intf_bind_vrf_rpc_handler for all VRFs soon. */
cps_api_return_code_t nas_vrf_process_cps_vrf_intf_msg(cps_api_transaction_params_t * param, size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    cps_api_return_code_t rc = cps_api_ret_code_OK;

    if (obj == NULL) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS","Route VRF Intf object is not present");
        return cps_api_ret_code_ERR;
    }

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    cps_api_object_t cloned = cps_api_object_create();
    if (!cloned) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "CPS malloc error");
        return cps_api_ret_code_ERR;
    }
    cps_api_object_clone(cloned,obj);
    cps_api_object_list_append(param->prev,cloned);

    const char *vrf_name = (const char *)cps_api_object_get_data(obj,
                                                                 NI_IF_INTERFACES_INTERFACE_BIND_NI_NAME);
    const char *if_name = (const char *)cps_api_object_get_data(obj,
                                                                IF_INTERFACES_INTERFACE_NAME);
    if ((vrf_name == nullptr) || (if_name == nullptr)) {
        NAS_VRF_LOG_ERR("INTF-VRF-RPC","VRF or Interface not present");
        return cps_api_ret_code_ERR;
    }

    if (strlen(vrf_name) > NAS_VRF_NAME_SZ) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "Error - VRF name:%s is more than the allowed length:%d if-name:%s",
                        vrf_name, NAS_VRF_NAME_SZ, if_name);
        return cps_api_ret_code_ERR;
    }

    if (strncmp(vrf_name, NAS_MGMT_VRF_NAME, sizeof(NAS_MGMT_VRF_NAME)) != 0) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS","Invalid object usage for non-mgmt VRF configuration!");
        return cps_api_ret_code_ERR;
    }
    if (op == cps_api_oper_CREATE) {
        NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "In VRF Intf CREATE ");
        if(nas_os_bind_if_name_to_mgmt_vrf(obj) != STD_ERR_OK){
            NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "OS VRF add failed");
            rc = cps_api_ret_code_ERR;
        }
        NAS_VRF_LOG_INFO("NAS-RT-CPS-SET", "OS Intf:%s associated with VRF successful", if_name);
    } else if (op == cps_api_oper_DELETE) {
        NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "In VRF intf del ");
        if(nas_os_unbind_if_name_from_mgmt_vrf(obj) != STD_ERR_OK){
            NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "OS VRF del failed");
            rc = cps_api_ret_code_ERR;
        }
        NAS_VRF_LOG_INFO("NAS-RT-CPS-SET", "OS Intf:%s disassociated with VRF successful", if_name);
    }

    if (rc == cps_api_ret_code_OK) {
        cps_api_key_set(cps_api_object_key(obj),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_OBSERVED);
        if (nas_vrf_publish_event(obj) != cps_api_ret_code_OK) {
            NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "VRF interface publish failed!");
        }
        cps_api_key_set(cps_api_object_key(obj),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_TARGET);
    }

    return rc;
}

static cps_api_return_code_t nas_vrf_publish_intf_bind(cps_api_object_t vrf_intf_obj, const char *vrf_name,
                                                       const char *if_name, uint32_t op) {
    cps_api_object_t obj = cps_api_object_create();
    if(obj == NULL){
        NAS_VRF_LOG_ERR("VRF-INTF-PUB", "Failed to allocate memory to cps object");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_guard obj_g (obj);
    cps_api_key_t key;
    cps_api_key_from_attr_with_qual(&key, NI_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_OBSERVED);
    cps_api_operation_types_t oper = cps_api_oper_CREATE;
    if (op == BASE_CMN_OPERATION_TYPE_DELETE) {
        oper = cps_api_oper_DELETE;
    }
    cps_api_object_set_type_operation(&key,oper);
    cps_api_object_set_key(obj,&key);

    cps_api_object_attr_add(obj,NI_IF_INTERFACES_INTERFACE_BIND_NI_NAME, vrf_name, strlen(vrf_name)+1);
    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_NAME, if_name, strlen(if_name)+1);

    if (op != BASE_CMN_OPERATION_TYPE_DELETE) {
        cps_api_object_attr_t if_index_attr = cps_api_object_attr_get(vrf_intf_obj, VRF_MGMT_INTF_BIND_NI_OUTPUT_IFINDEX);
        const char *rt_if_name = (const char *)cps_api_object_get_data(vrf_intf_obj,
                                                                       VRF_MGMT_INTF_BIND_NI_OUTPUT_IFNAME);
        const char *rt_mac_addr = (const char *)cps_api_object_get_data(vrf_intf_obj,
                                                                        VRF_MGMT_INTF_BIND_NI_OUTPUT_MAC_ADDR);
        if ((if_index_attr == nullptr) || (rt_if_name == nullptr) || (rt_mac_addr == nullptr)) {
            NAS_VRF_LOG_ERR("VRF-INTF-PUB", "Router interface information not present, VRF publish failed!");
            return cps_api_ret_code_ERR;
        }
        cps_api_object_attr_add(obj, VRF_MGMT_NI_IF_INTERFACES_INTERFACE_IFNAME, (const void *)rt_if_name,
                                strlen(rt_if_name)+1);

        cps_api_object_attr_add_u32(obj,VRF_MGMT_NI_IF_INTERFACES_INTERFACE_IFINDEX,
                                    cps_api_object_attr_data_u32(if_index_attr));
        cps_api_object_attr_add(obj, VRF_MGMT_NI_IF_INTERFACES_INTERFACE_MAC_ADDR, (const void *)rt_mac_addr,
                                strlen(rt_mac_addr)+1);
    }
    if (nas_vrf_publish_event(obj) != cps_api_ret_code_OK) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "VRF publish failed!");
        return cps_api_ret_code_ERR;
    }
    return cps_api_ret_code_OK;
}

cps_api_return_code_t nas_intf_bind_vrf_rpc_handler (void *context, cps_api_transaction_params_t * param, size_t ix) {
    interface_ctrl_t parent_intf_ctrl;
    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);

    if (obj == NULL) {
        NAS_VRF_LOG_ERR("INTF-VRF-RPC","Route VRF Intf object is not present");
        return cps_api_ret_code_ERR;
    }

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if (op != cps_api_oper_ACTION) {
        NAS_VRF_LOG_ERR("INTF-VRF-RPC","Invalid action for Route VRF Intf RPC");
        return cps_api_ret_code_ERR;
    }
    const char *vrf_name = (const char *)cps_api_object_get_data(obj,
                                                                 VRF_MGMT_INTF_BIND_NI_INPUT_NI_NAME);
    const char *if_name = (const char *)cps_api_object_get_data(obj,
                                                                VRF_MGMT_INTF_BIND_NI_INPUT_INTERFACE);
    if ((vrf_name == nullptr) || (if_name == nullptr)) {
        NAS_VRF_LOG_ERR("INTF-VRF-RPC","VRF or Interface not present");
        return cps_api_ret_code_ERR;
    }
    if (strlen(vrf_name) > NAS_VRF_NAME_SZ) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "Error - VRF name:%s is more than the allowed length:%d if-name:%s",
                        vrf_name, NAS_VRF_NAME_SZ, if_name);
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_t op_attr = cps_api_object_attr_get(obj, VRF_MGMT_INTF_BIND_NI_INPUT_OPERATION);
    if (op_attr == nullptr) {
        NAS_VRF_LOG_ERR("INTF-VRF-RPC","Operation is not present in the VRF%s Intf:%s RPC", vrf_name, if_name);
        return cps_api_ret_code_ERR;
    }
    uint32_t oper = cps_api_object_attr_data_u32(op_attr);

    if (oper == BASE_CMN_OPERATION_TYPE_DELETE) {
        if (strncmp(vrf_name, NAS_DEFAULT_VRF_NAME, sizeof(NAS_DEFAULT_VRF_NAME)) == 0) {
            /* Dont remove the port from default VRF since it's used for L2 operations as well,
             * return sucess */
            NAS_VRF_LOG_INFO("INTF-VRF-RPC", "Default VRF:%s Intf:%s disassociated!", vrf_name, if_name);
            nas_vrf_publish_intf_bind(obj, vrf_name, if_name, oper);
            return cps_api_ret_code_OK;
        }
        NAS_VRF_LOG_DEBUG("INTF-VRF-RPC", "In VRF intf del ");
        memset(&parent_intf_ctrl, 0, sizeof(parent_intf_ctrl));
        parent_intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF_NAME;
        safestrncpy(parent_intf_ctrl.if_name, if_name, HAL_IF_NAME_SZ);

        if (dn_hal_get_interface_info(&parent_intf_ctrl) != STD_ERR_OK) {
            NAS_VRF_LOG_INFO("INTF-VRF-RPC", "VRF:%s Intf:%s does not exist!", vrf_name, if_name);
            nas_vrf_publish_intf_bind(obj, vrf_name, if_name, oper);
            return cps_api_ret_code_OK;
        }

        char if_ietf_type[256];
        memset(if_ietf_type, 0, sizeof(if_ietf_type));
        if (!nas_to_ietf_if_type_get(parent_intf_ctrl.int_type, if_ietf_type, sizeof(if_ietf_type))) {
            NAS_VRF_LOG_ERR("INTF-VRF-RPC", "Failed to get IETF interface:%s type for type id %d VRF:%s",
                            if_name, parent_intf_ctrl.int_type, vrf_name);
            return cps_api_ret_code_ERR;
        }
        cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_TYPE, (const void *)if_ietf_type,
                strlen(if_ietf_type) + 1);


        if (parent_intf_ctrl.l3_intf_info.if_index == 0) {
            NAS_VRF_LOG_ERR("INTF-VRF-RPC", "Router VRF:%s intf not present on Intf:%s ",
                            vrf_name, if_name);
            return cps_api_ret_code_ERR;
        }
        interface_ctrl_t intf_ctrl;
        memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));
        intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
        intf_ctrl.vrf_id = parent_intf_ctrl.l3_intf_info.vrf_id;
        intf_ctrl.if_index = parent_intf_ctrl.l3_intf_info.if_index;

        if ((dn_hal_get_interface_info(&intf_ctrl)) != STD_ERR_OK) {
            NAS_VRF_LOG_ERR("INTF-VRF-RPC", "VRF:%d Intf:%d does not exist!",
                            intf_ctrl.vrf_id, intf_ctrl.if_index);
            return cps_api_ret_code_ERR;
        }

        /* Move the mode of the router interface (e.g.v-e101-001-0/v-bo1/v-br10) which is in
         * non-default context to L2 (flush the route/nbr associated with this interface in NPU)
         * so as to use router interface (e.g e101-001-0/bo1/br10 - move to L3 mode)
         * in default context for L3 operations. */
        nas_intf_handle_intf_mode_change(intf_ctrl.if_name, BASE_IF_MODE_MODE_L2);

        if (nas_intf_l3mc_intf_delete (intf_ctrl.if_name, BASE_IF_MODE_MODE_L3) == false) {
            NAS_VRF_LOG_ERR("INTF-VRF-RPC",
                    "Update to NAS-L3-MCAST interface delete failed. VRF:%s, intf:%s",
                    vrf_name, intf_ctrl.if_name);
        }

        if(nas_os_unbind_if_name_from_vrf(obj) != STD_ERR_OK){
            NAS_VRF_LOG_DEBUG("INTF-VRF-RPC", "OS VRF del failed");
            return cps_api_ret_code_ERR;
        }
        nas_intf_handle_intf_mode_change(if_name, BASE_IF_MODE_MODE_L3);

        NAS_VRF_LOG_INFO("INTF-VRF-RPC", "OS VRF:%s Parent Intf:%s rt-intf VRF-id:%d intf:%s(%d)"
                         " disassociated with VRF successful", vrf_name,
                         if_name, intf_ctrl.vrf_id, intf_ctrl.if_name, intf_ctrl.if_index);
    } else {
        memset(&parent_intf_ctrl, 0, sizeof(parent_intf_ctrl));
        parent_intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF_NAME;
        safestrncpy(parent_intf_ctrl.if_name, if_name, HAL_IF_NAME_SZ);

        if (dn_hal_get_interface_info(&parent_intf_ctrl) != STD_ERR_OK) {
            NAS_VRF_LOG_ERR("INTF-VRF-RPC", "VRF:%s Intf:%s does not exist!", vrf_name, if_name);
            return cps_api_ret_code_ERR;
        }

        if ((parent_intf_ctrl.l3_intf_info.if_index != 0) && (parent_intf_ctrl.l3_intf_info.vrf_id)) {
            NAS_VRF_LOG_ERR("INTF-VRF-RPC", "Router intf:%s (router intf:%d) is already associated with VRF-id:%d ",
                            if_name, parent_intf_ctrl.l3_intf_info.if_index,
                            parent_intf_ctrl.l3_intf_info.vrf_id);
            return cps_api_ret_code_ERR;
        }


        char if_ietf_type[256];
        memset(if_ietf_type, 0, sizeof(if_ietf_type));
        if (!nas_to_ietf_if_type_get(parent_intf_ctrl.int_type, if_ietf_type, sizeof(if_ietf_type))) {
            NAS_VRF_LOG_ERR("INTF-VRF-RPC", "Failed to get IETF interface:%s type for type id %d VRF:%s",
                            if_name, parent_intf_ctrl.int_type, vrf_name);
            return cps_api_ret_code_ERR;
        }
        cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_TYPE, (const void *)if_ietf_type,
                strlen(if_ietf_type) + 1);

        if(nas_os_bind_if_name_to_vrf(obj) != STD_ERR_OK){
            NAS_VRF_LOG_DEBUG("INTF-VRF-RPC", "OS VRF intf add failed for VRF:%s intf:%s", vrf_name, if_name);
            return cps_api_ret_code_ERR;
        }
        if (strncmp(vrf_name, NAS_DEFAULT_VRF_NAME, sizeof(NAS_DEFAULT_VRF_NAME)) == 0) {
            /* Dont remove the port from default VRF since it's used for L2 operations as well,
             * return sucess */
            NAS_VRF_LOG_INFO("INTF-VRF-RPC", "Default VRF:%s Intf:%s associated!", vrf_name, if_name);
            nas_vrf_publish_intf_bind(obj, vrf_name, if_name, oper);
            return cps_api_ret_code_OK;
        }

        /* Move the mode of the L3 interface (e.g.e101-001-0/bo1/br10) which is in default context
         * to L2 (flush the route/nbr associated with this interface in NPU) so as to use router
         * interface (e.g v-e101-001-0/v-bo1/v-br10 - default mode is L3,
         * no explicit mode change required from VRF module to NAS-L3) in non-default context for L3 operations. */
        nas_intf_handle_intf_mode_change(if_name, BASE_IF_MODE_MODE_L2);

        if (nas_intf_l3mc_intf_mode_change(if_name, BASE_IF_MODE_MODE_L2) == false) {
            NAS_VRF_LOG_ERR("INTF-VRF-RPC",
                    "Update to NAS-L3-MCAST interface(%s) mode change to L2 failed.",
                    if_name);
        }

        NAS_VRF_LOG_INFO("INTF-VRF-RPC", "OS VRF:%s Intf:%s associated with VRF successful", vrf_name, if_name);
    }
    nas_vrf_publish_intf_bind(obj, vrf_name, if_name, oper);
    return cps_api_ret_code_OK;
}

