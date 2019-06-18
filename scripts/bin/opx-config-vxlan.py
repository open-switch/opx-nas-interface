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

import argparse
import nas_vxlan_utils as vxlan_utils
from argparse import ArgumentParser

OS10_VXLAN_CONFIG_TOOL_VERSION = 1.0


def _parse_args(args):
    if args.command == 'create':
        if args.subcommand == 'bridge':
            return vxlan_utils.create_bridge_interface(args.name, [])
        elif args.subcommand == 'vtep':
            return vxlan_utils.create_vtep(args.name, args.vni, args.ip, args.family)
        elif args.subcommand == 'vlanSubIntf':
            return vxlan_utils.create_vlan_subintf(args.parent, args.vlan_id)
        return

    elif args.command == 'delete':
        if args.subcommand == 'bridge':
            return vxlan_utils.delete_bridge_interface(args.name)
        elif args.subcommand == 'vtep':
            return vxlan_utils.delete_vtep(args.name)
        elif args.subcommand == 'vlanSubIntf':
            return vxlan_utils.delete_vlan_subintf(args.parent, args.vlan_id)
        return

    elif args.command == 'add':
        if args.subcommand == 'tagged':
            return vxlan_utils.add_tagged_access_ports_to_bridge_interface(args.bridge_name, [args.tagged_member_name])
        elif args.subcommand == 'untagged':
            return vxlan_utils.add_untagged_access_ports_to_bridge_interface(args.bridge_name, [args.untagged_member_name])
        elif args.subcommand == 'vtep':
            return vxlan_utils.add_vteps_to_bridge_interface(args.bridge_name, [args.vtep_name])
        elif args.subcommand == 'remote-endpoint':
            re = vxlan_utils.RemoteEndpoint(args.ip, args.family, args.flooding_enabled)
            return vxlan_utils.add_remote_endpoint(args.vtep_name, [re])
        return

    elif args.command == 'remove':
        if args.subcommand == 'tagged':
            return vxlan_utils.remove_tagged_access_ports_to_bridge_interface(args.bridge_name, [args.tagged_member_name])
        elif args.subcommand == 'untagged':
            return vxlan_utils.remove_untagged_access_ports_to_bridge_interface(args.bridge_name, [args.untagged_member_name])
        elif args.subcommand == 'vtep':
            return vxlan_utils.remove_vteps_from_bridge_interface(args.bridge_name, [args.vtep_name])
        elif args.subcommand == 'remote-endpoint':
            re = vxlan_utils.RemoteEndpoint(args.ip, args.family, args.flooding_enabled)
            return vxlan_utils.remove_remote_endpoint(args.vtep_name, [re])
        return


if __name__ == '__main__':

    parser = ArgumentParser(version=OS10_VXLAN_CONFIG_TOOL_VERSION, description="This is a VxLAN config tool.")
    parser.add_argument('--verbose',
                        dest='verbose',
                        default=False,
                        action='store_true',
                        help='Verbose mode. Default: %(default)s')
    subparsers = parser.add_subparsers(title='Commands', description='valid commands')
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(help='command', title="command", dest="command")

    # Create command
    create_parser = subparsers.add_parser('create', help='Create Interfaces specific to VxLAN')
    create_subparsers = create_parser.add_subparsers(help='subcommand', title="subcommand", dest="subcommand")

    # Create Bridge Interface
    create_bridge_parser = create_subparsers.add_parser('bridge', help='Create a bridge')
    create_bridge_parser.add_argument('--name', dest='name', required=True, action='store', type=str, help='Name of the Bridge Interface')

    # Create VTEP Interface
    create_vtep_parser = create_subparsers.add_parser('vtep', help='Create a VTEP')
    create_vtep_parser.add_argument('--name', dest='name', required=True, action='store', type=str, help='Name of the VTEP Interface')
    create_vtep_parser.add_argument('--vni', dest='vni', required=True, action='store', type=str, help='VNI of the VTEP Interface')
    create_vtep_parser.add_argument('--ip', dest='ip', required=True, action='store', type=str, help='IP Address of the VTEP Interface')
    create_vtep_parser.add_argument('--family', choices=['ipv4','ipv6'], dest='family', required=True, action='store', type=str, help='IP Address Family of the VTEP Interface')

    # Create VLAN SubInterface Interface
    create_vlan_subintf_parser = create_subparsers.add_parser('vlanSubIntf', help='Create a VLAN SubInterface')
    create_vlan_subintf_parser.add_argument('--parent', dest='parent', required=True, action='store', type=str, help='Name of the parent Interface')
    create_vlan_subintf_parser.add_argument('--vlanId', dest='vlan_id', required=True, action='store', type=int, help='VLAN ID of the VlanSubInterface')

    # Delete command
    delete_parser = subparsers.add_parser('delete', help='Delete Interfaces specific to VxLAN')
    delete_subparsers = delete_parser.add_subparsers(help='subcommand', title="subcommand", dest="subcommand")

    # Delete Bridge Interface
    delete_bridge_parser = delete_subparsers.add_parser('bridge', help='Delete a bridge')
    delete_bridge_parser.add_argument('--name', dest='name', required=True, action='store', type=str, help='Name of the Bridge Interface')

    # Delete VTEP Interface
    delete_vtep_parser = delete_subparsers.add_parser('vtep', help='Delete a VTEP')
    delete_vtep_parser.add_argument('--name', dest='name', required=True, action='store', type=str, help='Name of the VTEP Interface')

    # Delete VLAN SubInterface Interface
    delete_vlan_subintf_parser = delete_subparsers.add_parser('vlanSubIntf', help='Delete a VLAN SubInterface')
    delete_vlan_subintf_parser.add_argument('--parent', dest='parent', required=True, action='store', type=str, help='Name of the parent Interface')
    delete_vlan_subintf_parser.add_argument('--vlanId', dest='vlan_id', required=True, action='store', type=int, help='VLAN ID of the  VlanSubInterface')

    # Add Command
    ad_parser = subparsers.add_parser('add', help='Add Interfaces to VxLAN')
    ad_subparsers = ad_parser.add_subparsers(help='subcommand', title="subcommand", dest="subcommand")

    # Add Tagged Members
    add_tagged_member_parser = ad_subparsers.add_parser('tagged', help='Add tagged members to a bridge')
    add_tagged_member_parser.add_argument('--brname', dest='bridge_name', required=True, action='store', type=str, help='Name of the Bridge Interface')
    add_tagged_member_parser.add_argument('--ifname', dest='tagged_member_name', required=True, action='store', type=str, help='Name of the Tagged Interface')

    # Add UnTagged Members
    add_untagged_member_parser = ad_subparsers.add_parser('untagged', help='Add untagged members to a bridge')
    add_untagged_member_parser.add_argument('--brname', dest='bridge_name', required=True, action='store', type=str, help='Name of the Bridge Interface')
    add_untagged_member_parser.add_argument('--ifname', dest='untagged_member_name', required=True, action='store', type=str, help='Name of the Untagged Interface')

    # Add VTEP Members
    add_vtep_parser = ad_subparsers.add_parser('vtep', help='Add a VTEP to a bridge')
    add_vtep_parser.add_argument('--brname', dest='bridge_name', required=True, action='store', type=str, help='Name of the Bridge Interface')
    add_vtep_parser.add_argument('--vtepname', dest='vtep_name', required=True, action='store', type=str, help='Name of the VTEP Interface')

    # Add RemoteEndpoint
    add_remote_endpoint_parser = ad_subparsers.add_parser('remote-endpoint', help='Add remote endpoint to a bridge')
    add_remote_endpoint_parser.add_argument('--vtepname', dest='vtep_name', required=True, action='store', type=str, help='Name of the Bridge Interface')
    add_remote_endpoint_parser.add_argument('--ip', dest='ip', required=True, action='store', type=str, help='IP Address of the Remote Endpoint')
    add_remote_endpoint_parser.add_argument('--family', choices=['ipv4', 'ipv6'], dest='family', required=True, action='store', type=str, help='IP Address Family of the VTEP Interface')
    add_remote_endpoint_parser.add_argument('--flooding', dest='flooding_enabled', required=True, action='store', type=int, choices=[0,1], help='Flooding enabled or disabled')

    # Remove Command
    remov_parser = subparsers.add_parser('remove', help='Remove Interfaces from VxLAN')
    remov_subparsers = remov_parser.add_subparsers(help='subcommand', title="subcommand", dest="subcommand")

    # Remove Tagged Members
    remove_tagged_member_parser = remov_subparsers.add_parser('tagged', help='Remove tagged members from a bridge')
    remove_tagged_member_parser.add_argument('--brname', dest='bridge_name', required=True, action='store', type=str, help='Name of the Bridge Interface')
    remove_tagged_member_parser.add_argument('--ifname', dest='tagged_member_name', required=True, action='store', type=str, help='Name of the Tagged Interface')

    # Remove UnTagged Members
    remove_untagged_member_parser = remov_subparsers.add_parser('untagged', help='Remove untagged members from a bridge')
    remove_untagged_member_parser.add_argument('--brname', dest='bridge_name', required=True, action='store', type=str, help='Name of the Bridge Interface')
    remove_untagged_member_parser.add_argument('--ifname', dest='untagged_member_name', required=True, action='store', type=str, help='Name of the Untagged Interface')

    # Remove VTEP Members
    remove_vtep_parser = remov_subparsers.add_parser('vtep', help='Remove a VTEP from a bridge')
    remove_vtep_parser.add_argument('--brname', dest='bridge_name', required=True, action='store', type=str, help='Name of the Bridge Interface')
    remove_vtep_parser.add_argument('--vtepname', dest='vtep_name', required=True, action='store', type=str, help='Name of the VTEP Interface')

    # Remove RemoteEndpoint
    remove_remote_endpoint_parser = remov_subparsers.add_parser('remote-endpoint', help='Remove remote endpoint from a bridge')
    remove_remote_endpoint_parser.add_argument('--vtepname', dest='vtep_name', required=True, action='store', type=str, help='Name of the Bridge Interface')
    remove_remote_endpoint_parser.add_argument('--ip', dest='ip', required=True, action='store', type=str, help='IP Address of the Remote Endpoint')
    remove_remote_endpoint_parser.add_argument('--family', choices=['ipv4', 'ipv6'], dest='family', required=True, action='store', type=str, help='IP Address Family of the VTEP Interface')
    remove_remote_endpoint_parser.add_argument('--flooding', dest='flooding_enabled', required=True, action='store', type=int, choices=[0,1], help='Flooding enabled or disabled')

    args = parser.parse_args()
    print(args)
    _parse_args(args)
