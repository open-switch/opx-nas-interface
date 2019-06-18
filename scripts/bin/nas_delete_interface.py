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
import sys
import cps_object

import nas_os_if_utils as nas_if

_g_if_name = 'if/interfaces/interface/name'

def create_if_map(lst):
    if_n = {}

    for i in lst:
        obj = cps_object.CPSObject(obj=i)
        name = obj.get_attr_data(_g_if_name)
        if_n[name] = obj

    return if_n

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print "Missing parameters.. please provide interface name eg.. e00-1"
        sys.exit(1)

    l = nas_if.nas_os_if_list()

    if_names = create_if_map(l)

    if not sys.argv[1] in if_names:
        print "Interface is invalid... " + sys.argv[1]
        sys.exit(1)

    obj = if_names[sys.argv[1]]
    # disable the port first before delteting the interface
    obj.add_attr('enabled', False)

    ch = {'operation': 'set', 'change': obj.get()}
    if cps.transaction([ch]) != True:
        print "Failed to shutdown interface, exiting..."
        sys.exit(0)

    ch = {'operation': 'delete', 'change': obj.get()}
    if cps.transaction([ch]):
        print "Successful"
        sys.exit(0)

    print "Failed to delete interface..."
    print obj.get()
