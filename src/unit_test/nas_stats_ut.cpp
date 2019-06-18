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
 * filename: nas_stat_ut.cpp
 */


#include "cps_api_events.h"
#include "cps_api_key.h"
#include "cps_api_operation.h"
#include "cps_api_object.h"
#include "cps_api_errors.h"
#include "gtest/gtest.h"
#include "cps_class_map.h"
#include "iana-if-type.h"
#include "dell-base-if.h"
#include "dell-interface.h"
#include "ietf-interfaces.h"
#include "cps_api_object_key.h"

void nas_stat_dump_object_content(cps_api_object_t obj){
    cps_api_object_it_t it;
    cps_api_object_it_begin(obj,&it);

    for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it) ) {
        std::cout<<cps_api_object_attr_id(it.attr)<<"  "<<
        cps_api_object_attr_data_u64(it.attr)<<std::endl;
    }
}


bool nas_stat_get_if(){

    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);

    if (obj == NULL) {
        std::cout<<"Can not create new object"<<std::endl;
        return false;
    }

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
            DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_STATISTICS_OBJ, cps_api_qualifier_OBSERVED);

    cps_api_object_attr_add(obj,IF_INTERFACES_STATE_INTERFACE_TYPE,
      (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_ETHERNETCSMACD,
      sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_ETHERNETCSMACD));

    const char *if_name = "e101-001-0";
    cps_api_set_key_data(obj,IF_INTERFACES_STATE_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
                  if_name,strlen(if_name)+1);

    bool rc = false;

    if (cps_api_get(&gp)==cps_api_ret_code_OK) {

        size_t mx = cps_api_object_list_size(gp.list);
        for (size_t ix = 0 ; ix < mx ; ++ix ) {
            cps_api_object_t obj = cps_api_object_list_get(gp.list,ix);
            nas_stat_dump_object_content(obj);
        }
        rc = true;
    }

    cps_api_get_request_close(&gp);
    return rc;
}

bool nas_stat_del_if(){

    cps_api_object_t obj = cps_api_object_create();
    if(!obj) return false;

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
            DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_STATISTICS_OBJ, cps_api_qualifier_OBSERVED);

    cps_api_object_attr_add(obj,IF_INTERFACES_STATE_INTERFACE_TYPE,
      (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_ETHERNETCSMACD,
      sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_ETHERNETCSMACD));

    const char *if_name = "e101-001-0";
    cps_api_set_key_data(obj,IF_INTERFACES_STATE_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
                  if_name,strlen(if_name)+1);

    cps_api_object_attr_add_u32(obj,DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_GREEN_DISCARD_DROPPED_PACKETS,0);
    cps_api_object_attr_add_u32(obj,DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_YELLOW_DISCARD_DROPPED_PACKETS,0);
    cps_api_object_attr_add_u32(obj,DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_RED_DISCARD_DROPPED_PACKETS,0);

    cps_api_transaction_params_t tr;
    if(cps_api_transaction_init(&tr)!=cps_api_ret_code_OK){
        return false;
    }

    cps_api_delete(&tr,obj);

    if(cps_api_commit(&tr)!=cps_api_ret_code_OK){
        return false;
    }

    cps_api_transaction_close(&tr);

    return true;
}

bool nas_stat_get_vlan(){

    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);

    if (obj == NULL) {
        std::cout<<"Can not create new object"<<std::endl;
        return false;
    }

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
            DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_STATISTICS_OBJ, cps_api_qualifier_OBSERVED);

    cps_api_object_attr_add(obj,IF_INTERFACES_STATE_INTERFACE_TYPE,
         (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
         sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

    const char *vlan_name = "br1";
    cps_api_set_key_data(obj,IF_INTERFACES_STATE_INTERFACE_NAME,cps_api_object_ATTR_T_BIN,
                         vlan_name,strlen(vlan_name)+1);


    bool rc = false;

    if (cps_api_get(&gp)==cps_api_ret_code_OK) {

        size_t mx = cps_api_object_list_size(gp.list);
        for (size_t ix = 0 ; ix < mx ; ++ix ) {
            cps_api_object_t obj = cps_api_object_list_get(gp.list,ix);
            nas_stat_dump_object_content(obj);
        }
        rc = true;
    }

    cps_api_get_request_close(&gp);
    return rc;
}


TEST(cps_api_events,sflow_test) {
    ASSERT_TRUE(nas_stat_get_if());
    ASSERT_TRUE(nas_stat_get_vlan());
    ASSERT_TRUE(nas_stat_del_if());
}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
