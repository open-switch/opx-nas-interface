#!/usr/bin/python
# Copyright (c) 2019 Dell Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License. You may obtain
# a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# THIS CODE IS PROVIDED ON AN *AS IS* BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
# LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
# FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
#
# See the Apache Version 2.0 License for specific language governing
# permissions and limitations under the License.

import nas_os_if_utils as nas_if
import xml.etree.ElementTree as ET
import copy
import nas_fp_port_utils as fp_utils
import nas_common_header as nas_comm

appl_mac_info_cache = {}

def get_mac_addr_base_range(appl_name, type_name):

    def add_node_to_cache(appl, xml_node):
        n_type = xml_node.get('type')
        n_base = int(xml_node.get('base-offset'))
        n_range = int(xml_node.get('offset-range'))
        nas_if.log_info('%-15s: base %d range %d' % (n_type, n_base, n_range))
        appl_mac_info_cache[(appl, n_type)] = (n_base, n_range)

    if len(appl_mac_info_cache) == 0:
        try:
            cfg = ET.parse('/etc/opx/mac_address_alloc.xml')
        except IOError:
            nas_if.log_err('No mac address config file')
            return None
        root = cfg.getroot()
        for node in root.findall('interface'):
            add_node_to_cache('interface', node)
        for node in root.findall('application'):
            appl = node.get('name')
            for sub_node in node.findall('mac-range'):
                add_node_to_cache(appl, sub_node)

    if not (appl_name, type_name) in appl_mac_info_cache:
        nas_if.log_err('No mac address setting for application %s and type %s' %
                       (appl_name, type_name))
        return None
    return appl_mac_info_cache[(appl_name, type_name)]

def get_offset_mac_addr(base_addr, offset):
    if isinstance(base_addr, str):
        if base_addr.find(':') >= 0:
            base_addr = ''.join(base_addr.split(':'))
        arr = [int(base_addr[i:i+2],16) for i in range(0, len(base_addr), 2)]
    elif isinstance(base_addr, bytearray):
        arr = copy.copy(base_addr)
    else:
        nas_if.log_err('Invalid mac address type')
        return None
    idx = len(arr)
    while idx > 0:
        addr_num = arr[idx - 1] + offset
        arr[idx - 1] = addr_num % 256
        offset = addr_num / 256
        if offset == 0:
            break
        idx -= 1
    return ':'.join('%02x' % x for x in arr)

base_mac_addr = None
def get_base_mac_addr():
    global base_mac_addr
    if base_mac_addr is None:
        base_mac_addr = nas_if.get_base_mac_address()
    return base_mac_addr

def get_alloc_mac_addr_params(if_type, cps_obj, fp_cache = None):
    ret_list = {'if_type': if_type}

    if if_type == 'front-panel':
        try:
            front_panel_port = cps_obj.get_attr_data(nas_comm.yang.get_value('fp_port', 'attr_name'))
        except ValueError:
            nas_if.log_info('Create virtual interface without mac address assigned')
            return None
        try:
            subport_id = cps_obj.get_attr_data(nas_comm.yang.get_value('subport_id', 'attr_name'))
        except ValueError:
            subport_id = 0
        try:
            mac_offset = fp_utils.get_mac_offset_from_fp(front_panel_port, subport_id,
                                                         fp_cache)
        except ValueError as inst:
            nas_if.log_err('Failed to get mac address from <%d %d>' %
                           (front_panel_port, subport_id))
            nas_if.log_info(inst.args[0])
            return None
        ret_list['fp_mac_offset'] = mac_offset
    elif if_type == 'vlan':
        try:
            vlan_id = cps_obj.get_attr_data(nas_comm.yang.get_value('vlan_id', 'attr_name'))
        except ValueError:
            nas_if.log_err('Input object does not contain VLAN id attribute')
            return None
        ret_list['vlan_id'] = vlan_id
    elif if_type == 'lag':
        try:
            lag_name = cps_obj.get_attr_data(nas_comm.yang.get_value('if_name', 'attr_name'))
        except ValueError:
            nas_if.log_err('Input object does not contain name attribute')
            return None
        lag_id = nas_if.get_lag_id_from_name(lag_name)
        ret_list['lag_id'] = lag_id
    elif if_type == 'virtual-network':
        pass
    else:
        nas_if.log_err('Unknown interface type %s' % if_type)
        return None
    return ret_list

def if_get_mac_addr(if_type, fp_mac_offset = None, vlan_id = None, lag_id = None):
    base_mac = get_base_mac_addr()
    base_range = get_mac_addr_base_range('interface', if_type)
    if base_range is None:
        nas_if.log_err('Failed to get mac addr base and range for if type %s' % if_type)
        return None
    (base_offset, addr_range) = base_range
    if addr_range <= 0:
        nas_if.log_info('Bypass mac address setup for if type %s' % if_type)
        return ''
    mac_offset = 0
    get_mac_offset = lambda boff, brange, val: boff + val % brange
    if if_type == 'front-panel':
        if fp_mac_offset is None:
            nas_if.log_err('No mac offset input for front panel port')
            return None
        if fp_mac_offset > 0:
            # MAC offset is decremented because it is always 1 based
            # in platform config file
            fp_mac_offset -= 1
        if fp_mac_offset > addr_range:
            nas_if.log_err('Input mac offset for front panel port %d is bigger than range %d' %
                           (fp_mac_offset, addr_range))
            return None
        mac_offset = fp_mac_offset + base_offset
    elif if_type == 'vlan' or if_type  == 'virtual-network':
        mac_offset = get_mac_offset(base_offset, addr_range, 1)
    elif if_type == 'lag':
        if lag_id == None:
            nas_if.log_err('No LAG id for LAG port')
            return None
        mac_offset = get_mac_offset(base_offset, addr_range, lag_id)
    elif if_type == 'management':
        mac_offset = base_offset
    else:
        nas_if.log_err('if type %s not supported' % if_type)
        return None

    mac_addr = get_offset_mac_addr(base_mac, mac_offset)
    if mac_addr is None:
        nas_if.log_err('Failed to calculate mac address with offset')
        return None
    return (mac_addr, addr_range)

def get_intf_mac_addr(if_type, cps_obj, fp_cache = None):
    param_list = get_alloc_mac_addr_params(if_type, cps_obj, fp_cache)
    if param_list is None:
        nas_if.log_err('No enough attributes in input object to get mac address')
        return None
    return if_get_mac_addr(**param_list)

def get_appl_mac_addr(appl_name, type_name, offset = 0):
    base_mac = get_base_mac_addr()
    base_range = get_mac_addr_base_range(appl_name, type_name)
    if base_range is None:
        nas_if.log_err('Failed to get mac addr base and range for application %s type %s' %
                       (appl_name, type_name))
        return None
    (base_offset, addr_range) = base_range
    if offset >= addr_range:
        nas_if.log_err('Required offset %d should be smaller than configured range %d' % (offset, addr_range))
        return None
    mac_addr = get_offset_mac_addr(base_mac, base_offset + offset)
    if mac_addr is None:
        nas_if.log_err('Failed to calculate mac address with offset')
        return None
    return (mac_addr, addr_range)
