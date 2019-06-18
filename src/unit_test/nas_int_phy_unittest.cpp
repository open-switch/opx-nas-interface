/*
 * Copyright (c) 2019 Dell Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 * LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 * FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 * See the Apache Version 2.0 License for specific language governing
 * permissions and limitations under the License.
 */

/*
 * nas_int_phy_unittest.cpp
 *
 */

#include "cps_api_object.h"
#include "cps_api_operation.h"
#include "cps_api_object_category.h"
#include "cps_api_events.h"
#include "hal_interface_defaults.h"
#include "hal_if_mapping.h"
#include "cps_class_map.h"
#include "cps_api_object_key.h"
#include "dell-base-if-phy.h"
#include "dell-base-if.h"
#include "dell-interface.h"
#include "dell-base-common.h"
#include "iana-if-type.h"
#include "std_utils.h"

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <net/if.h>

using namespace std;

unordered_map<BASE_IF_SPEED_t, const char*, hash<int>> speed_name_map = {
    {BASE_IF_SPEED_0MBPS, "0M"},
    {BASE_IF_SPEED_10MBPS, "10M"},
    {BASE_IF_SPEED_100MBPS, "100M"},
    {BASE_IF_SPEED_1GIGE, "1G"},
    {BASE_IF_SPEED_10GIGE, "10G"},
    {BASE_IF_SPEED_20GIGE, "20G"},
    {BASE_IF_SPEED_25GIGE, "25G"},
    {BASE_IF_SPEED_40GIGE, "40G"},
    {BASE_IF_SPEED_50GIGE, "50G"},
    {BASE_IF_SPEED_100GIGE, "100G"},
    {BASE_IF_SPEED_AUTO, "Auto"}
};

static const char* get_speed_name(BASE_IF_SPEED_t speed)
{
    if (speed_name_map.find(speed) == speed_name_map.end()) {
        return "Unknown";
    }
    return speed_name_map.at(speed);
}

using hwport_list_t = vector<uint32_t>;
using phy_port_list_t = vector<npu_port_t>;

typedef struct {
    uint32_t media_id;
    hwport_list_t hwport_list;
    set<npu_port_t> phy_port_list;
    uint32_t control_port;
    string dft_name;
    uint32_t mac_offset;
} front_panel_port_t;

using fp_port_cache_t = unordered_map<uint32_t, front_panel_port_t>;

typedef struct {
    uint32_t control_port;
    uint32_t subport_id;
    uint32_t front_panel_port;
} hardware_port_t;

using hw_port_cache_t = unordered_map<uint32_t, hardware_port_t>;

typedef struct {
    uint32_t front_panel_port;
    BASE_IF_SPEED_t speed;
    hwport_list_t hwport_list;
} physical_port_t;

using phy_port_cache_t = unordered_map<npu_port_t, physical_port_t>;

using hw_to_phy_port_map_t = unordered_map<uint32_t, npu_port_t>;

static void get_hw_to_phy_map(hw_to_phy_port_map_t& port_map,
                              const phy_port_cache_t& phy_cache)
{
    for (auto& item: phy_cache) {
        for (auto hwport: item.second.hwport_list) {
            port_map[hwport] = item.first;
        }
    }
}

using hw_to_index_map_t = unordered_map<uint32_t, size_t>;

static void get_hw_to_index_map(hw_to_index_map_t& index_map,
                                const hw_port_cache_t& hw_cache,
                                const fp_port_cache_t& fp_cache)
{
    for (auto& hw_item: hw_cache) {
        uint32_t hw_port = hw_item.first;
        uint32_t fp_port = hw_item.second.front_panel_port;
        if (fp_cache.find(fp_port) != fp_cache.end()) {
            auto& fp_item = fp_cache.at(fp_port);
            size_t index = 0;
            bool found = false;
            for (uint32_t id: fp_item.hwport_list) {
                if (id == hw_port) {
                    found = true;
                    break;
                }
                index ++;
            }
            if (!found) {
                index = 0;
            }
            index_map[hw_port] = index;
        }
    }
}

static bool get_fp_port_cache(fp_port_cache_t& cache,
                              const phy_port_cache_t& phy_cache)
{
    hw_to_phy_port_map_t hw_port_map;
    get_hw_to_phy_map(hw_port_map, phy_cache);

    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);
    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    BASE_IF_PHY_FRONT_PANEL_PORT_OBJ,
                                    cps_api_qualifier_TARGET);
    if (cps_api_get(&gp) != cps_api_ret_code_OK) {
        cout << "Failed to get front panel port objects" << endl;
        cps_api_get_request_close(&gp);
        return false;
    }

    size_t mx = cps_api_object_list_size(gp.list);
    for (size_t ix = 0; ix < mx; ix ++) {
        obj = cps_api_object_list_get(gp.list, ix);
        cps_api_object_attr_t attr = cps_api_get_key_data(obj,
                                        BASE_IF_PHY_FRONT_PANEL_PORT_FRONT_PANEL_PORT);
        if (attr == nullptr) {
            cout << "Can't get port id from object " << ix << endl;
            continue;
        }
        uint32_t port_id = cps_api_object_attr_data_u32(attr);
        front_panel_port_t port_info = {};
        cps_api_object_it_t it;
        cps_api_object_it_begin(obj, &it);
        for (; cps_api_object_it_valid(&it); cps_api_object_it_next(&it)) {
            cps_api_attr_id_t id = cps_api_object_attr_id(it.attr);
            switch(id) {
            case BASE_IF_PHY_FRONT_PANEL_PORT_MEDIA_ID:
                port_info.media_id = cps_api_object_attr_data_u32(it.attr);
                break;
            case BASE_IF_PHY_FRONT_PANEL_PORT_PORT:
            {
                uint32_t hw_port = cps_api_object_attr_data_u32(it.attr);
                port_info.hwport_list.push_back(hw_port);
                if (hw_port_map.find(hw_port) == hw_port_map.end()) {
                    break;
                }
                port_info.phy_port_list.insert(hw_port_map[hw_port]);
                break;
            }
            case BASE_IF_PHY_FRONT_PANEL_PORT_CONTROL_PORT:
                port_info.control_port = cps_api_object_attr_data_u32(it.attr);
                break;
            case BASE_IF_PHY_FRONT_PANEL_PORT_DEFAULT_NAME:
                port_info.dft_name = (char *)cps_api_object_attr_data_bin(it.attr);
                break;
            case BASE_IF_PHY_FRONT_PANEL_PORT_MAC_OFFSET:
                port_info.mac_offset = cps_api_object_attr_data_u32(it.attr);
                break;
            }
        }
        cache[port_id] = port_info;
    }

    cps_api_get_request_close(&gp);
    return true;
}

static bool get_hw_port_cache(hw_port_cache_t& cache)
{
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);
    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    BASE_IF_PHY_HARDWARE_PORT_OBJ,
                                    cps_api_qualifier_TARGET);
    if (cps_api_get(&gp) != cps_api_ret_code_OK) {
        cout << "Failed to get hardware port objects" << endl;
        cps_api_get_request_close(&gp);
        return false;
    }

    size_t mx = cps_api_object_list_size(gp.list);
    for (size_t ix = 0; ix < mx; ix ++) {
        obj = cps_api_object_list_get(gp.list, ix);
        cps_api_object_attr_t attr = cps_api_get_key_data(obj,
                                        BASE_IF_PHY_HARDWARE_PORT_HW_PORT);
        if (attr == nullptr) {
            cout << "Can't get port id from object " << ix << endl;
            continue;
        }
        uint32_t port_id = cps_api_object_attr_data_u32(attr);
        hardware_port_t port_info = {};
        cps_api_object_it_t it;
        cps_api_object_it_begin(obj, &it);
        for (; cps_api_object_it_valid(&it); cps_api_object_it_next(&it)) {
            cps_api_attr_id_t id = cps_api_object_attr_id(it.attr);
            switch(id) {
            case BASE_IF_PHY_HARDWARE_PORT_HW_CONTROL_PORT:
                port_info.control_port = cps_api_object_attr_data_u32(it.attr);
                break;
            case BASE_IF_PHY_HARDWARE_PORT_SUBPORT_ID:
                port_info.subport_id = cps_api_object_attr_data_u32(it.attr);
                break;
            case BASE_IF_PHY_HARDWARE_PORT_FRONT_PANEL_PORT:
                port_info.front_panel_port = cps_api_object_attr_data_u32(it.attr);
                break;
            }
        }
        cache[port_id] = port_info;
    }

    cps_api_get_request_close(&gp);
    return true;
}

static bool get_phy_port_cache(phy_port_cache_t& cache)
{
    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);
    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(gp.filters);
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    BASE_IF_PHY_PHYSICAL_OBJ,
                                    cps_api_qualifier_TARGET);
    if (cps_api_get(&gp) != cps_api_ret_code_OK) {
        cout << "Failed to get physical port objects" << endl;
        cps_api_get_request_close(&gp);
        return false;
    }

    size_t mx = cps_api_object_list_size(gp.list);
    for (size_t ix = 0; ix < mx; ix ++) {
        obj = cps_api_object_list_get(gp.list, ix);
        cps_api_object_attr_t attr = cps_api_get_key_data(obj,
                                        BASE_IF_PHY_PHYSICAL_PORT_ID);
        if (attr == nullptr) {
            cout << "Can't get port id from object " << ix << endl;
            continue;
        }
        uint32_t port_id = cps_api_object_attr_data_u32(attr);
        physical_port_t port_info = {};
        cps_api_object_it_t it;
        cps_api_object_it_begin(obj, &it);
        for (; cps_api_object_it_valid(&it); cps_api_object_it_next(&it)) {
            cps_api_attr_id_t id = cps_api_object_attr_id(it.attr);
            switch(id) {
            case BASE_IF_PHY_PHYSICAL_FRONT_PANEL_NUMBER:
                port_info.front_panel_port = cps_api_object_attr_data_u32(it.attr);
                break;
            case BASE_IF_PHY_PHYSICAL_HARDWARE_PORT_LIST:
                port_info.hwport_list.push_back(
                    cps_api_object_attr_data_u32(it.attr));
                break;
            case BASE_IF_PHY_PHYSICAL_SPEED:
                port_info.speed = (BASE_IF_SPEED_t)cps_api_object_attr_data_u32(it.attr);
                break;
            }
        }
        cache[port_id] = port_info;
    }

    cps_api_get_request_close(&gp);
    return true;
}

uint32_t get_front_panel_port(const char *if_name,
                                     hal_ifindex_t ifindex)
{
    char name_buf[IF_NAMESIZE];
    if (if_name == nullptr) {
        if_name = if_indextoname(ifindex, name_buf);
        if (if_name == nullptr) {
            cout << "Failed to get interface name from ifindex " << ifindex << endl;
            return (uint32_t)-1;
        }
    } else {
        if (if_nametoindex(if_name) == 0) {
            cout << "Interface " << if_name << " not exist" << endl;
            return (uint32_t)-1;
        }
    }
    string name_str = if_name;
    string item;
    stringstream ss(name_str);
    vector<string> elems;
    while(getline(ss, item, '-')) {
        elems.push_back(item);
    }
    if (elems.size() < 3) {
        cout << "Invalid interface name format: " << name_str << endl;
        return (uint32_t) -1;
    }

    return stoul(elems[1]);
}

static bool create_phy_port(BASE_IF_SPEED_t speed,
                            hwport_list_t hwport_list,
                            npu_port_t& phy_port)
{
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    BASE_IF_PHY_PHYSICAL_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_PHYSICAL_NPU_ID, 0);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_PHYSICAL_SPEED, speed);
    for (auto hw_port: hwport_list) {
        cps_api_object_attr_add_u32(obj, BASE_IF_PHY_PHYSICAL_HARDWARE_PORT_LIST,
                                    hw_port);
    }

    cps_api_transaction_params_t tr;
    cps_api_transaction_init(&tr);
    cps_api_create(&tr, obj);
    if (cps_api_commit(&tr) != cps_api_ret_code_OK) {
        cout << "Failed to commit create request" << endl;
        cps_api_transaction_close(&tr);
        return false;
    }

    obj = cps_api_object_list_get(tr.change_list, 0);
    cps_api_object_attr_t attr = cps_api_object_attr_get(obj,
                                        BASE_IF_PHY_PHYSICAL_PORT_ID);
    if (attr == nullptr) {
        cout << "No port id attribute found" << endl;
        cps_api_transaction_close(&tr);
        return false;
    }
    phy_port = cps_api_object_attr_data_u32(attr);
    cps_api_transaction_close(&tr);

    return true;
}

static bool create_front_panel_port(uint32_t fp_port,
                                    BASE_IF_SPEED_t speed,
                                    const fp_port_cache_t& fp_cache,
                                    phy_port_list_t& phy_port_list)
{
    if (fp_cache.find(fp_port) == fp_cache.end()) {
        cout << "Front panel port " << fp_port << " not found in cache" << endl;
        return false;
    }
    auto& fp_item = fp_cache.at(fp_port);
    if (fp_item.hwport_list.size() != 4) {
        cout << "Front panel port doesn't contain 4 hardware lanes" << endl;
        return false;
    }

    vector<hwport_list_t> hwport_ll;
    switch(speed) {
    case BASE_IF_SPEED_10GIGE:
    case BASE_IF_SPEED_25GIGE:
        for (auto hw_port: fp_item.hwport_list) {
            hwport_list_t hp_list {hw_port};
            hwport_ll.push_back(hp_list);
        }
        break;
    case BASE_IF_SPEED_20GIGE:
    case BASE_IF_SPEED_50GIGE:
    {
        hwport_list_t hp_list_1 {fp_item.hwport_list[0], fp_item.hwport_list[1]};
        hwport_ll.push_back(hp_list_1);
        hwport_list_t hp_list_2 {fp_item.hwport_list[2], fp_item.hwport_list[3]};
        hwport_ll.push_back(hp_list_2);
        break;
    }
    case BASE_IF_SPEED_40GIGE:
    case BASE_IF_SPEED_100GIGE:
    {
        hwport_list_t hp_list;
        for (auto hw_port: fp_item.hwport_list) {
            hp_list.push_back(hw_port);
        }
        hwport_ll.push_back(hp_list);
        break;
    }
    default:
        cout << "Unsupported speed type: " << speed << endl;
        return false;
    }

    for (auto& hwport_list: hwport_ll) {
        npu_port_t phy_port = 0;
        cout << "Creating physical port" << endl;
        if (!create_phy_port(speed, hwport_list, phy_port)) {
            cout << "Failed to create physical port" << endl;
            return false;
        }
        cout << "Physical port " << phy_port << " created" << endl;
        phy_port_list.push_back(phy_port);
    }

    return true;
}

static bool delete_phy_port(npu_port_t phy_port)
{
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    BASE_IF_PHY_PHYSICAL_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_PHYSICAL_NPU_ID, 0);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_PHYSICAL_PORT_ID, phy_port);
    cps_api_transaction_params_t tr;
    cps_api_transaction_init(&tr);
    cps_api_delete(&tr, obj);
    if (cps_api_commit(&tr) != cps_api_ret_code_OK) {
        cout << "Failed to commit delete request" << endl;
        cps_api_transaction_close(&tr);
        return false;
    }

    cps_api_transaction_close(&tr);
    return true;
}

static bool delete_front_panel_port(uint32_t fp_port,
                                    const fp_port_cache_t& fp_cache)
{
    if (fp_cache.find(fp_port) == fp_cache.end()) {
        cout << "Front panel port " << fp_port << " not found in cache" << endl;
        return false;
    }

    auto& fp_item = fp_cache.at(fp_port);
    for (auto phy_port: fp_item.phy_port_list) {
        cout << "Deleting physical port " << phy_port << endl;
        if (!delete_phy_port(phy_port)) {
            cout << "Failed" << endl;
            return false;
        }
    }

    return true;
}

static bool create_logical_interface(uint32_t fp_port, uint32_t subport,
                                     npu_port_t phy_port,
                                     hal_ifindex_t& ifindex)
{
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_SET_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_SET_INTERFACE_INPUT_OPERATION, 1);
    char name_buf[20];
    char desc_buf[MAX_INTF_DESC_LEN];
    sprintf(name_buf, "e101-%03d-%d", fp_port, subport);
    sprintf(desc_buf, "Interface description for %s", name_buf);
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_NAME, name_buf,
                            strlen(name_buf) + 1);
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_DESCRIPTION, desc_buf, strlen(desc_buf) + 1);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_HARDWARE_PORT_FRONT_PANEL_PORT, fp_port);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_HARDWARE_PORT_SUBPORT_ID, subport);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_IF_INTERFACES_INTERFACE_NPU_ID, 0);
    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_IF_INTERFACES_INTERFACE_PORT_ID, phy_port);
    cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_MTU, 1532);
    cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_NEGOTIATION, 1);
    cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_SPEED, BASE_IF_SPEED_AUTO);
    cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_DUPLEX,
                                     BASE_CMN_DUPLEX_TYPE_AUTO);
    const char *if_type = "ianaift:ethernetCsmacd";
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_TYPE, if_type, strlen(if_type) + 1);

    cps_api_transaction_params_t tr;
    cps_api_transaction_init(&tr);
    cps_api_action(&tr, obj);
    if (cps_api_commit(&tr) != cps_api_ret_code_OK) {
        cout << "Failed to commit rpc request" << endl;
        cps_api_transaction_close(&tr);
        return false;
    }

    obj = cps_api_object_list_get(tr.change_list, 0);
    cps_api_object_attr_t attr = cps_api_object_attr_get(obj,
                                        DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
    if (attr == nullptr) {
        cout << "No ifindex attribute found" << endl;
        cps_api_transaction_close(&tr);
        return false;
    }
    ifindex = cps_api_object_attr_data_u32(attr);
    cps_api_transaction_close(&tr);

    return true;
}

static bool delete_logical_interface(const char *if_name)
{
    cps_api_object_t obj = cps_api_object_create();
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_SET_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_SET_INTERFACE_INPUT_OPERATION, 2);
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_NAME, if_name,
                            strlen(if_name) + 1);

    cps_api_transaction_params_t tr;
    cps_api_transaction_init(&tr);
    cps_api_action(&tr, obj);
    if (cps_api_commit(&tr) != cps_api_ret_code_OK) {
        cout << "Failed to commit rpc request" << endl;
        cps_api_transaction_close(&tr);
        return false;
    }

    cps_api_transaction_close(&tr);
    return true;
}

uint32_t g_fp_port_1 = 54, g_fp_port_2 = 53;
BASE_IF_SPEED_t g_if_speed = BASE_IF_SPEED_10GIGE;

TEST(nas_physical_interface, dump_interface)
{
    hw_port_cache_t hw_cache;
    ASSERT_TRUE(get_hw_port_cache(hw_cache));
    phy_port_cache_t phy_cache;
    ASSERT_TRUE(get_phy_port_cache(phy_cache));
    fp_port_cache_t fp_cache;
    ASSERT_TRUE(get_fp_port_cache(fp_cache, phy_cache));
    vector<uint32_t> fp_list;
    if (g_fp_port_1 == 0) {
        for (auto fp_item: fp_cache) {
            fp_list.push_back(fp_item.first);
        }
    } else {
        ASSERT_TRUE(fp_cache.find(g_fp_port_1) != fp_cache.end());
        fp_list.push_back(g_fp_port_1);
    }
    for (auto fp_id: fp_list) {
        auto& fp_item = fp_cache[fp_id];
        cout << "Interface " << fp_item.dft_name << " ";
        cout << "Front_Panel " << fp_id << endl;
        for (auto phy_port: fp_item.phy_port_list) {
            cout << "  Physical port: " << phy_port << endl;
            ASSERT_TRUE(phy_cache.find(phy_port) != phy_cache.end());
            for (auto hw_port: phy_cache[phy_port].hwport_list) {
                cout << "    Hardware port: " << hw_port << endl;
            }
        }
    }
}

static void ut_delete_interface(uint32_t fp_port, BASE_IF_SPEED_t& old_speed)
{
    phy_port_cache_t phy_cache;
    ASSERT_TRUE(get_phy_port_cache(phy_cache));
    fp_port_cache_t fp_cache;
    ASSERT_TRUE(get_fp_port_cache(fp_cache, phy_cache));
    ASSERT_TRUE(fp_cache.find(fp_port) != fp_cache.end());
    auto& fp_item = fp_cache.at(fp_port);
    old_speed = BASE_IF_SPEED_MAX;
    for (npu_port_t phy_port: fp_item.phy_port_list) {
        ASSERT_TRUE(phy_cache.find(phy_port) != phy_cache.end());
        auto& phy_item = phy_cache.at(phy_port);
        if (old_speed == BASE_IF_SPEED_MAX) {
            old_speed = phy_item.speed;
        } else {
            ASSERT_TRUE(old_speed == phy_item.speed);
        }
    }
    ASSERT_TRUE(delete_front_panel_port(fp_port, fp_cache));
}

static void ut_create_interface(uint32_t fp_port, BASE_IF_SPEED_t speed)
{
    phy_port_cache_t phy_cache;
    ASSERT_TRUE(get_phy_port_cache(phy_cache));
    fp_port_cache_t fp_cache;
    ASSERT_TRUE(get_fp_port_cache(fp_cache, phy_cache));
    ASSERT_TRUE(fp_cache.find(fp_port) != fp_cache.end());
    phy_port_list_t phy_port_list;
    ASSERT_TRUE(create_front_panel_port(fp_port, speed, fp_cache,
                                        phy_port_list));
}

BASE_IF_SPEED_t g_cur_speed = BASE_IF_SPEED_MAX;

TEST(nas_physical_interface, delete_interface_1)
{
    ut_delete_interface(g_fp_port_1, g_cur_speed);
}

TEST(nas_physical_interface, create_interface)
{
    ut_create_interface(g_fp_port_1, g_if_speed);
    cout << "Front panel port " << g_fp_port_1 << ": "
         << "speed changed from " << get_speed_name(g_cur_speed)
         << " to " << get_speed_name(g_if_speed) << endl;
}

TEST(nas_physical_interface, delete_interface_2)
{
    if (g_cur_speed != BASE_IF_SPEED_MAX) {
        g_if_speed = g_cur_speed;
    }
    ut_delete_interface(g_fp_port_1, g_cur_speed);
}

TEST(nas_physical_interface, create_interface_restore)
{
    ut_create_interface(g_fp_port_1, g_if_speed);
    cout << "Front panel port " << g_fp_port_1 << ": "
         << "speed restored from " << get_speed_name(g_cur_speed)
         << " to " << get_speed_name(g_if_speed) << endl;
}

static void ut_change_breakout_mode(uint32_t fp_port, BASE_IF_SPEED_t speed)
{
    hw_port_cache_t hw_cache;
    ASSERT_TRUE(get_hw_port_cache(hw_cache));
    phy_port_cache_t phy_cache;
    ASSERT_TRUE(get_phy_port_cache(phy_cache));
    fp_port_cache_t fp_cache;
    ASSERT_TRUE(get_fp_port_cache(fp_cache, phy_cache));
    ASSERT_TRUE(fp_cache.find(fp_port) != fp_cache.end());

    hw_to_index_map_t hw_idx_map;
    char name_buf[32];
    uint32_t subport = 0, hw_port = 0;
    auto& fp_item = fp_cache.at(fp_port);
    if (fp_item.phy_port_list.size() == 1) {
        sprintf(name_buf, "e101-%03d-0", fp_port);
        cout << "Deleting interface " << name_buf << endl;
        ASSERT_TRUE(delete_logical_interface(name_buf));
    } else {
        get_hw_to_index_map(hw_idx_map, hw_cache, fp_cache);
        for (npu_port_t phy_port: fp_item.phy_port_list) {
            ASSERT_TRUE(phy_cache.find(phy_port) != phy_cache.end());
            auto& phy_item = phy_cache.at(phy_port);
            ASSERT_TRUE(phy_item.hwport_list.size() == 1);
            hw_port = phy_item.hwport_list[0];
            cout << "Physical port " << phy_port << " Hardware port " << hw_port << endl;
            ASSERT_TRUE(hw_idx_map.find(hw_port) != hw_idx_map.end());
            subport = hw_idx_map.at(hw_port) + 1;
            sprintf(name_buf, "e101-%03d-%d", fp_port, subport);
            cout << "Deleting interface " << name_buf << endl;
            ASSERT_TRUE(delete_logical_interface(name_buf));
        }
    }

    cout << "Deleting front panel port " << fp_port << endl;
    ASSERT_TRUE(delete_front_panel_port(fp_port, fp_cache));
    cout << "Creating front panel port " << fp_port << endl;
    phy_port_list_t phy_port_list;
    ASSERT_TRUE(create_front_panel_port(fp_port, speed, fp_cache, phy_port_list));
    cout << "Created " << phy_port_list.size() << " physical port" << endl;

    phy_cache.clear();
    fp_cache.clear();
    hw_idx_map.clear();
    ASSERT_TRUE(get_phy_port_cache(phy_cache));
    ASSERT_TRUE(get_fp_port_cache(fp_cache, phy_cache));
    get_hw_to_index_map(hw_idx_map, hw_cache, fp_cache);
    hal_ifindex_t ifindex = 0;
    for (auto phy_port: phy_port_list) {
        ASSERT_TRUE(phy_cache.find(phy_port) != phy_cache.end());
        auto& phy_item = phy_cache.at(phy_port);
        if (phy_port_list.size() == 1) {
            subport = 0;
        } else {
            ASSERT_TRUE(phy_item.hwport_list.size() > 0);
            hw_port = phy_item.hwport_list[0];
            cout << "Physical port " << phy_port << " Hardware port " << hw_port << endl;
            ASSERT_TRUE(hw_idx_map.find(hw_port) != hw_idx_map.end());
            subport = hw_idx_map.at(hw_port) + 1;
            cout << "Creating logical interface for front panel port " << fp_port
                 << " subport " << subport << endl;
        }
        ASSERT_TRUE(create_logical_interface(fp_port, subport, phy_port, ifindex));
    }
}

TEST(nas_physical_interface, enable_breakout)
{
    ut_change_breakout_mode(g_fp_port_2, BASE_IF_SPEED_10GIGE);
}

TEST(nas_physical_interface, disable_breakout)
{
    ut_change_breakout_mode(g_fp_port_2, BASE_IF_SPEED_40GIGE);
}

int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);

    if (argc > 1) {
        if (strcmp(argv[1], "all") == 0) {
            g_fp_port_1 = 0;
        } else {
            char *port_str = strtok(argv[1], ",");
            if (port_str != nullptr) {
                g_fp_port_1 = strtoul(port_str, NULL, 0);
                port_str = strtok(NULL, ",");
                if (port_str != nullptr) {
                    g_fp_port_2 = strtoul(port_str, NULL, 0);
                }
            }
        }
    }
    if (argc > 2) {
        if (strcmp(argv[2], "10g") == 0) {
            g_if_speed = BASE_IF_SPEED_10GIGE;
        } else if (strcmp(argv[2], "20g") == 0) {
            g_if_speed = BASE_IF_SPEED_20GIGE;
        } else if (strcmp(argv[2], "25g") == 0) {
            g_if_speed = BASE_IF_SPEED_25GIGE;
        } else if (strcmp(argv[2], "40g") == 0) {
            g_if_speed = BASE_IF_SPEED_40GIGE;
        } else if (strcmp(argv[2], "50g") == 0) {
            g_if_speed = BASE_IF_SPEED_50GIGE;
        } else if (strcmp(argv[2], "100g") == 0) {
            g_if_speed = BASE_IF_SPEED_100GIGE;
        } else {
            cout << "Unsupported speed " << argv[2] << ", use default" << endl;
        }
    }

    return RUN_ALL_TESTS();
}
