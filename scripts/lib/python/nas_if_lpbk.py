#!/usr/bin/python
# Copyright (c) 2019 Dell Inc.
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


import cps
import cps_object
import cps_utils
import event_log as ev
import dn_base_ip_tool

import ifindex_utils
import nas_mac_addr_utils as ma
import nas_os_if_utils as nas_if
import nas_common_header as nas_comm

def  set_loopback_interface(obj):
    name = None
    try:
        name = obj.get_attr_data('if/interfaces/interface/name')
    except:
        pass
    if name is None:
        nas_if.log_err("Failed to create interface without name")
        return False

    try:
        mtu = obj.get_attr_data('dell-if/if/interfaces/interface/mtu')
        if dn_base_ip_tool.set_if_mtu(name, mtu) == False:
            nas_if.log_err("Failed to execute request..." + str(res))
            return False
    except:
        pass

    try:
        state = obj.get_attr_data('if/interfaces/interface/enabled')
        if dn_base_ip_tool.set_if_state(name, state) == False:
            nas_if.log_err(('Failed to set the interface state.', name, state))
            return False
    except:
        pass
    try:
        mac_str = obj.get_attr_data('dell-if/if/interfaces/interface/phys-address')
        if dn_base_ip_tool.set_if_mac(name, mac_str) == False:
            nas_if.log_err(('Failed to set the interface mac.' +str(name)+' , ' +str(mac_str)))
            return False
    except:
        pass

    return True

def create_loopback_interface(obj, params):
    name = None
    mac_str = None
    mac = None
    try:
        name = obj.get_attr_data('if/interfaces/interface/name')
        mac_str = obj.get_attr_data('dell-if/if/interfaces/interface/phys-address')
    except:
        pass
    if name is None:
        nas_if.log_err("Failed to create interface without name")
        return False
    nas_if.log_info("interface name is" +str(name))
    lst = dn_base_ip_tool.get_if_details(name)
    if (len(lst)) > 0:
        nas_if.log_err("Interface already exists" +str(name))
        return False
    if mac_str is None:
        mac_str = ma.get_offset_mac_addr(ma.get_base_mac_addr(), 0)
    rc = dn_base_ip_tool.create_loopback_if(name, mac=mac_str)
    if rc:
        nas_if.log_info("loopback interface is created" +str(name))
        rc = set_loopback_interface(obj)
        if_index = ifindex_utils.if_nametoindex(name)
        obj.add_attr(nas_comm.yang.get_value('if_index', 'attr_name'), if_index)
        params['change'] = obj.get()
    return rc

def delete_loopback_interface(obj):
    name = None
    try:
        name = obj.get_attr_data('if/interfaces/interface/name')
    except:
        return False
    return dn_base_ip_tool.delete_if(name)

