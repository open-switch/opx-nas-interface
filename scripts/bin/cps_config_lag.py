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

import sys
import getopt
import cps
import cps_object
import nas_common_utils as nas_common

intf_rpc_key_id = 'dell-base-if-cmn/set-interface'
intf_rpc_op_attr_id = 'dell-base-if-cmn/set-interface/input/operation'
intf_rpc_op_type_map = {'create': 1, 'delete': 2, 'set': 3}

intf_lag_key = "dell-base-if-cmn/if/interfaces/interface"

learn_mode_to_val = {"drop":1,"disable":2,"hw":3,"cpu_trap":4,"cpu_log":5}

def nas_lag_op(op, data_dict,commit=True):
    if op == 'get':
        obj = cps_object.CPSObject(module=intf_lag_key, data=data_dict)
    else:
        if op in intf_rpc_op_type_map:
            data_dict[intf_rpc_op_attr_id] = intf_rpc_op_type_map[op]
        else:
            print 'Unknown operation type %s' % op
            return False
        obj = cps_object.CPSObject(
            module=intf_rpc_key_id,
            data=data_dict)
        op = 'rpc'
    obj.add_attr("if/interfaces/interface/type","ianaift:ieee8023adLag")
    if commit:
        nas_common.get_cb_method(op)(obj)
    else:
        return (obj, op)
    return None

def usage():
    ''' This is the Usage Method '''

    print '<cps_config_lag:- Lag operations >'
    print '\t\t cps_config_lag.py  --create --lname <lagname> [--port <port_list> [--frwd <yes/no>]'
    print '\t\t cps_config_lag.py  --delete --lname <lagname>'
    print '\t\t cps_config_lag.py  --add --lname <lagname> --port <port_list>'
    print '\t\t cps_config_lag.py  --remove --lname <lagname> --port <port_list>'
    print '\t\t cps_config_lag.py  --set --lname <lagname> --port <block/unblock port_list> --frwd <yes/no>'
    print '\t\t cps_config_lag.py  --set --lname <lagname> --mac  <macaddr> '
    print '\t\t cps_config_lag.py  --set --lname <lagname> --admn <up/down> '
    print '\t\t cps_config_lag.py  --get --lname <lagname> '
    print '\t\t cps_config_lag.py  --lname <lagname> --mac-learn <drop/disable/hw/cpu_trap/cpu_log> '
    print '\n Example'
    print '\t\t cps_config_lag.py --create --lanme bond1'
    print '\t\t cps_config_lag.py --delete --lname bond1'
    print '\t\t cps_config_lag.py --add --lname bond1 --port e101-001-0,e101-002-0 '
    print '\t\t cps_config_lag.py --set --lname bond1 --port e101-003-0,e101-004-0 '
    print '\t\t cps_config_lag.py --remove --lname bond1 --port e101-001-0,e101-002-0 '
    print '\t\t cps_config_lag.py --set --lname bond1 --port e101-001-0,e101-002-0 --frwd yes/no'
    print '\t\t cps_config_lag.py --set --lname bond1 --mac ae:6e:2e:b4:40:ff'
    print '\t\t cps_config_lag.py --set --lname bond1 --admn up'
    print '\t\t cps_config_lag.py --get --lname bond1'
    print '\t\t cps_config_lag.py --lname bond1 --mac-learn drop  '
    sys.exit(1)


def main(argv):
    ''' The main function will read the user input from the
    command line argument and  process the request  '''

    lag_id = ''
    choice = ''
    port = ''
    mac_addr = ''
    admn_status = ''
    frwd_enable = ''
    lag_name = ''
    mode = ''

    try:
        opts, args = getopt.getopt(argv, "hladpi:",
                                   ["id=", "port=", "lname=", "mac=", "admn=", "frwd=",
                                    "help", "create", "delete", "set", "get","add","remove","mac-learn="])

    except getopt.GetoptError:
        usage()

    for opt, arg in opts:

        if opt in ('-h', '--help'):
            choice = 'help'

        elif opt in ('-p', '--port'):
            port = arg

        elif opt in ('-m', '--mac'):
            mac_addr = arg

        elif opt in ('-i', '--id'):
            lag_id = arg

        elif opt in ('-c', '--create'):
            choice = 'create'

        elif opt in ('-d', '--delete'):
            choice = 'delete'

        elif opt in ('-s', '--set'):
            choice = 'set'

        elif opt in ('-g', '--get'):
            choice = 'get'

        elif opt in ('-a', '--add'):
            choice = 'add'

        elif opt in ('-r', '--remove'):
            choice = 'remove'

        elif opt == '--lname':
            lag_name = arg

        elif opt == '--admn':
            admn_status = arg

        elif opt == '--frwd':
            frwd_enable = arg

        elif opt in ('-ml','--mac-learn'):
            choice = 'mac-learn'
            mode = arg


    if choice == 'create' and lag_name != '':
        cmd_data = {"if/interfaces/interface/name":lag_name}
        nas_lag_op("create", cmd_data)
        if port != '':
            (obj, op)  = nas_lag_op("set", cmd_data, False)
            port_list = port.split(",")
            l = ["dell-if/if/interfaces/interface/member-ports","0","name"]
            index = 0
            for i in port_list:
                l[1]=str(index)
                index = index +1
                obj.add_embed_attr(l,i)
            nas_common.get_cb_method(op)(obj)
            if frwd_enable != '':
                cmd_data["dell-if/if/interfaces/interface/member-ports/name"] = port_list
                # Member ports are unblocked by default
                if frwd_enable == 'no':
                    cmd_data["base-if-lag/if/interfaces/interface/block-port-list"] = port_list
                nas_lag_op("set", cmd_data)

    elif choice == 'mac-learn' and lag_name != '':
        nas_lag_op("set", {"if/interfaces/interface/name":lag_name,
                           "base-if-lag/if/interfaces/interface/learn-mode":learn_mode_to_val[mode]})


    elif choice == 'delete' and lag_name != '':
        nas_lag_op(
            "delete",
            {"if/interfaces/interface/name":lag_name })

    elif choice == 'get' and lag_name != '':
        nas_lag_op("get", {"if/interfaces/interface/name": lag_name})

    elif choice == 'set' and port != '' and lag_name != '' and frwd_enable != '':
        port_list = port.split(",")
        nas_lag_op("set", {"if/interfaces/interface/name":lag_name,
                           "base-if-lag/if/interfaces/interface/unblock-port-list"
                           if (frwd_enable == 'yes') else
                           "base-if-lag/if/interfaces/interface/block-port-list": port_list})

    elif (choice == 'add' or choice == 'remove' or choice == 'set') and port != '' and lag_name != '':
        op = "set" if (choice == "add" or choice == "set") else "delete"
        attr_data = {"if/interfaces/interface/name": lag_name}
        (obj, op) = nas_lag_op(op,
                   attr_data, False)
        cur_mem_ports = []

        if choice == "add":
            obj_list = []
            r_obj = cps_object.CPSObject(module = intf_lag_key, data = attr_data)
            if cps.get([r_obj.get()], obj_list):
                if len(obj_list) == 1:
                    cps_obj = cps_object.CPSObject(obj=obj_list[0])
                    member_ports_obj = cps_obj.get_attr_data('dell-if/if/interfaces/interface/member-ports')
                    for key in member_ports_obj:
                        cur_mem_ports.append(member_ports_obj[key]['name'])

        port_list = sorted(port.split(",") + cur_mem_ports)
        l = ["dell-if/if/interfaces/interface/member-ports","0","name"]
        index = 0
        for i in port_list:
            l[1]=str(index)
            index = index +1
            obj.add_embed_attr(l,i)
        nas_common.get_cb_method(op)(obj)

    elif choice == 'set' and mac_addr != '' and lag_name != '':
        nas_lag_op(
            "set",
            {"if/interfaces/interface/name": lag_name,
             "dell-if/if/interfaces/interface/phys-address": mac_addr})

    elif choice == 'set' and admn_status != '' and lag_name != '':
        nas_lag_op("set", {"if/interfaces/interface/name": lag_name, "if/interfaces/interface/enabled":
                           "0" if (admn_status == 'down') else "1"})
    else:
        usage()

# Calling the main method
if __name__ == "__main__":
    main(sys.argv[1:])
