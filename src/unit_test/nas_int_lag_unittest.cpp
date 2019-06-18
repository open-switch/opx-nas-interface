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
 * nas_int_lag_unittest.cpp
 *
 */

#include "cps_class_map.h"
#include "cps_api_object.h"
#include "cps_api_operation.h"
#include "cps_api_object_key.h"
#include "cps_api_object_category.h"
#include "cps_api_events.h"
#include "dell-base-if-lag.h"
#include "dell-base-if-vlan.h"
#include "dell-base-if.h"
#include "dell-interface.h"
#include "ietf-interfaces.h"
#include "iana-if-type.h"
#include "hal_interface_defaults.h"
#include "nas_ndi_obj_id_table.h"
#include "std_utils.h"

#include <gtest/gtest.h>
#include <iostream>
#include <net/if.h>

using namespace std;

TEST(std_lag_create_test, create_lag_intf_no_id)
{

    cps_api_object_t obj = cps_api_object_create();


    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
               DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

       cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));
    /* create bond without a ID in the name */
       const char *lag_name = "bond";
    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_NAME,lag_name,strlen(lag_name)+1);


    const char * mac_addr = "12:34:56:78:12:35";
    cps_api_object_attr_add(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS , mac_addr, strlen(mac_addr)+1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_create(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_object_t recvd_obj = cps_api_object_list_get(tr.change_list,0);

    cps_api_object_attr_t lag_attr = cps_api_get_key_data(recvd_obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);

    int ifindex = cps_api_object_attr_data_u32(lag_attr);

    cout <<"IF Index from Kernel is "<< ifindex << endl;

    cps_api_transaction_close(&tr);
}

TEST(std_lag_create_test, create_lag_intf)
{

    cps_api_object_t obj = cps_api_object_create();


    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
               DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

       cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

       const char *lag_name = "bond1";
    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_NAME,lag_name,strlen(lag_name)+1);


    const char * mac_addr = "12:34:56:78:12:34";
    cps_api_object_attr_add(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS , mac_addr, strlen(mac_addr)+1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_create(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_object_t recvd_obj = cps_api_object_list_get(tr.change_list,0);

    cps_api_object_attr_t lag_attr = cps_api_get_key_data(recvd_obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);

    int ifindex = cps_api_object_attr_data_u32(lag_attr);

    cout <<"IF Index from Kernel is "<< ifindex << endl;

    cps_api_transaction_close(&tr);
}

TEST(std_lag_state_test, get_lag_intf_state)
{
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);
    ASSERT_TRUE(obj != nullptr);

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj), DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_OBJ,
                                    cps_api_qualifier_OBSERVED);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
        (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
        sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    const char *lag_ifname = "bond1";

    cps_api_object_attr_add(obj,IF_INTERFACES_STATE_INTERFACE_NAME,lag_ifname,strlen(lag_ifname)+1);

    if (cps_api_get(&gp)==cps_api_ret_code_OK) {
        cps_api_object_t obj = cps_api_object_list_get(gp.list, 0);
        cps_api_object_attr_t name_attr = cps_api_get_key_data(obj, IF_INTERFACES_STATE_INTERFACE_NAME);
        const char *name = (const char*) cps_api_object_attr_data_bin(name_attr);
        ASSERT_TRUE(strlen(name) == strlen(lag_ifname));
        ASSERT_TRUE(strncmp(name, lag_ifname, strlen(lag_ifname)) == 0);
    }

    cps_api_get_request_close(&gp);
}

TEST(std_set_mac, set_mac_lag)
{

    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
            DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
        (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
        sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    const char *lag_name = "bond1";
    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
                lag_name,strlen(lag_name)+1);

    const char * mac_addr = "12:34:56:78:12:34";
    cps_api_object_attr_add(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS , mac_addr, strlen(mac_addr)+1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);

}

TEST(std_set_desc, set_desc_lag)
{

    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
            DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
        (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
        sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    const char *lag_name = "bond1";
    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
                lag_name,strlen(lag_name)+1);

    const char *desc = "bond1 interface description";
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_DESCRIPTION, desc, strlen(desc) + 1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);

}

TEST(std_set_mtu, set_mtu_lag)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
            DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
        (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
        sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    const char *lag_name = "bond1";
       cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
                   lag_name,strlen(lag_name)+1);

    uint_t mtu=2000;

    cps_api_object_attr_add_u32(obj,DELL_IF_IF_INTERFACES_INTERFACE_MTU,mtu);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);

}

TEST(std_set_admin_up,set_admin_lag_up)
{

    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
        DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

       cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

       const char *lag_name = "bond1";
       cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
               lag_name,strlen(lag_name)+1);

    bool state = true;
    cps_api_object_attr_add_u32(obj,IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS,state);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);

}

TEST(std_set_admin_down, set_admin_lag_down)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
        (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
        sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    const char *lag_name = "bond1";
    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
            lag_name,strlen(lag_name)+1);

    bool state = false;
    cps_api_object_attr_add_u32(obj,IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS,state);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);

}

TEST(std_all_ports_del, delete_all_ports_from_lag)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
       DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

       cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

       const char *lag_name = "bond1";
       cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
               lag_name,strlen(lag_name)+1);

    cps_api_object_attr_add(obj,DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS, 0,0);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);
    cps_api_transaction_close(&tr);
}

TEST(std_lag_add_port_test, add_ports_to_lag)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
           DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
        (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
        sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    const char *lag_name = "bond1";
    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
            lag_name,strlen(lag_name)+1);

    cps_api_attr_id_t ids[3] = {DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS,0,
                                DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS_NAME };
    const int ids_len = sizeof(ids)/sizeof(ids[0]);
    cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_BIN,"e101-001-0",strlen("e101-001-0")+1);
    ids[1]++;
    cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_BIN,"e101-002-0",strlen("e101-001-0")+1);
    ids[1]++;
    cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_BIN,"e101-003-0",strlen("e101-001-0")+1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_create(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);
}

TEST(std_lag_add_port_test, add_ports_check_mac)
{
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);
    ASSERT_TRUE(obj != nullptr);

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
            DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
        (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
        sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    const char *lag_ifname = "bond1";
    const char *test_mac_addr = "12:34:56:78:12:34";

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_NAME,lag_ifname,strlen(lag_ifname)+1);

    if (cps_api_get(&gp)==cps_api_ret_code_OK) {
        cps_api_object_t obj = cps_api_object_list_get(gp.list, 0);
        cps_api_object_attr_t mac_attr = cps_api_get_key_data(obj, DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS);
        const char *mac = (const char*) cps_api_object_attr_data_bin(mac_attr);
        ASSERT_TRUE(strlen(mac) == strlen(test_mac_addr));
        ASSERT_TRUE(strncmp(mac, test_mac_addr, strlen(test_mac_addr)) == 0);
    }

    cps_api_get_request_close(&gp);
}

TEST(std_lag_block_ports, block_ports_to_lag)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
              DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

       cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

       const char *lag_name = "bond1";
       cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
               lag_name,strlen(lag_name)+1);

       cps_api_object_attr_add(obj,BASE_IF_LAG_IF_INTERFACES_INTERFACE_BLOCK_PORT_LIST,"e101-002-0",strlen("e101-002-0")+1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);
}

TEST(std_lag_block_ports, unblock_ports_to_lag)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
              DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

       cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

       const char *lag_name = "bond1";
       cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
               lag_name,strlen(lag_name)+1);

       cps_api_object_attr_add(obj,BASE_IF_LAG_IF_INTERFACES_INTERFACE_UNBLOCK_PORT_LIST,"e101-002-0",strlen("e101-002-0")+1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);
}

TEST(std_lag_block_ports, block_and_unblock_ports_to_lag)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
              DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

       cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

       const char *lag_name = "bond1";
       cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
               lag_name,strlen(lag_name)+1);

       cps_api_object_attr_add(obj,BASE_IF_LAG_IF_INTERFACES_INTERFACE_BLOCK_PORT_LIST,"e101-001-0",strlen("e101-001-0")+1);
       cps_api_object_attr_add(obj,BASE_IF_LAG_IF_INTERFACES_INTERFACE_UNBLOCK_PORT_LIST,"e101-002-0",strlen("e101-002-0")+1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);
}

TEST(std_lag_block_ports, verify_block_unblock_ports)
{
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);
    ASSERT_TRUE(obj != nullptr);

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj), DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_OBJ,
                                    cps_api_qualifier_OBSERVED);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
        (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
        sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    const char *lag_ifname = "bond1";
    cps_api_object_attr_add(obj,IF_INTERFACES_STATE_INTERFACE_NAME,lag_ifname,strlen(lag_ifname)+1);

    if (cps_api_get(&gp)==cps_api_ret_code_OK) {
        cps_api_object_t obj = cps_api_object_list_get(gp.list, 0);
        cps_api_object_attr_t blocked_ports_attr = cps_api_get_key_data(obj, BASE_IF_LAG_IF_INTERFACES_INTERFACE_BLOCK_PORT_LIST);
        cps_api_object_attr_t unblocked_ports_attr = cps_api_get_key_data(obj, BASE_IF_LAG_IF_INTERFACES_INTERFACE_UNBLOCK_PORT_LIST);
        const char *blocked_ports = (const char*) cps_api_object_attr_data_bin(blocked_ports_attr);
        const char *unblocked_ports = (const char*) cps_api_object_attr_data_bin(unblocked_ports_attr);
        ASSERT_TRUE(strlen(blocked_ports) == strlen("e101-001-0"));
        ASSERT_TRUE(strlen(unblocked_ports) == strlen("e101-002-0"));
        ASSERT_TRUE(strncmp(blocked_ports, "e101-001-0", strlen(blocked_ports)) == 0);
        ASSERT_TRUE(strncmp(unblocked_ports, "e101-002-0", strlen(unblocked_ports)) == 0);
    }

    cps_api_get_request_close(&gp);
}

TEST(std_lag_with_vlan, add_lag_to_vlan)
{
    // Create VLAN
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));
    cps_api_object_attr_add_u32(obj,BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID,100);
    const char * mac =  "00:11:11:11:11:11";
    cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS, mac, strlen(mac) + 1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_create(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_object_t recvd_obj = cps_api_object_list_get(tr.change_list,0);

    cps_api_object_attr_t vlan_attr = cps_api_object_attr_get(recvd_obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
    int ifindex = cps_api_object_attr_data_u32(vlan_attr);
    cout <<"VLAN is created, ifindex = "<< ifindex << endl;

    cps_api_transaction_close(&tr);

    // Add LAG to VLAN
    obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));
    const char *br_name = "br100";
    cps_api_set_key_data(obj, IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                             br_name, strlen(br_name) + 1 );
    const char *lag_name = "bond1";
    cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS, lag_name,
                            strlen(lag_name) + 1);

    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);
}

void nas_print_vlan_object(cps_api_object_t obj)
{
    cps_api_object_it_t it;
    cps_api_object_it_begin(obj,&it);

    for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it) ) {
        int id = (int) cps_api_object_attr_id(it.attr);
        switch(id) {
        case DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX:
            std::cout<<"VLAN INDEX "<<cps_api_object_attr_data_u32(it.attr)<<std::endl;
            break;
        case BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID:
            std::cout<<"VLAN ID "<<cps_api_object_attr_data_u32(it.attr)<<std::endl;
            break;

        case IF_INTERFACES_INTERFACE_NAME:
            printf("Name %s \n", (char *)cps_api_object_attr_data_bin(it.attr));
            break;

        case DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS:
            printf("Tagged Port %s \n", (char *)cps_api_object_attr_data_bin(it.attr));
            break;

        case DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS:
            printf("Untagged Port %s \n", (char *)cps_api_object_attr_data_bin(it.attr));
            break;
        case DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS:
            printf("MAC Address %s \n", (char *)cps_api_object_attr_data_bin(it.attr));
            break;

        case IF_INTERFACES_INTERFACE_ENABLED:
            std::cout<<"Admin Status "<< cps_api_object_attr_data_u32(it.attr)<<std::endl;
            break;

        case DELL_IF_IF_INTERFACES_INTERFACE_LEARNING_MODE:
            std::cout<<"Learning mode "<<cps_api_object_attr_data_u32(it.attr)<<std::endl;
            break;

        default :
            break;

        }
    }
}

TEST(std_lag_with_vlan, print_lag_in_vlan)
{
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);
    ASSERT_TRUE(obj != nullptr);

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj), DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));
    cps_api_object_attr_add_u32(obj, BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, 100);

    if (cps_api_get(&gp)==cps_api_ret_code_OK) {
        size_t mx = cps_api_object_list_size(gp.list);
        for (size_t ix = 0 ; ix < mx ; ++ix ) {
            cps_api_object_t obj = cps_api_object_list_get(gp.list,ix);
            std::cout<<"VLAN Information"<<std::endl;
            nas_print_vlan_object(obj);
        }
    }
    cps_api_get_request_close(&gp);
}

TEST(std_lag_with_vlan, remove_lag_from_vlan)
{
    // Remove LAG from VLAN
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

    const char *br_name = "br100";
    int index = if_nametoindex(br_name);

    cps_api_set_key_data(obj, IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                             br_name, strlen(br_name) + 1 );

    index = 0;
    size_t len = 0;
    cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS,
                            &index, len);


    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);

    // Delete VLAN
    obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                        DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                        cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

    cps_api_set_key_data(obj, IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                             br_name, strlen(br_name) + 1 );

    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_delete(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);
    cps_api_transaction_close(&tr);
}

TEST(std_lag_unblock_ports,unblock_ports_to_lag)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
            DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
        (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
        sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    const char *lag_name = "bond1";
    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
            lag_name,strlen(lag_name)+1);

    cps_api_object_attr_add(obj,BASE_IF_LAG_IF_INTERFACES_INTERFACE_UNBLOCK_PORT_LIST,"e101-003-0",strlen("e101-003-0")+1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);
}

TEST(std_lag_delete_port_test, delete_ports_from_lag)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
        DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
        (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
        sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    const char *lag_name = "bond1";
    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
            lag_name,strlen(lag_name)+1);

    cps_api_attr_id_t ids[3] = {DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS,0,
                                DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS_NAME };
    const int ids_len = sizeof(ids)/sizeof(ids[0]);
    cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_BIN,"e101-001-0",strlen("e101-001-0")+1);
    ids[1]++;
    cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_BIN,"e101-002-0",strlen("e101-001-0")+1);
    ids[1]++;
    cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_BIN,"e101-003-0",strlen("e101-001-0")+1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_delete(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);
}

TEST(std_lag_delete_test, delete_lag_intf)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
        DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
        (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
        sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    const char *lag_name = "bond1";
    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
            lag_name,strlen(lag_name)+1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_delete(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);
    cps_api_transaction_close(&tr);
}

TEST(std_lag_delete_test, delete_lag_intf_no_id)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
        DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
        (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
        sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    const char *lag_name = "bond";
    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
            lag_name,strlen(lag_name)+1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_delete(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);
    cps_api_transaction_close(&tr);
}

TEST(std_lag_delete_test, delete_lag_intf_with_ports)
{
    /** Create the LAG inter **/
    cps_api_object_t obj = cps_api_object_create();


    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
               DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    const char *lag_name = "bond1";
    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_NAME,lag_name,strlen(lag_name)+1);


    const char *mac_addr = "12:34:56:78:12:34";
    cps_api_object_attr_add(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS , mac_addr, strlen(mac_addr)+1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_create(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);
    cps_api_transaction_close(&tr);

    /** Add some member ports **/
    obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
           DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
        (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
        sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
            lag_name,strlen(lag_name)+1);

    cps_api_attr_id_t ids[3] = {DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS,0,
                                DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS_NAME };
    const int ids_len = sizeof(ids)/sizeof(ids[0]);
    cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_BIN,"e101-001-0",strlen("e101-001-0")+1);
    ids[1]++;
    cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_BIN,"e101-002-0",strlen("e101-001-0")+1);
    ids[1]++;
    cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_BIN,"e101-003-0",strlen("e101-001-0")+1);

    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);
    cps_api_transaction_close(&tr);

    /** Delete the interface **/
    obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
           DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
        (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
        sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
            lag_name,strlen(lag_name)+1);
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_delete(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);
    cps_api_transaction_close(&tr);
}

void nas_lag_print_memeber_ports(cps_api_object_t obj, const cps_api_object_it_t & it){
    cps_api_object_it_t it_lvl_1 = it;
    cps_api_attr_id_t ids[3] = {DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS,0,
                                DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS_NAME };
    const int ids_len = sizeof(ids)/sizeof(ids[0]);

    for (cps_api_object_it_inside (&it_lvl_1); cps_api_object_it_valid (&it_lvl_1);
         cps_api_object_it_next (&it_lvl_1)) {

        ids[1] = cps_api_object_attr_id (it_lvl_1.attr);
        cps_api_object_attr_t intf = cps_api_object_e_get(obj,ids,ids_len);

        if(intf){
            cout<<cps_api_object_attr_data_bin(intf)<<endl;
        }
    }
}

void nas_print_lag_object(cps_api_object_t obj){
    cps_api_object_it_t it;
    cps_api_object_it_begin(obj,&it);

    cout << "\nLag:- Index\tID\tName\tslaves\t\t SMAC \t\t\tAStatus \t\tNPU-ID/NDI-SAI-ID\n\t";
    for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it) ) {
        int id = (int) cps_api_object_attr_id(it.attr);
        switch(id) {
            case DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX:
                cout<<cps_api_object_attr_data_u32(it.attr)<<'\t';
                break;
            case BASE_IF_LAG_IF_INTERFACES_INTERFACE_ID:
                cout<<cps_api_object_attr_data_u32(it.attr)<<'\t';
                break;
            case IF_INTERFACES_INTERFACE_NAME:
                cout<<(char *)cps_api_object_attr_data_bin(it.attr)<<'\t';
                break;
            case DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS:
                nas_lag_print_memeber_ports(obj,it);
                break;
            case DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS:
                cout<<cps_api_object_attr_data_bin(it.attr)<<'\t';
                break;
            case IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS:
                 cout<<(cps_api_object_attr_data_u32(it.attr) ? "DOWN":"UP");
                 cout<<"\t\t";
                break;
            case BASE_IF_LAG_IF_INTERFACES_INTERFACE_LAG_OPAQUE_DATA:
                {
                    nas::ndi_obj_id_table_t lag_opaque_data_table;
                    cps_api_attr_id_t  attr_id_list[] = {BASE_IF_LAG_IF_INTERFACES_INTERFACE_LAG_OPAQUE_DATA};
                    nas::ndi_obj_id_table_cps_unserialize (lag_opaque_data_table, obj, attr_id_list,
                            sizeof(attr_id_list)/sizeof(attr_id_list[0]));
                    auto it = lag_opaque_data_table.begin();
                    cout<<"\t";
                    cout<<it->first<<"/" <<it->second;
                    cout<<'\n';
                }
                break;
            default :
                break;

        }
    }
    fflush(stdout);
}


TEST(std_lag_get, get_lags_created)
{
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

       cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));


    if (cps_api_get(&gp)==cps_api_ret_code_OK) {
        size_t mx = cps_api_object_list_size(gp.list);
        for (size_t ix = 0 ; ix < mx ; ++ix ) {
            cps_api_object_t obj = cps_api_object_list_get(gp.list,ix);
            cout<<"Inside loop"<<endl;
            nas_print_lag_object(obj);
        }
        cout<<"\n";
    }
    cps_api_get_request_close(&gp);

}

TEST(std_lag_opaque_get, get_lags_opaque)
{
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);

     cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj), DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
            cps_api_qualifier_TARGET);

     cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG));

    uint64_t ndi_lag_id= 562949953421312;
    cps_api_object_attr_add_u64(obj,BASE_IF_LAG_IF_INTERFACES_INTERFACE_LAG_OPAQUE_DATA,ndi_lag_id);

    if (cps_api_get(&gp)==cps_api_ret_code_OK) {
        size_t mx = cps_api_object_list_size(gp.list);
        for (size_t ix = 0 ; ix < mx ; ++ix ) {
            cps_api_object_t obj = cps_api_object_list_get(gp.list,ix);
            nas_print_lag_object(obj);
        }
        cout<<"\n";
    }
    cps_api_get_request_close(&gp);
}

#if 0
static bool lag_obj_event_cb(cps_api_object_t obj, void *param)
{

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if( op == cps_api_oper_CREATE){
        cout << "Operation Type :- Add Lag "<< endl;
    }
    if(op == cps_api_oper_DELETE){
        cout << "Operation Type :- Delete Lag  "<< endl;
    }
    if(op == cps_api_oper_SET){
        cout << "Operation Type :- Ports Add/Del. "<< endl;
    }

    nas_print_lag_object(obj);
    return true;
}

TEST(std_lag_event_rx_test, get_lag_events)
{
    /*  register for lag object */

    // register for events
    cps_api_event_reg_t reg;
    memset(&reg,0,sizeof(reg));

    if (cps_api_event_service_init() != cps_api_ret_code_OK) {
        printf("***ERROR*** cps_api_event_service_init() failed\n");
        return ;
    }

    if (cps_api_event_thread_init() != cps_api_ret_code_OK) {
        printf("***ERROR*** cps_api_event_thread_init() failed\n");
        return;
    }

    cps_api_key_t key;

    cps_api_key_from_attr_with_qual(&key, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
            cps_api_qualifier_OBSERVED);

    reg.number_of_objects = 1;
    reg.objects = &key;

    if (cps_api_event_thread_reg(&reg,lag_obj_event_cb,NULL)!=cps_api_ret_code_OK) {
        cout << " registration failure"<<endl;
        return;
    }

    while(1);
    // infinite loop
}
#endif


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}


