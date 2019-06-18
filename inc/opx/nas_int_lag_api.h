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
 * filename: nas_int_lag_api.h
 */


#ifndef NAS_INTF_LAG_API_H_
#define NAS_INTF_LAG_API_H_

#include "cps_api_operation.h"
#include "ds_common_types.h"
#include "nas_ndi_common.h"
#include "std_mutex_lock.h"
#include "ietf-interfaces.h"
#include "dell-base-if-phy.h"

#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <unordered_set>


#define NAS_IFF_LAG_SLAVE 0x800
#define NAS_IFF_LAG_MASTER 0x400
#define NAS_IFF_UP 0x1
#define NAS_IFF_RUNNING 0x40

#define is_master(if_flags)   (if_flags&NAS_IFF_LAG_MASTER)
#define is_slave(if_flags)    (if_flags&NAS_IFF_LAG_SLAVE)
#define is_intf_up(if_flags)   (if_flags&NAS_IFF_UP)



/*
 * nas_lag_port_list_t
*/
typedef std::unordered_set<hal_ifindex_t> nas_lag_port_list_t;

typedef std::unordered_map<hal_ifindex_t,bool> nas_lag_port_oper_status_t;


/** @struct nas_lag_master_info_t
 *  @brief NAS Lag info structure
 */

typedef struct nas_lag_s{
    hal_ifindex_t ifindex;   //Lag ifindex reference in NAS
    nas_lag_id_t lag_id;     //lag ID reference
    std::string mac_addr; //MAC address of this lag
    nas_lag_port_list_t port_list; // List of port ifindex associated with lag
    nas_lag_port_oper_status_t port_oper_list;
    nas_lag_port_list_t block_port_list; /* list of LAG member ports which are
                                          Egress disabled, these are used in LACP
                                          case, till the port gets aggregated
                                          after aggrgate, on unblock from app
                                          (IFM) the port gets removed and Egress
                                          disable will be reset */
    ndi_obj_id_t ndi_lag_id; // ndi lag ID
    bool admin_status;
    char name[HAL_IF_NAME_SZ]; //Just store it for now.
    BASE_IF_PHY_MAC_LEARN_MODE_t mac_learn_mode;
    bool mac_learn_mode_set = false;
    bool oper_status = false;
}nas_lag_master_info_t;

using master_ifindex = hal_ifindex_t ;

/*
* Map to maintain master info to master idx mapping
*/

using nas_lag_master_table_t = std::unordered_map <master_ifindex, nas_lag_master_info_t> ;

/**
 * @brief Create and insert the lag details in a RB tree
 *
 * @param index -   ifindex of the lag
 *
 * @param if_name - interface name of the lag
 *
 * @param lag_id - lag_id used to map with other layers.
 *
 * @return pointer to the lag
 */

t_std_error nas_lag_master_add(hal_ifindex_t index,const char *if_name,nas_lag_id_t lag_id);

/**
 * @brief Handle the lag interface delete request
 *
 * @param if_index -   ifindex of the lag interface
 *
 * @return STD_ERR or STD_ERR_OK
 */

t_std_error nas_lag_master_delete(hal_ifindex_t index);

/**
 * @brief Add member to LAG
 *
 * @param lag_master_id - ifindex of the lag
 *
 * @param if_index -   ifindex of the lag interface
 *
 * @param lag_id - lag_id used to map with other layers.
 *
 * @return STD_ERR or STD_ERR_OK
 */

t_std_error nas_lag_member_add(hal_ifindex_t lag_master_id,hal_ifindex_t if_index);

/**
 * @brief Delete member from LAG
 *
 * @param lag_master_id - ifindex of the lag
 *
 * @param if_index -   ifindex of the lag interface
 *
 * @param lag_id - lag_id used to map with other layers.
 *
 * @return STD_ERR or STD_ERR_OK
 */

t_std_error nas_lag_member_delete(hal_ifindex_t lag_master_id,hal_ifindex_t if_index);

/**
 *  Check if a port is a member of bond.
 */
bool nas_lag_if_port_is_lag_member(hal_ifindex_t lag_master_id, hal_ifindex_t ifindex);
/**
 * @brief init nas_lag rb tree.
*/

t_std_error nas_init_lag(void);


/**
 * @brief Utility to use simple mutex lock for lag resources
 *        access across multiple threads.
 */

std_mutex_type_t  *nas_lag_mutex_lock();


nas_lag_master_info_t *nas_get_lag_node(hal_ifindex_t index);
nas_lag_master_table_t & nas_get_lag_table(void);
t_std_error nas_lag_set_desc(hal_ifindex_t index,const char *desc);
t_std_error nas_lag_set_mac(hal_ifindex_t index,const char *lag_mac);
t_std_error nas_lag_set_admin_status(hal_ifindex_t index, bool enable);
t_std_error nas_lag_block_port(nas_lag_master_info_t  *p_lag_info ,hal_ifindex_t slave_ifindex,bool block_state);
t_std_error nas_lag_get_port_mode(hal_ifindex_t slave_ifindex,bool& block_state);
hal_ifindex_t nas_get_master_idx(hal_ifindex_t ifindex);
void nas_cps_handle_mac_set (const char *lag_name, hal_ifindex_t lag_index);

cps_api_return_code_t lag_object_publish(nas_lag_master_info_t *nas_lag_entry,hal_ifindex_t lag_idx,
        cps_api_operation_types_t op);

t_std_error nas_lag_get_ndi_lag_id(hal_ifindex_t lag_index, ndi_obj_id_t *ndi_lag_id);


#endif /* NAS_INTF_LAG_API_H__ */

