#!/usr/bin/python
# Copyright (c) 2017 DellEMC Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License. You may obtain
# a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
# LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
# FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
#
# See the Apache Version 2.0 License for specific language governing
# permissions and limitations under the License.

import cps_object
import dn_base_ip_tool

import nas_os_if_utils as nas_if


vrf_name = 'ni/if/interfaces/interface/bind-ni-name'
if_name = 'if/interfaces/interface/name'
parent_interface = 'dell-if/if/interfaces/interface/parent-interface'
physical_addr = 'dell-if/if/interfaces/interface/phys-address'
if_state = 'if/interfaces/interface/enabled'


def create_macvlan_interface(obj):
    data = obj.get()['data']
    nas_if.log_info("data = " + str(data))
    vrf = 'default'

    #Check if attributes are present in the object or not
    if if_name not in data or parent_interface not in data or physical_addr not in data:
        nas_if.log_err("Missing Macvlan interface, or parent interface or mac address attribute")
        return False

    if vrf_name in data:
        vrf = obj.get_attr_data(vrf_name)

    #Get the attribute data
    name = obj.get_attr_data(if_name)
    parent_if = obj.get_attr_data(parent_interface)
    mac_addr = obj.get_attr_data(physical_addr)

    #Check if the attributes are empty strings or not
    if name is None or parent_if is None or mac_addr is None:
        nas_if.log_err("Empty Maclvan interface, or parent interface, or mac address")
        return False

    # Check if interface already exists
    lst = dn_base_ip_tool.get_if_details(vrf, name)
    if (len(lst)) > 0:
        nas_if.log_err("Interface " + str(name) + " already exists")
        return False

    rc = dn_base_ip_tool.create_macvlan_if(name, parent_if, mac_addr, vrf)
    if rc:
        nas_if.log_info("Macvlan interface " + str(name) + " is created")
        rc = set_macvlan_interface(obj)

    return rc


def set_macvlan_interface(obj):
    data = obj.get()['data']
    nas_if.log_info("data = " + str(data))
    vrf = 'default'

    if if_name not in data:
        nas_if.log_err("Missing Maclvan interface name")
        return False
    name = obj.get_attr_data(if_name)
    if name is None:
        nas_if.log_err("Empty Macvlan interface name")
    if vrf_name in data:
        vrf = obj.get_attr_data(vrf_name)

    if physical_addr in data:
        mac_addr = obj.get_attr_data(physical_addr)
        if dn_base_ip_tool.set_if_mac(name, mac_addr, vrf) == False:
            nas_if.log_err("Failed to set MAC address for " + str(name))
            return False

    if if_state in data:
        state = obj.get_attr_data(if_state)
        if dn_base_ip_tool.set_if_state(name, state, vrf) == False:
            nas_if.log_err("Failed to set the interface state for " + str(name))
            return False
    return True

def delete_macvlan_interface(obj):
    data = obj.get()['data']
    nas_if.log_info("data = " + str(data))
    vrf = 'default'

    if if_name not in data:
        nas_if.log_err("Missing Maclvan interface name")
        return False
    name = obj.get_attr_data(if_name)
    if name is None:
        nas_if.log_err("Empty Macvlan interface name")
    if vrf_name in data:
        vrf = obj.get_attr_data(vrf_name)

    nas_if.log_info("Macvlan interface to be deleted = " + str(name))
    return dn_base_ip_tool.delete_if(name, vrf)
