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
import cps
import cps_utils
import cps_object

import nas_os_if_utils as nas_if

import sys

types = cps_utils.CPSTypes()


def print_object(obj):
    types.print_object(obj)


def args_to_dict(args):
    d = {}
    for i in args:
        if i.find('=') != -1:
            l = i.split('=', 1)
            d[l[0]] = l[1]


def fill_obj(obj, args):
    for arg_str in args:
        if arg_str.find('=') == -1:
            continue
        (key, val) = arg_str.split('=')
        if val.isdigit():
            val = int(val)
        nas_if.set_obj_val(obj, key, val)


def change_phy(args):
    npu = None
    port = None

    if 'npu' in args:
        npu = int(args['npu'])
    if 'port' in args:
        port = int(args['port'])
    l = nas_if.nas_os_phy_list(npu, port)
    if l is None or len(l) == 0 or len(l) > 1:
        print "Cound not find interface"
        sys.exit(1)

    obj = l[0]

    fill_obj(obj, args)

    print "Changing.. port"
    print obj

    ch = {'operation': 'set', 'change': obj}
    if 'operation' in args:
        ch['operation'] = args['operation']

    cps.transaction([ch])
    print_object(ch['change'])


def create_if(args):
    obj = nas_if.make_if_obj()

    fill_obj(obj, args)

    ch = {'operation': 'create', 'change': obj}
    print ch

    cps.transaction([ch])
    types.print_object(ch['change'])


def delete_if(args):
    _phy = 'base-port/interface'
    obj = get_if_obj(args[0])
    args = args[1:]
    ch = {'change': obj, 'operation': 'delete'}

    if cps.transaction([ch]) != True:
        print "Failed...."
    else:
        types.print_object(ch['change'])

def create_if_rpc(args):
    obj = cps_object.CPSObject(module='dell-base-if-cmn/set-interface',
                               data={'dell-base-if-cmn/set-interface/input/operation': 1})

    fill_obj(obj, args)

    ch = {'operation': 'rpc', 'change': obj.get()}
    print ch

    cps.transaction([ch])
    types.print_object(ch['change'])

def set_if_rpc(args):
    obj = cps_object.CPSObject(module='dell-base-if-cmn/set-interface',
                               data={'dell-base-if-cmn/set-interface/input/operation': 3})

    fill_obj(obj, args)

    ch = {'operation': 'rpc', 'change': obj.get()}
    print ch

    cps.transaction([ch])
    types.print_object(ch['change'])


def delete_if_rpc(args):
    obj = cps_object.CPSObject(module='dell-base-if-cmn/set-interface',
                               data = {'dell-base-if-cmn/set-interface/input/operation': 2})
    fill_obj(obj, args)
    ch = {'change': obj.get(), 'operation': 'rpc'}

    if cps.transaction([ch]) != True:
        print "Failed...."
    else:
        types.print_object(ch['change'])


def change_if(args):
    _phy = 'base-port/interface'
    obj = get_if_obj(args[0])

    args = args[1:]

    if len(args) == 0:
        types.print_object(obj)
    else:
        change_int(_phy, obj, args, 'mtu')
        change_int(_phy, obj, args, 'admin-status')

        ch = {'change': obj, 'operation': 'set'}

        cps.transaction([ch])
        types.print_object(ch['change'])

handlers = {
    #'phy-print': phy_print,
    #'if-print': if_print,
    'phy-set': change_phy,
    'if-create': create_if,
    'if-set': change_if,
    'if-delete': delete_if,
    'if-create-rpc': create_if_rpc,
    'if-delete-rpc': delete_if_rpc,
    'if-set-rpc': set_if_rpc,
}

if __name__ == '__main__':
    args = sys.argv[1:]
    if len(args) == 0 or args[0] not in handlers:
        print "Missing args.."
        sys.exit(1)

    handlers[args[0]](args[1:])
