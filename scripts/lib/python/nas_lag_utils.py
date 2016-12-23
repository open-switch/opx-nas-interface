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

import cps_utils

lag_key_string = ["dell-base-if-cmn/if/interfaces/interface"]


def get_lag_keys():
    return lag_key_string


def lag_port_list_func(port_list):
    if isinstance(port_list, list):
        return port_list
    ifindex_list = port_list.split(",")
    return ifindex_list

cps_utils.add_convert_function("base-lag/entry/port-list", lag_port_list_func)
