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

def test_get_all_intf():
  cps_obj = cps_object.CPSObject(module = "dell-base-if-cmn/if/interfaces/interface", qual="target")

    ret_list = []
    print "Input object: ", cps_obj.get()

    if cps.get([cps_obj.get()], ret_list):
        if ret_list:
            for ret_obj in ret_list:
                print "-----------------------------------------------------"
                cps_utils.printable(ret_obj)
                print ret_obj
                print "-----------------------------------------------------"


def test_get_exact_intf():
  cps_obj = cps_object.CPSObject(module = "dell-base-if-cmn/if/interfaces/interface", qual="target")
  ret_list = []
  cps_obj.add_attr("dell-base-if-cmn/if/interfaces/interface/if-index", 10)
  print "Input object: ", cps_obj.get()

  if cps.get([cps_obj.get()], ret_list):
    if ret_list:
        for ret_obj in ret_list:
            print "-----------------------------------------------------"
            cps_utils.printable(ret_obj)
            print ret_obj
            print "-----------------------------------------------------"

def test_get_all_vlan_intf():
  cps_obj = cps_object.CPSObject(module = "dell-base-if-cmn/if/interfaces/interface", qual="target")
  ret_list = []
  cps_obj.add_attr("if/interfaces/interface/type", "ianaift:l2vlan")
  print "Input object: ", cps_obj.get()

  if cps.get([cps_obj.get()], ret_list):
    if ret_list:
        for ret_obj in ret_list:
            print "-----------------------------------------------------"
            cps_utils.printable(ret_obj)
            print ret_obj
            print "-----------------------------------------------------"

def test_get_all_lag_intf():
  cps_obj = cps_object.CPSObject(module = "dell-base-if-cmn/if/interfaces/interface", qual="target")
  ret_list = []
  cps_obj.add_attr("if/interfaces/interface/type", "ianaift:ieee8023adLag")
  print "Input object: ", cps_obj.get()

  if cps.get([cps_obj.get()], ret_list):
    if ret_list:
        for ret_obj in ret_list:
            print "-----------------------------------------------------"
            cps_utils.printable(ret_obj)
            print ret_obj
            print "-----------------------------------------------------"

def test_get_all_vxlan_intf():
  cps_obj = cps_object.CPSObject(module = "dell-base-if-cmn/if/interfaces/interface", qual="target")
  ret_list = []
  cps_obj.add_attr("if/interfaces/interface/type", "base-if:vxlan")
  print "Input object: ", cps_obj.get()

  if cps.get([cps_obj.get()], ret_list):
    if ret_list:
        for ret_obj in ret_list:
            print "-----------------------------------------------------"
            cps_utils.printable(ret_obj)
            print ret_obj
            print "-----------------------------------------------------"

def test_get_all_vlan_sub_intf():
  cps_obj = cps_object.CPSObject(module = "dell-base-if-cmn/if/interfaces/interface", qual="target")
  ret_list = []
  cps_obj.add_attr("if/interfaces/interface/type", "base-if:vlanSubInterface")
  print "Input object: ", cps_obj.get()

  if cps.get([cps_obj.get()], ret_list):
    if ret_list:
        for ret_obj in ret_list:
            print "-----------------------------------------------------"
            cps_utils.printable(ret_obj)
            print ret_obj
            print "-----------------------------------------------------"
