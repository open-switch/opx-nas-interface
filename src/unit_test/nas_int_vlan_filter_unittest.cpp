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
 * nas_int_vlan_filter_unittest.cpp
 *
 */

#include "cps_class_map.h"
#include "cps_api_object.h"
#include "cps_api_operation.h"
#include "cps_api_object_key.h"
#include "cps_api_object_category.h"
#include "cps_api_events.h"
#include "dell-base-if.h"
#include "dell-interface.h"
#include "ietf-interfaces.h"
#include "iana-if-type.h"
#include "std_utils.h"

#include <gtest/gtest.h>
#include <iostream>

using namespace std;
static uint32_t ifindex = 0;

static uint32_t get_vlan_filter_value (void)
{
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);
    uint32_t filter_type = 0;

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);
    if (obj == nullptr)
    {
        cout<<"Get VLAN Filter - CPS Object create failed  "<<endl;
        return 0;
    }

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj), DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_OBJ,
                                    cps_api_qualifier_OBSERVED);

    const char *ifname = "e101-001-0";

    cps_api_object_attr_add(obj,IF_INTERFACES_STATE_INTERFACE_NAME,ifname,strlen(ifname)+1);

    if (cps_api_get(&gp)==cps_api_ret_code_OK) {
        cps_api_object_t obj = cps_api_object_list_get(gp.list, 0);
        cps_api_object_attr_t filter_attr = cps_api_get_key_data(obj,
                DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_VLAN_FILTER);
        if (filter_attr == nullptr)
        {
            cout<<"VLAN Filter Get Object Failed "<<endl;
            cps_api_get_request_close(&gp);
            return 0;
        }

        cps_api_object_attr_t if_attr = cps_api_object_attr_get(obj, IF_INTERFACES_STATE_INTERFACE_IF_INDEX);
        if (if_attr == nullptr)
        {
            cout<<"Interface Index Attribute Get Failed "<<endl;
            cps_api_get_request_close(&gp);
            return 0;
        }
        ifindex = cps_api_object_attr_data_u32(if_attr);

        cout <<"IF Index : "<< ifindex << endl;
        filter_type = cps_api_object_attr_data_u32(filter_attr);
        cout<<"VLAN FILTER TYPE  :  "<<filter_type<<endl;
    }

    cps_api_get_request_close(&gp);
    return filter_type;
}

TEST(std_vlan_filter_test, get_vlan_filter_type)
{
    uint32_t filter_type;
    filter_type = get_vlan_filter_value ();
    ASSERT_TRUE(filter_type !=0);
}

TEST(std_vlan_filter_set, set_vlan_filter_type)
{
    uint32_t filter_type_config = 4;
    uint32_t filter_type_observed = 0;
    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
            DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, ifindex);
    cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_VLAN_FILTER, filter_type_config);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    /* Check if the value set is properly programmed in HW */
    filter_type_observed = get_vlan_filter_value ();
    ASSERT_TRUE(filter_type_config == filter_type_observed);

    cps_api_transaction_close(&tr);

}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}


