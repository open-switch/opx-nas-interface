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
import nas_yang_values as yv

global yang
yang = yv.YangValues()

# returns Eth speed for Hwp from FC speed
def get_hwp_fc_to_eth_speed(fc_speed):
    if fc_speed in yang.get_tbl('fc-to-ether-speed'):
        return yang.get_tbl('fc-to-ether-speed')[fc_speed]
    else:
        return None

def subport_to_lane(br_mode, subport_id):
    if br_mode not in yang.get_tbl('breakout-to-subport-lane-map'):
        return None
    subport_to_lane_map = yang.get_tbl('breakout-to-subport-lane-map')[br_mode]
    if subport_id not in subport_to_lane_map:
        return None
    return subport_to_lane_map[subport_id]

def lane_to_subport(br_mode, lane_id, qsfp28_40g_mode = False):
    if br_mode not in yang.get_tbl('breakout-to-subport-lane-map'):
        return None
    subport_to_lane_map = yang.get_tbl('breakout-to-subport-lane-map')[br_mode]
    subport_id = None
    for sp, lane in subport_to_lane_map.items():
        flag = False
        if isinstance(lane, tuple):
            lane, flag = lane
        if lane_id == lane and flag == qsfp28_40g_mode:
            subport_id = sp
            break
    return subport_id

def is_fec_supported(fec_mode, if_speed):
    if ((if_speed == yang.get_value('25g', 'yang-speed') or
         if_speed == yang.get_value('50g', 'yang-speed') or
         if_speed == yang.get_value('100g', 'yang-speed')) and
        (fec_mode == yang.get_value('auto', 'yang-fec') or
         fec_mode == yang.get_value('off', 'yang-fec'))):
        # AUTO and OFF modes are supported by FEC enabled port
        return True

    if ((if_speed == yang.get_value('25g', 'yang-speed') or
         if_speed == yang.get_value('50g', 'yang-speed')) and
        (fec_mode == yang.get_value('CL74', 'yang-fec') or
         fec_mode == yang.get_value('CL108', 'yang-fec'))):
        # 25G/50G port supports CL74 and CL108 modes
        return True

    if (if_speed == yang.get_value('100g', 'yang-speed') and
        fec_mode == yang.get_value('CL91', 'yang-fec')):
        # 100G port supports CL91 modes
        return True
    return False
