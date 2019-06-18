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
 * filename: nas_int_lag_api.cpp
 */



#include "nas_int_lag.h"
#include "hal_if_mapping.h"
#include "nas_if_utils.h"
#include "nas_int_lag_api.h"
#include "std_mutex_lock.h"
#include "event_log.h"
#include "std_utils.h"
#include "dell-base-if-lag.h"
#include "dell-base-if.h"
#include "dell-interface.h"
#include "nas_ndi_port.h"
#include "nas_int_lag_cps.h"
#include "nas_ndi_lag.h"
#include "nas_switch.h"


/** struct nas_lag_slave_info_t
 *   NAS Lag slave info structure
 */
typedef struct {
    hal_ifindex_t ifindex;
    hal_ifindex_t master_idx;
    ndi_obj_id_t ndi_lag_member_id;
}nas_lag_slave_info_t;

typedef hal_ifindex_t  slave_ifindex;

/*
 * Map to maintain slave info to slave idx mapping
 */

using nas_lag_slave_table_t = std::unordered_map <slave_ifindex , nas_lag_slave_info_t> ;

/*
 * Table to maintain slave info
 */
static auto nas_lag_slave_table = new nas_lag_slave_table_t;


/*
 * Table to maintain master info
 */
auto nas_lag_master_table = new nas_lag_master_table_t;


static std_mutex_lock_create_static_init_rec(lag_lock);

std_mutex_type_t *nas_lag_mutex_lock()
{
    return &lag_lock;
}

t_std_error nas_add_slave_node(hal_ifindex_t lag_master_id,hal_ifindex_t ifindex,
        ndi_obj_id_t ndi_lag_member_id){

    EV_LOGGING(INTERFACE, INFO, "NAS-LAG","LagID %d Ifindex %d, lag mem id %lu",
               lag_master_id, ifindex, ndi_lag_member_id);

    nas_lag_slave_table->insert({ifindex ,{ ifindex, lag_master_id ,ndi_lag_member_id}});
    return STD_ERR_OK;
}


t_std_error nas_remove_slave_node(hal_ifindex_t ifindex)
{
    auto slave_table_it = nas_lag_slave_table->find(ifindex);
    if (slave_table_it != nas_lag_slave_table->end()) {
        nas_lag_slave_table->erase(slave_table_it);
    }
    return STD_ERR_OK;
}


nas_lag_slave_info_t *nas_get_slave_node(hal_ifindex_t ifindex)
{
    auto slave_table_it = nas_lag_slave_table->find(ifindex);
    if (slave_table_it != nas_lag_slave_table->end()) {
        nas_lag_slave_info_t & slave_entry = slave_table_it->second;
        return &slave_entry;
    }
    return NULL;
}

hal_ifindex_t nas_get_master_idx(hal_ifindex_t ifindex){

    nas_lag_slave_info_t *nas_slave_entry = NULL;

    // Delete netlink doesn't provide master index
    // retrive it from slave idx
    nas_slave_entry = nas_get_slave_node (ifindex);
    if(nas_slave_entry == NULL){
        return -1;
    }
    return (nas_slave_entry->master_idx);
}

t_std_error nas_remove_all_slave_node(nas_lag_master_info_t *nas_lag_entry)
{
    for (auto it=nas_lag_entry->port_list.begin(); it!=nas_lag_entry->port_list.end(); ++it) {
        if(nas_remove_slave_node(*it) != STD_ERR_OK){
            return STD_ERR(INTERFACE,FAIL, 0);
        }
    }
    return STD_ERR_OK;
}

void nas_lag_entry_insert(nas_lag_master_info_t &master_entry)
{
    nas_lag_master_table->insert({master_entry.ifindex, master_entry});
}


t_std_error nas_lag_entry_erase(hal_ifindex_t ifindex)
{
    auto master_table_it = nas_lag_master_table->find(ifindex);

    if (master_table_it != nas_lag_master_table->end()) {
        nas_lag_master_table->erase(master_table_it);
    }else {
        EV_LOGGING(INTERFACE, ERR, "NAS-LAG","Invalid Lag Index %d", ifindex);
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    return STD_ERR_OK;
}


nas_lag_master_info_t *nas_get_lag_node(hal_ifindex_t ifindex)
{
    auto master_table_it = nas_lag_master_table->find(ifindex);
    if (master_table_it != nas_lag_master_table->end()) {
        nas_lag_master_info_t & master_entry = master_table_it->second;
        EV_LOG_INFO(ev_log_t_INTERFACE,3, "NAS-LAG", "%d Lag found ",ifindex);
        return &master_entry;
    }
    EV_LOGGING(INTERFACE, INFO, "NAS-LAG", "No Lag Found %d", ifindex);
    return NULL;
}


nas_lag_master_table_t & nas_get_lag_table(void)
{
    return *nas_lag_master_table;
}


static bool nas_lag_exist(const char *bond_name)
{
    interface_ctrl_t intf_ctrl;
    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));
    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    safestrncpy(intf_ctrl.if_name, bond_name, sizeof(intf_ctrl.if_name));

    if (dn_hal_get_interface_info(&intf_ctrl) != STD_ERR_OK) {
        return false;
    }
    return true;
}


static bool nas_lag_intf_to_port(hal_ifindex_t ifindex, interface_ctrl_t *intf_ctrl) {
    memset(intf_ctrl, 0, sizeof(interface_ctrl_t));
    intf_ctrl->q_type = HAL_INTF_INFO_FROM_IF;
    intf_ctrl->if_index = ifindex;

    if (dn_hal_get_interface_info(intf_ctrl) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR,"NAS-LAG","Interface %d has NO slot %d, port %d",
                   intf_ctrl->if_index, intf_ctrl->npu_id, intf_ctrl->port_id);
        return false;
    }
    return true;
}

bool nas_lag_if_port_is_lag_member(hal_ifindex_t lag_master_id, hal_ifindex_t ifindex) {

    nas_lag_slave_info_t *slave_entry = NULL;
    slave_entry = nas_get_slave_node (ifindex);
    if (slave_entry == NULL) {
        return false;
    }
    if (slave_entry->master_idx != lag_master_id) {
        EV_LOGGING(INTERFACE, ERR,"NAS-LAG",
            "Slave and master records inconsistent: slave port %d master id %d, slave masterid %d",
                ifindex, lag_master_id, slave_entry->master_idx);
        return false;

    }
    return true;

}


t_std_error nas_lag_member_add(hal_ifindex_t lag_master_id,hal_ifindex_t ifindex)
{
    nas_lag_master_info_t *nas_lag_entry = NULL;
    t_std_error ret = STD_ERR_OK;
    nas_obj_id_t ndi_lag_member_id;
    ndi_port_t nas_lag_ndi_port;

    nas_lag_entry = nas_get_lag_node(lag_master_id);
    if(nas_lag_entry == NULL){
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    interface_ctrl_t intf_ctrl;
    if (!nas_lag_intf_to_port(ifindex, &intf_ctrl)) {
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    nas_lag_ndi_port.npu_id = intf_ctrl.npu_id;
    nas_lag_ndi_port.npu_port = intf_ctrl.port_id;

    // Add port to NPU LAG
    if(nas_add_port_to_lag(nas_lag_ndi_port.npu_id,nas_lag_entry->ndi_lag_id,
                &nas_lag_ndi_port,&ndi_lag_member_id) != STD_ERR_OK){
        return  STD_ERR(INTERFACE,FAIL, 0);
    }

    // Adding master NDI id to slave list.
    if(nas_add_slave_node(lag_master_id,ifindex,ndi_lag_member_id) != STD_ERR_OK){
        return  STD_ERR(INTERFACE,FAIL, 0);
    }


    ndi_intf_link_state_t state;
    if ((ndi_port_link_state_get(intf_ctrl.npu_id,intf_ctrl.port_id, &state))
                                   == STD_ERR_OK) {
        nas_lag_entry->port_oper_list[ifindex]= (state.oper_status ==ndi_port_OPER_UP) ? true : false;
        if (!nas_lag_entry->oper_status && nas_lag_entry->port_oper_list[ifindex]) {
            nas_lag_entry->oper_status = true;
        }
    }

    return ret;
}



t_std_error nas_lag_member_delete(hal_ifindex_t lag_master_id,hal_ifindex_t ifindex)
{
    nas_lag_master_info_t *nas_lag_entry= NULL;
    nas_lag_slave_info_t *nas_slave_entry = NULL;
    t_std_error ret = STD_ERR_OK;

    if(lag_master_id < 0)
        return STD_ERR(INTERFACE,FAIL, 0);

    //Retrive Master index from slave DS
    //netlink only gives slave Idx
    nas_slave_entry = nas_get_slave_node (ifindex);
    if(nas_slave_entry == NULL){
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    nas_lag_entry = nas_get_lag_node(nas_slave_entry->master_idx);

    if(nas_lag_entry == NULL){
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    ndi_port_t nas_lag_ndi_port;

    interface_ctrl_t intf_ctrl;
    if (!nas_lag_intf_to_port(ifindex, &intf_ctrl)) {
        return  STD_ERR(INTERFACE,FAIL, 0);
    }

    nas_lag_ndi_port.npu_id = intf_ctrl.npu_id;
    nas_lag_ndi_port.npu_port= intf_ctrl.port_id;

    EV_LOGGING(INTERFACE, INFO, "NAS-Lag", "Deleting LAG MEM ID %lu",
               nas_slave_entry->ndi_lag_member_id);
    if(nas_del_port_from_lag(nas_lag_ndi_port.npu_id,
                nas_slave_entry->ndi_lag_member_id) != STD_ERR_OK){
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    if(nas_remove_slave_node(ifindex) != STD_ERR_OK){
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    return ret;
}

t_std_error nas_register_lag_intf(nas_lag_master_info_t *nas_lag_entry, hal_intf_reg_op_type_t op)
{
    interface_ctrl_t details;

    EV_LOGGING(INTERFACE, DEBUG, "NAS-Lag", "Registering LAG %d %d with ifCntrl",
               nas_lag_entry->ifindex, nas_lag_entry->lag_id);
    memset(&details,0,sizeof(details));
    details.q_type = HAL_INTF_INFO_FROM_IF;
    details.if_index = nas_lag_entry->ifindex;
    details.lag_id = nas_lag_entry->ndi_lag_id;
    details.int_type = nas_int_type_LAG;
    details.desc = nullptr;
    strncpy(details.if_name, nas_lag_entry->name, sizeof(details.if_name)-1);

    if (dn_hal_if_register(op, &details)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-LAG",
                   "LAG Not registered with ifCntrl %d:%d - mapping error",
                   nas_lag_entry->ifindex, nas_lag_entry->lag_id);
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;
}

t_std_error nas_lag_master_add(hal_ifindex_t index,const char *if_name,
                               nas_lag_id_t lag_id)
{
    nas_lag_master_info_t nas_lag_entry;
    t_std_error rc = STD_ERR_OK;
    nas_obj_id_t ndi_lag_id;
    npu_id_t npu = 0;   //@TODO to retrive NPU ID in multi npu case


    if(nas_lag_exist(if_name)){
        EV_LOGGING(INTERFACE, INFO, "NAS-LAG", "Lag exists index %d name %s ", index, if_name);
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    // Create Lag in NPU
    if(nas_lag_create(npu, &ndi_lag_id) != STD_ERR_OK){
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    /* only add resilient hash attribute if supported by platform */
    if (nas_switch_resilient_hash_lag_supported()) {
        ndi_set_lag_resilient_hash(npu, ndi_lag_id, nas_lag_hash_value_get());
    }
   
    nas_lag_entry.ifindex = index;
    nas_lag_entry.lag_id = lag_id;
    nas_lag_entry.ndi_lag_id = ndi_lag_id;
    safestrncpy(nas_lag_entry.name, if_name, sizeof(nas_lag_entry.name));
    nas_lag_entry.admin_status = false;

    nas_lag_entry_insert(nas_lag_entry);

    if(nas_register_lag_intf(&nas_lag_entry, HAL_INTF_OP_REG) != STD_ERR_OK) {
        return (STD_ERR(INTERFACE,FAIL, 0));
    }

    EV_LOGGING(INTERFACE, INFO, "NAS-LAG", "Object Publish for index %x",index);
    // publish CPS_LAG Create
    if(lag_object_publish(&nas_lag_entry, index, cps_api_oper_CREATE)!= cps_api_ret_code_OK){
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                   "LAG events publish failure");
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    return rc;
}

t_std_error nas_lag_master_delete(hal_ifindex_t ifindex)
{
    nas_lag_master_info_t *nas_lag_entry=NULL;
    t_std_error rc = STD_ERR_OK;

    EV_LOGGING(INTERFACE, INFO, "NAS-LAG", "Lag intf %d for deletion", ifindex);


    nas_lag_entry = nas_get_lag_node(ifindex);

    if(nas_lag_entry == NULL){
        EV_LOGGING(INTERFACE, ERR, "NAS-LAG", "Lag intf %d Err in deletion",
                   ifindex);
        return STD_ERR(INTERFACE,FAIL, 0);
    }
    /* @TODO - NPU ID for Multi-npu case */
    if(nas_lag_delete(0, nas_lag_entry->ndi_lag_id) != STD_ERR_OK){
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    if(nas_register_lag_intf(nas_lag_entry, HAL_INTF_OP_DEREG) != STD_ERR_OK) {
        return (STD_ERR(INTERFACE,FAIL, 0));
    }

    EV_LOGGING(INTERFACE, INFO, "NAS-LAG", "Object Publish for index %x",
               ifindex);
    // publish CPS_LAG delete
    if(lag_object_publish(nas_lag_entry, ifindex, cps_api_oper_DELETE)!= cps_api_ret_code_OK){
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                   "LAG events publish failure");
        return STD_ERR(INTERFACE,FAIL, 0);
    }


    if(nas_remove_all_slave_node(nas_lag_entry)!= STD_ERR_OK){
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    if(nas_lag_entry_erase(ifindex) != STD_ERR_OK){
        return STD_ERR(INTERFACE,FAIL, 0);
    }



    return rc;
}

t_std_error nas_lag_set_desc(hal_ifindex_t index, const char *desc) {
    nas_lag_master_info_t *nas_lag_entry = NULL;
    interface_ctrl_t intf;

    EV_LOGGING(INTERFACE, INFO, "NAS-LAG", "Lag intf %d for set_desc", index);

    nas_lag_entry = nas_get_lag_node(index);

    if(nas_lag_entry == NULL){
        EV_LOGGING(INTERFACE, ERR, "NAS-LAG", "Lag intf %d Err in set_desc",
                   index);
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    memset(&intf, 0, sizeof(intf));

    if (!nas_lag_intf_to_port(nas_lag_entry->ifindex, &intf)) {
        EV_LOGGING(INTERFACE, ERR ,"NAS-LAG", "Error getting port info for LAG %d", nas_lag_entry->ifindex);
        return cps_api_ret_code_ERR;
    }

    intf.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    if (dn_hal_update_intf_desc(&intf, desc) != STD_ERR_OK)  {
        EV_LOGGING(INTERFACE, ERR ,"NAS-LAG", "Failure saving LAG %d desc in intf block", nas_lag_entry->ifindex);
        return cps_api_ret_code_ERR;
    }

    return STD_ERR_OK;
}


t_std_error nas_lag_set_mac(hal_ifindex_t index,const char *mac)
{
    nas_lag_master_info_t *nas_lag_entry = NULL;

    EV_LOGGING(INTERFACE, INFO, "NAS-LAG", "Lag intf %d for set_mac", index);

    nas_lag_entry = nas_get_lag_node(index);

    if(nas_lag_entry == NULL){
        EV_LOGGING(INTERFACE, ERR, "NAS-LAG", "Lag intf %d Err in set_mac",
                   index);
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    nas_lag_entry->mac_addr = mac;
    if (dn_hal_update_intf_mac(index, mac) != STD_ERR_OK)  {
        EV_LOGGING(INTERFACE, ERR ,"NAS-LAG", "Failure saving LAG %d MAC in intf block", index);
        return cps_api_ret_code_ERR;
    }

    return STD_ERR_OK;
}

t_std_error nas_lag_set_admin_status(hal_ifindex_t index, bool enable)
{
    nas_lag_master_info_t *nas_lag_entry = NULL;

    EV_LOGGING(INTERFACE, INFO, "NAS-LAG", "Lag intf %d for set_admin_status",
               index);

    nas_lag_entry = nas_get_lag_node(index);

    if(nas_lag_entry == NULL){
        EV_LOGGING(INTERFACE, ERR, "NAS-LAG",
                   "Lag intf %d Err in set_admin_statue", index);
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    nas_lag_entry->admin_status = enable;
    EV_LOGGING(INTERFACE, INFO, "NAS-LAG", "LAG Object Publish for index %x",
               index);
    if(lag_object_publish(nas_lag_entry,index,cps_api_oper_SET)!= cps_api_ret_code_OK){
        EV_LOGGING(INTERFACE,ERR,"NAS-CPS-LAG",
                   "Lag events publish failure");
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return STD_ERR_OK;
}

t_std_error nas_lag_block_port(nas_lag_master_info_t  *nas_lag_entry ,hal_ifindex_t slave_ifindex,
        bool block_state)
{

    nas_lag_slave_info_t *nas_slave_entry = NULL;
    EV_LOGGING(INTERFACE, INFO, "NAS-LAG",
               "Block/unblock port l_if %d if %d b %d",
               nas_lag_entry->ifindex, slave_ifindex,block_state);
    auto it_index = nas_lag_entry->port_list.find(slave_ifindex);
    if( it_index == nas_lag_entry->port_list.end()){
        EV_LOGGING(INTERFACE, INFO, "NAS-LAG", "%d Port does not exist",
                   slave_ifindex);
        return STD_ERR_OK;
    }

    ndi_port_t nas_lag_ndi_port;
    interface_ctrl_t intf_ctrl;
    if (!nas_lag_intf_to_port(slave_ifindex, &intf_ctrl)) {
        return (STD_ERR(INTERFACE,FAIL, 0));
    }

    nas_lag_ndi_port.npu_id = intf_ctrl.npu_id;
    nas_lag_ndi_port.npu_port= intf_ctrl.port_id;

    // Retrive ndi_lag_member_id
    nas_slave_entry = nas_get_slave_node (slave_ifindex);
    if(nas_slave_entry == NULL){
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    if(nas_set_lag_member_attr(nas_lag_ndi_port.npu_id,nas_slave_entry->ndi_lag_member_id,
                block_state) != STD_ERR_OK) {
        return (STD_ERR(INTERFACE,FAIL,0));
    }

    return STD_ERR_OK;
}

t_std_error nas_lag_get_port_mode(hal_ifindex_t slave_ifindex, bool& block_state)
{

    nas_lag_slave_info_t *nas_slave_entry = NULL;
    ndi_port_t nas_lag_ndi_port;
    interface_ctrl_t intf_ctrl;
    if (!nas_lag_intf_to_port(slave_ifindex, &intf_ctrl)) {
        return (STD_ERR(INTERFACE,FAIL, 0));
    }

    nas_lag_ndi_port.npu_id = intf_ctrl.npu_id;
    nas_lag_ndi_port.npu_port= intf_ctrl.port_id;

    // Retrive ndi_lag_member_id
    nas_slave_entry = nas_get_slave_node (slave_ifindex);
    if(nas_slave_entry == NULL){
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    if(nas_get_lag_member_attr(nas_lag_ndi_port.npu_id,nas_slave_entry->ndi_lag_member_id,
                &block_state) != STD_ERR_OK) {
        return (STD_ERR(INTERFACE,FAIL,0));
    }

    return STD_ERR_OK;
}

/* public API */
t_std_error nas_lag_get_ndi_lag_id(hal_ifindex_t lag_index, ndi_obj_id_t *ndi_lag_id)
{
    if (ndi_lag_id == NULL) {
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    nas_lag_master_info_t * lag_node = nas_get_lag_node(lag_index);
    if (lag_node == NULL) {
        return STD_ERR(INTERFACE,FAIL, 0);
    }
    *ndi_lag_id = lag_node->ndi_lag_id;
    return STD_ERR_OK;
}
