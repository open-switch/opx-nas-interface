#!/usr/bin/python
# Copyright (c) 2015 Dell Inc.
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

if_mac_info_cache = {}
def get_mac_addr_base_range(if_type):
    if len(if_mac_info_cache) == 0:
        try:
            cfg = ET.parse('/etc/opx/mac_address_alloc.xml')
        except IOError:
            nas_if.log_err('No mac address config file')
            return None
        root = cfg.getroot()
        for i in root.findall('interface'):
            type_name = i.get('type')
            base = int(i.get('base-offset'))
            off_range = int(i.get('offset-range'))
            if_mac_info_cache[type_name] = (base, off_range)
            nas_if.log_info('%-15s: base %d range %d' % (type_name, base, off_range))
    if not if_type in if_mac_info_cache:
        nas_if.log_err('No mac address setting for type %s' % if_type)
        return None
    return if_mac_info_cache[if_type]

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
            front_panel_port = cps_obj.get_attr_data(nas_comm.get_value(
                                                     nas_comm.attr_name, 'fp_port'))
        except ValueError:
            nas_if.log_info('Create virtual interface without mac address assigned')
            return None
        try:
            subport_id = cps_obj.get_attr_data(nas_comm.get_value(
                                               nas_comm.attr_name, 'subport_id'))
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
            vlan_id = cps_obj.get_attr_data(nas_comm.get_value(
                                            nas_comm.attr_name, 'vlan_id'))
        except ValueError:
            nas_if.log_err('Input object does not contain VLAN id attribute')
            return None
        ret_list['vlan_id'] = vlan_id
    elif if_type == 'lag':
        try:
            lag_name = cps_obj.get_attr_data(nas_comm.get_value(
                                             nas_comm.attr_name, 'if_name'))
        except ValueError:
            nas_if.log_err('Input object does not contain name attribute')
            return None
        lag_id = nas_if.get_lag_id_from_name(lag_name)
        ret_list['lag_id'] = lag_id
    else:
        nas_if.log_err('Unknown interface type %s' % if_type)
        return None
    return ret_list

def if_get_mac_addr(if_type, fp_mac_offset = None, vlan_id = None, lag_id = None):
    base_mac = get_base_mac_addr()
    base_range = get_mac_addr_base_range(if_type)
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
    elif if_type == 'vlan':
        if vlan_id == None:
            nas_if.log_err('No VLAN id for VLAN port')
            return None
        mac_offset = get_mac_offset(base_offset, addr_range, vlan_id)
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
    if mac_addr == None:
        nas_if.log_err('Failed to calculate mac address with offset')
        return None
    return mac_addr
