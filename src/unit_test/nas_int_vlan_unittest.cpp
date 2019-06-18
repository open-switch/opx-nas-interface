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
 * nas_int_vlan_unittest.cpp
 *
 */

#include "cps_api_object.h"
#include "cps_api_operation.h"
#include "cps_api_object_category.h"
#include "cps_api_events.h"
#include "hal_interface_defaults.h"
#include "cps_class_map.h"
#include "cps_api_object_key.h"
#include "dell-interface.h"
#include "dell-base-if.h"
#include "dell-base-if-vlan.h"
#include "iana-if-type.h"
#include "std_utils.h"

#include <gtest/gtest.h>
#include <iostream>
#include <net/if.h>

using namespace std;

void nas_print_vlan_object(cps_api_object_t obj){
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
            std::cout<<"Tagged Port "<<cps_api_object_attr_data_u32(it.attr)<<std::endl;
            break;

        case DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS:
            std::cout<<"Untagged Port "<<cps_api_object_attr_data_u32(it.attr)<<std::endl;
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

/* The next 3 cases use e101-005-0 which has ip assigned so the SAI fails to add it to a vlan hence roll_back is hit */
TEST(std_vlan_test , lag_add_roll_bk) {
/* DO SAME TEST WITH UNTAGGED PORTS TOO */
    if(system("ip link add name bo33 type bond miimon 100 mode 4 lacp_rate 1 min_links 1")){}
    if(system("ip link add name bo30 type bond miimon 100 mode 4 lacp_rate 1 min_links 1")){}
    if(system("ip link set e101-001-0 master bo33")){}
    if(system("ip link set e101-002-0 master bo33")){}
    if(system(" ip link set e101-003-0 master bo30")){}
    if(system("ip link set e101-004-0 master bo30")){}
    if(system("ifconfig e101-005-0 1.1.1.1/8")){}
    sleep (5);
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

    cps_api_object_attr_add_u32(obj,BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID,44);
    cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS,
             "bo30", strlen("bo30")+1);
    cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS,
             "e101-011-0", strlen("e101-011-0")+1);
    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_create(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);
    cps_api_object_t recvd_obj = cps_api_object_list_get(tr.change_list,0);

    cps_api_object_attr_t vlan_attr = cps_api_object_attr_get(recvd_obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);

    int ifindex = cps_api_object_attr_data_u32(vlan_attr);

    cout <<"Roll_back lag_add vlan id 44 ifindex %d "<< ifindex <<  endl;

    sleep(2);
    //system("hshell -c \"vlan show\"");
    //system("hshell -c \"trunk show\"");
    //system("brctl show");
    //system("ip link show | grep bo");
    //sleep(15);
    cps_api_object_t obj2 = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj2),
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add(obj2,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));


    const char *br_name = "br44";

    cps_api_set_key_data(obj2, IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                             br_name, strlen(br_name) + 1 );


    cps_api_object_attr_add(obj2, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS,
             "e101-005-0", strlen("e101-005-0")+1);
    cps_api_object_attr_add(obj2, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS,
             "bo33", strlen("bo33")+1);
    cps_api_transaction_params_t tr2;
    ASSERT_TRUE(cps_api_transaction_init(&tr2)==cps_api_ret_code_OK);
    cps_api_set(&tr2,obj2);
    ASSERT_TRUE(cps_api_commit(&tr2)==cps_api_ret_code_ERR);
    cps_api_transaction_close(&tr2);
    sleep(2);
}


TEST(std_vlan_test, ROLL_BK_CREATE_VLAN)
{

    if(system("ifconfig e101-005-0 1.1.1.1/8")){}
    sleep(2);
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

    cps_api_object_attr_add_u32(obj,BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID,23);
    cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS,
             "e101-005-0", strlen("e101-004-1")+1);
    cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS,
             "e101-006-0", strlen("e101-004-1")+1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_create(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_ERR);

    cout <<"Roll_back for bridge create done" <<  endl;
    sleep(5);

}


TEST(std_vlan_test, roll_back_set_vlan_params)
{

    const char * mac =  "22:22:22:22:22:22";
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

    cps_api_object_attr_add_u32(obj,BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID,22);
    cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS, mac, strlen(mac) + 1);

    cps_api_object_attr_add_u32(obj,IF_INTERFACES_INTERFACE_ENABLED,false);
    cps_api_object_attr_add_u32(obj,DELL_IF_IF_INTERFACES_INTERFACE_LEARNING_MODE, true);
    cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS,
             "e101-007-0", strlen("e101-004-1")+1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_create(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);
    cps_api_object_t recvd_obj = cps_api_object_list_get(tr.change_list,0);

    cps_api_object_attr_t vlan_attr = cps_api_object_attr_get(recvd_obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);

    int ifindex = cps_api_object_attr_data_u32(vlan_attr);

    cout <<"IF Index from Kernel is "<< ifindex << endl;
    cps_api_transaction_close(&tr);

    sleep(5);
    if(system("ip link show br22")){}
    if(system("hshell -c \"vlan show\"")){}
    sleep(30);

    const char *br_name = "br22";
    const char * mac2 =  "22:22:22:22:11:11";
    cps_api_object_t obj2 = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj2),
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add(obj2,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

    cps_api_set_key_data(obj2, IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                             br_name, strlen(br_name) + 1 );

    cps_api_object_attr_add(obj2, DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS, mac2, strlen(mac2) + 1);
    cps_api_object_attr_add_u32(obj2,IF_INTERFACES_INTERFACE_ENABLED,true);
    cps_api_object_attr_add_u32(obj2,DELL_IF_IF_INTERFACES_INTERFACE_LEARNING_MODE, false);
    cps_api_object_attr_add(obj2, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS,
             "e101-005-0", strlen("e101-004-1")+1);

    cps_api_object_attr_add(obj2, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS,
             "e101-009-0", strlen("e101-004-1")+1);

    cps_api_object_attr_add(obj2, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS,
             "e101-008-0", strlen("e101-009-1")+1);

    cps_api_transaction_params_t tr2;
    ASSERT_TRUE(cps_api_transaction_init(&tr2)==cps_api_ret_code_OK);
    cps_api_set(&tr2,obj2);
    ASSERT_TRUE(cps_api_commit(&tr2)==cps_api_ret_code_ERR);


    cps_api_transaction_close(&tr2);
    if(system("ip link show br22")){}
    if(system("hshell -c \"vlan show\"")){}
    cout <<"Roll_back teste for seti params done "<<  endl;
}

TEST(std_vlan_test, create_vlan)
{
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

    cout <<"IF Index from Kernel is "<< ifindex << endl;

    cps_api_transaction_close(&tr);
}

TEST(std_vlan_test, del_port_vlan)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

    const char *br_name = "br1";

    cps_api_set_key_data(obj, IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                             br_name, strlen(br_name) + 1 );

    int port_index = 0;
    size_t len = 0;
    //set the length to 0 to delete all ports

    cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS,
                            &port_index, len);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);
    cps_api_transaction_close(&tr);
}

TEST(std_vlan_test, add_bond_vlan100)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

    const char *br_name = "br100";
    char port_name[HAL_IF_NAME_SZ] = "\0";

    int port_name_len = 5;
    safestrncpy(port_name, "bond1", port_name_len);
    int port_index = if_nametoindex(port_name);

    cps_api_set_key_data(obj, IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                             br_name, strlen(br_name) + 1 );

    cps_api_object_attr_add_u32(obj,DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS, port_index);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);
}

TEST(std_vlan_test, del_bond_vlan100)
{
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
}


TEST(std_vlan_test, add_mac_vlan)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

    const char *br_name = "br100";


    cps_api_set_key_data(obj, IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                             br_name, strlen(br_name) + 1 );

    const char * mac =  "00:11:11:11:11:11";

    cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS, mac,
                                                                strlen(mac) + 1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);
}

TEST(std_vlan_set_admin, set_vlan_admin)
{

    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                         DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

    const char *br_name = "br100";


    cps_api_set_key_data(obj, IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                             br_name, strlen(br_name) + 1 );

    cps_api_object_attr_add_u32(obj,IF_INTERFACES_INTERFACE_ENABLED,true);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);

}

TEST(std_vlan_set_desc, set_vlan_desc)
{

    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                         DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

    const char *br_name = "br100";


    cps_api_set_key_data(obj, IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                             br_name, strlen(br_name) + 1 );

    const char *desc = "br100 interface description";
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_DESCRIPTION, desc, strlen(desc) + 1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);

}

TEST(std_vlan_test, get_all_vlan)
{
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);
    cps_api_key_t *key;

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);

    key = cps_api_object_key(obj);
    cps_api_key_from_attr_with_qual(key, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

    gp.key_count = 1;
    gp.keys = key;

    if (cps_api_get(&gp)==cps_api_ret_code_OK) {
        size_t mx = cps_api_object_list_size(gp.list);
        for (size_t ix = 0 ; ix < mx ; ++ix ) {
            cps_api_object_t obj = cps_api_object_list_get(gp.list,ix);
            nas_print_vlan_object(obj);
        }
    }
    cps_api_get_request_close(&gp);

}


TEST(std_vlan_test, del_vlan)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                        DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                        cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

    const char *br_name = "br100";


    cps_api_set_key_data(obj, IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                             br_name, strlen(br_name) + 1 );

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_delete(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);
    cps_api_transaction_close(&tr);
}


TEST(std_vlan_test, get_default_vlan)
{

    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);

    if (obj == NULL) {
      std::cout<<"Can not create new object"<<std::endl;
      return ;
    }

    cps_api_key_t key;
    cps_api_key_from_attr_with_qual(&key,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,cps_api_qualifier_TARGET);
    cps_api_object_set_key(obj,&key);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));


    int id =0;
    cps_api_set_key_data(obj,BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID,cps_api_object_ATTR_T_U32,
                         &id,sizeof(id));


    if (cps_api_get(&gp)==cps_api_ret_code_OK) {
        size_t mx = cps_api_object_list_size(gp.list);
        for (size_t ix = 0 ; ix < mx ; ++ix ) {
            cps_api_object_t obj = cps_api_object_list_get(gp.list,ix);
            std::cout<<" Default VLAN Information"<<std::endl;
            nas_print_vlan_object(obj);
        }
    }
    cps_api_get_request_close(&gp);
}

TEST(std_vlan_test, add_learning_mode)
{
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

    const char *br_name = "br100";

    bool learning_mode = false;

    cps_api_set_key_data(obj, IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                             br_name, strlen(br_name) + 1 );

    cps_api_object_attr_add_u32(obj,DELL_IF_IF_INTERFACES_INTERFACE_LEARNING_MODE, learning_mode);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);
}

bool vlan_obj_event_cb(cps_api_object_t obj, void *param)
{
    cps_api_object_it_t it;
    cps_api_object_it_begin(obj,&it);

    cps_api_key_t *key = cps_api_object_key(obj);
    cout << "Received VLAN notification .. "<< endl;

    if (cps_api_key_get_cat(key) != cps_api_obj_cat_INTERFACE) {
        cout << "Category type is not Interface. "<< endl;
    }
    uint_t obj_id = cps_api_key_get_subcat(key);

    if(obj_id == DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ) {

        cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

        if( op == cps_api_oper_CREATE){
            cout << "Operation Type :- Create VLAN "<< endl;
        }
        if(op == cps_api_oper_DELETE){
            cout << "Operation Type :- Delete VLAN "<< endl;
        }
        if(op == cps_api_oper_SET){
            cout << "Operation Type :- Ports Add/Del. "<< endl;
        }

        cps_api_object_attr_t vlan_attr = cps_api_object_attr_get(obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
        hal_ifindex_t vlan_index = (hal_ifindex_t) cps_api_object_attr_data_u32(vlan_attr);
        cout <<"Vlan I/F Index - " << vlan_index;

        for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it) ) {
            int id = (int) cps_api_object_attr_id(it.attr);

            switch(id){
                case DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS:
                    std::cout<<"Tagged Port "<<cps_api_object_attr_data_u32(it.attr)<<',';
                    break;
                case DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS:
                    std::cout<<"Untagged Port "<<cps_api_object_attr_data_u32(it.attr)<<',';
                    break;
                case BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID:
                    std::cout<<"VLAN ID "<<cps_api_object_attr_data_u32(it.attr)<<std::endl;
                    break;
                default :
                    break;
            }
        }
    }
    fflush(stdout);
    return true;
}

static cps_api_return_code_t nas_vlan_process_ports(cps_api_object_t obj, bool add_ports)
{
    hal_ifindex_t vlan_index = 0;

    cout << "NAS Vlan port update for" << add_ports ;

    cps_api_object_attr_t vlan_attr = cps_api_object_attr_get(obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);

    if(vlan_attr == NULL) {
        cout << "Missing ifindex attribute for CPS Set" << endl ;
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_t vlan_id_attr = cps_api_object_attr_get(obj, BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID);

    if(vlan_id_attr == NULL) {
        cout << "Missing Vlan ID for CPS Set" << endl;
        return cps_api_ret_code_ERR;
    }

    hal_vlan_id_t vlan_id = (hal_vlan_id_t) cps_api_object_attr_data_u32(vlan_id_attr);

    cout << "Vlan ID "<< vlan_id ;

    vlan_index = (hal_ifindex_t) cps_api_object_attr_data_u32(vlan_attr);

    cout << ", Vlan index "<< vlan_index ;

    cps_api_object_attr_t tag_port_attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS);

    cps_api_object_attr_t untag_port_attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS);

    if((tag_port_attr == NULL) && (untag_port_attr == NULL)){
        cout << "Missing Port list from VLAN update" << endl ;
        return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}

static bool nas_vlan_port_event_func_cb(cps_api_object_t obj, void *param)
{
    cps_api_object_it_t it;
    cps_api_object_it_begin(obj,&it);

    cout << "Received VLAN Port notification .. "<< endl;

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if ((op != cps_api_oper_CREATE) && (op != cps_api_oper_DELETE)){
        cout << "Received Invalid VLAN operation code.. "<< endl;
        cout <<"Operation is "<< op << endl;

        return cps_api_ret_code_ERR;
    }

    bool add_ports = false;

    if( op == cps_api_oper_CREATE){
        add_ports = true;
    }

    if(nas_vlan_process_ports(obj, add_ports) != cps_api_ret_code_OK){
        return cps_api_ret_code_ERR;
    }

    cout << "Port processing done .. "<< endl;
    return cps_api_ret_code_OK;
}

static bool nas_vlan_utag_port_event_func_cb(cps_api_object_t obj, void *param)
{
    cps_api_object_it_t it;
    cps_api_object_it_begin(obj,&it);

    cout << "Received Untagged Port notification from VLAN.. "<< endl;

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if ((op != cps_api_oper_CREATE) && (op != cps_api_oper_DELETE)){
        cout << "Received Invalid VLAN operation code.. "<< endl;
        cout <<"Operation is "<< op << endl;

        return cps_api_ret_code_ERR;
    }

    bool add_ports = false;

    if( op == cps_api_oper_CREATE){
        add_ports = true;
    }

    if(nas_vlan_process_ports(obj, add_ports) != cps_api_ret_code_OK){
        return cps_api_ret_code_ERR;
    }

    cout << "Port processing done .. "<< endl;
    return cps_api_ret_code_OK;
}

TEST(std_vlan_event_rx_test, get_tagged_port_notification)
{
    cps_api_event_reg_t reg;
    cps_api_key_t key;

    memset(&reg,0,sizeof(reg));

    if (cps_api_event_service_init() != cps_api_ret_code_OK) {
        cout << "***ERROR*** cps_api_event_service_init() failed\n" << endl;
        return ;
    }

    if (cps_api_event_thread_init() != cps_api_ret_code_OK) {
        cout << "***ERROR*** cps_api_event_thread_init() failed\n" <<endl;
        return;
    }
#if 0
    cps_api_key_from_attr_with_qual(&key, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_OBSERVED);

    reg.number_of_objects = 1;
    reg.objects = &key;

    if (cps_api_event_thread_reg(&reg, vlan_obj_event_cb,NULL)!=cps_api_ret_code_OK) {
        cout << " registration failure"<<endl;
        return;
    }
#endif
    cout << "Registering for Tagged Vlan port notification!!!" << endl ;

    memset(&reg,0,sizeof(reg));

    cps_api_key_from_attr_with_qual(&key, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS,
                                    cps_api_qualifier_OBSERVED);

    reg.number_of_objects = 1;
    reg.objects = &key;

    if (cps_api_event_thread_reg(&reg, nas_vlan_port_event_func_cb, NULL)!=cps_api_ret_code_OK)
        cout << "Failure registering for Vlan event notification!!!" <<endl ;

    while(1);
    // infinite loop
}

TEST(std_vlan_event_rx_test, get_untagged_port_notification)
{
    cps_api_event_reg_t reg;
    cps_api_key_t key;

    memset(&reg,0,sizeof(reg));

    if (cps_api_event_service_init() != cps_api_ret_code_OK) {
        cout << "***ERROR*** cps_api_event_service_init() failed\n" << endl;
        return ;
    }

    if (cps_api_event_thread_init() != cps_api_ret_code_OK) {
        cout << "***ERROR*** cps_api_event_thread_init() failed\n" <<endl;
        return;
    }

    cout << "Registering for Untagged Vlan port notification!!!" << endl ;

    memset(&reg,0,sizeof(reg));

    cps_api_key_from_attr_with_qual(&key, DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS,
                                    cps_api_qualifier_OBSERVED);

    reg.number_of_objects = 1;
    reg.objects = &key;

    if (cps_api_event_thread_reg(&reg, nas_vlan_utag_port_event_func_cb, NULL)!=cps_api_ret_code_OK)
        cout << "Failure registering for Untagged port event notification!!!" <<endl ;

    while(1);
    // infinite loop
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}




