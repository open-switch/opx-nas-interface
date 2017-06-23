#!/usr/bin/python
# Copyright (c) 2015 Dell Inc.
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

import sys
import getopt
import cps_utils
import nas_ut_framework as nas_ut
import nas_os_utils
import cps_object
import nas_os_if_utils as nas_if


intf_rpc_key_id = 'dell-base-if-cmn/set-interface'
intf_rpc_op_attr_id = 'dell-base-if-cmn/set-interface/input/operation'
intf_rpc_op_type_map = {'create': 1, 'delete': 2, 'set': 3}

def nas_vlan_op(op, data_dict):
    if op == 'get':
        obj = cps_object.CPSObject( nas_if.get_if_key(), data=data_dict)
    else:
        if op in intf_rpc_op_type_map:
            data_dict[intf_rpc_op_attr_id] = intf_rpc_op_type_map[op]
        else:
            print 'Unknown operation type %s' % op
            return False
        obj = cps_object.CPSObject( intf_rpc_key_id, data=data_dict)
        op = 'rpc'
    nas_ut.get_cb_method(op)(obj)

# All the function definition


def usage():
    ''' Usage Method '''

    print '< Usage >'
    print 'cps_config_vlan can be used to configure the VLAN and ports for VLAN\n'
    print '-h, --help: Show the option and usage'
    print '-a, --add : Add VLAN to the configuration'
    print '-d, --del : Delete the specific VLAN'
    print '-i, --id  : VLAN ID user want to operate on'
    print '-p, --port: port(s) that need to be added for a VLAN'
    print '-m, --mac : mac address that need to configured for the given VLAN'
    print '--ifdex   : ifIndex of the VLAN that is configured previously'
    print '--addport : option to add port to the given VLAN'
    print '--addmac  : add a mac address to the VLAN '
    print '-s, --show: show the VLAN parameter, when no VLAN ID given show all'
    print '-t, --tagged: If user want to add the port as tagged port, default untagged\n'
    print '--vlantype : Type of vlan (data/management)'

    print 'Example:'
    print 'cps_config_vlan.py  --add --id 100 --port e101-001-0,e101-004-0'
    print 'cps_config_vlan.py  --del --name br100'
    print 'cps_config_vlan.py  --addport --name br100 --port e101-001-0,e101-004-0'
    print 'cps_config_vlan.py  --addmac  --name br100 --mac 90:b1:1c:f4:a8:b1'
    print 'cps_config_vlan.py  --show    [--name br100]'
    sys.exit(1)


name_attr_id = 'if/interfaces/interface/name'
type_attr_id = 'if/interfaces/interface/type'
vlan_attr_id = 'base-if-vlan/if/interfaces/interface/id'
mac_attr_id =  'dell-if/if/interfaces/interface/phys-address'
tagged_port_attr_id = 'dell-if/if/interfaces/interface/tagged-ports'
untagged_port_attr_id = 'dell-if/if/interfaces/interface/untagged-ports'
vlan_type_attr_id = 'dell-if/if/interfaces/interface/vlan-type'

vlan_if_type = 'ianaift:l2vlan'

def _port_name_list(ports):
    l = []
    port_list = str.split(ports, ",")
    for port in port_list:
        l.append((port.strip()))
    return l

def main(argv):
    ''' The main function will read the user input from the
    command line argument and  process the request  '''

    vlan_id = ''
    vlan_type = ''
    choice = ''
    ports = ''
    if_name = ''
    port_type = untagged_port_attr_id
    mac_id = ''

    try:
        opts, args = getopt.getopt(argv, "hadtsp:i:m:v:",
                                   ["help", "add", "del", "tagged", "port=", "show", "id=", "name=", "addport", "mac=", "addmac", "vlantype="])

    except getopt.GetoptError:
        usage()

    for opt, arg in opts:

        if opt in ('-h', '--help'):
            choice = 'help'

        elif opt in ('-p', '--port'):
            ports = arg

        elif opt in ('-i', '--id'):
            vlan_id = arg

        elif opt in ('-a', '--add'):
            choice = 'add'

        elif opt in ('-d', '--del'):
            choice = 'del'

        elif opt == '--addport':
            choice = 'addport'

        elif opt in ('-s', '--show'):
            choice = 'get'

        elif opt == '--addmac':
            choice = 'mac'

        elif opt == '--name':
            if_name = arg

        elif opt in ('-t', '--tagged'):
            port_type = tagged_port_attr_id

        elif opt in ('-m', '--mac'):
            mac_id = arg

        elif opt == '--vlantype':
            vlan_type = arg

    if choice == 'add' and vlan_id != '':
        ifname_list = []
        if ports != '':
            ifname_list = _port_name_list(ports)
        nas_vlan_op("create", {vlan_attr_id: vlan_id, type_attr_id:vlan_if_type, port_type: ifname_list, vlan_type_attr_id: vlan_type})

    elif choice == 'del' and if_name != '':
        nas_vlan_op("delete", {name_attr_id: if_name})

    elif choice == 'get':
        if if_name != '':
            nas_vlan_op("get", {name_attr_id: if_name})
        else:
            nas_vlan_op("get", {type_attr_id:vlan_if_type})

    elif choice == 'addport' and if_name != '' and ports != '':
        ifname_list = []
        ifname_list = _port_name_list(ports)
        nas_vlan_op("set", {name_attr_id: if_name, port_type: ifname_list})

    elif choice == 'mac' and if_name != '' and mac_id != '':
        nas_vlan_op("set", {name_attr_id: if_name, mac_attr_id: mac_id})

    else:
        usage()

# Calling the main method
if __name__ == "__main__":
    main(sys.argv[1:])
