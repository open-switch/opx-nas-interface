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
#include "dell-base-if.h"
#include "dell-base-common.h"
#include "dell-base-interface-common.h"
#include "dell-interface.h"
#include "ietf-interfaces.h"
#include "iana-if-type.h"
#include "std_utils.h"
#include <netinet/in.h>
#include <arpa/inet.h>


#include <gtest/gtest.h>
#include <iostream>
#include <net/if.h>

using namespace std;

TEST(std_vxlan_set_test1, vtep_add)
{
    system ("brctl addbr br100");
    system ("ip link add vtep100 type vxlan id 100 local 10.11.1.2 dstport 4789");

    sleep(1);

   /* Now form CPS Message */
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
               DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    const char *i_name = "vtep100";
    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,i_name,strlen(i_name)+1);

    cps_api_attr_id_t ids[3] = {DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT,0,
                                DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR_FAMILY };

    const int ids_len = 3;
    std::string dni[2] = {"10.10.1.9", "10.10.1.21"};
    BASE_IF_MAC_LEARN_MODE_t pni[2] = {BASE_IF_MAC_LEARN_MODE_DISABLE, BASE_IF_MAC_LEARN_MODE_HW};
    for (auto ix = 0; ix < 2; ++ix) {
        ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR_FAMILY; /*Family is u32 */
        BASE_CMN_AF_TYPE_t ev_type = BASE_CMN_AF_TYPE_INET;
        cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U32,&ev_type,sizeof(ev_type));

       ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR;
        uint32_t ip;
        struct in_addr a;
        inet_aton(dni[ix].c_str(), &a);
        ip=a.s_addr;
        cps_api_object_e_add(obj, ids, ids_len, cps_api_object_ATTR_T_BIN,
                             &ip,sizeof(ip));

        ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_MAC_LEARNING_MODE;
        cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U32,&pni[ix],sizeof(ev_type));


        ++ids[1];
     }

     cps_api_transaction_params_t tr;
     ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
     cps_api_set(&tr,obj);
     ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);
     cps_api_transaction_close(&tr);
     sleep(1);
     system("brctl addif br100 vxlan-100");
     sleep(1);
     system("bridge fdb append 00:00:00:00:00:00 dev vxlan-100 dst 10.10.1.9");
     system("bridge fdb append 00:00:00:00:00:00 dev vxlan-100 dst 10.10.1.21");


}
TEST(std_vxlan_set_from_mac_last, vtep_add)
{
    system ("brctl addbr br300");
    system ("ip link add vtep300 type vxlan id 300 local 10.11.1.2 dstport 4789");

    system("brctl addif br300 vtep300");
    sleep(1);

    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
               DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    const char *i_name = "vtep300";

    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,i_name,strlen(i_name)+1);

    cps_api_attr_id_t ids[3] = {DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT,0,
                                DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR_FAMILY };

    const int ids_len = 3;
    std::string dni[2] = {"10.10.1.6", "10.10.1.7"};
    BASE_IF_MAC_LEARN_MODE_t pni[2] = {BASE_IF_MAC_LEARN_MODE_DISABLE, BASE_IF_MAC_LEARN_MODE_HW};
    for (auto ix = 0; ix < 2; ++ix) {
        ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR_FAMILY; /*Family is u32 */
        BASE_CMN_AF_TYPE_t ev_type = BASE_CMN_AF_TYPE_INET;
        cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U32,&ev_type,sizeof(ev_type));

       ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR;
        uint32_t ip;
        struct in_addr a;
        inet_aton(dni[ix].c_str(), &a);
        ip=a.s_addr;
        cps_api_object_e_add(obj, ids, ids_len, cps_api_object_ATTR_T_BIN,
                             &ip,sizeof(ip));

        ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_MAC_LEARNING_MODE;
        cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U32,&pni[ix],sizeof(ev_type));


        ++ids[1];
     }

     cps_api_transaction_params_t tr;
     ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
     cps_api_set(&tr,obj);
     ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);
     cps_api_transaction_close(&tr);
     system("bridge fdb append 00:00:00:00:00:00 dev vtep300 dst 10.10.1.6");
     system("bridge fdb append 00:00:00:00:00:00 dev vtep300 dst 10.10.1.7");
}


TEST(std_vxlan_set_test1, set_mac_after_vtep_add)
{
    system ("brctl addbr br200");
    system ("ip link add vtep200 type vxlan id 200 local 10.11.1.2 dstport 4789");

    sleep(1);
    system("brctl addif br200 vtep200");

    system("bridge fdb append 00:00:00:00:00:00 dev vtep200 dst 10.10.1.3");
    system("bridge fdb append 00:00:00:00:00:00 dev vtep200 dst 10.10.1.4");

   /* Now form CPS Message */

    cps_api_object_t obj = cps_api_object_create();


    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
               DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    const char *i_name = "vtep200";
    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,i_name,strlen(i_name)+1);

    cps_api_attr_id_t ids[3] = {DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT,0,
                                DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR_FAMILY };

    const int ids_len = 3;

    std::string dni[2] = {"10.10.1.3", "10.10.1.4"};
    BASE_IF_MAC_LEARN_MODE_t pni[2] = {BASE_IF_MAC_LEARN_MODE_DISABLE, BASE_IF_MAC_LEARN_MODE_HW};

    for (auto ix = 0; ix < 2; ++ix) {
        ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR_FAMILY; /*Family is u32 */
        BASE_CMN_AF_TYPE_t ev_type = BASE_CMN_AF_TYPE_INET;
        cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U32,&ev_type,sizeof(ev_type));

       ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR;
        uint32_t ip;
        struct in_addr a;
        inet_aton(dni[ix].c_str(), &a);
        ip=a.s_addr;
        cps_api_object_e_add(obj, ids, ids_len, cps_api_object_ATTR_T_BIN,
                             &ip,sizeof(ip));

        ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_MAC_LEARNING_MODE;
        cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U32,&pni[ix],sizeof(BASE_IF_MAC_LEARN_MODE_t));


        ++ids[1];
     }

     sleep(1);
     cps_api_transaction_params_t tr;
     ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
     cps_api_set(&tr,obj);
     ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

     cps_api_transaction_close(&tr);
}




int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

