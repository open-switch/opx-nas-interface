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
#include <cstdlib>
#include <unordered_set>

using namespace std;

const char *test_ifname = "e101-005-0";
const uint32_t test_port_id = 45;
const uint32_t test_front_panel_port = 2;
const uint32_t INVALID_FRONT_PANEL_PORT = static_cast<uint32_t>(-1);
uint32_t fanout_front_panel_port = INVALID_FRONT_PANEL_PORT;
const uint32_t fanout_subport_id = 1;
const uint32_t check_subport_id = 2;
const char *lp_ifname = "test_loopback";
const char *virt_ifname = "test_virt_if";
const char *phy_ifname = "e101-002-0";

static void delete_interface_rpc(const char *ifname, bool loopback = false)
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
    if (loopback) {
        cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_TYPE,
                                IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_SOFTWARELOOPBACK,
                                strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_SOFTWARELOOPBACK) + 1);
    }
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

static uint32_t get_fp_port_from_name(const char* if_name)
{
    const char* NAME_PREFIX = "e101-";
    if (strncmp(if_name, NAME_PREFIX, strlen(NAME_PREFIX)) != 0) {
        return INVALID_FRONT_PANEL_PORT;
    }
    char* ptr;
    uint32_t port_id = strtol(&if_name[strlen(NAME_PREFIX)], &ptr, 0);
    if (*ptr != '-') {
        return INVALID_FRONT_PANEL_PORT;
    }
    return port_id;
}

static void check_port_fanout_cap(uint32_t port_id, bool& could_fanout)
{
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);
    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);
    ASSERT_TRUE(obj != nullptr);
    ASSERT_TRUE(cps_api_key_from_attr_with_qual(cps_api_object_key(obj), BASE_IF_PHY_FRONT_PANEL_PORT_OBJ,
                                                cps_api_qualifier_TARGET));
    ASSERT_TRUE(cps_api_object_attr_add_u32(obj, BASE_IF_PHY_FRONT_PANEL_PORT_FRONT_PANEL_PORT, port_id));
    ASSERT_TRUE(cps_api_get(&gp) == cps_api_ret_code_OK);
    size_t mx = cps_api_object_list_size(gp.list);
    cps_api_key_t port_key;
    memset(&port_key, 0, sizeof(port_key));
    ASSERT_TRUE(cps_api_key_from_attr_with_qual(&port_key, BASE_IF_PHY_FRONT_PANEL_PORT_OBJ,
                                                cps_api_qualifier_TARGET));
    could_fanout = false;
    for (size_t ix = 0; ix < mx; ++ix) {
        cps_api_object_t ret_obj = cps_api_object_list_get(gp.list, ix);
        if (cps_api_key_matches(cps_api_object_key(ret_obj), &port_key, true) == 0) {
            cps_api_object_it_t it, internal;
            cps_api_object_it_begin(ret_obj, &it);
            for(; cps_api_object_it_valid(&it); cps_api_object_it_next(&it)) {
                if (cps_api_object_attr_id(it.attr) != BASE_IF_PHY_FRONT_PANEL_PORT_BR_CAP) {
                    continue;
                }
                unordered_set<BASE_IF_PHY_BREAKOUT_MODE_t> br_modes{};
                internal = it;
                cps_api_object_it_inside(&internal);
                cps_api_attr_id_t ids[3] = {BASE_IF_PHY_FRONT_PANEL_PORT_BR_CAP, 0,
                                            BASE_IF_PHY_FRONT_PANEL_PORT_BR_CAP_BREAKOUT_MODE};
                const int ids_len = sizeof(ids) /sizeof(ids[0]);
                for (; cps_api_object_it_valid(&internal); cps_api_object_it_next(&internal)) {
                    ids[1] = cps_api_object_attr_id(internal.attr);
                    auto br_mode_attr = cps_api_object_e_get(ret_obj, ids, ids_len);
                    if (br_mode_attr != nullptr) {
                        br_modes.insert(static_cast<BASE_IF_PHY_BREAKOUT_MODE_t>(cps_api_object_attr_data_u32(br_mode_attr)));
                    }
                }
                if ((br_modes.find(BASE_IF_PHY_BREAKOUT_MODE_BREAKOUT_1X1) != br_modes.end()) &&
                    (br_modes.find(BASE_IF_PHY_BREAKOUT_MODE_BREAKOUT_4X1) != br_modes.end())) {
                    could_fanout = true;
                    break;
                }
            }
            break;
        }
    }
    cps_api_get_request_close(&gp);
}

static void get_fanout_port()
{
    if (fanout_front_panel_port != INVALID_FRONT_PANEL_PORT) {
        return;
    }
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);
    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);
    ASSERT_TRUE(obj != nullptr);
    ASSERT_TRUE(cps_api_key_from_attr_with_qual(cps_api_object_key(obj), DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_OBJ,
                                                cps_api_qualifier_OBSERVED));
    ASSERT_TRUE(cps_api_get(&gp) == cps_api_ret_code_OK);
    size_t mx = cps_api_object_list_size(gp.list);
    vector<uint32_t> fanout_ports{};
    for (size_t ix = 0; ix < mx; ++ix) {
        cps_api_object_t ret_obj = cps_api_object_list_get(gp.list, ix);
        cps_api_object_attr_t speed_attr = cps_api_object_attr_get(ret_obj, DELL_IF_IF_INTERFACES_STATE_INTERFACE_MAX_SPEED);
        if (speed_attr == nullptr) {
            continue;
        }
        BASE_IF_SPEED_t speed = static_cast<BASE_IF_SPEED_t>(cps_api_object_attr_data_u32(speed_attr));
        if (speed == BASE_IF_SPEED_40GIGE) {
            // search for 40G port for fanout
            cps_api_object_attr_t name_attr = cps_api_get_key_data(ret_obj, IF_INTERFACES_STATE_INTERFACE_NAME);
            ASSERT_TRUE(name_attr != nullptr);
            const char* if_name = static_cast<const char*>(cps_api_object_attr_data_bin(name_attr));
            auto port_id = get_fp_port_from_name(if_name);
            if (port_id != INVALID_FRONT_PANEL_PORT) {
                bool fanout_port = false;
                check_port_fanout_cap(port_id, fanout_port);
                if (fanout_port) {
                    fanout_ports.push_back(port_id);
                }
            }
        }
    }
    if (fanout_ports.empty()) {
        cout << "Could not find any front panel port for fanout" << endl;
    } else {
        fanout_front_panel_port = fanout_ports[rand() % fanout_ports.size()];
    }
    cps_api_get_request_close(&gp);
}

TEST(int_rpc_test, delete_interface_fanout)
{
    char ifname[64];
    char cmdbuf[256];
    /* NOTE : Currently, the Unit cases are tested in 40G platform (Eg. S6000) so using 40G Speed/Mode
     * as default here. If using other platforms, speed and mode should be giving explicitly here */
    const char *if_speed = "10g";
    const char *if_mode = "4x1";


    get_fanout_port();
    if (fanout_front_panel_port == INVALID_FRONT_PANEL_PORT) {
        cout << "No fanout port found, bypass this test" << endl;
        return;
    }
    snprintf(ifname, sizeof(ifname), "e101-%03d-0", fanout_front_panel_port);
    cout << "Enable fanout of interface " << ifname << endl;
    snprintf(cmdbuf,  sizeof(cmdbuf), "opx-config-fanout --port %s --mode %s --speed %s 2>/dev/null", ifname, if_mode, if_speed);
    system(cmdbuf);
    snprintf(ifname, sizeof(ifname), "e101-%03d-%d", fanout_front_panel_port, fanout_subport_id);
    delete_interface_rpc(ifname);
}

TEST(int_rpc_test, check_fanout_port_speed)
{
    char ifname[64];
    if (fanout_front_panel_port == INVALID_FRONT_PANEL_PORT) {
        cout << "No fanout port found, bypass this test" << endl;
        return;
    }

    snprintf(ifname, sizeof(ifname), "e101-%03d-%d", fanout_front_panel_port, check_subport_id);
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);
    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);
    ASSERT_TRUE(obj != nullptr);
    ASSERT_TRUE(cps_api_key_from_attr_with_qual(cps_api_object_key(obj), DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_OBJ,
                                                cps_api_qualifier_OBSERVED));
    cps_api_object_attr_add(obj, IF_INTERFACES_STATE_INTERFACE_NAME, ifname, strlen(ifname) + 1);
    ASSERT_TRUE(cps_api_get(&gp) == cps_api_ret_code_OK);
    size_t mx = cps_api_object_list_size(gp.list);
    ASSERT_TRUE(mx == 1);
    cps_api_object_t ret_obj = cps_api_object_list_get(gp.list, 0);
    cps_api_object_attr_t speed_attr = cps_api_object_attr_get(ret_obj, DELL_IF_IF_INTERFACES_STATE_INTERFACE_MAX_SPEED);
    ASSERT_TRUE(speed_attr != nullptr);
    BASE_IF_SPEED_t speed = static_cast<BASE_IF_SPEED_t>(cps_api_object_attr_data_u32(speed_attr));
    ASSERT_TRUE(speed == BASE_IF_SPEED_10GIGE);
}

TEST(int_rpc_test, create_interface_front_panel_fanout)
{
    char ifname[64];
    char cmdbuf[256];
    /* NOTE : Currently, the Unit cases are tested in 40G platform (Eg. S6000) so using 40G Speed/Mode
     * as default here. If using other platforms, speed and mode should be giving explicitly here */
    const char *if_speed = "40G";
    const char *if_mode = "1x1";

    if (fanout_front_panel_port == INVALID_FRONT_PANEL_PORT) {
        cout << "No fanout port found, bypass this test" << endl;
        return;
    }

    snprintf(ifname, sizeof(ifname), "e101-%03d-%d", fanout_front_panel_port, fanout_subport_id);
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
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_HARDWARE_PORT_FRONT_PANEL_PORT, fanout_front_panel_port);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_HARDWARE_PORT_SUBPORT_ID, fanout_subport_id);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr) == cps_api_ret_code_OK);
    cps_api_action(&tr, obj);
    ASSERT_TRUE(cps_api_commit(&tr) == cps_api_ret_code_OK);

    dump_return_obj(tr);

    cps_api_transaction_close(&tr);

    cout << "Disable fanout of interface " << ifname << endl;
    snprintf(cmdbuf, sizeof(cmdbuf), "opx-config-fanout --port %s --mode %s --speed %s 2>/dev/null", ifname, if_mode,if_speed);
    system(cmdbuf);
}

TEST(int_rpc_test, create_interface_loopback)
{
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_SET_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_SET_INTERFACE_INPUT_OPERATION,
                                DELL_BASE_IF_CMN_OPERATION_TYPE_CREATE);
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_TYPE,
                            IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_SOFTWARELOOPBACK,
                            strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_SOFTWARELOOPBACK) + 1);

    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_NAME, lp_ifname, strlen(lp_ifname) + 1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr) == cps_api_ret_code_OK);
    cps_api_action(&tr, obj);
    ASSERT_TRUE(cps_api_commit(&tr) == cps_api_ret_code_OK);

    dump_return_obj(tr);

    cps_api_transaction_close(&tr);
}

TEST(int_rpc_test, get_loopback_intf_state)
{
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);
    ASSERT_TRUE(obj != nullptr);

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj), DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_OBJ,
                                    cps_api_qualifier_OBSERVED);

    cps_api_object_attr_add(obj,IF_INTERFACES_STATE_INTERFACE_TYPE,
         (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_SOFTWARELOOPBACK,
         sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_SOFTWARELOOPBACK));

    cps_api_object_attr_add(obj,IF_INTERFACES_STATE_INTERFACE_NAME,lp_ifname,strlen(lp_ifname)+1);

    if (cps_api_get(&gp)==cps_api_ret_code_OK) {
        cps_api_object_t obj = cps_api_object_list_get(gp.list, 0);
        cps_api_object_attr_t name_attr = cps_api_get_key_data(obj, IF_INTERFACES_STATE_INTERFACE_NAME);
        const char *name = (const char*) cps_api_object_attr_data_bin(name_attr);
        ASSERT_TRUE(strlen(name) == strlen(lp_ifname));
        ASSERT_TRUE(strncmp(name, lp_ifname, strlen(lp_ifname)) == 0);
    }

    cps_api_get_request_close(&gp);
}

TEST(int_rpc_test, delete_interface_loopback)
{
    delete_interface_rpc(lp_ifname, true);
}

TEST(int_rpc_test, create_virtual_interface)
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
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_NAME, virt_ifname, strlen(virt_ifname) + 1);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr) == cps_api_ret_code_OK);
    cps_api_action(&tr, obj);
    ASSERT_TRUE(cps_api_commit(&tr) == cps_api_ret_code_OK);

    dump_return_obj(tr);

    cps_api_transaction_close(&tr);
}

TEST(int_rpc_test, unmap_phy_interface)
{
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_SET_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_SET_INTERFACE_INPUT_OPERATION,
                                DELL_BASE_IF_CMN_OPERATION_TYPE_UPDATE);
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_NAME, phy_ifname, strlen(phy_ifname) + 1);
    cps_api_object_attr_add(obj, BASE_IF_PHY_HARDWARE_PORT_FRONT_PANEL_PORT, &test_front_panel_port, 0);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr) == cps_api_ret_code_OK);
    cps_api_action(&tr, obj);
    ASSERT_TRUE(cps_api_commit(&tr) == cps_api_ret_code_OK);

    dump_return_obj(tr);

    cps_api_transaction_close(&tr);
}

TEST(int_rpc_test, map_virtual_interface)
{
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_SET_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_SET_INTERFACE_INPUT_OPERATION,
                                DELL_BASE_IF_CMN_OPERATION_TYPE_UPDATE);
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_NAME, virt_ifname, strlen(virt_ifname) + 1);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_HARDWARE_PORT_FRONT_PANEL_PORT, test_front_panel_port);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_HARDWARE_PORT_SUBPORT_ID, 0);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr) == cps_api_ret_code_OK);
    cps_api_action(&tr, obj);
    ASSERT_TRUE(cps_api_commit(&tr) == cps_api_ret_code_OK);

    dump_return_obj(tr);

    cps_api_transaction_close(&tr);
}

TEST(int_rpc_test, delete_virtual_interface)
{
    delete_interface_rpc(virt_ifname);
}

TEST(int_rpc_test, remap_phy_interface)
{
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_SET_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_SET_INTERFACE_INPUT_OPERATION,
                                DELL_BASE_IF_CMN_OPERATION_TYPE_UPDATE);
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_NAME, phy_ifname, strlen(phy_ifname) + 1);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_HARDWARE_PORT_FRONT_PANEL_PORT, test_front_panel_port);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_HARDWARE_PORT_SUBPORT_ID, 0);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr) == cps_api_ret_code_OK);
    cps_api_action(&tr, obj);
    ASSERT_TRUE(cps_api_commit(&tr) == cps_api_ret_code_OK);

    dump_return_obj(tr);

    cps_api_transaction_close(&tr);
}

int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
