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
import cps_object


def if_nametoindex(name):
    """
    Converts interface name to ifindex
    @name - inteface name
    @return - ifindex in case of success, else raises exception
    """
    obj = cps_object.CPSObject("base-ip/ipv4", data={"name": name})
    l = []
    if cps.get([obj.get()], l):
        if len(l) == 1:
            get_obj = cps_object.CPSObject(obj=l[0])
            return get_obj.get_attr_data("ifindex")
    raise RuntimeError("Invalid Name")


def if_indextoname(ifindex):
    """
    Converts interface index to name
    @ifindex - inteface index
    @return - interface name in case of success, else raises exception
    """
    obj = cps_object.CPSObject("base-ip/ipv4", data={"ifindex": ifindex})
    l = []
    if cps.get([obj.get()], l):
        if len(l) == 1:
            get_obj = cps_object.CPSObject(obj=l[0])
            return get_obj.get_attr_data("name")
    raise RuntimeError("Invalid Index")
