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


import cps_object
import cps
import cps_utils
import socket
import os
import subprocess

#Operation
_create=1
_delete=2
_update=3
_add_member=4
_delete_member=5

#member op
_add = 1
_remove = 2

INET_TO_STR_MAP = {'ipv4': socket.AF_INET, 'ipv6': socket.AF_INET6}

def exec_shell(cmd):
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
    (out, err) = proc.communicate()
    prGreen(out)
    return out

class RemoteEndpoint(object):
    """RemoteEndpoint class"""
    def __init__(self, ip, addr_family, flooding_enabled, mac_addr="00:00:00:00:00:00"):
        """Constructor"""
        self.ip = ip
        self.addr_family = addr_family
        self.flooding_enabled = flooding_enabled
        self.mac_addr = mac_addr

#Create Bridge interface
def create_bridge_interface(name, member_interfaces):
    cps_obj = cps_object.CPSObject(module='bridge-domain/set-bridge', data={})
    cps_obj.add_attr('bridge-domain/set-bridge/input/operation', _create)
    cps_obj.add_attr("bridge-domain/bridge/name", name)
    cps_obj.add_list('bridge-domain/bridge/member-interface', member_interfaces)
    return cps.transaction([{'operation': 'rpc', 'change': cps_obj.get()}])

#Delete Bridge interface
def delete_bridge_interface(name):
    cps_obj = cps_object.CPSObject(module='bridge-domain/set-bridge', data={})
    cps_obj.add_attr('bridge-domain/set-bridge/input/operation', _delete)
    cps_obj.add_attr("bridge-domain/bridge/name", name)
    return cps.transaction([{'operation': 'rpc', 'change': cps_obj.get()}])

#Create a VTEP
def create_vtep(name, vni, ip_addr, addr_family):
    cps_obj = cps_object.CPSObject(module='dell-base-if-cmn/set-interface', data={})
    cps_obj.add_attr('dell-base-if-cmn/set-interface/input/operation', _create)
    cps_obj.add_attr("if/interfaces/interface/name", name)
    cps_obj.add_attr('if/interfaces/interface/type', "base-if:vxlan")
    cps_obj.add_attr("dell-if/if/interfaces/interface/vni", vni)
    cps_obj.add_attr("dell-if/if/interfaces/interface/source-ip/addr-family", INET_TO_STR_MAP[addr_family])
    cps_utils.cps_attr_types_map.add_type("dell-if/if/interfaces/interface/source-ip/addr", addr_family)
    cps_obj.add_attr("dell-if/if/interfaces/interface/source-ip/addr", ip_addr)
    return cps.transaction([{'operation': 'rpc', 'change': cps_obj.get()}])

#Delete a VTEP
def delete_vtep(name):
    cps_obj = cps_object.CPSObject(module='dell-base-if-cmn/set-interface', data={})
    cps_obj.add_attr('dell-base-if-cmn/set-interface/input/operation', _delete)
    cps_obj.add_attr("if/interfaces/interface/name", name)
    cps_obj.add_attr('if/interfaces/interface/type', "base-if:vxlan")
    return cps.transaction([{'operation': 'rpc', 'change': cps_obj.get()}])

#Create a VLAN SubInterface
def create_vlan_subintf(parent_intf, vlan_id):
    cps_obj = cps_object.CPSObject(module='dell-base-if-cmn/set-interface', data={})
    cps_obj.add_attr('dell-base-if-cmn/set-interface/input/operation', _create)
    cps_obj.add_attr("dell-if/if/interfaces/interface/parent-interface", parent_intf)
    cps_obj.add_attr('if/interfaces/interface/type', "base-if:vlanSubInterface")
    cps_obj.add_attr("dell-if/if/interfaces/interface/vlan-id", vlan_id)
    return cps.transaction([{'operation': 'rpc', 'change': cps_obj.get()}])

#Delete a VLAN SubInterface
def delete_vlan_subintf(parent_intf, vlan_id):
    cps_obj = cps_object.CPSObject(module='dell-base-if-cmn/set-interface', data={})
    cps_obj.add_attr('dell-base-if-cmn/set-interface/input/operation', _delete)
    cps_obj.add_attr("dell-if/if/interfaces/interface/parent-interface", parent_intf)
    cps_obj.add_attr('if/interfaces/interface/type', "base-if:vlanSubInterface")
    cps_obj.add_attr("dell-if/if/interfaces/interface/vlan-id", vlan_id)
    return cps.transaction([{'operation': 'rpc', 'change': cps_obj.get()}])

#Add Tunnel Endpoints to Bridge interface
def add_vteps_to_bridge_interface(name, tunnel_endpoints):
    cps_obj = cps_object.CPSObject(module='bridge-domain/set-bridge', data={})
    cps_obj.add_attr('bridge-domain/set-bridge/input/operation', _add_member)
    cps_obj.add_attr("bridge-domain/bridge/name", name)
    cps_obj.add_list('bridge-domain/bridge/member-interface', tunnel_endpoints)
    return cps.transaction([{'operation': 'rpc', 'change': cps_obj.get()}])

#Remove Tunnel Endpoints to Bridge interface
def remove_vteps_from_bridge_interface(name, tunnel_endpoints):
    cps_obj = cps_object.CPSObject(module='bridge-domain/set-bridge', data={})
    cps_obj.add_attr('bridge-domain/set-bridge/input/operation', _delete_member)
    cps_obj.add_attr("bridge-domain/bridge/name", name)
    cps_obj.add_list('bridge-domain/bridge/member-interface', tunnel_endpoints)
    return cps.transaction([{'operation': 'rpc', 'change': cps_obj.get()}])

#Add Remote Endpoints
def add_remote_endpoint(name, remote_endpoints):
    cps_obj = cps_object.CPSObject(module='dell-base-if-cmn/set-interface', data={})
    cps_obj.add_attr('dell-base-if-cmn/set-interface/input/operation', _update)
    cps_obj.add_attr("if/interfaces/interface/name", name)
    cps_obj.add_attr('if/interfaces/interface/type', "base-if:vxlan")

    cps_obj.add_attr('dell-base-if-cmn/set-interface/input/member-op', _add)
    idx = 0
    for re in remote_endpoints:
        cps_utils.cps_attr_types_map.add_type(cps_obj.generate_path(['dell-if/if/interfaces/interface/remote-endpoint', str(idx), 'addr']), re.addr_family)
        cps_obj.add_embed_attr(['dell-if/if/interfaces/interface/remote-endpoint', str(idx), 'addr'], re.ip, 4)
        cps_obj.add_embed_attr(['dell-if/if/interfaces/interface/remote-endpoint', str(idx), 'addr-family'], INET_TO_STR_MAP[re.addr_family], 4)
        cps_obj.add_embed_attr(['dell-if/if/interfaces/interface/remote-endpoint', str(idx), 'flooding-enabled'], re.flooding_enabled, 4)
        idx += 1
    return cps.transaction([{'operation': 'rpc', 'change': cps_obj.get()}])

#Remove Remote Endpoints
def remove_remote_endpoint(name, remote_endpoints):
    cps_obj = cps_object.CPSObject(module='dell-base-if-cmn/set-interface', data={})
    cps_obj.add_attr('dell-base-if-cmn/set-interface/input/operation', _update)
    cps_obj.add_attr("if/interfaces/interface/name", name)
    cps_obj.add_attr('if/interfaces/interface/type', "base-if:vxlan")

    cps_obj.add_attr('dell-base-if-cmn/set-interface/input/member-op', _remove)
    idx = 0
    for re in remote_endpoints:
        cps_utils.cps_attr_types_map.add_type(cps_obj.generate_path(['dell-if/if/interfaces/interface/remote-endpoint', str(idx), 'addr']), re.addr_family)
        cps_obj.add_embed_attr(['dell-if/if/interfaces/interface/remote-endpoint', str(idx), 'addr'], re.ip, 4)
        cps_obj.add_embed_attr(['dell-if/if/interfaces/interface/remote-endpoint', str(idx), 'addr-family'], INET_TO_STR_MAP[re.addr_family], 4)
        cps_obj.add_embed_attr(['dell-if/if/interfaces/interface/remote-endpoint', str(idx), 'flooding-enabled'], re.flooding_enabled, 4)
        idx += 1
    return cps.transaction([{'operation': 'rpc', 'change': cps_obj.get()}])

#Add Tagged Access Ports to Bridge interface
def add_tagged_access_ports_to_bridge_interface(name, tagged_access_ports):
    cps_obj = cps_object.CPSObject(module='bridge-domain/set-bridge', data={})
    cps_obj.add_attr('bridge-domain/set-bridge/input/operation', _add_member)
    cps_obj.add_attr("bridge-domain/bridge/name", name)
    cps_obj.add_list('bridge-domain/bridge/member-interface', tagged_access_ports)
    return cps.transaction([{'operation': 'rpc', 'change': cps_obj.get()}])

#Remove Tagged Access Ports to Bridge interface
def remove_tagged_access_ports_to_bridge_interface(name, tagged_access_ports):
    cps_obj = cps_object.CPSObject(module='bridge-domain/set-bridge', data={})
    cps_obj.add_attr('bridge-domain/set-bridge/input/operation', _delete_member)
    cps_obj.add_attr("bridge-domain/bridge/name", name)
    cps_obj.add_list('bridge-domain/bridge/member-interface', tagged_access_ports)
    return cps.transaction([{'operation': 'rpc', 'change': cps_obj.get()}])

#Add Untagged Access Ports to Bridge interface
def add_untagged_access_ports_to_bridge_interface(name, untagged_access_ports):
    cps_obj = cps_object.CPSObject(module='bridge-domain/set-bridge', data={})
    cps_obj.add_attr('bridge-domain/set-bridge/input/operation', _add_member)
    cps_obj.add_attr("bridge-domain/bridge/name", name)
    cps_obj.add_list('bridge-domain/bridge/member-interface', untagged_access_ports)
    return cps.transaction([{'operation': 'rpc', 'change': cps_obj.get()}])

#Remove Untagged Access Ports to Bridge interface
def remove_untagged_access_ports_to_bridge_interface(name, untagged_access_ports):
    cps_obj = cps_object.CPSObject(module='bridge-domain/set-bridge', data={})
    cps_obj.add_attr('bridge-domain/set-bridge/input/operation', _delete_member)
    cps_obj.add_attr("bridge-domain/bridge/name", name)
    cps_obj.add_list('bridge-domain/bridge/member-interface', untagged_access_ports)
    return cps.transaction([{'operation': 'rpc', 'change': cps_obj.get()}])

#Create a VTEP with remote endpoints
def create_vtep_with_remote_endpoints(name, vni, ip_addr, addr_family, remote_endpoints):
    cps_obj = cps_object.CPSObject(module='dell-base-if-cmn/set-interface', data={})
    cps_obj.add_attr('dell-base-if-cmn/set-interface/input/operation', _create)
    cps_obj.add_attr("if/interfaces/interface/name", name)
    cps_obj.add_attr('if/interfaces/interface/type', "base-if:vxlan")
    cps_obj.add_attr("dell-if/if/interfaces/interface/vni", vni)
    cps_obj.add_attr("dell-if/if/interfaces/interface/source-ip/addr-family", INET_TO_STR_MAP[addr_family])
    cps_utils.cps_attr_types_map.add_type("dell-if/if/interfaces/interface/source-ip/addr", addr_family)
    cps_obj.add_attr("dell-if/if/interfaces/interface/source-ip/addr", ip_addr)

    cps_obj.add_attr('dell-base-if-cmn/set-interface/input/member-op', _add)
    idx = 0
    for re in remote_endpoints:
        cps_utils.cps_attr_types_map.add_type(cps_obj.generate_path(['dell-if/if/interfaces/interface/remote-endpoint', str(idx), 'addr']), re.addr_family)
        cps_obj.add_embed_attr(['dell-if/if/interfaces/interface/remote-endpoint', str(idx), 'addr'], re.ip, 4)
        cps_obj.add_embed_attr(['dell-if/if/interfaces/interface/remote-endpoint', str(idx), 'addr-family'], INET_TO_STR_MAP[re.addr_family], 4)
        cps_obj.add_embed_attr(['dell-if/if/interfaces/interface/remote-endpoint', str(idx), 'flooding-enabled'], re.flooding_enabled, 4)
        idx += 1

    return cps.transaction([{'operation': 'rpc', 'change': cps_obj.get()}])

