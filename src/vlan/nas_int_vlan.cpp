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
 * filename: nas_int_vlan.c
 */


#include "nas_ndi_vlan.h"
#include "nas_int_bridge.h"
#include "nas_int_vlan.h"
#include "event_log.h"
#include "event_log_types.h"
#include "nas_int_utils.h"
#include "nas_ndi_port.h"
#include "dell-base-if-vlan.h"
#include "dell-base-if.h"
#include "dell-interface.h"
#include "iana-if-type.h"
#include "nas_ndi_switch.h"
#include "nas_os_interface.h"
#include "cps_api_object_key.h"
#include "cps_api_events.h"
#include "cps_class_map.h"
#include "nas_switch.h"
#include "std_mac_utils.h"
#include "hal_interface_common.h"
#include "iana-if-type.h"
#include "nas_if_utils.h"
#include "nas_int_base_if.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


const static int MAX_CPS_MSG_BUFF=10000;

static t_std_error nas_check_intf_vlan_membership(nas_bridge_t *p_bridge_node, hal_ifindex_t ifindex,
                                      nas_port_mode_t port_mode, nas_int_type_t intf_type,
                                      bool *is_member)
{
    nas_list_t *members = NULL;
    if (p_bridge_node == NULL) {
        return STD_ERR(INTERFACE, PARAM, 0);
    }
    if ((intf_type == nas_int_type_PORT) || (intf_type == nas_int_type_FC)) {
        if(port_mode == NAS_PORT_TAGGED) {
            members = &p_bridge_node->tagged_list;
        } else {
            members = &p_bridge_node->untagged_list;
        }
    } else if (intf_type == nas_int_type_LAG) {
       if(port_mode == NAS_PORT_TAGGED) {
            members = &p_bridge_node->tagged_lag;
       } else {
            members = &p_bridge_node->untagged_lag;
       }
    } else {
        EV_LOGGING(INTERFACE, INFO, "NAS-Vlan", "wrong type of the interface %d bridge %d  type %d ", ifindex,
                                    p_bridge_node->ifindex, intf_type);
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    nas_list_node_t *p_link_node = nas_get_link_node(&members->port_list, ifindex);
    if (p_link_node == NULL) {
        *is_member = false;
    } else {
        *is_member = true;
    }
    return STD_ERR_OK;
}


cps_api_return_code_t nas_publish_vlan_port_list(nas_bridge_t *p_bridge_node, nas_port_list_t &port_list,
                                                 nas_port_mode_t port_mode, cps_api_operation_types_t op)
{
    char buff[MAX_CPS_MSG_BUFF];
    memset(buff,0,sizeof(buff));
    int id = 0;
    cps_api_object_t obj_pub = cps_api_object_init(buff, sizeof(buff));

    EV_LOGGING(INTERFACE, INFO, "NAS-VLAN","VLAN publish event oper %d for VLAN %d",
           op, p_bridge_node->vlan_id);

    if(port_mode == NAS_PORT_TAGGED) {
        id = DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS;
    }
    else {
        id = DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS;
    }

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj_pub), id,
                                    cps_api_qualifier_OBSERVED);

    cps_api_set_key_data(obj_pub,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, cps_api_object_ATTR_T_U32,
                         &p_bridge_node->ifindex, sizeof(p_bridge_node->ifindex));

    cps_api_object_set_type_operation(cps_api_object_key(obj_pub),op);

    for ( auto it = port_list.begin(); it != port_list.end();++it) {
        cps_api_object_attr_add_u32(obj_pub, id, *it);
    }

    cps_api_object_attr_add_u32(obj_pub, BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, p_bridge_node->vlan_id);

    if (cps_api_event_thread_publish(obj_pub)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR ,"NAS-VLAN","Failed to send VLAN publish event.  Service issue");
        return cps_api_ret_code_ERR;
    }
    return cps_api_ret_code_OK;
}

t_std_error nas_vlan_create(npu_id_t npu_id, hal_vlan_id_t vlan_id)
{
    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                "Creating VLAN %d in NPU %d", vlan_id, npu_id);

    return (ndi_create_vlan(npu_id, vlan_id));
}

void nas_handle_bridge_mac(nas_bridge_t *b_node)
{
    char buff[MAX_CPS_MSG_BUFF];
    char  mac_str[MAC_STRING_SZ];

    memset(buff,0,MAX_CPS_MSG_BUFF);
    memset(mac_str,0,MAC_STRING_SZ);
    if (nas_if_get_assigned_mac(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
                                b_node->name, b_node->vlan_id, mac_str, sizeof(mac_str))
            != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR , "NAS-Vlan",
                 "Error get base mac");
        return;
    }
    cps_api_object_t name_obj = cps_api_object_init(buff, sizeof(buff));
    cps_api_set_key_data(name_obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,cps_api_object_ATTR_T_U32,
            &b_node->ifindex,sizeof(b_node->ifindex));
    cps_api_object_attr_add(name_obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS,
            mac_str ,strlen(mac_str)+1);
    nas_cps_set_vlan_mac(name_obj, b_node);
}

static std::pair<bool, bool> nas_get_intf_packet_drop(hal_ifindex_t ifindex)
{
    int untagged_cnt = 0, tagged_cnt = 0;
    auto master_cb = [&untagged_cnt, &tagged_cnt](if_master_info_t m_info) {
        if (m_info.type != nas_int_type_VLAN) {
            return;
        }
        if (m_info.mode == NAS_PORT_UNTAGGED || m_info.mode == NAS_PORT_HYBRID) {
            untagged_cnt ++;
        }
        if (m_info.mode == NAS_PORT_TAGGED || m_info.mode == NAS_PORT_HYBRID) {
            tagged_cnt ++;
        }
    };
    nas_intf_master_callback(ifindex, master_cb);
    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan", "Interface with ifindex %d is untagged member of %d bridges and \
tagged member of %d bridges",
               ifindex, untagged_cnt, tagged_cnt);
    bool drop_untag = true, drop_tag = true;
    if (untagged_cnt > 0) {
        drop_untag = false;
    }
    if (tagged_cnt > 0) {
        drop_tag = false;
    }
    if (untagged_cnt == 0 && tagged_cnt == 0) {
        drop_untag = drop_tag = false;
    }
    return std::make_pair(drop_untag, drop_tag);
}

t_std_error nas_add_or_del_port_to_vlan(npu_id_t npu_id, hal_vlan_id_t vlan_id,
                                        ndi_port_t *p_ndi_port, nas_port_mode_t port_mode,
                                        bool add_port, hal_ifindex_t ifindex)
{
    ndi_port_list_t ndi_port_list;
    ndi_port_list_t *untag_list = NULL;
    ndi_port_list_t *tag_list = NULL;

    ndi_port_list.port_count = 1;
    ndi_port_list.port_list =p_ndi_port;
    t_std_error rc = STD_ERR_OK;

    if (port_mode == NAS_PORT_TAGGED) {
        tag_list = &ndi_port_list;
    }
    else {
        untag_list = &ndi_port_list;
    }

    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                "Updating VLAN member: %s %s port <%d %d> %s VLAN %d", (add_port ? "add" : "delete"),
                p_ndi_port->npu_id, p_ndi_port->npu_port, (add_port ? "from" : "to"),
                (port_mode == NAS_PORT_TAGGED ? "tagged" : "untagged"),
                vlan_id);

    if (ndi_add_or_del_ports_to_vlan(npu_id, vlan_id, tag_list, untag_list, add_port)
            != STD_ERR_OK) {
        return (STD_ERR(INTERFACE,FAIL, 0));
    }

    if (port_mode == NAS_PORT_UNTAGGED) {
        if (add_port) {
            if ((rc = ndi_set_port_vid(npu_id, p_ndi_port->npu_port, vlan_id)) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE, ERR, "NAS-Port",
                        "Error setting untagged port <%d %d> VID",
                        npu_id, p_ndi_port->npu_port);
            }
        }
    }

    auto pkt_drop = nas_get_intf_packet_drop(ifindex);
    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
               "Updating packet drop for port <%d %d>: untagged - %s; tagged - %s",
               p_ndi_port->npu_id, p_ndi_port->npu_port,
               pkt_drop.first ? "drop" : "not drop",
               pkt_drop.second ? "drop" : "not drop");
    if ((rc = ndi_port_set_packet_drop(npu_id, p_ndi_port->npu_port,
                                       NDI_PORT_DROP_UNTAGGED, pkt_drop.first)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Port",
                   "Error setting untagged drop for port <%d %d>",
                   npu_id, p_ndi_port->npu_port);
    }
    if ((rc = ndi_port_set_packet_drop(npu_id, p_ndi_port->npu_port,
                                       NDI_PORT_DROP_TAGGED, pkt_drop.second)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Port",
                   "Error setting tagged drop for port <%d %d>",
                   npu_id, p_ndi_port->npu_port);
    }

    return STD_ERR_OK;

}

t_std_error nas_vlan_delete(npu_id_t npu_id, hal_vlan_id_t vlan_id)
{
    return (ndi_delete_vlan(npu_id, vlan_id));
}

static t_std_error nas_add_port_list_to_vlan(npu_id_t npu_id, hal_vlan_id_t vlan_id,
                                const std::vector<std::pair<ndi_port_t, hal_ifindex_t>>& port_list,
                                bool tagged_port_list)
{
    t_std_error rc = STD_ERR_OK;
    std::vector<ndi_port_t> tmp_port_list{};
    for (auto& port_info: port_list) {
        tmp_port_list.push_back(port_info.first);
    }
    ndi_port_list_t ndi_port_list{tmp_port_list.size(), tmp_port_list.data()};
    if (tagged_port_list) {
        if ((rc = ndi_add_or_del_ports_to_vlan(npu_id, vlan_id,  &ndi_port_list, NULL, true))
                != STD_ERR_OK)
            return rc;
    }
    else {
        if ((rc = ndi_add_or_del_ports_to_vlan(npu_id, vlan_id, NULL, &ndi_port_list, true))
             != STD_ERR_OK)
            return rc;
    }
    for (auto& port_info: port_list) {
        npu_port_t port_id = port_info.first.npu_port;
        auto pkt_drop = nas_get_intf_packet_drop(port_info.second);
        EV_LOGGING(INTERFACE, INFO, "NAS-Port",
                   "Updating packet drop for port <%d %d>: untagged - %s; tagged - %s",
                   npu_id, port_id,
                   pkt_drop.first ? "drop" : "not drop",
                   pkt_drop.second ? "drop" : "not drop");
        rc = ndi_port_set_packet_drop(npu_id, port_id, NDI_PORT_DROP_UNTAGGED, pkt_drop.first);
        if (rc != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, INFO, "NAS-Port", "Failed to disable untagged drop for port <%d %d>",
                       npu_id, port_id);
        }
        rc = ndi_port_set_packet_drop(npu_id, port_id, NDI_PORT_DROP_TAGGED, pkt_drop.second);
        if (rc != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, INFO, "NAS-Port", "Failed to disable untagged drop for port <%d %d>",
                       npu_id, port_id);
        }
    }
    return STD_ERR_OK;
}

static t_std_error nas_vlan_get_intf_ctrl_info(hal_ifindex_t index, interface_ctrl_t &i){
    t_std_error rc;
    memset(&i,0,sizeof(i));
    i.if_index = index;
    i.q_type = HAL_INTF_INFO_FROM_IF;
    if ((rc = dn_hal_get_interface_info(&i)) != STD_ERR_OK){
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                   "Failed to get interface control information for "
                   "interface %d", index);
        return rc;
    }
    return STD_ERR_OK;

}

//@TODO change this to the vector..
static void nas_copy_bridge_ports_to_ndi_port_list(std_dll_head *p_port_list,
                                    std::vector<std::pair<ndi_port_t, hal_ifindex_t>>& ndi_ports)
{
    nas_list_node_t *p_link_iter_node = NULL, *p_temp_node = NULL;

    p_link_iter_node = nas_get_first_link_node(p_port_list);

    while (p_link_iter_node != NULL)
    {
        EV_LOGGING(INTERFACE, DEBUG, "NAS-Vlan",
                    "Copying untagged port %d",
                     p_link_iter_node->ndi_port.npu_port);
        ndi_ports.push_back(std::make_pair(p_link_iter_node->ndi_port, p_link_iter_node->ifindex));

        p_temp_node = p_link_iter_node;
        p_link_iter_node = nas_get_next_link_node(p_port_list, p_temp_node);
    }
}


static t_std_error nas_add_all_ut_ports_to_vlan(nas_bridge_t *p_bridge_node)
{
    size_t port_count = p_bridge_node->untagged_list.port_count;
    int npu_id = 0;
    nas_list_node_t *p_iter_node = NULL;

    t_std_error err = STD_ERR_OK;

    if (port_count != 0) {
        do {
            std::vector<std::pair<ndi_port_t, hal_ifindex_t>> port_list{};
            nas_copy_bridge_ports_to_ndi_port_list(&p_bridge_node->untagged_list.port_list,
                                                   port_list);

            /* @todo : NPU_ID for bridge */
            if (nas_add_port_list_to_vlan(npu_id, p_bridge_node->vlan_id,
                                      port_list,
                                      false) != STD_ERR_OK) {
                err = (STD_ERR(INTERFACE,FAIL, 0));
                break;
            }

            p_iter_node = nas_get_first_link_node(&p_bridge_node->untagged_list.port_list);

            while (p_iter_node != NULL)
            {
                ndi_set_port_vid(p_iter_node->ndi_port.npu_id,
                                 p_iter_node->ndi_port.npu_port, p_bridge_node->vlan_id);
                p_iter_node = nas_get_next_link_node(&p_bridge_node->untagged_list.port_list,
                                                     p_iter_node);
            }

        } while(0);
    }
    return err;
}

static t_std_error nas_add_all_ut_lags_to_vlan(nas_bridge_t *p_bridge_node)
{
    nas_list_node_t *p_iter_node = NULL;
    t_std_error rc = STD_ERR_OK;

    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                "Checking untagged bond list in bridge %d", p_bridge_node->ifindex);

    p_iter_node = nas_get_first_link_node(&p_bridge_node->untagged_lag.port_list);

    while (p_iter_node != NULL)
    {
        EV_LOGGING(INTERFACE, DEBUG, "NAS-Vlan",
                    "Found untagged bond %d in bridge %d", p_iter_node->ifindex,
                    p_bridge_node->ifindex);

        if((rc = nas_handle_lag_add_to_vlan(p_bridge_node, p_iter_node->ifindex,
                                 NAS_PORT_UNTAGGED, false, nullptr)) != STD_ERR_OK) {
            rc = STD_ERR(INTERFACE,FAIL, rc);
        }
        p_iter_node = nas_get_next_link_node(&p_bridge_node->untagged_lag.port_list,
                                             p_iter_node);
    }
    return rc;
}

t_std_error nas_process_lag_for_vlan_del(nas_list_t *p_list,
                                              hal_ifindex_t if_index)
{
    nas_list_node_t *p_link_node = NULL;

    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                "Get lag interface %d ",
                 if_index);

    p_link_node = nas_get_link_node(&p_list->port_list, if_index);
    if (p_link_node) {
        EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                    "Found lag interface %d for deletion",
                     if_index);

        nas_delete_link_node(&p_list->port_list, p_link_node);
        p_list->port_count--;
    }
    return STD_ERR_OK;
}
t_std_error nas_process_list_for_vlan_del(nas_bridge_t *p_bridge,
                                          nas_list_t *p_list,
                                          hal_ifindex_t if_index,
                                          hal_vlan_id_t vlan_id,
                                          nas_port_mode_t port_mode)
{
    nas_list_node_t *p_link_node = NULL;
    t_std_error rc = STD_ERR_OK;

    p_link_node = nas_get_link_node(&p_list->port_list, if_index);
    if (p_link_node) {
        EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                    "Found vlan Interface %d maps to slot %d, port %d",
                     if_index, p_link_node->ndi_port.npu_id,
                     p_link_node->ndi_port.npu_port);

        if (nas_add_or_del_port_to_vlan(p_link_node->ndi_port.npu_id, vlan_id,
                                        &(p_link_node->ndi_port), port_mode, false, if_index)
                != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                        "Error deleting port %d from vlan %d",
                         if_index, vlan_id);
            return (STD_ERR(INTERFACE,FAIL, 0));
        }

        nas_delete_link_node(&p_list->port_list, p_link_node);
        p_list->port_count--;
    }
    return rc;
}

/*
 *  Following actions are taken here
 *  1. check if port is present in the bridge memeber list
 *  2. delete from NPU
 *  3. update local DB
 *  4. publish deletion of the member
 **/
void nas_process_del_vlan_mem_from_os (hal_ifindex_t bridge_id, nas_port_list_t &port_list,
                                             nas_port_mode_t port_mode)
{
    nas_bridge_t *p_bridge_node = NULL;
    nas_int_type_t intf_type;
    nas_list_t *p_list = NULL;
    hal_ifindex_t if_index = 0;
    nas_port_list_t publish_list;


    if (port_list.empty()) {
        return;
    }
    nas_bridge_lock();
    if ((p_bridge_node = nas_get_bridge_node(bridge_id)) == NULL) {
        EV_LOGGING(INTERFACE, ERR , "NAS-Vlan",
                "Error finding bridge Interface %d\n", bridge_id);
        nas_bridge_unlock();
        return;
    }
    for (auto it=port_list.begin();it != port_list.end(); ++it) {

        if_index = *it;
        p_list = (port_mode == NAS_PORT_UNTAGGED) ?
                &p_bridge_node->untagged_list : &p_bridge_node->tagged_list;

        if (nas_get_int_type(if_index, &intf_type) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                       "Error finding index %d in intf_ctrl", if_index);
            break;
        }

        if_master_info_t master_info = { nas_int_type_VLAN, port_mode, p_bridge_node->ifindex};
        BASE_IF_MODE_t intf_mode = nas_intf_get_mode(if_index);

        if(!nas_intf_del_master(if_index, master_info)){
            EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN","Failed to del master for vlan memeber port");
        } else {
            BASE_IF_MODE_t new_mode = nas_intf_get_mode(if_index);
            if (new_mode != intf_mode) {
                if (nas_intf_handle_intf_mode_change(if_index, new_mode) == false) {
                    EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN",
                            "Update to NAS-L3 about interface mode change failed(%d)", if_index);
                }
            }
        }
        if (!nas_is_virtual_port(if_index) && (intf_type == nas_int_type_PORT)) {
            EV_LOGGING(INTERFACE, INFO ,"NAS-Vlan",
                        "Delete Port %d from bridge %d ", if_index, bridge_id);

            if(!nas_intf_cleanup_l2mc_config(if_index,  p_bridge_node->vlan_id)) {
                EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                   "Error cleaning L2MC membership for interface %d", if_index);
            }
            //check the untagged list first
            if (nas_process_list_for_vlan_del(p_bridge_node, p_list,
                                             if_index, p_bridge_node->vlan_id,
                                             port_mode) != STD_ERR_OK) {
                break;
            }

            publish_list.insert(if_index); // TODO combine with LAG members
        } else if (intf_type == nas_int_type_LAG) {

            if(!nas_intf_cleanup_l2mc_config(if_index,  p_bridge_node->vlan_id)) {
                EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                   "Error cleaning L2MC membership for interface %d", if_index);
            }

            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                "Delete %s LAG %d from Bridge %d ", (port_mode == NAS_PORT_UNTAGGED) ? "untagged" : "tagged",
                if_index, bridge_id);

            nas_add_or_del_lag_in_vlan(if_index, p_bridge_node->vlan_id, port_mode, false, false);
            if (port_mode == NAS_PORT_TAGGED) {
                nas_process_lag_for_vlan_del(&p_bridge_node->tagged_lag, if_index);
            } else {
                nas_process_lag_for_vlan_del(&p_bridge_node->untagged_lag, if_index);
            }

        } else {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                    "Unknown interface type or not supported: if_index %d type %d ", if_index, intf_type);
            break;
        }
    }

    /*  publish the member update */
    nas_publish_vlan_port_list(p_bridge_node, publish_list, port_mode, cps_api_oper_DELETE);
    nas_bridge_unlock();
    return;
}

nas_list_node_t *nas_create_vlan_port_node(nas_bridge_t *p_bridge_node,
                                      hal_ifindex_t ifindex,
                                      nas_port_mode_t port_mode,
                                      bool *create_flag) {
    nas_list_node_t *p_link_node = NULL;
    ndi_port_t ndi_port;
    t_std_error rc = STD_ERR_OK;

    memset(&ndi_port, 0, sizeof(ndi_port_t));
    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                "Insert member %d mode %d in bridge %d",
                ifindex, port_mode, p_bridge_node->ifindex);
    if( port_mode == NAS_PORT_TAGGED) {
        p_link_node = nas_get_link_node(&p_bridge_node->tagged_list.port_list, ifindex);
    } else {
        p_link_node = nas_get_link_node(&p_bridge_node->untagged_list.port_list, ifindex);
    }

    if (p_link_node == NULL) {
        *create_flag = true;
        if((rc = (nas_int_get_npu_port(ifindex, &ndi_port))) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                       "Interface %d returned error %d ",
                        ifindex, rc);
            /* Could be a management interface: proceed ahead */
        }

        p_link_node = (nas_list_node_t *)malloc(sizeof(nas_list_node_t));
        if (p_link_node != NULL) {
            memset(p_link_node, 0, sizeof(nas_list_node_t));
            p_link_node->ifindex = ifindex;
            p_link_node->ndi_port.npu_port = ndi_port.npu_port;
            p_link_node->ndi_port.npu_id = ndi_port.npu_id;

            if (port_mode == NAS_PORT_TAGGED) {
                p_bridge_node->tagged_list.port_count++;
                nas_insert_link_node(&p_bridge_node->tagged_list.port_list, p_link_node);
            }
            else {
                nas_insert_link_node(&p_bridge_node->untagged_list.port_list, p_link_node);
                p_bridge_node->untagged_list.port_count++;
            }
            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                       "Success adding vlan ifindex %d in bridge %d",
                        ifindex, p_bridge_node->ifindex);
        }
        else
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                       "Vlan member insertion %d failure", ifindex);
    }
    else {
        *create_flag = false;
    }
    return p_link_node;
}

t_std_error nas_create_untagged_lag_node(nas_bridge_t *p_bridge_node,
                                         hal_ifindex_t ifindex)
{
    nas_list_node_t *p_link_node = NULL;

    p_link_node = (nas_list_node_t *)malloc(sizeof(nas_list_node_t));
    if (p_link_node != NULL) {
        memset(p_link_node, 0, sizeof(nas_list_node_t));
        p_link_node->ifindex = ifindex;

        p_bridge_node->untagged_lag.port_count++;
        nas_insert_link_node(&p_bridge_node->untagged_lag.port_list, p_link_node);
    }
    else {
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return STD_ERR_OK;
}

static t_std_error nas_handle_create_vlan(nas_bridge_t *p_bridge_node, hal_vlan_id_t vlan_id)
{
    t_std_error rc = (STD_ERR(INTERFACE,FAIL, 0));

    if ((p_bridge_node->vlan_id == 0) && (vlan_id != 0)) {
        if (!nas_handle_vid_to_br(vlan_id, p_bridge_node->ifindex)) {
            EV_LOGGING(INTERFACE, ERR , "NAS-Vlan",
                               "Error Same Vlan ID %d exists in another bridge", vlan_id);
            return rc;
        }
        p_bridge_node->vlan_id = vlan_id;
        //create this vlan
        if ((rc = nas_vlan_create(0, vlan_id)) != STD_ERR_OK) {
            return rc;
        }
        /* Register this VLAN interface */
        if (nas_register_vlan_intf(p_bridge_node, HAL_INTF_OP_REG) != STD_ERR_OK) {
            return rc;
        }

        nas_handle_bridge_mac(p_bridge_node);
   }
   return STD_ERR_OK;
}

t_std_error nas_process_member_addition_to_vlan(nas_bridge_t *p_bridge_node, hal_ifindex_t port_idx,
                                                nas_int_type_t intf_type, nas_port_mode_t port_mode,
                                                hal_vlan_id_t vlan_id)
{
    nas_list_node_t *p_link_node = NULL;
    t_std_error rc = STD_ERR_OK;
    bool create_flag=false;

    if (intf_type == nas_int_type_PORT) {
        p_link_node = nas_create_vlan_port_node(p_bridge_node, port_idx,
                                       port_mode, &create_flag);
        if(p_link_node == NULL) {
             rc = STD_ERR(INTERFACE,FAIL, 0);
             return rc;
        }
        if(create_flag == false) {
            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                        "Port already exists %d in Bridge %d", port_idx, p_bridge_node->ifindex);

            return rc;
        }
    } else {
        /* Storing the untagged bonds until the vlan id gets created */
        if(p_bridge_node->vlan_id == 0) {
            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                        "Creating untagged bond %d in bridge %d", port_idx, p_bridge_node->ifindex);

            if(nas_create_untagged_lag_node(p_bridge_node, port_idx) != STD_ERR_OK)
                return STD_ERR(INTERFACE, FAIL, 0);
        }
    }

    if (p_bridge_node->vlan_id != 0 ) { //handle both tagged and untagged vlan

        if(intf_type == nas_int_type_PORT) {
            //add tagged port to vlan
            if ((rc = nas_add_or_del_port_to_vlan(p_link_node->ndi_port.npu_id, p_bridge_node->vlan_id,
                                                  &(p_link_node->ndi_port), port_mode, true, port_idx))
                    != STD_ERR_OK) {
                rc = (STD_ERR(INTERFACE,FAIL, rc));
                return rc;
            }
        }
        else if(intf_type == nas_int_type_LAG){
            if((rc = nas_handle_lag_add_to_vlan(p_bridge_node, port_idx,
                                       port_mode, false, nullptr)) != STD_ERR_OK) {
                rc= (STD_ERR(INTERFACE, FAIL,rc));
            }
        }
    }
    return rc;
}

void nas_process_add_vlan_mem_from_os(hal_ifindex_t bridge_id, nas_port_list_t &port_list,
                                         hal_vlan_id_t vlan_id, nas_port_mode_t port_mode)
{
    nas_bridge_t *p_bridge_node = NULL;
    nas_int_type_t intf_type;
    nas_port_list_t publish_list;
    hal_ifindex_t if_index;

    if (port_list.empty()) {
         return;
    }
    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                "Add %s ports to vlan %d bridge ifindex %d",
                (port_mode == NAS_PORT_UNTAGGED) ? "untagged" : "tagged", vlan_id, bridge_id);
    nas_bridge_lock();
    if ((p_bridge_node = nas_get_bridge_node(bridge_id)) == NULL ) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                   "Error finding bridge %d ", bridge_id);
        nas_bridge_unlock();
        return;
    }
    if ((vlan_id != 0) && (p_bridge_node->vlan_id !=0)) {
        //check mismatch vlan case
        if (p_bridge_node->vlan_id != vlan_id ) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                      "Invalid Vlan ID %d for bridge %d ", vlan_id, bridge_id);
            nas_bridge_unlock();
            return;
        }
    }
    if (p_bridge_node->vlan_id == 0 && vlan_id != 0) {
        if (nas_handle_create_vlan(p_bridge_node, vlan_id) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR , "NAS-Vlan",
                 "VLAN creation failed for bridge %d vlan_id %d \n", p_bridge_node->ifindex, vlan_id);
            nas_bridge_unlock();
            return;

        }
        //vlan just got created, add all untagged stored ports
        if (nas_add_all_ut_ports_to_vlan(p_bridge_node) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                 "VLAN Add untagged ports failed %d vlan_id %d \n", p_bridge_node->ifindex, vlan_id);
        }
        if (nas_add_all_ut_lags_to_vlan(p_bridge_node) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                 "VLAN Add untagged bridge failed %d vlan_id %d \n", p_bridge_node->ifindex, vlan_id);
        }
        if (nas_publish_vlan_object(p_bridge_node, cps_api_oper_CREATE)!= cps_api_ret_code_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                    "Failure publishing VLAN create event");
        }
    }


    for (auto it=port_list.begin();it != port_list.end(); ++it) {
        if_index = *it;
        do {
            if(nas_get_int_type(if_index, &intf_type) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                           "Error finding index %d in intf_ctrl ", if_index);
                break;
            }
            bool is_member;
            if (nas_check_intf_vlan_membership(p_bridge_node, if_index, port_mode, intf_type, &is_member) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN", "Error finding Interface index %d membership in bridge index %d",
                        if_index, p_bridge_node->ifindex);
                break;
            }
            if (is_member) {
                EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN", "Interface idx %d is already member of bridge idx %d",
                        if_index, p_bridge_node->ifindex);
                break;
            }

            if (port_mode == NAS_PORT_TAGGED && vlan_id != 0) {
                /*Check if the tagged port exist in the os else don't add, it may have been deleted n this netlik message is stale */
                if (nas_ck_port_exist(vlan_id, if_index) !=true ) {
                    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                        "Tagged Port doesn't exists %d in os for bridge %d", if_index, p_bridge_node->ifindex);
                    break;
                }
            }
            if_master_info_t master_info = { nas_int_type_VLAN, port_mode, p_bridge_node->ifindex};
            BASE_IF_MODE_t intf_mode = nas_intf_get_mode(if_index);
            if(!nas_intf_add_master(if_index, master_info)){
                EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN","Failed to add master for vlan memeber port");
            } else {
                BASE_IF_MODE_t new_mode = nas_intf_get_mode(if_index);
                if (new_mode != intf_mode) {
                    if (nas_intf_handle_intf_mode_change(if_index, new_mode) == false) {
                        EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN", "Update to NAS-L3 about interface mode change failed(%d)", if_index);
                    }
                }
            }
            if(!nas_is_virtual_port(if_index)){
                if((nas_process_member_addition_to_vlan(p_bridge_node, if_index,
                                                    intf_type, port_mode, vlan_id))
                    != STD_ERR_OK) {
                    EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                            "Error in adding port %d in the bridge %d ", if_index, bridge_id);
                    break;
                }
            }
            publish_list.insert(if_index);
        }while(0);
    }
    nas_publish_vlan_port_list(p_bridge_node, publish_list, port_mode, cps_api_oper_CREATE);
    nas_bridge_unlock();
    return;
}


void nas_pack_vlan_port_list(cps_api_object_t obj, nas_list_t *p_list, int attr_id)
{
    nas_list_node_t *p_link_node = NULL;
    char name[HAL_IF_NAME_SZ] = "\0";

    p_link_node = nas_get_first_link_node(&p_list->port_list);

    while(p_link_node != NULL) {
        memset(name,0,sizeof(name));
        if (nas_int_get_if_index_to_name(p_link_node->ifindex, name, sizeof(name)) == STD_ERR_OK) {
            cps_api_object_attr_add(obj, attr_id, (const void *)name, strlen(name)+1);
        }
        p_link_node = nas_get_next_link_node(&p_list->port_list, p_link_node);
    }
}

void nas_pack_vlan_if(cps_api_object_t obj, nas_bridge_t *p_bridge)
{
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
            cps_api_qualifier_TARGET);

    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                         &p_bridge->name, strlen(p_bridge->name)+1);

    cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, p_bridge->ifindex);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
                            (const void *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
                            strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN)+1);

    cps_api_object_attr_add_u32(obj, BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, p_bridge->vlan_id);

    nas_pack_vlan_port_list(obj, &p_bridge->tagged_list, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS);

    nas_pack_vlan_port_list(obj, &p_bridge->tagged_lag, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS);

    nas_pack_vlan_port_list(obj, &p_bridge->untagged_list,DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS);

    nas_pack_vlan_port_list(obj, &p_bridge->untagged_lag,DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS);

    cps_api_object_attr_add(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS, p_bridge->mac_addr,
                            sizeof(p_bridge->mac_addr));

    cps_api_object_attr_add_u32(obj, IF_INTERFACES_INTERFACE_ENABLED,
            (p_bridge->admin_status == IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS_UP) ? true: false);

    bool learning_mode = p_bridge->learning_disable ? false : true;

    cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_LEARNING_MODE, learning_mode);

    cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_VLAN_TYPE,
                                 p_bridge->int_sub_type);
    /*
     * Get MTU on vlan Interface
     */
    nas_os_get_interface_mtu(p_bridge->name, obj);


    interface_ctrl_t intf_ctrl_blk;
    if(nas_vlan_get_intf_ctrl_info(p_bridge->ifindex, intf_ctrl_blk) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                "Error getting intf ctrl blk info.");
        return;
    }

    if(intf_ctrl_blk.desc) {
        cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_DESCRIPTION,
                                    (const void*)intf_ctrl_blk.desc, strlen(intf_ctrl_blk.desc) + 1);
    }
}


t_std_error nas_get_vlan_intf(const char *if_name, hal_ifindex_t ifindex, cps_api_object_list_t list)
{
    nas_bridge_t *p_bridge = NULL;

    if(if_name) EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
           "Get vlan interface %s", if_name);
    else if(ifindex) EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
           "Get vlan index %d", ifindex);


    if(if_name) p_bridge = nas_get_bridge_node_from_name(if_name);
    else p_bridge = nas_get_bridge_node(ifindex);

    if(p_bridge == NULL) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                               "Error finding bridge %s for get operation", if_name);
        return(STD_ERR(INTERFACE, FAIL, 0));
    }
    cps_api_object_t object = cps_api_object_list_create_obj_and_append(list);
    if (object == nullptr)  {
        return(STD_ERR(INTERFACE, FAIL, 0));
    }
    nas_pack_vlan_if(object, p_bridge);
    return STD_ERR_OK;
}

t_std_error nas_get_vlan_intf_from_vid(hal_vlan_id_t vid, cps_api_object_list_t list)
{
    nas_bridge_t *p_bridge = NULL;

    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
           "Get vlan interface from vid %d", vid);

    p_bridge = nas_get_bridge_node_from_vid(vid);

    if(p_bridge == NULL) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                               "Error finding bridge vlan ID %d for get operation", vid);
        return(STD_ERR(INTERFACE, FAIL, 0));
    }
    cps_api_object_t object = cps_api_object_list_create_obj_and_append(list);
    if (object == nullptr)  {
        return(STD_ERR(INTERFACE, FAIL, 0));
    }
    nas_pack_vlan_if(object, p_bridge);
    return STD_ERR_OK;
}

t_std_error nas_register_vlan_intf(nas_bridge_t *p_bridge, hal_intf_reg_op_type_t op)
{
    interface_ctrl_t details;

    EV_LOGGING(INTERFACE, INFO ,"NAS-Vlan", "Registering VLAN %d %d with ifCntrl",
           p_bridge->ifindex, p_bridge->vlan_id);

    memset(&details,0,sizeof(details));
    details.q_type = HAL_INTF_INFO_FROM_IF;
    details.if_index = p_bridge->ifindex;
    details.vlan_id = p_bridge->vlan_id;
    details.int_type = nas_int_type_VLAN;
    details.int_sub_type = p_bridge->int_sub_type;
    details.desc = NULL;
    strncpy(details.if_name, p_bridge->name, sizeof(details.if_name)-1);

    if (dn_hal_if_register(op, &details)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-Vlan", "VLAN %d %d Not registered with ifCntrl ",
               p_bridge->ifindex, p_bridge->vlan_id);
        return STD_ERR(INTERFACE,FAIL,0);
    }
    return STD_ERR_OK;
}

cps_api_return_code_t nas_publish_vlan_object(nas_bridge_t *p_bridge_node, cps_api_operation_types_t op)
{
    char buff[MAX_CPS_MSG_BUFF];
    memset(buff,0,sizeof(buff));

    cps_api_object_t obj_pub = cps_api_object_init(buff, sizeof(buff));
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj_pub), BASE_IF_VLAN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_OBSERVED);

    cps_api_set_key_data(obj_pub,IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                         &p_bridge_node->name, strlen(p_bridge_node->name)+1);

    cps_api_object_attr_add_u32(obj_pub,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, p_bridge_node->ifindex);

    cps_api_object_attr_add_u32(obj_pub, BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, p_bridge_node->vlan_id);

    cps_api_object_attr_add_u32(obj_pub, DELL_IF_IF_INTERFACES_INTERFACE_VLAN_TYPE,
            p_bridge_node->int_sub_type);

    nas_pack_vlan_port_list(obj_pub, &p_bridge_node->tagged_list, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS);
    nas_pack_vlan_port_list(obj_pub, &p_bridge_node->untagged_list, DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS);

    cps_api_object_set_type_operation(cps_api_object_key(obj_pub),op);

    if (cps_api_event_thread_publish(obj_pub)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-VLAN","Failed to send VLAN publish event.  Service issue");
        return cps_api_ret_code_ERR;
    }
    return cps_api_ret_code_OK;
}

t_std_error nas_default_vlan_cache_init(void)
{
    const nas_switches_t *switches = nas_switch_inventory();

    for (size_t ix = 0; ix < switches->number_of_switches; ++ix) {
        const nas_switch_detail_t * sd = nas_switch((nas_switch_id_t) ix);

        if (sd == NULL) {
            EV_LOGGING(INTERFACE,ERR,"NAS-VLAN","Switch Details Configuration file is erroneous");
            return STD_ERR(INTERFACE,PARAM,0);
        }

        for (size_t sd_ix = 0; sd_ix < sd->number_of_npus; ++sd_ix) {
            if(ndi_del_new_member_from_default_vlan(sd->npus[sd_ix],0,true) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE,ERR,"NAS-VLAN","Failed to remove members"
                        " from default vlan for NPU %d.",sd->npus[sd_ix]);
            }
        }
    }

    return STD_ERR_OK;
}
