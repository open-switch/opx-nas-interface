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
 * nas_int_media_unittest.cpp
 *
 */

#include "cps_api_object.h"
#include "cps_api_object_key.h"
#include "cps_api_operation.h"
#include "cps_api_object_category.h"
#include "cps_api_events.h"
#include "dell-base-platform-common.h"
#include "dell-base-pas.h"
#include "cps_class_map.h"
#include "ds_common_types.h"

#include <gtest/gtest.h>
#include <iostream>
#include <net/if.h>
#include <stdio.h>
#include <stdint.h>

using namespace std;

TEST(std_sys_mac_test, get_sys_mac)
{

    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                          BASE_PAS_CHASSIS_OBJ, cps_api_qualifier_OBSERVED);

    if (cps_api_get(&gp)==cps_api_ret_code_OK) {

        hal_mac_addr_t base_mac;
        cps_api_object_t obj = cps_api_object_list_get(gp.list,0);
        cps_api_object_attr_t base_mac_attr = cps_api_object_attr_get(obj,BASE_PAS_CHASSIS_BASE_MAC_ADDRESSES);
        cps_api_object_attr_t max_mac_num = cps_api_object_attr_get(obj,BASE_PAS_CHASSIS_NUM_MAC_ADDRESSES);
        void *_base_mac = cps_api_object_attr_data_bin(base_mac_attr);
        memcpy(base_mac, _base_mac,sizeof(base_mac));
        cout<< "MAC size :" << cps_api_object_attr_data_u32(max_mac_num)<< endl;
        cout << "MAC Address: ";
        for (size_t i =0; i < sizeof(base_mac); i++) {
            printf("%x:",base_mac[i]);
        }
    }

    cps_api_get_request_close(&gp);
    return;
}

static void print_media_info(cps_api_object_t obj)
{
    cps_api_object_attr_t _slot = cps_api_get_key_data(obj, BASE_PAS_MEDIA_SLOT);
    cps_api_object_attr_t _sfp_idx = cps_api_get_key_data(obj, BASE_PAS_MEDIA_PORT);
    cps_api_object_attr_t _present = cps_api_object_attr_get(obj, BASE_PAS_MEDIA_PRESENT);

    bool bool_presence = *(uint8_t *)cps_api_object_attr_data_bin(_present);
    if (bool_presence) {
        cout << "Media is present at slot "<<cps_api_object_attr_data_uint(_slot);
        cout <<" and port "<<cps_api_object_attr_data_uint(_sfp_idx);
        cps_api_object_attr_t _media_type = cps_api_object_attr_get(obj,BASE_PAS_MEDIA_TYPE);
        cout << " media type is " << cps_api_object_attr_data_u32(_media_type)<<endl;
    } else {
        cout << "Media is NOT present at slot "<<cps_api_object_attr_data_uint(_slot);
        cout <<" and port "<<cps_api_object_attr_data_uint(_sfp_idx)<<endl;

    }
}

TEST(std_media_test, get_media_type)
{

#define MAX_SFP 32
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);


    uint8_t slot = 1;
    uint8_t start = 1; /*  starts with 1 */
    for(uint8_t sfp_idx = start; sfp_idx < MAX_SFP+start; sfp_idx++) {
        cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);
        cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                          BASE_PAS_MEDIA_OBJ, cps_api_qualifier_OBSERVED);
        cps_api_set_key_data_uint(obj, BASE_PAS_MEDIA_SLOT, &slot, sizeof(slot));
        cps_api_set_key_data_uint(obj, BASE_PAS_MEDIA_PORT, &sfp_idx, sizeof(sfp_idx));
    }

    if (cps_api_get(&gp)==cps_api_ret_code_OK) {

        size_t mx = cps_api_object_list_size(gp.list);
        cout<<"Returned Objects..."<<mx<<endl;
        for (size_t ix = 0 ; ix < mx ; ++ix ) {
            cps_api_object_t obj = cps_api_object_list_get(gp.list,ix);
            print_media_info(obj);

        }
    }

    cps_api_get_request_close(&gp);
    return;
}
static bool test_phy_media_event_cb(cps_api_object_t obj, void *param)
{
    cps_api_key_t *key = cps_api_object_key(obj);

    cout << "received Media event.. "<< endl;
    if (cps_api_key_get_cat(key) != cps_api_obj_CAT_BASE_PAS) {
        return true;
    }
    uint_t obj_id = cps_api_key_get_subcat(key);

    if( obj_id==BASE_PAS_MEDIA_OBJ) {
        print_media_info(obj);
    }
    return true;
}
TEST(std_media_test, phy_media_monitor)
{
    /*  register for any change in the media status   */

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

    cps_api_key_init(&key, cps_api_qualifier_OBSERVED,
                cps_api_obj_CAT_BASE_PAS, BASE_PAS_MEDIA_OBJ, 0);

    reg.number_of_objects = 1;
    reg.objects = &key;

    if (cps_api_event_thread_reg(&reg,test_phy_media_event_cb,NULL)!=cps_api_ret_code_OK) {
        cout << " registration failure"<<endl;
        return;
    }

    while(1);
    // infinite loop
}
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
