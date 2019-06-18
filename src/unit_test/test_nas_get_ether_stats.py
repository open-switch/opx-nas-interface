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

def test_get_ether_stat():
  cps_obj = cps_object.CPSObject(module = "dell-base-if-cmn/if/interfaces-state/interface/statistics", qual="observed")
  cps_obj.add_attr('if/interfaces-state/interface/name', "e101-001-0")
  ret_list = []
  print "Input object: ", cps_obj.get()
  assert cps.get([cps_obj.get()], ret_list)
  for ret in ret_list:
    o = cps_object.CPSObject(obj=ret)
    cps_utils.print_obj(o.get())

