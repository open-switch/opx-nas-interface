
/*
 * Copyright (c) 2018 Dell Inc.
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
 * filename: nas_interface_utils.cpp
 */

#include "dell-base-if-vlan.h"
#include "interface/nas_interface_utils.h"
#include "nas_ndi_1d_bridge.h"
#include "nas_os_vlan.h"
#include "hal_if_mapping.h"
#include "nas_os_interface.h"
#include <std_utils.h>

void nas_interface_cps_publish_event(std::string &if_name, nas_int_type_t if_type, cps_api_operation_types_t op)
{
    cps_api_object_guard og(cps_api_object_create());
    if (!cps_api_key_from_attr_with_qual(cps_api_object_key(og.get()),
                DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                cps_api_qualifier_OBSERVED)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF","Could not translate to logical interface key ");
        return;
    }
    cps_api_object_set_type_operation(cps_api_object_key(og.get()), op);
    cps_api_set_key_data(og.get(),IF_INTERFACES_INTERFACE_NAME,
                               cps_api_object_ATTR_T_BIN, if_name.c_str(), strlen(if_name.c_str())+1);
    char ietf_if_type[256];
    if (!nas_to_ietf_if_type_get(if_type, ietf_if_type, sizeof(ietf_if_type))) {
        EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Failed to get IETF interface type for type id %d",
                   if_type);
        return;
    }
    cps_api_object_attr_add(og.get(),IF_INTERFACES_INTERFACE_TYPE,
                                                (const void *)ietf_if_type, strlen(ietf_if_type)+1);
    if (op == cps_api_oper_CREATE) {

        NAS_INTERFACE *nas_intf = nas_interface_map_obj_get(if_name);
        if (nas_intf == nullptr) {
            EV_LOGGING(INTERFACE,ERR,"NAS-IF","Publish failure : Could not get interface object");
            return;
        }
        /*
         * To be fixed when we create sub classes for lag and physical interfaces
         */
        if(if_type == nas_int_type_VLANSUB_INTF){
            NAS_VLAN_INTERFACE *intf = dynamic_cast<NAS_VLAN_INTERFACE *>(nas_intf);
            if (intf->nas_interface_fill_info(og.get()) != cps_api_ret_code_OK) {
                EV_LOGGING(INTERFACE,ERR,"NAS-IF","Could not get common interface info");
                return;
            }
        }
        if(if_type == nas_int_type_VXLAN){
            NAS_VXLAN_INTERFACE *intf = dynamic_cast<NAS_VXLAN_INTERFACE *>(nas_intf);
            if (intf->nas_interface_fill_info(og.get()) != cps_api_ret_code_OK) {
                EV_LOGGING(INTERFACE,ERR,"NAS-IF","Could not get common interface info");
                return;
            }
        }

    }
    cps_api_event_thread_publish(og.get());
    return;
}

static bool nas_intf_cntrl_blk_register(hal_ifindex_t ifindex,const std::string & ifname,
                                        nas_int_type_t type ,bool add) {

    hal_intf_reg_op_type_t reg_op;

    interface_ctrl_t if_entry;
    memset(&if_entry,0,sizeof(if_entry));
    if_entry.int_type = type;
    safestrncpy(if_entry.if_name,ifname.c_str(),sizeof(if_entry.if_name));
    if_entry.if_index = ifindex;

    if (add) {
        reg_op = HAL_INTF_OP_REG;
    } else {
        reg_op = HAL_INTF_OP_DEREG;
    }

    if (dn_hal_if_register(reg_op,&if_entry) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,INFO,"NAS-INT",
            "interface register Error %s - mapping error or interface already present",ifname.c_str());
        return false;
    }
    return true;
}
t_std_error nas_interface_utils_vlan_create(std::string intf_name,
                                 hal_ifindex_t intf_idx,
                                 hal_vlan_id_t vlan,
                                 std::string parent,
                                 NAS_VLAN_INTERFACE **vlan_obj)
{
    /* Create vlan intf obj  */
    NAS_VLAN_INTERFACE *obj = new NAS_VLAN_INTERFACE(intf_name, intf_idx, vlan, parent);

    /*  Add it in the map */
    if (nas_interface_map_obj_add(intf_name, (NAS_INTERFACE *)obj) != STD_ERR_OK) {
        /*  Failed to add in the map */
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    NAS_INTERFACE * intf_obj = nas_interface_map_obj_get(parent);
    if(intf_obj){
        intf_obj->add_sub_intf(intf_name);
    }
    /*  Publish interface create event */
    if (vlan_obj != nullptr) { *vlan_obj = obj;}
    return STD_ERR_OK;
}


t_std_error nas_interface_utils_vlan_delete(std::string intf_name) {

    NAS_VLAN_INTERFACE *vlan_obj = nullptr;
    if (nas_interface_map_obj_remove(intf_name, (NAS_INTERFACE **)&vlan_obj) != STD_ERR_OK) {
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    if(vlan_obj){
        std::string parent = vlan_obj->parent_name_get();
        NAS_INTERFACE * intf_obj = nas_interface_map_obj_get(parent);
        if(intf_obj){
            intf_obj->del_sub_intf(intf_name);
        }
    }
    /*  delete API or unique ptr */
    if (vlan_obj != nullptr) {
        delete vlan_obj;
    }
    return STD_ERR_OK;
}

/*  Create vxlan interface object and publish event */
t_std_error nas_interface_utils_vxlan_create(std::string intf_name, hal_ifindex_t intf_idx,
        BASE_CMN_VNI_t vni, hal_ip_addr_t local_ip, NAS_VXLAN_INTERFACE **vxlan_obj)
{
    /* Create vxlan intf obj  */
    NAS_VXLAN_INTERFACE *obj = new NAS_VXLAN_INTERFACE(intf_name, intf_idx, vni, local_ip);

    /*  Add it in the map */
    if (nas_interface_map_obj_add(intf_name, (NAS_INTERFACE *)obj) != STD_ERR_OK) {
        /*  Failed to add in the map */
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    if (vxlan_obj != nullptr) { *vxlan_obj = obj;}
    return STD_ERR_OK;
}

/*  Delete vxlan interface object and publish event */
t_std_error nas_interface_utils_vxlan_delete(std::string intf_name) {

    NAS_VXLAN_INTERFACE *vxlan_obj = nullptr;
    if (nas_interface_map_obj_remove(intf_name, (NAS_INTERFACE **)&vxlan_obj) != STD_ERR_OK) {
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    /*  delete API or unique ptr */
    if (vxlan_obj != nullptr) {
        delete vxlan_obj;
    }
    return STD_ERR_OK;
}

t_std_error nas_interface_utils_parent_name_get(std::string &intf_name, std::string &parent) {
   NAS_INTERFACE *intf_obj = nas_interface_map_obj_get(intf_name);
   if ((intf_obj != nullptr) && (intf_obj->intf_type_get() == nas_int_type_VLANSUB_INTF)) {
       NAS_VLAN_INTERFACE *vlan_intf_obj =  dynamic_cast<NAS_VLAN_INTERFACE *>(intf_obj);
       parent = vlan_intf_obj->parent_name_get();
       return STD_ERR_OK;
   }
   return STD_ERR(INTERFACE, FAIL, 0);
}

t_std_error nas_interface_utils_vlan_id_get(std::string &intf_name, hal_vlan_id_t &vlan_id ) {
   NAS_INTERFACE *intf_obj = nas_interface_map_obj_get(intf_name);
   if ((intf_obj != nullptr) && (intf_obj->intf_type_get() == nas_int_type_VLANSUB_INTF)) {
       NAS_VLAN_INTERFACE *vlan_intf_obj =  dynamic_cast<NAS_VLAN_INTERFACE *>(intf_obj);
       vlan_id = vlan_intf_obj->vlan_id_get();
       return STD_ERR_OK;
   }
   return STD_ERR(INTERFACE, FAIL, 0);
}

t_std_error nas_interface_utils_ifindex_get(const std::string &intf_name, hal_ifindex_t &ifindex ) {
   NAS_INTERFACE *intf_obj = nas_interface_map_obj_get(intf_name);
   if (intf_obj != nullptr) {
       ifindex = intf_obj->if_index;
       return STD_ERR_OK;
   }
   return STD_ERR(INTERFACE, FAIL, 0);
}

// Create sub interface in the kernel and store in the map

t_std_error nas_interface_utils_os_vlan_intf_create_delete(const char *if_name, hal_vlan_id_t vlan_id, const char *parent_name,
                                                        cps_api_operation_types_t op,
                                                        hal_ifindex_t *if_index=nullptr )
{
    if (if_index == NULL)  return STD_ERR(INTERFACE, FAIL, 0);

    cps_api_object_guard _og(cps_api_object_create());
    cps_api_set_key_data(_og.get(),IF_INTERFACES_INTERFACE_NAME,
                               cps_api_object_ATTR_T_BIN, if_name, strlen(if_name)+1);
    //TODO  add as attribute also Temporary fix
    cps_api_object_attr_add(_og.get(),IF_INTERFACES_INTERFACE_NAME,
                                        if_name, strlen(if_name)+1);
    cps_api_object_attr_add(_og.get(),DELL_IF_IF_INTERFACES_INTERFACE_PARENT_INTERFACE,
                                        parent_name, strlen(parent_name)+1);

    cps_api_object_attr_add_u32(_og.get(),BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, vlan_id);

    if(op == cps_api_oper_CREATE){
        if (nas_os_create_subinterface(_og.get()) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR, "NAS-INTF", " Failed to add interface %s in the kernel ",
                                if_name);
            return STD_ERR(INTERFACE, FAIL, 0);
        }
        cps_api_object_attr_t if_index_attr = cps_api_object_attr_get(_og.get(), DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
        if ((if_index != nullptr) && (if_index_attr != nullptr)) {
            *if_index = (hal_ifindex_t) cps_api_object_attr_data_u32(if_index_attr);
        }
    }else if(op ==cps_api_oper_DELETE){
        if(nas_os_del_interface(*if_index)!=STD_ERR_OK){
            EV_LOGGING(INTERFACE,ERR,"VXLAN","Failed deleting vlan sub-interface %s in the os",if_name);
            return STD_ERR(INTERFACE,FAIL,0);
        }
    }

    return STD_ERR_OK;
}
t_std_error nas_interface_vlan_subintf_create(std::string &intf_name, hal_vlan_id_t vlan_id, std::string &parent, bool in_os)
{
    NAS_INTERFACE *intf_obj = nas_interface_map_obj_get(intf_name);
    if (intf_obj != nullptr) {
        EV_LOGGING(INTERFACE,ERR, "NAS-INTF", " Failed to add interface %s: Already present ",
                                intf_name.c_str());
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    hal_ifindex_t if_index =NAS_IF_INDEX_INVALID;
    if (in_os) {
        if (nas_interface_utils_os_vlan_intf_create_delete(intf_name.c_str(), vlan_id, parent.c_str(),
                                                    cps_api_oper_CREATE, &if_index) != STD_ERR_OK) {
            return STD_ERR(INTERFACE, FAIL, 0);
        }
    }
    if (nas_interface_utils_vlan_create(intf_name, if_index, vlan_id, parent) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR, "NAS-INTF", " Failed to add interface %s: Already present  in the cache",
                                intf_name.c_str());
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    if(!nas_intf_cntrl_blk_register(if_index,intf_name,nas_int_type_VLANSUB_INTF,true)){
        EV_LOGGING(INTERFACE,ERR,"NAS-VLAN-SUB-INTF","Failed to register the vlan sub interface %s",
                    intf_name.c_str());
        return STD_ERR(INTERFACE,FAIL,0);
    }

    nas_interface_cps_publish_event(intf_name,nas_int_type_VLANSUB_INTF,cps_api_oper_CREATE);

    return STD_ERR_OK;
}

t_std_error nas_interface_vlan_subintf_list_create(intf_list_t & intf_list, hal_vlan_id_t vlan_id, bool in_os)
{
    std_mutex_simple_lock_guard lock(get_vlan_mutex());
    for (auto  intf_name : intf_list ) {

        NAS_INTERFACE *intf_obj = nas_interface_map_obj_get(intf_name);
        if (intf_obj != nullptr) {
            EV_LOGGING(INTERFACE,ERR, "NAS-INTF", " Failed to add interface %s: Already present ",
                                    intf_name.c_str());
            // Just continue
            continue;
        }
        char _intf_name[HAL_IF_NAME_SZ];
        memset(_intf_name, 0, sizeof(_intf_name));
        char * saveptr;
        safestrncpy(_intf_name, intf_name.c_str(), sizeof(_intf_name));

        const char *_parent = strtok_r(_intf_name,".",&saveptr);
        std::string parent;
        if(_parent != nullptr){
            parent = std::string(_parent);
        }else{
            parent = intf_name;
            intf_name = intf_name + "." + std::to_string(vlan_id);
        }
        EV_LOGGING(INTERFACE,NOTICE, "NAS-INTF", " Create sub interface with parent %s", parent.c_str());

        nas_interface_vlan_subintf_create(intf_name,vlan_id,parent,in_os);
    }
    return STD_ERR_OK;
}

t_std_error nas_interface_vlan_subintf_delete(std::string &intf_name)
{
    NAS_INTERFACE *intf_obj = nas_interface_map_obj_get(intf_name);
    if (intf_obj == nullptr) {
        EV_LOGGING(INTERFACE,ERR, "NAS-INTF", " Failed to delete interface %s: Not present ",
                                intf_name.c_str());
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    NAS_VLAN_INTERFACE *vlan_intf_obj =  dynamic_cast<NAS_VLAN_INTERFACE *>(intf_obj);
    hal_vlan_id_t vlan_id = vlan_intf_obj->vlan_id_get();
    std::string parent = vlan_intf_obj->parent_name_get();
    if (nas_interface_utils_os_vlan_intf_create_delete(intf_name.c_str(), vlan_id, parent.c_str(),
                                            cps_api_oper_DELETE,&vlan_intf_obj->if_index) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR, "NAS-INTF", " Failed to delete interface %s: in the kernel ", intf_name.c_str());
        //  TODO it may not be created
    }

    if(!nas_intf_cntrl_blk_register(vlan_intf_obj->if_index,vlan_intf_obj->if_name,
                                    nas_int_type_VLANSUB_INTF,false)){
        EV_LOGGING(INTERFACE,ERR,"NAS-VLAN-SUB-INTF","Failed to de-register the vlan sub interface %s",
                vlan_intf_obj->if_name.c_str());
        return STD_ERR(INTERFACE,FAIL,0);
    }

    nas_interface_cps_publish_event(intf_name,nas_int_type_VLANSUB_INTF,cps_api_oper_DELETE);

    if (nas_interface_utils_vlan_delete(intf_name) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR, "NAS-INTF", " Failed to delete interface %s from cache",
                                intf_name.c_str());
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    return STD_ERR_OK;
}

t_std_error nas_interface_vlan_subintf_list_delete(intf_list_t &list) {
    std_mutex_simple_lock_guard lock(get_vlan_mutex());
    for (auto intf : list) {
        if (nas_interface_vlan_subintf_delete(intf) != STD_ERR_OK) {
            // Rollback is not possible since records are removed. just continue to delete whatever can be
            continue;
        }
    }
    return STD_ERR_OK;
}

static auto sub_intf_attr_fn = [](const std::string & intf_name, nas_com_id_value_t & val) -> t_std_error {
    NAS_INTERFACE *obj = nullptr;
    if(((obj = nas_interface_map_obj_get(intf_name)) == nullptr)){
        EV_LOGGING(INTERFACE,ERR,"SUB-INTF-ATTR-SET","No Interface %s exsist in the map",intf_name.c_str());
        return STD_ERR(INTERFACE,PARAM,0);
    }

    NAS_VLAN_INTERFACE *vlan_obj = dynamic_cast<NAS_VLAN_INTERFACE *>(obj);
    if(!vlan_obj){
        EV_LOGGING(INTERFACE,ERR,"SUB-INTF-ATTR-SET","Failed to typecast %s to vlan interface obejct",intf_name.c_str());
        return STD_ERR(INTERFACE,FAIL,0);
    }

    interface_ctrl_t intf_entry;
    memset(&intf_entry,0,sizeof(intf_entry));
    intf_entry.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    std::string parent_intf = vlan_obj->parent_name_get();
    memcpy(intf_entry.if_name,parent_intf.c_str(),sizeof(intf_entry.if_name));

    t_std_error rc = STD_ERR_OK;
    if((rc = dn_hal_get_interface_info(&intf_entry))!=STD_ERR_OK){
        return rc;
    }

    ndi_port_type_t ndi_port_type = ndi_port_type_PORT;
    ndi_obj_id_t ndi_obj_id = 0;
    if(intf_entry.int_type == nas_int_type_PORT){
        ndi_port_type = ndi_port_type_PORT;
        ndi_obj_id = intf_entry.port_id;
    }else if(intf_entry.int_type == nas_int_type_LAG){
        ndi_port_type = ndi_port_type_LAG;
        ndi_obj_id = intf_entry.lag_id;
    }else{
        EV_LOGGING(INTERFACE,ERR,"SUB-INTF-ATTR-SET","Invalid Interface type %d",intf_entry.int_type);
    }

    EV_LOGGING(INTERFACE,INFO,"SUB-INTF-ATTR-SET","Setting attr %lu in npu for "
                "interface %s",val.attr_id,intf_name.c_str());
    return ndi_bridge_sub_port_attr_set(intf_entry.npu_id,ndi_obj_id,ndi_port_type,vlan_obj->vlan_id_get(),&val,1);
};

t_std_error nas_interface_utils_set_vlan_subintf_attr(const std::string & intf_name,  nas_com_id_value_t  val[],
                                                      size_t len){
    t_std_error rc = STD_ERR_OK;
    for(size_t ix = 0 ; ix < len ; ++ix){
        rc = sub_intf_attr_fn(intf_name,val[ix]);
        if(rc != STD_ERR_OK) break;
    }

    return rc;
}

t_std_error nas_interface_utils_set_all_sub_intf_attr(const std::string & parent_intf,  nas_com_id_value_t  val[],
                                                      size_t len){

    NAS_INTERFACE *obj = nullptr;
    if((obj = nas_interface_map_obj_get(parent_intf)) == nullptr){
        EV_LOGGING(INTERFACE,ERR,"SUB-INTF-ATTR-SET","No Interface %s exsist in the map",parent_intf.c_str());
        return STD_ERR(INTERFACE,PARAM,0);
    }

    t_std_error rc = STD_ERR_OK;
    for(size_t ix = 0 ; ix < len ; ++ix){
        rc = obj->for_all_sub_intf(sub_intf_attr_fn,val[ix]);
        if(rc != STD_ERR_OK) break;
    }

    return rc;
}
