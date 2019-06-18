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


"""Module for creation of the VxLAN Interface"""
import dn_base_ip_tool
import dn_base_br_tool
import ifindex_utils
import nas_os_if_utils as nas_if
import nas_vtep_config_obj as if_vtep_config
import copy
import cps_utils
import cps

op_attr_name = 'dell-base-if-cmn/set-interface/input/operation'

use_linux_path = False

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

def __if_present(name):
    """Method to check if the interface is present in linux"""
    if len(dn_base_ip_tool.get_if_details(name)) > 0:
        nas_if.log_info("Interface exists " + str(name))
        return True
    nas_if.log_info("Interface doesn't exist " + str(name))
    return False

def __create_vtep_if(vtep_name, vni, src_ip, addr_family):
    """Method to create vtep interface in Linux"""
    if __if_present(vtep_name) is False and \
       dn_base_ip_tool.create_vxlan_if(str(vtep_name), str(vni),
                                       str(src_ip), addr_family) is True:
        nas_if.log_info("Successfully created VTEP Interface " + str(vtep_name))
        if_index = ifindex_utils.if_nametoindex(vtep_name)
        return True, if_index
    nas_if.log_err("Failed to create VTEP Interface " + str(vtep_name))
    return False, None

def __delete_vtep_if(vtep_name):
    """Method to delete vtep interface in Linux"""
    if __if_present(vtep_name) is True and \
       dn_base_ip_tool.delete_if(vtep_name):
            nas_if.log_info("Successfully deleted VTEP Interface " + str(vtep_name))
            return True
    nas_if.log_err("Failed to delete VTEP Interface " + str(vtep_name))
    return False

def __add_remote_endpoints(vtep_name, remote_endpoints):
    """Method to Add Remote Endpoints for the VxLAN in Linux"""
    rlist = {}
    for src_ip in remote_endpoints:
        remote_endpoint = remote_endpoints[src_ip]
        if remote_endpoint.flooding_enabled is 1:
            if dn_base_br_tool.add_learnt_mac_to_vtep_fdb(vtep_name, src_ip, \
                                                          remote_endpoint.addr_family, \
                                                          remote_endpoint.mac_addr) is False:
                nas_if.log_err("Failed to add remote endpoints " + str(src_ip) + " to VTEP Interface " + str(vtep_name))
        rlist[src_ip] = remote_endpoints[src_ip]
        nas_if.log_info("Successfully added remote endpoint %s to Bridge Interface" % (str(src_ip)))
    return True, rlist

def __remove_remote_endpoints(vtep_name, remote_endpoints):
    """Method to Remove Remote Endpoints for the VxLAN in Linux"""
    rlist = {}
    for src_ip in remote_endpoints:
        remote_endpoint = remote_endpoints[src_ip]
        if dn_base_br_tool.del_learnt_mac_from_vtep_fdb(vtep_name, src_ip, \
                                                        remote_endpoint.addr_family, \
                                                        remote_endpoint.mac_addr) is False:
            nas_if.log_info("Failed to remove remote endpoints " + str(src_ip) + " to VTEP Interface " + str(vtep_name))
        rlist[src_ip] = remote_endpoints[src_ip]
        nas_if.log_info("Successfully removed remote endpoint %s to Bridge Interface" % (str(src_ip)))
    return True, rlist


''' Create, Update and Delete '''
def update(cps_obj, params, cfg_obj):
    """Method to set attributes for the VTEP in Linux"""
    _remote_endpoints = remote_endpoints = []
    member_op = if_vtep_config.get_member_op(cps_obj)
    ret, remote_endpoints = if_vtep_config.get_remote_endpoint_list(cps_obj)

    while True:
        if member_op == 'add':
            ret, _remote_endpoints = __add_remote_endpoints(cfg_obj.name, remote_endpoints)
            if ret is False:
                break
        else:
            ret, _remote_endpoints = __remove_remote_endpoints(cfg_obj.name, remote_endpoints)
            if ret is False:
                break
        return True

    nas_if.log_err("Update VTEP Failed, Rolling back")
    if member_op == 'add':
        __remove_remote_endpoints(cfg_obj.name, _remote_endpoints)
    else:
        __add_remote_endpoints(cfg_obj.name, _remote_endpoints)
    return False

def create(cps_obj, params, cfg_obj):
    """Method to create the VTEP in Linux"""
    _remote_endpoints = []
    while(True):
        ret, if_index = __create_vtep_if(cfg_obj.name, cfg_obj.vni, cfg_obj.ip, cfg_obj.addr_family)
        if ret is False:
            break
        ret, _remote_endpoints = __add_remote_endpoints(cfg_obj.name, cfg_obj.remote_endpoints)
        if ret is False:
            break
        return True

    nas_if.log_err("Create VTEP Failed, Rolling back")
    __remove_remote_endpoints(cfg_obj.name, _remote_endpoints)
    __delete_vtep_if(cfg_obj.name)
    return False

def delete(cps_obj, params, cfg_obj):
    """Method to delete the VTEP in Linux"""
    __remove_remote_endpoints(cfg_obj.name, cfg_obj.remote_endpoints)
    __delete_vtep_if(cfg_obj.name)
    return True

def _send_obj_to_base(cps_obj,op):
    nas_if.log_info('Sending vxlan object to base')
    in_obj = copy.deepcopy(cps_obj)
    in_obj.set_key(cps.key_from_name('target', 'dell-base-if-cmn/if/interfaces/interface'))
    obj = in_obj.get()
    if 'operation' in obj:
        del obj['operation']
    upd = (op, obj)
    _ret = cps_utils.CPSTransaction([upd]).commit()
    if not _ret:
        nas_if.log_err('BASE transaction for vxlan failed')
        return False
    return True

def handle_vtep_intf(cps_obj, params):
    """Method to handle VTEP interface set, create or delete operations"""
    op = _get_op_id(cps_obj)
    if op is None:
        return False


    if not use_linux_path:
        if _send_obj_to_base(cps_obj,op) is False:
            nas_if.log_err("BASE  %s request for vxlan failed" % (str(op)))
            return False
        return True

    cfg_obj = if_vtep_config.create(cps_obj)
    if cfg_obj is None:
        return False

    if op == 'create' and create(cps_obj, params, cfg_obj) is True:
        return if_vtep_config.cache_add(cfg_obj.name, cfg_obj)
    if op == 'delete' and delete(cps_obj, params, cfg_obj) is True:
        return if_vtep_config.cache_del(cfg_obj.name)
    if op == 'set' and update(cps_obj, params, cfg_obj) is True:
        return if_vtep_config.cache_update(cfg_obj.name, cfg_obj)
    return False

