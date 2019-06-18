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

import os
import cps
import cps_utils
import cps_object
import sys

import nas_os_if_utils as nas_if

# Following unit test script was tested in S4112T with 100G support
# If your platform has different IF name for the 100G interface, REPLACE
# this string appropriately.
# ***************************** IMPORTANT NOTE ***************************
# For any other IF Speed like 40G,50G, EDIT this script to execute only
# allowable breakout modes
# ************************************************************************

ifname ="e101-013-0"

def cps_get(obj, attrs={}):
    resp = []
    if cps.get([cps_object.CPSObject(obj, qual='observed', data=attrs).get()], resp):
        return resp
    return None

def check_intf_presence(intf_name):
    attr = 'if/interfaces-state/interface/if-index'
    if_details = cps_get('dell-base-if-cmn/if/interfaces-state/interface', {'if/interfaces-state/interface/name' : intf_name })
    if not if_details:
        print 'Invalid Interface : ' + intf_name
        return 1 #ERROR SCENARIO

    #print "CPS GET DUMP" + str(if_details)
    d = if_details[0]['data']
    if attr not in d:
        return 1 #ERROR SCENARIO
    if_index = cps_utils.cps_attr_types_map.from_data(attr, d[attr])
    print("Interface %s IF Index is %s " % (intf_name, str(if_index)))
    return 0


nas_fp_cache = nas_if.FpPortCache()
front_port = nas_if.get_front_port_from_name(ifname)
fp_port_id=front_port[0]
fp_obj = nas_fp_cache.get(fp_port_id)
pg_name= nas_if.get_cps_attr(fp_obj, 'port-group') # None if interface not part of port-group

# Run fanout on the port-group
if pg_name:
    def test_fanout_100g_to_10g():
        print '=========== Configuring fanout for %s with Mode 4x1 and 10G Speed ===========' % pg_name
        cmd_str = '/usr/bin/opx-config-fanout --pg_name ' + pg_name + ' --mode 4x1  --speed 10g'
        os.system(cmd_str)
        #Get interfaces in the port group
        temp = ifname[:-1]+str(1)
        #Check if the fanout interface present
        assert check_intf_presence(temp) == 0

    def revert_fanout_100g():
        #Deleting the fanout config in the given port-group name
        print '=========== Deleting fanout for %s =========== ' % pg_name
        cmd_str = '/usr/bin/opx-config-fanout  --pg_name ' + pg_name + ' --mode 1x1 --speed 100g'
        return os.system(cmd_str)

    def test_complete():
         assert revert_fanout_100g() == 0

# Run fanout on the interface
else:
    def test_fanout_100g_to_10g():
         print '=========== Configuring fanout for %s with Mode 4x1 and 10G Speed ===========' % ifname
         cmd_str = '/usr/bin/opx-config-fanout --port ' + ifname + ' --mode 4x1 --speed 10g'
         os.system(cmd_str)
         temp = ifname[:-1] + str(4)
         assert check_intf_presence(temp) == 0

    def revert_fanout_100g():
         #Deleting the fanout config using one of the newly created fanout interface
         print '=========== Deleting fanout for %s =========== ' % ifname
         temp = ifname[:-1] + str(2)
         cmd_str = '/usr/bin/opx-config-fanout --port ' + temp + ' --mode 1x1 --speed 100g'
         return os.system(cmd_str)

    def test_fanout_100g_to_25g():
         revert_fanout_100g()
         print '=========== Configuring fanout for %s with Mode 4x1 and 25G Speed ===========' % ifname
         cmd_str = '/usr/bin/opx-config-fanout --port ' + ifname + ' --mode 4x1 --speed 25g'
         os.system(cmd_str)
         temp = ifname[:-1] + str(3)
         assert check_intf_presence(temp) == 0

    def test_fanout_100g_to_50g():
         revert_fanout_100g()
         print '=========== Configuring fanout for %s with Mode 2x1 and 50G Speed ===========' % ifname
         cmd_str = '/usr/bin/opx-config-fanout --port ' + ifname + ' --mode 2x1 --speed 50g'
         os.system(cmd_str)
         temp = ifname[:-1] + str(2)
         assert check_intf_presence(temp) == 0

    def test_complete():
         assert revert_fanout_100g() == 0

