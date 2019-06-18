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
import cps
import nas_common_header as nas_comm
import nas_os_if_utils as nas_if


hg_state_key = cps.key_from_name('observed', 'base-pg/dell-pg/port-groups-state/hybrid-group-state')
hg_key = cps.key_from_name('target', 'base-pg/dell-pg/port-groups/hybrid-group')


def hg_attr(t):
    return 'dell-pg/port-groups/hybrid-group/' + t


def base_hg_attr(t):
    return 'base-pg/' + hg_attr(t)


def hg_state_attr(t):
    return 'dell-pg/port-groups-state/hybrid-group-state/' + t


def base_hg_state_attr(t):
    return 'base-pg/' + hg_state_attr(t)


def print_hg_cps_obj(o):
    if o is None:
        return None

    obj = cps_object.CPSObject(obj=o)
    hg_name = nas_if.get_cps_attr(obj, hg_attr('id'))
    hg_profile = nas_if.get_cps_attr(obj, hg_attr('profile'))
    port_list = nas_if.get_cps_attr(obj, hg_attr('port'))
    print("Hybrid Group Name: %s Profile: %s " % (hg_name, hg_profile))

    for port_idx in port_list:
        port = port_list[port_idx]
        port_id = port['port-id']
        phy_mode = port['phy-mode']
        breakout_mode = port['breakout-mode']
        port_speed = port['port-speed']
        breakout_option = (breakout_mode,port_speed)
        breakout = nas_comm.yang.get_key(breakout_option, 'yang-breakout-port-speed')
        print("Port ID: %s Phy-mode: %s Breakout-mode: %s " % (str(port_id),
                                                               str(nas_comm.yang.get_key(phy_mode, 'yang-phy-mode')),
                                                               str(breakout)))


#Get Hybrid Group State Object
def get_hg_state(hg_name=""):
    resp = []
    obj = cps_object.CPSObject('base-pg/dell-pg/port-groups-state/hybrid-group-state',
                               qual='observed',
                               data={hg_state_attr('id'):hg_name})
    cps.get([obj.get()], resp)
    return resp


#Get Hybrid Group Object
def get_hg(hg_name=""):
    resp = []
    obj = cps_object.CPSObject('base-pg/dell-pg/port-groups/hybrid-group',
                               qual='target',
                               data={hg_attr('id'):hg_name})
    cps.get([obj.get()], resp)
    return resp


#Set Hybrid Group State Object
def set_hg(hg_name, profile=None, port_id=None, br_mode=None, port_speed=None, phy_mode=None):
    cps_obj = cps_object.CPSObject(module="base-pg/dell-pg/port-groups/hybrid-group", data={})
    cps_obj.add_attr(hg_attr('id'), hg_name)

    if profile is not None:
        cps_obj.add_attr(hg_attr('profile'), profile)

    if port_id is not None and br_mode is not None and port_speed is not None and phy_mode is not None:
        cps_obj.add_embed_attr([hg_attr('port'), "0", "port-id"], str(port_id), 4)
        cps_obj.add_embed_attr([hg_attr('port'), "0", "breakout-mode"], br_mode, 4)
        cps_obj.add_embed_attr([hg_attr('port'), "0", "port-speed"], port_speed, 4)
        cps_obj.add_embed_attr([hg_attr('port'), "0", "phy-mode"], phy_mode, 4)

    cps.transaction([{"operation": "set", "change": cps_obj.get()}])
