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
from subprocess import call

set_intf_key = 'dell-base-if-cmn/set-interface'
intf_key = 'dell-base-if-cmn/if/interfaces/interface'
oper_key = 'dell-base-if-cmn/set-interface/input/operation'
intf_type_key = 'if/interfaces/interface/type'
intf_name_key = 'if/interfaces/interface/name'
fp_port_key = 'base-if-phy/hardware-port/front-panel-port'
subport_id_key = 'base-if-phy/hardware-port/subport-id'

intf_types = {
                'fc':'ianaift:fibreChannel',
                'ether':'ianaift:ethernetCsmacd',
                'lo':'ianaift:softwareLoopback',
                'lag':'ianaift:ieee8023adLag'
             }

oper_type = {'create':'1','delete':'2','set':'3','get':'4'}

def show_intf(args):
    if args.intf_name == 'all':
        call(["cps_get_oid.py", "target/"+intf_key, intf_type_key + '=' + intf_types[args.type]])

    else:
        call(["cps_get_oid.py", "target/"+intf_key, intf_name_key + '=' + args.intf_name])

'''
Form an ethernet interface name string, based on port id and subport id. eg- e101-001-0
'''
def ether_if_name(port_id, subport_id):
    if (port_id <= 9):
        intf_name = 'e101-00' + str(port_id) + '-' + str(subport_id)
    else:
        intf_name = 'e101-0' + str(port_id) + '-' + str(subport_id)
    print intf_name
    return intf_name


'''
Delete an ethernet interface
'''

def delete_intf(port_id, subport_id=0):
    intf_name = ether_if_name(port_id, subport_id)
    call(["cps_set_oid.py", "-qua", "target", "-oper", "action", set_intf_key,
       oper_key + '=' + oper_type['delete'],
       intf_type_key+'='+intf_types['ether'], intf_name_key+'='+ intf_name,
       fp_port_key+'='+str(port_id), subport_id_key+'='+str(subport_id)])


'''
Create an ethernet interface
'''

def create_intf(port_id, subport_id=0):
    intf_name = ether_if_name(port_id, subport_id)
    call(["cps_set_oid.py", "-qua", "target", "-oper", "action", set_intf_key,
       oper_key + '=' + oper_type['create'],
       intf_type_key+'='+intf_types['ether'], intf_name_key+'='+ intf_name,
       fp_port_key+'='+str(port_id), subport_id_key+'='+str(subport_id)])


def config_intf(args):

    if args.operation == 'create':
        l = args.intf_name.split('-')
	fp_port_id = ''
	subport_id = ''
	if len(l) > 2:
            fp_port_id = l[1]
            subport_id = l[2]
        call(["cps_set_oid.py", "-qua", "target", "-oper", "action", set_intf_key, oper_key + '=' + oper_type[args.operation],
              intf_type_key+'='+intf_types[args.type], intf_name_key+'='+ args.intf_name, fp_port_key+'='+fp_port_id, subport_id_key+'='+subport_id])
    elif args.operation == 'delete':
        call(["cps_set_oid.py", "-qua", "target", "-oper", "action", set_intf_key, oper_key + '=' + oper_type[args.operation],
              intf_name_key+'='+ args.intf_name])
    else:
        call(["cps_set_oid.py", "-qua", "target", "-oper", "action", set_intf_key, oper_key + '=' + oper_type[args.operation],
              intf_name_key+'='+ args.intf_name, args.Key+'='+args.Value])

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-o', '--operation', choices =oper_type, help=' get/create/delete/set interface ')
    parser.add_argument('intf_name', help=' Interface Name or All ')
    parser.add_argument('-k', '--Key', help=' Attribute ID')
    parser.add_argument('-v', '--Value', help=' value   ')
    parser.add_argument('-t', '--type', choices=intf_types, required=True, help=' Interface type ')


    args = parser.parse_args()
    print args.operation
    print args.intf_name
    print args.type
    print args.Key
    print args.Value
    if args.operation == 'get':
        show_intf(args)
    else:
        config_intf(args)
