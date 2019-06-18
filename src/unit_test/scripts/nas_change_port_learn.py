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

import cps_object
import nas_os_if_utils
import sys
from ifindex_utils import *
import nas_common_utils as nas_common

mac_learn_modes = {
    "drop": "1",
            "disable": "2",
            "hw": "3",
            "cpu_trap": "4",
            "cpu_log": "5",
}


def nas_port_learn_op(op, data_dict):
    obj = cps_object.CPSObject(
        module=nas_os_if_utils.get_if_keys()[0],
        data=data_dict)
    nas_common.get_cb_method(op)(obj)


def usage():
    print "\n\n nas_change_port_learn.py [interface] [learn_mode] - change the mac learn mode of interface"
    print "[learn_mode] - drop, disable, hw, cpu_trap, cpu_log"
    exit()


if __name__ == '__main__':
    if len(sys.argv) == 3:
        nas_port_learn_op("set", {"ifindex": if_nametoindex(sys.argv[1]),
                                  "learn-mode": mac_learn_modes[sys.argv[2]]})
    else:
        usage()
