#!/usr/bin/python
#
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
#

import cps
import cps_object
import cps_utils
import os
import argparse
import nas_common_header as nas_comm
import nas_yang_values as yv
yang = yv.YangValues()

def get_cps_object(module_name, attr_list = {}, qual='target'):
    cps_obj = cps_object.CPSObject(module = module_name, qual = qual, data = attr_list)
    ret_list = []
    if cps.get([cps_obj.get()], ret_list)  == False:
        return False
    for ret_obj in ret_list:
        cps_utils.print_obj(ret_obj)
    return True

def set_cps_object(module_name, attr_list = {}, op = 'set'):
    cps_obj = cps_object.CPSObject(module = module_name, data = attr_list)
    upd = (op, cps_obj.get())
    ret_cps_data = cps_utils.CPSTransaction([upd]).commit()
    if ret_cps_data == False:
        return False
    cps_utils.print_obj(ret_cps_data[0])
    return True

def extract_intf_attr(args, attr_list):
    if args.admin != None:
        if args.admin == 'up':
            attr_list['if/interfaces/interface/enabled'] = 1
        else:
            attr_list['if/interfaces/interface/enabled'] = 0
    if args.speed != None:
        if args.speed.isdigit():
            key_val = int(args.speed)
        else:
            key_val = args.speed
        if key_val in nas_comm.yang.get_tbl('mbps-to-yang-speed'):
            attr_list['dell-if/if/interfaces/interface/speed'] = nas_comm.yang.get_tbl('mbps-to-yang-speed')[key_val]
        elif key_val in nas_comm.yang.get_tbl('yang-to-mbps-speed'):
            attr_list['dell-if/if/interfaces/interface/speed'] = key_val
        else:
            print 'Invalid speed setting: %s' % args.speed
    if args.negotiation != None:
        if args.negotiation in nas_comm.yang.get_tbl('yang-autoneg'):
            attr_list['dell-if/if/interfaces/interface/negotiation'] = nas_comm.yang.get_value(args.negotiation, 'yang-autoneg')
        else:
            print 'Invalid negotiation setting: %s' % args.negotiation
    if args.duplex != None:
        if args.duplex in nas_comm.yang.get_tbl('yang-duplex'):
            attr_list['dell-if/if/interfaces/interface/duplex'] = nas_comm.yang.get_value(args.duplex, 'yang-duplex')
        else:
            print 'Invalid duplex setting: %s' % args.duplex
    if args.fec != None:
        if args.fec == 'cl91' or args.fec == 'cl108':
            args.fec += '-rs'
        elif args.fec == 'cl74':
            args.fec += '-fc'
        args.fec = args.fec.upper()
        if args.fec in nas_comm.yang.get_tbl('yang-fec'):
            attr_list['dell-if/if/interfaces/interface/fec'] = nas_comm.yang.get_value(args.fec, 'yang-fec')
        else:
            print 'Invalid fec setting: %s' % args.fec

if __name__ == '__main__':
    if_type_map = {'port': 'ianaift:ethernetCsmacd',
                   'vlan': 'ianaift:l2vlan',
                   'lag': 'ianaift:ieee8023adLag',
                   'cpu': 'base-if:cpu',
                   'loopback': 'ianaift:softwareLoopback',
                   'fc': 'ianaift:fibreChannel'}
    parser = argparse.ArgumentParser(description = 'Tool for interface management')
    parser.add_argument('operation', choices = ['create', 'delete', 'update', 'info', 'state'])
    parser.add_argument('if_type', choices = if_type_map.keys(), nargs = '?')
    parser.add_argument('if_name', help = 'Name of interface')

    parser.add_argument('-p', '--front-panel', type = int, help = 'Front panel port ID')
    parser.add_argument('-s', '--subport-id', type = int, default = 0, help = 'Subport ID')
    parser.add_argument('-m', '--mac-address', help = 'Mac address')
    parser.add_argument('--admin', choices = ['up', 'down'],
                        help = 'Administration state')
    parser.add_argument('--speed', help = 'Interface speed')
    parser.add_argument('--negotiation', choices = ['auto', 'on', 'off'], help = 'Auto-Negotiation mode')
    parser.add_argument('--duplex', choices = ['auto', 'full', 'half'], help = 'Duplex mode')
    parser.add_argument('--fec', choices = ['auto', 'off', 'cl91', 'cl74', 'cl108'], help = 'FEC mode')
    parser.add_argument('-c', '--connect', action='store_true',
                        help = 'Connect virtual interfce to physical port')
    parser.add_argument('-d', '--disconnect', action='store_true',
                        help = 'Disconnect physical port from logical interface')

    args = parser.parse_args()

    if args.operation == 'create':
        module_name = 'dell-base-if-cmn/set-interface'
        attr_list = {'dell-base-if-cmn/set-interface/input/operation': 1,
                     'if/interfaces/interface/name': args.if_name}
        if args.if_type != None:
            attr_list['if/interfaces/interface/type'] = if_type_map[args.if_type]
        else:
            print 'Interface type is required for interface creation'
            exit(1)
        if args.mac_address != None:
            attr_list['dell-if/if/interfaces/interface/phys-address'] = args.mac_address
        if args.front_panel != None:
            attr_list['base-if-phy/hardware-port/front-panel-port'] = args.front_panel
            attr_list['base-if-phy/hardware-port/subport-id'] = args.subport_id
        extract_intf_attr(args, attr_list)
        ret = set_cps_object(module_name, attr_list, 'rpc')
    elif args.operation == 'update':
        module_name = 'dell-base-if-cmn/set-interface'
        attr_list = {'dell-base-if-cmn/set-interface/input/operation': 3,
                     'if/interfaces/interface/name': args.if_name}
        if args.disconnect == True:
            attr_list['base-if-phy/hardware-port/front-panel-port'] = None
        elif args.connect == True:
            if args.front_panel == None:
                print 'No front panel port specified'
                exit(1)
            attr_list['base-if-phy/hardware-port/front-panel-port'] = args.front_panel
            attr_list['base-if-phy/hardware-port/subport-id'] = args.subport_id
        if args.mac_address != None:
            attr_list['dell-if/if/interfaces/interface/phys-address'] = args.mac_address
        extract_intf_attr(args, attr_list)
        ret = set_cps_object(module_name, attr_list, 'rpc')
    elif args.operation == 'delete':
        module_name = 'dell-base-if-cmn/set-interface'
        attr_list = {'dell-base-if-cmn/set-interface/input/operation': 2,
                     'if/interfaces/interface/name': args.if_name}
        ret = set_cps_object(module_name, attr_list, 'rpc')
    elif args.operation == 'info':
        module_name = 'dell-base-if-cmn/if/interfaces/interface'
        attr_list = {}
        if args.if_type != None:
            attr_list['if/interfaces/interface/type'] = if_type_map[args.if_type]
        if args.if_name != 'all':
            attr_list['if/interfaces/interface/name'] = args.if_name
        ret = get_cps_object(module_name, attr_list)
    elif args.operation == 'state':
        module_name = 'dell-base-if-cmn/if/interfaces-state/interface'
        attr_list = {}
        if args.if_type != None:
            attr_list['if/interfaces-state/interface/type'] = if_type_map[args.if_type]
        if args.if_name != 'all':
            attr_list['if/interfaces-state/interface/name'] = args.if_name
        ret = get_cps_object(module_name, attr_list, 'observed')
    else:
        print 'Invalid operation: %s' % args.operation
        exit(1)

    if ret == False:
        print 'Failed'
    else:
        print 'Succeed'
