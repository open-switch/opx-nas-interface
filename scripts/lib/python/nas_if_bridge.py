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


"""Module for creation of the Bridge Interface"""
import cps
import cps_object
import dn_base_ip_tool
import dn_base_br_tool
import nas_os_if_utils as nas_if
import nas_common_header as nas_comm
import nas_bridge_config_obj as if_bridge_config
import nas_mac_addr_utils as ma
import copy
import cps_utils

bridge_op_attr_name = 'bridge-domain/set-bridge/input/operation'

use_linux_path = False

virtual_network_mac_address = None
''' Helper Methods '''
def __if_present(name):
    """Method to check if the interface is present in linux"""
    if len(dn_base_ip_tool.get_if_details(name)) > 0:
        nas_if.log_info("Interface exists " + str(name))
        return True
    nas_if.log_info("Interface doesn't exist " + str(name))
    return False

def __create_br_if(br_name):
    """Method to create bridge interface in Linux"""
    if __if_present(br_name) is False and \
       dn_base_br_tool.create_br(br_name):
        nas_if.log_info("Successfully created Bridge Interface " + str(br_name))
        return True
    nas_if.log_err("Failed to create Bridge Interface " + str(br_name))
    return False

def __delete_br_if(br_name):
    """Method to delete bridge interface in Linux"""
    if __if_present(br_name) is True and \
       dn_base_br_tool.del_br(br_name) is False:
           nas_if.log_err("Failed to delete Bridge Interface " + str(br_name))
           return False
    nas_if.log_info("Successfully deleted Bridge Interface " + str(br_name))
    return True

def __add_member_interfaces_to_br(br_name, member_interfaces):
    """Method to add member interfaces to bridge in Linux"""
    rlist = {}
    for name in member_interfaces:
        if dn_base_br_tool.add_if(br_name, name) is False:
            nas_if.log_err("Failed to add member interface to Bridge " + str(name))
            return False, rlist
        nas_if.log_info("Successfully added member interface %s" % (str(name)))
        rlist[name] = member_interfaces[name]
    return True, rlist

def __remove_member_interfaces_from_br(br_name, member_interfaces):
    """Method to remove member interfaces from bridge in Linux"""
    rlist = {}
    for name in member_interfaces:
        if dn_base_br_tool.del_if(br_name, name) is False:
            nas_if.log_err("Failed to remove member_interface from Bridge " + str(name))
            return False, rlist
        nas_if.log_info("Successfully removed member interface %s" % (str(name)))
        rlist[name] = member_interfaces[name]
    return True, rlist

def _get_bridge_op_id(cps_obj):
    op_id_to_name_map = {1: 'create', 2: 'delete', 3: 'update', 4:'add-member', 5:'delete-member'}
    op_id = None
    try:
        op_id = cps_obj.get_attr_data(bridge_op_attr_name)
    except ValueError:
        nas_if.log_err('No operation attribute in object')
        return None
    if not op_id in op_id_to_name_map:
        nas_if.log_err('Invalid operation type '+str(op_id))
        return None
    return op_id_to_name_map[op_id]


''' Create, Update, Delete, Add-Members and Delete-Members '''
def create(cps_obj, params, cfg_obj):
    """Method to create the Bridge in Linux"""
    _member_interfaces = []

    while True:
        if __create_br_if(cfg_obj.name) is False:
            break
        ret, _member_interfaces = __add_member_interfaces_to_br(cfg_obj.name, cfg_obj.member_interfaces)
        if ret is False:
            break
        return True

    nas_if.log_err("Create Bridge Failed, Rolling back")
    __remove_member_interfaces_from_br(cfg_obj.name, _member_interfaces)
    __delete_br_if(cfg_obj.name)
    return False

def update(cps_obj, params, cfg_obj):
    """Method to update the Bridge in Linux"""
    return True

def delete(cps_obj, params, cfg_obj):
    """Method to delete the Bridge in Linux"""
    __remove_member_interfaces_from_br(cfg_obj.name, cfg_obj.member_interfaces)
    __delete_br_if(cfg_obj.name)
    return True

def add_members(cps_obj, params, cfg_obj):
    """Method to add member interfaces to the Bridge in Linux"""
    member_interfaces = _member_interfaces = []
    ret, member_interfaces = if_bridge_config.get_member_interfaces_list(cps_obj)

    while True:
        ret, _member_interfaces = __add_member_interfaces_to_br(cfg_obj.name, member_interfaces)
        if ret is False:
            break
        return True

    nas_if.log_err("Update Bridge Failed, Rolling back")
    __remove_member_interfaces_from_br(cfg_obj.name, _member_interfaces)
    return False

def delete_members(cps_obj, params, cfg_obj):
    """Method to delete member interfaces from the Bridge in Linux"""
    member_interfaces = _member_interfaces = []
    ret, member_interfaces = if_bridge_config.get_member_interfaces_list(cps_obj)

    while True:
        ret, _member_interfaces = __remove_member_interfaces_from_br(cfg_obj.name, member_interfaces)
        if ret is False:
            break
        return True

    nas_if.log_err("Update Bridge Failed, Rolling back")
    __add_member_interfaces_to_br(cfg_obj.name, _member_interfaces)
    return False

def _send_obj_to_base(cps_obj,op):
    nas_if.log_info('Sending bridge object to base')
    in_obj = copy.deepcopy(cps_obj)
    in_obj.set_key(cps.key_from_name('target', 'bridge-domain/bridge'))
    obj = in_obj.get()

    if 'operation' in obj:
        del obj['operation']

    if op in ['update','add-member','delete-member']:
        op = 'set'

    upd = (op, obj)
    _ret = cps_utils.CPSTransaction([upd]).commit()
    if not _ret:
        nas_if.log_err('BASE transaction for vlan sub interface failed')
        return False
    return True

def _handle_bridge_intf(cps_obj, params):
    """Method to handle Bridge interface set, create or delete operations"""
    op = _get_bridge_op_id(cps_obj)
    if op is None:
        return False

    global virtual_network_mac_address
    cps_obj.add_attr('bridge-domain/bridge/phys-address', virtual_network_mac_address)
    if not use_linux_path:
        return _send_obj_to_base(cps_obj,op)

    cfg_obj = if_bridge_config.create(cps_obj)
    if cfg_obj is None:
        return False

    if op == 'create' and create(cps_obj, params, cfg_obj) is True:
        return if_bridge_config.cache_add(cfg_obj.name, cfg_obj)
    if op == 'delete' and delete(cps_obj, params, cfg_obj) is True:
        return if_bridge_config.cache_del(cfg_obj.name)
    if op == 'update' and update(cps_obj, params, cfg_obj) is True:
        return if_bridge_config.cache_update(cfg_obj.name, cfg_obj)
    if op == 'add-member' and add_members(cps_obj, params, cfg_obj) is True:
        return if_bridge_config.cache_update(cfg_obj.name, cfg_obj)
    if op == 'delete-member' and delete_members(cps_obj, params, cfg_obj) is True:
        return if_bridge_config.cache_update(cfg_obj.name, cfg_obj)
    return False

def set_bridge_rpc_cb(methods, params):
    if params['operation'] != 'rpc':
        nas_if.log_err('Operation '+str(params['operation'])+' not supported')
        return False
    cps_obj = cps_object.CPSObject(obj = params['change'])
    return _handle_bridge_intf(cps_obj, params)

def set_bridge_cb(methods, params):
    try:
        return set_bridge_rpc_cb(methods, params)
    except:
        logging.exception('Bridge error')

def nas_bridge_cps_regster(handle):
    global virtual_network_mac_address
    # Fetch Virtual network MAC address
    if_type = 'virtual-network'
    virtual_network_mac_address, _ = ma.get_intf_mac_addr(if_type, None)
    d = {}
    d['transaction'] = set_bridge_cb
    cps.obj_register(handle, nas_comm.yang.get_value('set_bridge_key', 'keys_id'), d)
