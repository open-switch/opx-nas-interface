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

"""Module for creation and caching of the VLAN SubInterface specific config objects"""

import nas_os_if_utils as nas_if
import copy


class VLAN_SUBINTF(object):
    """VLAN SUBINTF config object class"""
    def __init__(self, parent_intf, vlan_id):
        """Constructor"""
        self.parent_intf = parent_intf
        self.vlan_id = vlan_id
        self.name = str(parent_intf) + '.' + str(vlan_id)


"""VLAN SubInterface Object Cache"""
VLAN_SUBINTF_MAP = {}

def cache_get(name):
    """Method to get a VLAN SUBINTF configuration object from cache"""
    if name in VLAN_SUBINTF_MAP:
        return VLAN_SUBINTF_MAP[name]
    return None

def cache_del(name):
    """Method to delete a VLAN SUBINTF configuration object from cache"""
    if name in VLAN_SUBINTF_MAP:
        del VLAN_SUBINTF_MAP[name]
        return True
    return False

def cache_update(name, config_obj):
    """Method to update a VLAN SUBINTF configuration object in the cache"""
    cache_del(name)
    return cache_add(name, config_obj)

def cache_add(name, config_obj):
    """Method to add a VLAN SUBINTF configuration object to the cache"""
    if name not in VLAN_SUBINTF_MAP:
        VLAN_SUBINTF_MAP[name] = config_obj
        return True
    return False


"""VLAN SUBINTF object related attributes"""
VLAN_PARENT_INTF_NAME = 'dell-if/if/interfaces/interface/parent-interface'
VLAN_SUBINTF_VLAN_ID = 'dell-if/if/interfaces/interface/vlan-id'

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

def create(cps_obj):
    """Method to convert the CPS object into a VLAN SUBINTF configuration object"""

    parent_intf = __read_attr(cps_obj, VLAN_PARENT_INTF_NAME)
    if parent_intf is None:
        return None

    cfg_obj = copy.deepcopy(cache_get(parent_intf))

    if cfg_obj is None:
        vlan_id = __read_attr(cps_obj, VLAN_SUBINTF_VLAN_ID)
        if vlan_id is None:
            return None
        cfg_obj = VLAN_SUBINTF(parent_intf, vlan_id)

    return cfg_obj
