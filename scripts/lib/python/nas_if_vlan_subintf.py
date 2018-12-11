#!/usr/bin/python
# Copyright (c) 2018 Dell Inc.
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


"""Module for creation of the VxLAN Interface"""
import dn_base_ip_tool
import nas_os_if_utils as nas_if
import nas_vlan_subintf_config_obj as if_vlan_subintf_config
import copy
import cps_utils
import cps

use_linux_path = False

op_attr_name = 'dell-base-if-cmn/set-interface/input/operation'

''' Helper Methods '''
def _get_op_id(cps_obj):
    op_id_to_name_map = {1: 'create', 2: 'delete', 3: 'set'}
    op_id = None
    try:
        op_id = cps_obj.get_attr_data(op_attr_name)
    except ValueError:
        nas_if.log_err('No operation attribute in object')
        return None
    if not op_id in op_id_to_name_map:
        nas_if.log_err('Invalid operation type '+str(op_id))
        return None
    return op_id_to_name_map[op_id]

def __create_vlan_sub_if(parent_intf, vlan_id):
    """Method to create VLAN SubInterface in Linux"""
    vlan_subintf_name = str(parent_intf) + '.' + str(vlan_id)
    if dn_base_ip_tool.configure_vlan_tag(parent_intf, vlan_id) is False:
        nas_if.log_err("Failed to configure VLAN tag for " + str(vlan_subintf_name))
        return False
    nas_if.log_info("Successfully created VLAN SubInterface " + str(vlan_subintf_name))
    return True

def __delete_vlan_sub_if(parent_intf, vlan_id):
    """Method to delete VLAN SubInterface in Linux"""
    vlan_subintf_name = str(parent_intf) + '.' + str(vlan_id)
    if dn_base_ip_tool.delete_if(vlan_subintf_name) is False:
        nas_if.log_err("Failed to delete VLAN SubInterface for " + str(vlan_subintf_name))
        return False
    nas_if.log_info("Successfully deleted VLAN SubInterface " + str(vlan_subintf_name))
    return True


''' Create, Update and Delete '''
def create(cps_obj, params, cfg_obj):
    """Method to create the VTEP in Linux"""
    return __create_vlan_sub_if(cfg_obj.parent_intf, cfg_obj.vlan_id)

def update(cps_obj, params, cfg_obj):
    """Method to set attributes for the VTEP in Linux"""
    return False

def delete(cps_obj, params, cfg_obj):
    """Method to delete the VTEP in Linux"""
    return __delete_vlan_sub_if(cfg_obj.parent_intf, cfg_obj.vlan_id)

def _send_obj_to_base(cps_obj,op):
    nas_if.log_info('Sending vlan sub interface object to base')
    in_obj = copy.deepcopy(cps_obj)
    in_obj.set_key(cps.key_from_name('target', 'dell-base-if-cmn/if/interfaces/interface'))
    obj = in_obj.get()
    if 'operation' in obj:
        del obj['operation']
    upd = (op, obj)
    _ret = cps_utils.CPSTransaction([upd]).commit()
    if not _ret:
        nas_if.log_err('BASE transaction for vlan sub interface failed')
        return False
    return True

def handle_vlan_sub_intf(cps_obj, params):
    """Method to handle VLAN SubInterface interface set, create or delete operations"""
    op = _get_op_id(cps_obj)
    if op is None:
        return False

    if not use_linux_path:
        return _send_obj_to_base(cps_obj,op)

    cfg_obj = if_vlan_subintf_config.create(cps_obj)
    if cfg_obj is None:
        return False

    if op == 'create' and create(cps_obj, params, cfg_obj) is True:
        return if_vlan_subintf_config.cache_add(cfg_obj.name, cfg_obj)
    if op == 'delete' and delete(cps_obj, params, cfg_obj) is True:
        return if_vlan_subintf_config.cache_del(cfg_obj.name)
    if op == 'set' and update(cps_obj, params, cfg_obj) is True:
        return if_vlan_subintf_config.cache_update(cfg_obj.name, cfg_obj)
    return False
