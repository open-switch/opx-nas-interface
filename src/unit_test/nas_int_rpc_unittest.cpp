/*
 * Copyright (c) 2016 Dell Inc.
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
 * nas_int_rpc_unittest.cpp
 *
 */

#include "cps_api_object.h"
#include "cps_api_operation.h"
#include "cps_api_object_category.h"
#include "cps_class_map.h"
#include "cps_api_object_key.h"
#include "dell-base-if.h"
#include "dell-base-if-phy.h"
#include "iana-if-type.h"
#include "dell-interface.h"

#include <gtest/gtest.h>
#include <iostream>

using namespace std;

const char *test_ifname = "e101-005-0";
const uint32_t test_port_id = 45;
const uint32_t test_front_panel_port = 2;
const uint32_t test_subport_id = 1;

static void delete_interface_rpc(const char *ifname)
{
    cout << "Delete interface " << ifname << endl;

    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_SET_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_SET_INTERFACE_INPUT_OPERATION,
                                DELL_BASE_IF_CMN_OPERATION_TYPE_DELETE);
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_NAME, ifname,
                            strlen(ifname) + 1);
    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr) == cps_api_ret_code_OK);
    cps_api_action(&tr, obj);
    ASSERT_TRUE(cps_api_commit(&tr) == cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);
}

TEST(int_rpc_test, delete_interface)
{
    delete_interface_rpc(test_ifname);
}

static void dump_return_obj(cps_api_transaction_params_t& param)
{
    size_t mx = cps_api_object_list_size(param.change_list);
    if (mx == 0) {
        cout << "No object returned" << endl;
        return;
    }
    for (size_t idx = 0; idx < mx; idx ++) {
        cout << "Return object " << idx + 1 << " :" << endl;
        cps_api_object_t obj = cps_api_object_list_get(param.change_list, idx);
        cps_api_object_attr_t attr = cps_api_object_attr_get(obj,
                                            DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
        if (attr == NULL) {
            cout << "No ifindex attribute in object" << endl;
            return;
        }
        uint32_t ifindex = cps_api_object_attr_data_u32(attr);
        cout << "  ifindex - " << ifindex << endl;
        attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS);
        if (attr == NULL) {
            cout << "No mac address attribute in object" << endl;
            return;
        }
        char *mac_addr = (char *)cps_api_object_attr_data_bin(attr);
        cout << "  mac address - " << mac_addr << endl;
    }
}

TEST(int_rpc_test, create_interface_port_id)
{
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_SET_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_SET_INTERFACE_INPUT_OPERATION,
                                DELL_BASE_IF_CMN_OPERATION_TYPE_CREATE);
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_TYPE,
                            IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_ETHERNETCSMACD,
                            strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_ETHERNETCSMACD) + 1);
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_NAME, test_ifname,
                            strlen(test_ifname) + 1);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_IF_INTERFACES_INTERFACE_NPU_ID, 0);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_IF_INTERFACES_INTERFACE_PORT_ID, test_port_id);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr) == cps_api_ret_code_OK);
    cps_api_action(&tr, obj);
    ASSERT_TRUE(cps_api_commit(&tr) == cps_api_ret_code_OK);

    dump_return_obj(tr);

    cps_api_transaction_close(&tr);
}

TEST(int_rpc_test, delete_interface_no_fanout)
{
    char ifname[64];
    snprintf(ifname, sizeof(ifname), "e101-%03d-0", test_front_panel_port);
    delete_interface_rpc(ifname);
}

TEST(int_rpc_test, create_interface_front_panel_no_fanout)
{
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_SET_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_SET_INTERFACE_INPUT_OPERATION,
                                DELL_BASE_IF_CMN_OPERATION_TYPE_CREATE);
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_TYPE,
                            IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_ETHERNETCSMACD,
                            strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_ETHERNETCSMACD) + 1);
    char ifname[64];
    snprintf(ifname, sizeof(ifname), "e101-%03d-0", test_front_panel_port);
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_NAME, ifname, strlen(ifname) + 1);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_IF_INTERFACES_INTERFACE_NPU_ID, 0);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_HARDWARE_PORT_FRONT_PANEL_PORT, test_front_panel_port);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_HARDWARE_PORT_SUBPORT_ID, 0);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr) == cps_api_ret_code_OK);
    cps_api_action(&tr, obj);
    ASSERT_TRUE(cps_api_commit(&tr) == cps_api_ret_code_OK);

    dump_return_obj(tr);

    cps_api_transaction_close(&tr);
}

TEST(int_rpc_test, delete_interface_fanout)
{
    char ifname[64];
    char cmdbuf[256];
    snprintf(ifname, sizeof(ifname), "e101-%03d-0", test_front_panel_port);
    cout << "Enable fanout of interface " << ifname << endl;
    snprintf(cmdbuf,  sizeof(cmdbuf), "opx-config-fanout %s true", ifname);
    ASSERT_TRUE(system(cmdbuf) == 0);
    snprintf(ifname, sizeof(ifname), "e101-%03d-%d", test_front_panel_port, test_subport_id + 1);
    delete_interface_rpc(ifname);
}

TEST(int_rpc_test, create_interface_front_panel_fanout)
{
    char ifname[64];
    char cmdbuf[256];
    snprintf(ifname, sizeof(ifname), "e101-%03d-%d", test_front_panel_port, test_subport_id + 1);

    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_SET_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_SET_INTERFACE_INPUT_OPERATION,
                                DELL_BASE_IF_CMN_OPERATION_TYPE_CREATE);
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_TYPE,
                            IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_ETHERNETCSMACD,
                            strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_ETHERNETCSMACD) + 1);
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_NAME, ifname, strlen(ifname) + 1);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_IF_INTERFACES_INTERFACE_NPU_ID, 0);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_HARDWARE_PORT_FRONT_PANEL_PORT, test_front_panel_port);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_HARDWARE_PORT_SUBPORT_ID, test_subport_id);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr) == cps_api_ret_code_OK);
    cps_api_action(&tr, obj);
    ASSERT_TRUE(cps_api_commit(&tr) == cps_api_ret_code_OK);

    dump_return_obj(tr);

    cps_api_transaction_close(&tr);

    cout << "Disable fanout of interface " << ifname << endl;
    snprintf(cmdbuf, sizeof(cmdbuf), "opx-config-fanout %s false", ifname);
    ASSERT_TRUE(system(cmdbuf) == 0);
}

int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
