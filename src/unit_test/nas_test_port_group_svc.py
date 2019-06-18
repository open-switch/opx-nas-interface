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
import cps_utils
import event_log as ev
import nas_fp_port_utils as fp_utils
import nas_front_panel_map as fp
import nas_os_if_utils as nas_if

import nas_port_group_utils as nas_pg

import sys
import time
import xml.etree.ElementTree as ET

if __name__ == '__main__':
    fp_utils.init()
    fp.show_all_fp_ports()
    fp.show_all_port_groups()

    handle = cps.obj_init()

    nas_pg.nas_pg_cps_register(handle)
    while True:
        time.sleep(1)

