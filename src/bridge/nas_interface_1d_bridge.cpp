
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
 * filename: nas_interface_1d_bridge.cpp
 */

#include "dell-base-if.h"
#include "bridge/nas_interface_bridge.h"
#include "bridge/nas_interface_1d_bridge.h"
#include "interface/nas_interface_vlan.h"
#include "interface/nas_interface_utils.h"
#include "nas_int_utils.h"
#include "nas_switch.h"
#include "nas_ndi_1d_bridge.h"
#include "nas_ndi_l2mc.h"
#include "nas_vrf_utils.h"
#include "nas_ndi_router_interface.h"
#include "std_ip_utils.h"
#include "bridge/nas_interface_bridge_com.h"


static ndi_obj_id_t g_vrf_id = 0;
static ndi_obj_id_t underlay_rif = 0;

t_std_error NAS_DOT1D_BRIDGE::nas_bridge_check_vxlan_membership(std::string mem_name, bool *present)
{
    *present = false;
    auto itr = _vxlan_members.find(mem_name);
    if(itr != _vxlan_members.end()){
        *present = true;
        return STD_ERR_OK;
    }
    return STD_ERR_OK;
}

t_std_error NAS_DOT1D_BRIDGE::nas_bridge_get_vxlan_member_list(memberlist_t &m_list)
{
    if (_vxlan_members.empty()) return STD_ERR_OK;

    for (auto it = _vxlan_members.begin(); it != _vxlan_members.end(); ++it) {
        m_list.insert(*it);
    }
    return STD_ERR_OK;
}

t_std_error NAS_DOT1D_BRIDGE::nas_bridge_l2mc_create(){

    t_std_error rc = STD_ERR_OK;

    /*  Create L2 MC groups add add flood control on bridge */
    if ((rc = ndi_l2mc_group_create(npu_id, &l2mc_group_id)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Failed to create  L2MC group for %s",
                  bridge_name.c_str());
        return rc;
    }
    if ((rc = ndi_flood_control_1d_bridge(npu_id, bridge_id, l2mc_group_id,
                                NDI_BRIDGE_PKT_ALL, true)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to set L2MC group %llu and "
                "unicast flooding on bridge %s",l2mc_group_id, bridge_name.c_str());
        return rc;
    }
    return rc;
}

t_std_error NAS_DOT1D_BRIDGE::nas_bridge_npu_create()
{
    t_std_error rc = STD_ERR_OK;
    /** Create .1d bridge in the NPU */
    if ( bridge_id == NAS_INVALID_BRIDGE_ID) {
        if ((rc =  ndi_create_bridge_1d(npu_id, &bridge_id)) != STD_ERR_OK ) { // TODO check the bridge id type
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to create 1d bridge in the NPU for %s",
                               bridge_name.c_str());
            return rc;
        }
        if ((rc = nas_bridge_intf_cntrl_block_register(HAL_INTF_OP_REG)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "DOT-1D", "Failure registering VLAN in cntrl block %s",
                             bridge_name.c_str());
            return rc;
        }
    }

    return nas_bridge_l2mc_create();

}


t_std_error NAS_DOT1D_BRIDGE::nas_bridge_l2mc_delete(){

    t_std_error rc = STD_ERR_OK;

    /* Delete L2 MC group   */
    if ((rc = ndi_flood_control_1d_bridge(npu_id, bridge_id, l2mc_group_id,
                                    NDI_BRIDGE_PKT_ALL, false)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to remove L2MC group %llu and "
                    "unicast flooding on bridge %s",l2mc_group_id, bridge_name.c_str());
        return rc;
    }

    if ((rc =  ndi_l2mc_group_delete(npu_id, l2mc_group_id)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to delete L2MC group for %s",
                       bridge_name.c_str());
        return rc;
    };

    return rc;
}


t_std_error NAS_DOT1D_BRIDGE::nas_bridge_npu_delete()
{
    t_std_error rc = STD_ERR_OK;

    if((rc = nas_bridge_l2mc_delete()) != STD_ERR_OK){
        return rc;
    }

    if (bridge_id != NAS_INVALID_BRIDGE_ID) {
        /** Delete bridge ID in the NPU (Make sure bridge exists & bridge has no members) */
        if ((rc = ndi_delete_bridge_1d(npu_id, bridge_id)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to delete DOT 1D Bridge for %s",
                               bridge_name.c_str());
            return rc;
        }
        if ((rc = nas_bridge_intf_cntrl_block_register(HAL_INTF_OP_DEREG)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "DOT-1D", "Failure de-registering in cntrl block %s",
                             bridge_name.c_str());
            return rc;
        }
        bridge_id = NAS_INVALID_BRIDGE_ID;
    }
    /** reset npu_bridge_id attribute */
    /*  TODO Update in the interface control block as well. */
    return STD_ERR_OK;
}

/* TODO: this function args needs to be reduced */
static t_std_error _nas_npu_add_remove_port_member( hal_ifindex_t ifx, ndi_port_t *port, nas_bridge_id_t bridge_id ,
                                                   ndi_obj_id_t l2mc_group_id,
                                                    hal_vlan_id_t vlan_id,
                                                   nas_port_mode_t port_mode,  bool add_member)
{

    t_std_error rc = STD_ERR_OK;
    bool tagged = (port_mode == NAS_PORT_TAGGED) ? true : false;
    bool reset_pvid =true;
    if (add_member) {
        hal_vlan_id_t port_vlan_id = 0;
        if (port_mode == NAS_PORT_UNTAGGED) {
            port_vlan_id = vlan_id ;
        } else {
            /*  set the port vlan id to current default vlan ID
             * if this port is not an untagged member of any vlan
             */
            auto untag_tag_cnt = nas_intf_untag_tag_count(ifx);
            if (untag_tag_cnt.first > 0) {
                reset_pvid = false;
                EV_LOGGING(INTERFACE, INFO, "NAS-Port",
                    "port %d is untagged member of some vlan so PVID not set to default",
                    port->npu_id);
            } else {
               nas_vlan_interface_get_default_vlan(&port_vlan_id);
            }
        }

        if (reset_pvid == true) {
            if ((rc = ndi_set_port_vid(port->npu_id, port->npu_port, port_vlan_id))
                         != STD_ERR_OK) {
                EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to set PVID of port %d in the NPU",
                        port->npu_port);
                return rc;
            }
        }
        rc = ndi_1d_bridge_member_port_add(port->npu_id, bridge_id, port->npu_port, vlan_id, tagged);
        if (rc != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to add port %d to the 1d bridge %x in the NPU",
                        port->npu_port, bridge_id);
            return rc;
        }
        rc = ndi_l2mc_handle_subport_add (port->npu_id, l2mc_group_id, port->npu_port, vlan_id, add_member);
        if (rc != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to add port %d to the to l2mc group %x in the NPU",
                        port->npu_port, l2mc_group_id);
            return rc;
        }


    } else {
        rc = ndi_l2mc_handle_subport_add (port->npu_id, l2mc_group_id, port->npu_port, vlan_id, add_member);
        if(rc != STD_ERR_OK){
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to delete port %d from the l2mc group %x in the NPU",
                        port->npu_port, l2mc_group_id);
            return rc;
        }

        rc = ndi_1d_bridge_member_port_delete(port->npu_id, port->npu_port, vlan_id);
        if (rc != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to remove port %d from the 1d bridge %x in the NPU",
                        port->npu_port, bridge_id);
            return rc;
        }
    }
    return rc;
}

/* TOD :REDUCE ARGS IN THIS FUNCT */
static t_std_error _nas_npu_add_remove_lag_member(npu_id_t npu_id, ndi_obj_id_t lag_id,hal_ifindex_t ifx, nas_bridge_id_t bridge_id ,
                                                  ndi_obj_id_t l2mc_group_id, hal_vlan_id_t vlan_id,
                                                  nas_port_mode_t port_mode, bool add_member)
{

    t_std_error rc = STD_ERR_OK;
    bool reset_pvid =true;
    bool tagged = (port_mode == NAS_PORT_TAGGED) ? true : false;
    if (add_member) {
        hal_vlan_id_t port_vlan_id = 0;
        if (port_mode == NAS_PORT_UNTAGGED) {
            port_vlan_id = vlan_id ;
        } else {

            /* set the lag vlan id to current default vlan ID
             * if this lag is not an untagged member of any vlan
             */
            auto untag_tag_cnt = nas_intf_untag_tag_count(ifx);
            if (untag_tag_cnt.first > 0) {
                reset_pvid = false;
                EV_LOGGING(INTERFACE, INFO, "NAS-Port",
                    "lad id  %llx is untagged member of some vlan so PVID not set to default",
                    lag_id);
            } else {
               nas_vlan_interface_get_default_vlan(&port_vlan_id);
            }
        }
        if (reset_pvid == true) {
            if ((rc = ndi_set_lag_pvid(npu_id, lag_id, port_vlan_id)) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to set PVID of lag %lu in the NPU",lag_id);
                return rc;
            }
        }
        rc = ndi_1d_bridge_member_lag_add(npu_id, bridge_id, lag_id, vlan_id, tagged);
        if (rc != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to add lag %lu to the 1d bridge %x in the NPU",
                       lag_id, bridge_id);
            return rc;
        }
        rc = ndi_l2mc_handle_lagport_add (npu_id, l2mc_group_id, lag_id, vlan_id, add_member);
        if (rc != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to add lag %lu to the to l2mc group %x in the NPU",
                        lag_id, l2mc_group_id);
            return rc;
        }

    } else {
        rc = ndi_l2mc_handle_lagport_add (npu_id, l2mc_group_id,lag_id, vlan_id, add_member);
        if(rc != STD_ERR_OK){
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to delete lag %lu from the l2mc group %x in the NPU",
                        lag_id, l2mc_group_id);
            return rc;
        }

        rc = ndi_1d_bridge_member_lag_delete(npu_id, lag_id, vlan_id);
        if (rc != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to remove lag %lu from the 1d bridge %x in the NPU",
                        lag_id, bridge_id);
            return rc;
        }
    }
    return rc;
}

t_std_error NAS_DOT1D_BRIDGE::nas_bridge_npu_add_remove_member(std::string &mem_name, nas_int_type_t mem_type, bool add_member)
{
    t_std_error rc = STD_ERR_OK;
    hal_vlan_id_t vlan_id = 0;
    nas_port_mode_t port_mode;
    if (mem_type == nas_int_type_VXLAN) {

        if (add_member) {
            return(nas_bridge_npu_add_vxlan_member(mem_name));
        } else {
            return(nas_bridge_npu_remove_vxlan_member(mem_name));

        }
    }

    interface_ctrl_t intf_ctrl;
    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));
    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    BASE_IF_MAC_LEARN_MODE_t learn_mode = BASE_IF_MAC_LEARN_MODE_HW;
    uint32_t i_sh_id = 0;
    nas_com_id_value_t attr[2];
    attr[0].attr_id = DELL_IF_IF_INTERFACES_INTERFACE_MAC_LEARN;
    attr[0].val = &learn_mode;
    attr[0].vlen = sizeof(BASE_IF_MAC_LEARN_MODE_t);
    attr[1].attr_id = DELL_IF_IF_INTERFACES_INTERFACE_INGRESS_SPLIT_HORIZON_ID;
    attr[1].val = &i_sh_id;
    attr[1].vlen = sizeof(uint32_t);

    NAS_INTERFACE *_intf_obj = nullptr;
    _intf_obj = (nas_interface_map_obj_get(mem_name));
    if (_intf_obj == nullptr) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "interface object not found  %s", mem_name.c_str());
        return STD_ERR(INTERFACE,FAIL, rc);
    }

    /*  Set member type a 1D  */
    if (add_member) {
         _intf_obj->nas_br_member_type_1d_set();
    } else  {
         _intf_obj->nas_br_member_type_none_set();
    }
    if ( mem_type == nas_int_type_VLANSUB_INTF) {
    /** If interface type is VLAN Subinterface
     *   retrieve physical interface (phy or lag) and vlan ID from vlan subinterface object,
     *    add membership to 1d bridge
     * */
        NAS_VLAN_INTERFACE *vlan_intf_obj = nullptr;
        vlan_intf_obj = dynamic_cast<NAS_VLAN_INTERFACE *>(_intf_obj);
        if (vlan_intf_obj == nullptr) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "vlan interface object not found  %s", mem_name.c_str());
            return STD_ERR(INTERFACE,FAIL, rc);
        }
        std::string phy_intf = vlan_intf_obj->parent_intf_name;
        safestrncpy(intf_ctrl.if_name, phy_intf.c_str(), sizeof(intf_ctrl.if_name));
        port_mode = NAS_PORT_TAGGED;
        vlan_id  = vlan_intf_obj->vlan_id;

        NAS_INTERFACE * parent_obj = nullptr;
        if((parent_obj = nas_interface_map_obj_get(phy_intf)) != nullptr){
            learn_mode = parent_obj->get_mac_learn_mode();
            i_sh_id = parent_obj->get_ingress_split_horizon_id();
        }

        /*  TODO Get phy port/LAg Index and vlan ID from the vlan sub interface Index  */
    } else if ((mem_type == nas_int_type_PORT) || (mem_type == nas_int_type_LAG)) {
        /** Else If interface type is phy/FC or lag (untagged),
         *     set mode of phy/LAG to L2 in NAS-L3,
         *     use default/reserved vlan id
         *     add interface to the bridge if interface not virtual)
         *
         * */
        safestrncpy(intf_ctrl.if_name, mem_name.c_str(), sizeof(intf_ctrl.if_name));
        port_mode = NAS_PORT_UNTAGGED;
        vlan_id =  nas_bridge_untagged_vlan_id_get();
        learn_mode = _intf_obj->get_mac_learn_mode();
        if (add_member) {
            _intf_obj->untagged_vlan_id_set(vlan_id);
        }
    } else {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Member addition Failed for bridge %s member %d due to",
                                 "Invalid Member type %d",  bridge_name.c_str(), intf_ctrl.if_index, mem_type);
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    if((rc= dn_hal_get_interface_info(&intf_ctrl)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-BRIDGE",
                   "Interface %s returned error %d", intf_ctrl.if_name, rc);
        return STD_ERR(INTERFACE,FAIL, rc);
    }

    /*  Change the mode to L2 if it is not already L2 */
    BASE_IF_MODE_t new_intf_mode;
    bool mode_change = false;
    if_master_info_t master_info = { nas_int_type_VLAN, port_mode, if_index };
    if (add_member) /*  Add member case */{
        if(!nas_intf_add_master(intf_ctrl.if_index, master_info, &new_intf_mode, &mode_change)){
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE","Failed to add master %s for memeber port %d",
                                        bridge_name.c_str(), intf_ctrl.if_index);
        }
    } else /*  delete member case */{
        if(!nas_intf_del_master(intf_ctrl.if_index, master_info, &new_intf_mode, &mode_change)){
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE","Failed to delete master %s for memeber port %d", bridge_name.c_str(), intf_ctrl.if_index);
        }
    }
    if (mode_change) {
        if (nas_intf_handle_intf_mode_change(intf_ctrl.if_index, new_intf_mode) == false) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Update to NAS-L3 about interface mode change failed(%s)", intf_ctrl.if_name);
        }
        if (nas_intf_l3mc_intf_mode_change(intf_ctrl.if_index, new_intf_mode) == false) {
            EV_LOGGING(INTERFACE, ERR, "NAS-BRIDGE", "L3 MC mode change RPC failed if_index(%d), mode(%d)",
                    intf_ctrl.if_index, new_intf_mode);
        }
    }

    if (intf_ctrl.int_type == nas_int_type_LAG) {

        if((rc = _nas_npu_add_remove_lag_member(npu_id,intf_ctrl.lag_id, intf_ctrl.if_index, bridge_id,l2mc_group_id,
                vlan_id,port_mode,add_member)) != STD_ERR_OK){
            return rc;
        }
        /* subintf_attr set handled later */
        if (add_member && (mem_type != nas_int_type_VLANSUB_INTF)) {
            nas_com_id_value_t valu;
            valu.attr_id = DELL_IF_IF_INTERFACES_INTERFACE_MAC_LEARN;
            valu.val = &learn_mode;
            ndi_bridge_sub_port_attr_set(npu_id, intf_ctrl.lag_id, ndi_port_type_LAG,vlan_id,&valu,1);
        }
        nas_bridge_set_lag_tag_untag_drop(npu_id, intf_ctrl.lag_id ,intf_ctrl.if_index);

    } else if ((intf_ctrl.int_type == nas_int_type_PORT) &&
                !(nas_is_virtual_port(intf_ctrl.if_index))) {

        // TODO replace the section with following call
        ndi_port_t _port = { npu_id, intf_ctrl.port_id};
        if((rc = _nas_npu_add_remove_port_member(intf_ctrl.if_index, &_port, bridge_id_get(), l2mc_group_id,
                                                  vlan_id, port_mode,  add_member)) != STD_ERR_OK){
            return rc;
        }
        /* subintf_attr set handled later */
        if (add_member && (mem_type != nas_int_type_VLANSUB_INTF)) {
            nas_com_id_value_t valu;
            valu.attr_id = DELL_IF_IF_INTERFACES_INTERFACE_MAC_LEARN;
            valu.val = &learn_mode;
            ndi_bridge_sub_port_attr_set(npu_id, intf_ctrl.port_id, ndi_port_type_PORT,vlan_id,&valu,1);
        }
        nas_bridge_set_tag_untag_drop(intf_ctrl.if_index, &_port);
    }

    if(mem_type == nas_int_type_VLANSUB_INTF  && add_member){
        if(( rc = nas_interface_utils_set_vlan_subintf_attr(mem_name,attr, sizeof(attr)/sizeof(attr[0]))) != STD_ERR_OK){
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE","Failed to set vlan subintf attr");
            return rc;
        }
    }

    /*  If member is port or LAg then add subport to the .1D bridde andd add in the L2MC group */
    nas_bridge_update_member_list(mem_name, port_mode, add_member);
    return STD_ERR_OK;
}

/*  Add a member to the bridge.  vxlan is considered as tagged category for internal processing*/
t_std_error NAS_DOT1D_BRIDGE::nas_bridge_add_remove_member(std::string & mem_name, nas_port_mode_t port_mode, bool add)
{
    // first add in the kernel and then add in the npu
    t_std_error  rc = STD_ERR_OK;

    if ((rc = nas_bridge_os_add_remove_member(mem_name, port_mode, add)) != STD_ERR_OK) {
        return rc;
    }
    nas_int_type_t mem_type ;
    if (port_mode == NAS_PORT_TAGGED) {
        mem_type = nas_int_type_VLANSUB_INTF;
    } else {
        if ((rc = nas_get_int_name_type(mem_name.c_str(), &mem_type)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " NAS OS L2 PORT Event: Failed to get member type %s ", mem_name.c_str());
            return rc ;
        }
    }
    /*  TODO IF port does not exist then add in the interface map */
    if ((rc = nas_bridge_npu_add_remove_member(mem_name, mem_type, add)) != STD_ERR_OK) {
        nas_bridge_os_add_remove_member(mem_name, port_mode, !add);
        EV_LOGGING(INTERFACE,ERR,"INT-DB-GET","Failed to %s member %s to/from bridge %s",
                            (add ? "add":"delete"), mem_name.c_str(), get_bridge_name().c_str());
        return rc;
    }
    if (port_mode == NAS_PORT_TAGGED) {
        NAS_VLAN_INTERFACE * intf = dynamic_cast<NAS_VLAN_INTERFACE *>(nas_interface_map_obj_get(mem_name));
        if(intf){
            if((rc = intf->set_mtu(nas_bridge_get_mtu())) != STD_ERR_OK){
                EV_LOGGING(INTERFACE, ERR ,"NAS-BRIDGE","Failed to set MTU for member %s", mem_name.c_str());
            }
        }
    }
    if (add == true) {
        nas_interface_util_bridge_name_set(mem_name, bridge_name);
    } else {
        nas_interface_util_bridge_name_clear(mem_name);
    }
    return STD_ERR_OK;
}

t_std_error NAS_DOT1D_BRIDGE::nas_bridge_add_remove_memberlist(memberlist_t & m_list, nas_port_mode_t port_mode, bool add)
{
    t_std_error  rc = STD_ERR_OK;
    memberlist_t processed_mem_list;
    for (auto mem : m_list) {
        if ((rc = nas_bridge_add_remove_member(mem, port_mode, add)) != STD_ERR_OK) {
            break;
        }
        // Save processed member in a list
        processed_mem_list.insert(mem);
    }
    if (rc != STD_ERR_OK) {
        // IF any failure then rollback the added or removed members
        for (auto mem : processed_mem_list) {
            if (nas_bridge_add_remove_member(mem, port_mode, !add) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE,ERR,"INT-DB-GET","Failed to rollback member %s update", mem.c_str());
            }
        }
    }
    return rc;
}

bool NAS_DOT1D_BRIDGE::nas_add_sub_interface()
{
    create_type_t flg = get_create_flag();
    if (flg == BOTH_MODEL || flg == INT_MOD_CREATE) {
        EV_LOGGING(INTERFACE,DEBUG , "NAS-BRIDGE",
           "1D bidge: Member  will  be added to br %s as mode is %d",
             get_bridge_name().c_str(), flg);
        return true;
    }
    EV_LOGGING(INTERFACE,DEBUG , "NAS-BRIDGE",
        "1D bridgei: Member will not be added to br %s as mode is %d",
        get_bridge_name().c_str(), flg);
    return false;
}

t_std_error NAS_DOT1D_BRIDGE:: nas_bridge_associate_npu_port(std::string &mem_name, ndi_port_t *port, nas_port_mode_t port_mode, bool associate)
{

    // TODO check if mem_name exists in the member list
    hal_vlan_id_t vlan_id;
    hal_ifindex_t if_index;
    t_std_error rc = STD_ERR_OK;

    if (nas_int_name_to_if_index(&if_index, mem_name.c_str()) != STD_ERR_OK) {
         EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Faied to associate as no ifindex found for %s",
                   mem_name.c_str());
    }
    if (port_mode == NAS_PORT_UNTAGGED) {
        nas_vlan_interface_get_default_vlan(&vlan_id);
    } else {
        NAS_VLAN_INTERFACE *vlan_intf_obj = nullptr;
        vlan_intf_obj = dynamic_cast<NAS_VLAN_INTERFACE *>(nas_interface_map_obj_get(mem_name));
        if (vlan_intf_obj == nullptr) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "VLAN Member %s not present ",mem_name.c_str());
            return STD_ERR(INTERFACE,FAIL, 0);
        }
        vlan_id  = vlan_intf_obj->vlan_id_get();

    }
    /* TODO : this association code path needs to be tested */
    rc =  _nas_npu_add_remove_port_member(if_index,port, bridge_id_get(), l2mc_group_id,
                                            vlan_id, port_mode,  associate);
    if (rc == STD_ERR_OK) {
        nas_bridge_set_tag_untag_drop(if_index, port);
    } else {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE","assocaite NPU port : Failed to add member %s to the Vlan %d in the NPU",
            mem_name.c_str(), vlan_id);

    }
    return rc;
}

t_std_error NAS_DOT1D_BRIDGE::nas_bridge_npu_add_vxlan_member(std::string mem_name)
{
    t_std_error rc = STD_ERR_OK;
    /** STEPS:
    *  1. Get the list of all remote endpoints in the vxlan */
    NAS_VXLAN_INTERFACE *vxlan_obj = nullptr;
    vxlan_obj = dynamic_cast<NAS_VXLAN_INTERFACE *>(nas_interface_map_obj_get(mem_name));
    if (vxlan_obj == nullptr) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "VXLAN Member add failed %s: vxlan intf %s does not exist.", bridge_name.c_str(), mem_name.c_str());
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    /*  2. Add tunnels corresponding to all remote endpoint in the npu */
    vxlan_obj->nas_interface_for_each_remote_endpoint([this](BASE_CMN_VNI_t vni, hal_ip_addr_t &local_ip, remote_endpoint_t &rm_endpoint) {
            /*  Get bridge_oid, vni, remote endpoint IP, local IP,   */
            /*  Call NDI API to create tunnel  */
            /*  Add the tunnel to L2MC group if flooding enabled */
        nas_bridge_add_remote_endpoint(vni, local_ip, &rm_endpoint);
    });
    /*
     *      create tunnel with VNI, bridge Id, remote endpoint, local endpoint
     *      if flooding flag enable then Add the tunnel to the L2MC group
     **/
    /*  Update the memberlist */
    rc = nas_bridge_add_vxlan_member_in_list(mem_name);
    // Now set MTU on the bridge and VXLAN interface in the kernel
    NAS_BRIDGE::nas_bridge_set_mtu();
    vxlan_obj->bridge_name = bridge_name;
    return rc;
}

t_std_error NAS_DOT1D_BRIDGE::nas_bridge_npu_remove_vxlan_member(std::string mem_name) {
    /** STEPS:
    *  1. Get the list of the remote endpoints in the vxlan
    *  2. Delete all remote endpoints tunnel in the NPU
    *  3. Update the cache
    **/
    NAS_VXLAN_INTERFACE *vxlan_obj = nullptr;
    vxlan_obj = dynamic_cast<NAS_VXLAN_INTERFACE *>(nas_interface_map_obj_get(mem_name));
    if (vxlan_obj == nullptr) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "VXLAN Member add failed %s: vxlan intf %s does not exist.", bridge_name.c_str(), mem_name.c_str());
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    /*  2. Add tunnels corresponding to all remote endpoint in the npu */
    vxlan_obj->nas_interface_for_each_remote_endpoint([&](BASE_CMN_VNI_t vni, hal_ip_addr_t &local_ip, remote_endpoint_t &rm_endpoint) {
            /*  Call NDI API to create tunnel  */
            /*  Add the tunnel to L2MC group if flooding enabled */
        this->nas_bridge_remove_remote_endpoint(vni, local_ip, &rm_endpoint);
        vxlan_obj->nas_interface_publish_remote_endpoint_event(&rm_endpoint, cps_api_oper_DELETE, true);
    });

    /*  Update the memberlist */
    nas_bridge_nas_bridge_remove_vxlan_member_from_list(mem_name);
    vxlan_obj->bridge_name.clear();
    return STD_ERR_OK;
}


t_std_error NAS_DOT1D_BRIDGE::nas_bridge_add_remote_endpoint(BASE_CMN_VNI_t vni, hal_ip_addr_t local_ip, remote_endpoint_t *rm_endpoint)
{
    t_std_error rc = STD_ERR_OK;
    //const char* bridge_vrf_name = "Bridge_vrf_name";
    if (rm_endpoint == nullptr)  { return STD_ERR(INTERFACE, FAIL, 0); }

    char buff[HAL_INET6_TEXT_LEN + 1];
    std_ip_to_string((const hal_ip_addr_t*) &rm_endpoint->remote_ip, buff, HAL_INET6_TEXT_LEN);

    EV_LOGGING(INTERFACE,DEBUG,"NAS-BRIDGE", "Add  remote endpoint :br %s, remote ip-address %s", bridge_name.c_str(), buff);
    if (g_vrf_id == 0) {
        if (nas_get_vrf_obj_id_from_vrf_name(NAS_DEFAULT_VRF_NAME, &g_vrf_id) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Failed to get vrf OID from VRF name %s", bridge_name.c_str());
            return STD_ERR(INTERFACE, FAIL, 0);
        }
        if (ndi_create_underlay_rif(npu_id, &underlay_rif, g_vrf_id) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Failed to create underlay rif %s", bridge_name.c_str());
            return STD_ERR(INTERFACE, FAIL, 0);
        }
        EV_LOGGING(INTERFACE,DEBUG,"NAS-BRIDGE", "CREATED :g_vrf_id is %llu underlay_rif %llu", g_vrf_id, underlay_rif);
    } else {
        EV_LOGGING(INTERFACE,DEBUG,"NAS-BRIDGE", "g_vrf_id is %llu underlay_rif ", g_vrf_id, underlay_rif);
    }

    nas_custom_id_value_t custom_params[3];
    nas_com_id_value_t tunnel_params[3]; /*  VNI, local IP, Remove IP, ( Later VR OID) */
    tunnel_params[0].attr_id = DELL_IF_IF_INTERFACES_INTERFACE_VNI;
    tunnel_params[0].val = &vni;
    tunnel_params[0].vlen  = sizeof(vni);
    tunnel_params[1].attr_id = DELL_IF_IF_INTERFACES_INTERFACE_SOURCE_IP_ADDR;
    tunnel_params[1].val = &local_ip;
    tunnel_params[1].vlen  = sizeof(local_ip);
    tunnel_params[2].attr_id = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR;
    tunnel_params[2].val = &(rm_endpoint->remote_ip);
    tunnel_params[2].vlen  = sizeof(rm_endpoint->remote_ip);

    custom_params[0].attr_id = VRF_OID;
    custom_params[0].val = &g_vrf_id;
    custom_params[0].vlen  = sizeof(g_vrf_id);
    custom_params[1].attr_id = UNDERLAY_OID;
    custom_params[1].val = &underlay_rif;
    custom_params[1].vlen  = sizeof(underlay_rif);
    custom_params[2].attr_id = BRIDGE_OID;
    custom_params[2].val = &bridge_id;
    custom_params[2].vlen  = sizeof(bridge_id);

    rc = nas_ndi_add_remote_endpoint(npu_id, tunnel_params, 3, custom_params, 3, &(rm_endpoint->tunnel_id));
    if (rc != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Failed to add remote endpoint to the bridge %s", bridge_name.c_str());
        return rc;
    }
    if (rm_endpoint->flooding_enabled) {
        rc =  ndi_l2mc_handle_tunnel_member(npu_id, l2mc_group_id, rm_endpoint->tunnel_id, &(rm_endpoint->remote_ip), true);
        if (rc != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Failed to add remote endpoint to the l2mc group %s", bridge_name.c_str());
            return rc;
        }
        l2mc_members.insert(rm_endpoint->tunnel_id);
    }

    if (rm_endpoint->mac_learn_mode) {
        rc = ndi_tunport_mac_learn_mode_set(npu_id, rm_endpoint->tunnel_id, (BASE_IF_PHY_MAC_LEARN_MODE_t) rm_endpoint->mac_learn_mode);
        if (rc != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Failed to set remote endpoint mac_learn_mode bridge %s ip %s",
                     bridge_name.c_str(), buff);
            return rc;
        }
    }
    return STD_ERR_OK;
}


t_std_error NAS_DOT1D_BRIDGE::nas_bridge_set_flooding(remote_endpoint_t *rm_endpoint)
{
    t_std_error rc = STD_ERR_OK;
    if ((l2mc_group_id == INVALID_L2MC_GROUP_ID) ||
         (rm_endpoint->tunnel_id == NAS_INVALID_TUNNEL_ID)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Failed to add remote endpoint: l2mc or tunnel id invalid %s",
                                   bridge_name.c_str());
        return STD_ERR(INTERFACE,FAIL, 0);
    }
    EV_LOGGING(INTERFACE,DEBUG,"NAS-BRIDGE", "Set flooding  to %d in NDI for bridge %s",
                                 rm_endpoint->flooding_enabled, bridge_name.c_str());

    rc =  ndi_l2mc_handle_tunnel_member(npu_id, l2mc_group_id, rm_endpoint->tunnel_id, &(rm_endpoint->remote_ip), rm_endpoint->flooding_enabled);
    if (rc != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Failed to add remote endpoint to the l2mc group %s", bridge_name.c_str());
        return rc;
    }
    if (rm_endpoint->flooding_enabled) {
        l2mc_members.insert(rm_endpoint->tunnel_id);
    } else {
        l2mc_members.erase(rm_endpoint->tunnel_id);
    }

    return rc;

}

t_std_error NAS_DOT1D_BRIDGE::nas_bridge_remove_remote_endpoint(BASE_CMN_VNI_t vni, hal_ip_addr_t local_ip, remote_endpoint_t *rm_endpoint)
{
    t_std_error rc = STD_ERR_OK;
    if (rm_endpoint == nullptr)  { return STD_ERR(INTERFACE, FAIL, 0); }

    char buff[HAL_INET6_TEXT_LEN + 1];
    std_ip_to_string((const hal_ip_addr_t*) &rm_endpoint->remote_ip, buff, HAL_INET6_TEXT_LEN);
    EV_LOGGING(INTERFACE,DEBUG,"NAS-BRIDGE", "Remove remote endpoint :br %s, remote ip-address %s", bridge_name.c_str(), buff);
    if(l2mc_members.find(rm_endpoint->tunnel_id) != l2mc_members.end()){
        rc =  ndi_l2mc_handle_tunnel_member(npu_id, l2mc_group_id, rm_endpoint->tunnel_id, NULL, false);
        if (rc != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Failed to remove remote endpoint to the l2mc group %s", bridge_name.c_str());
            return rc;
        }
        l2mc_members.erase(rm_endpoint->tunnel_id);
    }

    nas_com_id_value_t tunnel_params[2]; /*  VNI, local IP, Remove IP, */
    tunnel_params[0].attr_id = DELL_IF_IF_INTERFACES_INTERFACE_SOURCE_IP_ADDR;
    tunnel_params[0].val = &local_ip;
    tunnel_params[0].vlen  = sizeof(local_ip);
    tunnel_params[1].attr_id = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR;
    tunnel_params[1].val = &(rm_endpoint->remote_ip);
    tunnel_params[1].vlen  = sizeof(rm_endpoint->remote_ip);
    rc = nas_ndi_remove_remote_endpoint(npu_id,bridge_id, tunnel_params, sizeof(tunnel_params)/sizeof(tunnel_params[0]));
    if (rc != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Failed to remove remote endpoint to the bridge %s", bridge_name.c_str());
        return rc;
    }
    rm_endpoint->tunnel_id = NAS_INVALID_TUNNEL_ID;

    return STD_ERR_OK;
}

t_std_error NAS_DOT1D_BRIDGE::nas_bridge_get_remote_endpoint_stats(hal_ip_addr_t local_ip, remote_endpoint_t *rm_endpoint,
                                                                ndi_stat_id_t *ndi_stat_ids, uint64_t* stats_val, size_t len)
{

    nas_com_id_value_t tunnel_params[2]; /*  VNI, local IP, Remove IP, */
    tunnel_params[0].attr_id = DELL_IF_IF_INTERFACES_INTERFACE_SOURCE_IP_ADDR;
    tunnel_params[0].val = &local_ip;
    tunnel_params[0].vlen  = sizeof(local_ip);
    tunnel_params[1].attr_id = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR;
    tunnel_params[1].val = &(rm_endpoint->remote_ip);
    tunnel_params[1].vlen  = sizeof(rm_endpoint->remote_ip);

    /*
     * Get stats for bridge tunnel port
     */

    if(ndi_tunnel_bridge_port_stats_get(npu_id,bridge_id,tunnel_params,
        sizeof(tunnel_params)/sizeof(tunnel_params[0]),ndi_stat_ids,
        stats_val,len) != STD_ERR_OK){
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Failed to get remote endpoint stats");
        return STD_ERR(INTERFACE,FAIL,0);
    }
    return STD_ERR_OK;
}

cps_api_return_code_t NAS_DOT1D_BRIDGE::nas_bridge_fill_info(cps_api_object_t obj)
{
    if (nas_bridge_fill_com_info(obj) != cps_api_ret_code_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Failed to add 1D bridge info for bridge %s",
                bridge_name.c_str());
        return cps_api_ret_code_ERR;
    }
    /*  Fill VXLAN members  */
    if (!_vxlan_members.empty()) {
        for (auto it = _vxlan_members.begin(); it != _vxlan_members.end(); ++it) {
            cps_api_object_attr_add(obj, BRIDGE_DOMAIN_BRIDGE_MEMBER_INTERFACE, it->c_str(),
                    strlen(it->c_str())+1);
        }
    }
    return cps_api_ret_code_OK;
}

t_std_error NAS_DOT1D_BRIDGE::nas_bridge_intf_cntrl_block_register(hal_intf_reg_op_type_t op) {

    interface_ctrl_t details;

    EV_LOGGING(INTERFACE, INFO ,"NAS-BRIDGE", " %s Dot 1D  %d %lld with ifCntrl name %s",
            (op == HAL_INTF_OP_REG) ? "Registering" : "Deregistering", if_index, bridge_id, bridge_name.c_str());

    memset(&details,0,sizeof(details));
    details.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    details.if_index = if_index;
    safestrncpy(details.if_name,bridge_name.c_str(), sizeof(details.if_name));
    details.bridge_id = bridge_id;
    details.int_type = nas_int_type_DOT1D_BRIDGE;
    std::string intf_mac = get_bridge_mac();
    safestrncpy(details.mac_addr,intf_mac.c_str(), sizeof(details.mac_addr));

    if ((op == HAL_INTF_OP_DEREG) && ((dn_hal_get_interface_info(&details)) != STD_ERR_OK)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Bridge %d %s Not registered with ifCntrl ",
               if_index, bridge_name.c_str());
        return STD_ERR_OK;
    }

    if(op == HAL_INTF_OP_DEREG && details.int_type != nas_int_type_DOT1D_BRIDGE){
        EV_LOGGING(INTERFACE,INFO,"NAS-BRIDGE","Bridge %s is registered as %d type. Can't deregister "
                    "it using type %d",bridge_name.c_str(),details.int_type,nas_int_type_DOT1D_BRIDGE);
        return STD_ERR_OK;
    }
    if (dn_hal_if_register(op, &details)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "Bridge %s bridge id 0x%\"PRIx64\"(De)registered failed with ifCntrl op %d ", bridge_name.c_str(), bridge_id, op);
        return STD_ERR(INTERFACE,FAIL,0);
    }
    return STD_ERR_OK;
}

t_std_error nas_get_vrf_id_from_vrf_name (const char *vrf_name, ndi_obj_id_t *vrf_id)
{
    t_std_error rc;
    if (*vrf_id != 0) return STD_ERR_OK;

    ndi_vr_entry_t  vr_entry;
    /* Create a virtual router entry and get vr_id (maps to fib vrf id) */
    memset (&vr_entry, 0, sizeof (ndi_vr_entry_t));
    //hal_mac_addr_t  src_mac = {0x14, 0x18, 0x77,0x0c, 0xdf,0x58};
    //memcpy(&vr_entry.src_mac, src_mac, sizeof(hal_mac_addr_t));
    nas_switch_wait_for_sys_base_mac(&vr_entry.src_mac);

    /*
     * Set system MAC address for the VRs
     */
    vr_entry.flags = NDI_VR_ATTR_SRC_MAC_ADDRESS;

    if ((rc = ndi_route_vr_create(&vr_entry, vrf_id))!= STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "VRF oid creation failed for VRF:%s", vrf_name);
        return false;
    }
    return STD_ERR_OK;
}

bool NAS_DOT1D_BRIDGE::nas_bridge_vxlan_intf_present(void) {
    if (_vxlan_members.empty()) { return false;}
    return true;
}

t_std_error NAS_DOT1D_BRIDGE::nas_bridge_add_vxlan_member_in_list(std::string mem_name)
{
    try {
        _vxlan_members.insert(mem_name);
    } catch (std::exception& e) {
        EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " Failed to add untagged member in the list %s", e.what());
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return STD_ERR_OK;
}

t_std_error NAS_DOT1D_BRIDGE::nas_bridge_nas_bridge_remove_vxlan_member_from_list(std::string mem_name)
{
    for (auto itr = _vxlan_members.begin(); itr != _vxlan_members.end(); ++itr) {
        if (itr->compare(mem_name) == 0) {
            itr = _vxlan_members.erase(itr);
            return STD_ERR_OK;
        }
    }
    EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " Failed to remove vxlan member %s in the list ",mem_name.c_str());
    return STD_ERR(INTERFACE, FAIL, 0);
}

t_std_error NAS_DOT1D_BRIDGE::nas_bridge_set_learning_disable(bool disable) {
    /*
     * To add code to set learning disable in ndi
     */
    nas_bridge_com_set_learning_disable(disable);
    return STD_ERR_OK;
}

t_std_error NAS_DOT1D_BRIDGE::nas_bridge_set_mtu(cps_api_object_t obj, cps_api_object_it_t  & it)
{

    uint32_t mtu = cps_api_object_attr_data_u32(it.attr);
    t_std_error rc;

    for(auto member : _vxlan_members){
        NAS_VXLAN_INTERFACE * intf = dynamic_cast<NAS_VXLAN_INTERFACE *>(nas_interface_map_obj_get(member));
        if(intf){
            if((rc = intf->set_mtu(mtu))!=STD_ERR_OK){
                EV_LOGGING(INTERFACE, ERR ,"NAS-BRIDGE","Failed to set MTU for vxlan member %s", member.c_str());
            }
        }
    }
    return (NAS_BRIDGE::nas_bridge_set_mtu(obj, it));
}

