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

"""Module for creation and caching of the VxLAN specific config objects"""

import nas_os_if_utils as nas_if
import copy
import socket
import cps_utils
import bytearray_utils as ba
import logging

class RemoteEndpoint(object):
    """RemoteEndpoint class"""
    def __init__(self, ip, addr_family, flooding_enabled, mac_addr="00:00:00:00:00:00"):
        """Constructor"""
        self.ip = ip
        self.addr_family = addr_family
        self.flooding_enabled = flooding_enabled
        self.mac_addr = mac_addr


class VTEP(object):
    """VTEP config object class"""
    def __init__(self, name, vni, ip, addr_family):
        """Constructor"""
        self.name = name
        self.vni = vni
        self.ip = ip
        self.addr_family = addr_family
        self.remote_endpoints = {}
        self.multicast_ip = False

    def add_remote_endpoint(self, remote_endpoint):
        """Method to add Remote Endpoint"""
        self.remote_endpoints[remote_endpoint.ip] = remote_endpoint
        if remote_endpoint.mac_addr == "00:00:00:00:00:00":
            self.multicast_ip = True

    def remove_remote_endpoint(self, remote_endpoint):
        """Method to remove Remote Endpoint"""
        if remote_endpoint.ip in self.remote_endpoints:
            del self.remote_endpoints[remote_endpoint.ip]
            if remote_endpoint.mac_addr == "00:00:00:00:00:00":
                self.multicast_ip = False


"""VTEP Object Cache"""
VTEP_MAP = {}

def cache_get(name):
    """Method to get a VTEP configuration object from cache"""
    if name in VTEP_MAP:
        return VTEP_MAP[name]
    return None

def cache_del(name):
    """Method to delete a VTEP configuration object from cache"""
    if name in VTEP_MAP:
        del VTEP_MAP[name]
        return True
    return False

def cache_update(name, config_obj):
    """Method to update a VTEP configuration object in the cache"""
    cache_del(name)
    return cache_add(name, config_obj)

def cache_add(name, config_obj):
    """Method to add a VTEP configuration object to the cache"""
    if name not in VTEP_MAP:
        VTEP_MAP[name] = config_obj
        return True
    return False


"""VTEP object related attributes"""
VTEP_NAME = 'if/interfaces/interface/name'
VNI = 'dell-if/if/interfaces/interface/vni'
TUNNEL_SOURCE_IP_ADDR = 'dell-if/if/interfaces/interface/source-ip/addr'
TUNNEL_SOURCE_IP_ADDR_FAMILY = 'dell-if/if/interfaces/interface/source-ip/addr-family'
REMOTE_ENDPOINT_LIST = 'dell-if/if/interfaces/interface/remote-endpoint'
REMOTE_ENDPOINT_FLOODING_ENABLED = 'flooding-enabled'
REMOTE_ENDPOINT_ADDR_FAMILY = 'addr-family'
REMOTE_ENDPOINT_IP_ADDR = 'addr'
MEMBER_OP_ATTR_NAME = 'dell-base-if-cmn/set-interface/input/member-op'
OP_ID_TO_NAME_MAP = {1: 'add', 2: 'remove'}
INET_TO_STR_MAP = {socket.AF_INET: 'ipv4', socket.AF_INET6: 'ipv6'}

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

def get_vtep_name(cps_obj):
    """Method to read CPS attr of the interface name  from CPS Object"""
    vtep_name = __read_attr(cps_obj, VTEP_NAME)
    return vtep_name
def get_member_op(cps_obj):
    """Method to read CPS attr of the member operation from CPS Object"""
    member_op = __read_attr(cps_obj, MEMBER_OP_ATTR_NAME)
    if member_op in OP_ID_TO_NAME_MAP:
        return OP_ID_TO_NAME_MAP[member_op]

    nas_if.log_err('Invalid operation type ' + str(member_op))
    return member_op

def get_remote_endpoint_list(cps_obj):
    """Method to retrive a list of remote endpoints from the CPS object"""
    rlist = {}
    remote_endpoints = __read_attr(cps_obj, REMOTE_ENDPOINT_LIST)

    if remote_endpoints is not None:
        for name in remote_endpoints:
            ip = None
            addr_family = None
            flooding_enabled = 1
            try:
                endpoint = remote_endpoints[name]
                nas_if.log_info(" remote endpoint %s" % (str(remote_endpoints)))
                addr_family = endpoint[REMOTE_ENDPOINT_ADDR_FAMILY]
                if addr_family is None:
                    return False, rlist
                ip = None
                if addr_family is socket.AF_INET:
                    ip_ba = ba.hex_to_ba('ipv4', endpoint[REMOTE_ENDPOINT_IP_ADDR])
                    ip = ba.ba_to_ipv4str('ipv4', ip_ba)
                else:
                    ip_ba = ba.hex_to_ba('ipv6', endpoint[REMOTE_ENDPOINT_IP_ADDR])
                    ip = ba.ba_to_ipv6str('ipv6', ip_ba)
                if REMOTE_ENDPOINT_FLOODING_ENABLED in endpoint:
                    flooding_enabled = endpoint[REMOTE_ENDPOINT_FLOODING_ENABLED]
            except ValueError:
                logging.exception('error:')
                nas_if.log_err("Failed to read attr of remote endpoint")
                pass

            if ip is None or addr_family is None or flooding_enabled is None:
                return False, rlist
            rlist[ip] = RemoteEndpoint(ip, addr_family, flooding_enabled)

    return True, rlist

def create(cps_obj):
    """Method to convert the CPS object into a VTEP configuration object"""

    name = __read_attr(cps_obj, VTEP_NAME)
    if name is None:
        return None

    cfg_obj = copy.deepcopy(cache_get(name))

    if cfg_obj is None:

        vni = __read_attr(cps_obj, VNI)
        if vni is None:
            return None

        tunnel_source_ip_addr_family = __read_attr(cps_obj, TUNNEL_SOURCE_IP_ADDR_FAMILY)
        if tunnel_source_ip_addr_family is None:
            return None

        cps_utils.cps_attr_types_map.add_type(TUNNEL_SOURCE_IP_ADDR, INET_TO_STR_MAP[tunnel_source_ip_addr_family])
        tunnel_source_ip = __read_attr(cps_obj, TUNNEL_SOURCE_IP_ADDR)
        if tunnel_source_ip is None:
            return None

        cfg_obj = VTEP(name, vni, tunnel_source_ip, tunnel_source_ip_addr_family)

    # RPC doesn't have member op attribute
    member_op = get_member_op(cps_obj)
    if member_op is None:
        return cfg_obj

    # Add/Remove remote endpoints list
    ret, endpoints = get_remote_endpoint_list(cps_obj)
    if ret == False:
        return None

    for name in endpoints:
        if member_op == 'add':
            cfg_obj.add_remote_endpoint(endpoints[name])
        else:
            cfg_obj.remove_remote_endpoint(endpoints[name])

    return cfg_obj
