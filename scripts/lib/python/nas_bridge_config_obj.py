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

"""Module for creation and caching of the Bridge specific config object"""

import nas_os_if_utils as nas_if
import copy


class Bridge(object):
    """Bridge config object class"""
    def __init__(self, name):
        """Constructor"""
        self.name = name
        self.member_interfaces = {}

    def add_member_interface(self, interface):
        """Method to add Member Interface"""
        self.member_interfaces[interface] = interface
        return True

    def remove_member_interface(self, interface):
        """Method to remove Member Interface"""
        if interface in self.member_interfaces:
            del self.member_interfaces[interface]
            return True
        return False


"""Bridge Object Cache"""
BRIDGE_MAP = {}

def cache_get(name):
    """Method to get a BRIDGE configuration object from cache"""
    if name in BRIDGE_MAP:
        return BRIDGE_MAP[name]
    return None

def cache_del(name):
    """Method to delete a bridge configuration object from cache"""
    if name in BRIDGE_MAP:
        del BRIDGE_MAP[name]
        return True
    return False

def cache_update(name, cfg_obj):
    """Method to update a BRIDGE configuration object in the cache"""
    cache_del(name)
    return cache_add(name, cfg_obj)

def cache_add(name, cfg_obj):
    """Method to add a BRIDGE configuration object to the cache"""
    if name not in BRIDGE_MAP:
        BRIDGE_MAP[name] = cfg_obj
        return True
    return False


"""Bridge object related attributes"""
BRIDGE_NAME = 'bridge-domain/bridge/name'
MEMBER_INTERFACE_LIST = 'bridge-domain/bridge/member-interface'
BRIDGE_OP_ATTR_NAME = 'bridge-domain/set-bridge/input/operation'

def __read_attr(cps_obj, attr_id):
    """Method to read a CPS attribute value from the CPS object"""
    val = None
    try:
        val = cps_obj.get_attr_data(attr_id)
        nas_if.log_info("Value of CPS attr %s is %s: " % \
                        (str(attr_id), str(val)))
    except ValueError:
        nas_if.log_err("Failed to read value of the CPS attr %s" % str(attr_id))
    return val

def get_member_interfaces_list(cps_obj):
    """Method to retrieve a list of tunnel endpoints from the CPS object"""
    rlist = {}
    member_interfaces = __read_attr(cps_obj, MEMBER_INTERFACE_LIST)
    if member_interfaces is not None:
        for name in member_interfaces:
            if name is None:
                return False, rlist
            rlist[name] = name
    return True, rlist

def _get_bridge_member_op_id(cps_obj):
    op_id_to_name_map = {1: 'create', 2: 'delete', 3: 'update', 4:'add-member', 5:'delete-member'}
    op_id = None
    try:
        op_id = cps_obj.get_attr_data(BRIDGE_OP_ATTR_NAME)
    except ValueError:
        nas_if.log_err('No operation attribute in object')
        return None
    if not op_id in op_id_to_name_map:
        nas_if.log_err('Invalid operation type '+str(op_id))
        return None
    return op_id_to_name_map[op_id]

def create(cps_obj):
    """Method to convert the CPS object into a Bridge configuration object"""
    br_name = __read_attr(cps_obj, BRIDGE_NAME)
    if br_name is None:
        return None

    cfg_obj = copy.deepcopy(cache_get(br_name))
    if cfg_obj is None:
        cfg_obj = Bridge(br_name)

    # RPC doesn't have member op attribute
    op = _get_bridge_member_op_id(cps_obj)
    if op is None:
        return cfg_obj

    ret, member_interfaces = get_member_interfaces_list(cps_obj)
    if ret == False:
        return None
    for name in member_interfaces:
        if op == 'add-member' or op == 'create':
            cfg_obj.add_member_interface(member_interfaces[name])
        if op == 'delete-member':
            cfg_obj.remove_member_interface(member_interfaces[name])
    return cfg_obj
