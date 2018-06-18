#!/usr/bin/python

import os
import cps
import cps_utils
import cps_object
import sys

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
    print d
    if attr not in d:
        return 1 #ERROR SCENARIO
    if_index = cps_utils.cps_attr_types_map.from_data(attr, d[attr])
    print("Interface %s IF Index is %s " % (intf_name, str(if_index)))
    return 0

def test_fanout_100g_to_10g():
     print '=========== Configuring fanout for %s with Mode 4x1 and 10G Speed ===========' % ifname
     cmd_str = '/opt/dell/opx/bin/opx-config-fanout ' + ifname + ' 4x1 10G'
     os.system(cmd_str)
     temp = ifname[:-1] + str(4)
     assert check_intf_presence(temp) == 0

def revert_fanout_100g():
     #Deleting the fanout config using one of the newly created fanout interface
     print '=========== Deleting fanout for %s =========== ' % ifname
     temp = ifname[:-1] + str(2)
     cmd_str = '/opt/dell/opx/bin/opx-config-fanout ' + temp + ' 1x1 100G'
     return os.system(cmd_str)

def test_fanout_100g_to_25g():
     revert_fanout_100g()
     print '=========== Configuring fanout for %s with Mode 4x1 and 25G Speed ===========' % ifname
     cmd_str = '/opt/dell/opx/bin/opx-config-fanout ' + ifname + ' 4x1 25G'
     os.system(cmd_str)
     temp = ifname[:-1] + str(3)
     assert check_intf_presence(temp) == 0

def test_fanout_100g_to_50g():
     revert_fanout_100g()
     print '=========== Configuring fanout for %s with Mode 2x1 and 50G Speed ===========' % ifname
     cmd_str = '/opt/dell/opx/bin/opx-config-fanout ' + ifname + ' 2x1 50G'
     os.system(cmd_str)
     temp = ifname[:-1] + str(2)
     assert check_intf_presence(temp) == 0

def test_complete():
     assert revert_fanout_100g() == 0
