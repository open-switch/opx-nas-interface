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

import threading

import cps
import cps_object
import nas_common_header as nas_comm
import nas_front_panel_map as fp
import nas_phy_media as media
import nas_os_if_utils as nas_if
import time

def _get_obj_attr_value(obj, attr_name):
    try:
        value = obj.get_attr_data(attr_name)
        return value
    except:
        return None

def set_media_transceiver(interface_obj):
    # get front panel port from ifindex
    if_obj = cps_object.CPSObject(obj=interface_obj)
    npu = if_obj.get_attr_data('base-if-phy/if/interfaces/interface/npu-id')
    port = if_obj.get_attr_data('base-if-phy/if/interfaces/interface/port-id')
    enable = if_obj.get_attr_data('if/interfaces/interface/enabled')
    nas_if.log_info("set_media_transceiver as " + str(enable) + " for " + str(npu) + " , "  + str(port))
    port_list = nas_if.nas_os_phy_list(
        d={'npu-id': npu, 'port-id': port})
    phy_obj = cps_object.CPSObject(obj=port_list[0])
    try:
        hwport_list = phy_obj.get_attr_data('hardware-port-list')
    except:
        nas_if.log_err(" Error in setting media Transceiver for %s" % if_obj.get_attr_data('name'))
        return
    # set media transceiver using media Id and channel ID
    #
    for hwport in hwport_list:
        fp_details = fp.find_port_by_hwport(npu, hwport)
        if fp_details.port_group_id is None:
            _lane = fp_details.lane
        else:
            pg_list = fp.get_port_group_list()
            pg_obj = pg_list[fp_details.port_group_id]
            if ((pg_obj.get_profile_type()) == "ethernet_ddqsfp28"):
                _lane = pg_obj.get_lane(hwport)
            else:
                _lane = fp_details.lane
        media.media_transceiver_set(1, fp_details.media_id, _lane, enable)

def set_interface_media_speed(interface_obj, speed=None):
    try:
        if_obj = cps_object.CPSObject(obj=interface_obj)
        npu = if_obj.get_attr_data('base-if-phy/if/interfaces/interface/npu-id')
        port = if_obj.get_attr_data('base-if-phy/if/interfaces/interface/port-id')
        if speed is None:
            speed = if_obj.get_attr_data('dell-if/if/interfaces/interface/speed')
        hwport_details = nas_if.get_hwport_from_phy_port(npu, port)
        if hwport_details is None:
            nas_if.log_info("Hw  port not present")
            return
        fp_port,subport_id = nas_if.get_subport_id_from_hw_port(npu, hwport_details.hw_port)
        media_id = nas_if.get_media_id_from_fp_port(fp_port)
        media.media_led_set(1, media_id, subport_id, speed)
    except:
        nas_if.log_info("Failure in setting interface media speed")

def _update_fp(fp_obj):
    fr_port = nas_if.get_cps_attr(fp_obj, 'front-panel-port')
    fp_db = fp.find_front_panel_port(fr_port)
    fp_db.set_breakout_mode(nas_if.get_cps_attr(fp_obj, 'breakout-mode'))
    fp_db.set_speed(nas_if.get_cps_attr(fp_obj, 'port-speed'))

def _process_logical_if_event(obj):
    if_index = _get_obj_attr_value(obj, 'dell-base-if-cmn/if/interfaces/interface/if-index')
    speed = _get_obj_attr_value(obj, 'dell-if/if/interfaces/interface/speed')
    # check if if_index is present
    if if_index == None or speed is None:
        nas_if.log_err('Interface index not present in the interface event')
        return
    # Get Interface attributes
    if_obj_list = nas_if.nas_os_if_list(d={'if-index':if_index})
    if if_obj_list is None:
        nas_if.log_err('Failed to get Interface attributes by ifindex from NAS interface')
        return
    if len(if_obj_list) != 0:
        set_interface_media_speed(if_obj_list[0], speed)

def monitor_interface_event():
    handle = cps.event_connect()
    cps.event_register(handle, nas_comm.yang.get_value('physical_key', 'keys_id'))
    cps.event_register(handle, nas_comm.yang.get_value('logical_if_state_key', 'keys_id'))
    cps.event_register(handle, nas_comm.yang.get_value('obs_logical_if_key', 'keys_id'))
    _led_control = media.led_control_get()
    while True:
        o = cps.event_wait(handle)
        obj = cps_object.CPSObject(obj=o)

        if nas_comm.yang.get_value('fp_key', 'keys_id') == obj.get_key():
            _update_fp(obj)
        if nas_comm.yang.get_value('physical_key', 'keys_id') == obj.get_key():
            continue

        if nas_comm.yang.get_value('obs_logical_if_key', 'keys_id') == obj.get_key():
            _process_logical_if_event(obj)

        elif nas_comm.yang.get_value('logical_if_state_key', 'keys_id') == obj.get_key():
            if_index = _get_obj_attr_value(obj, 'if/interfaces-state/interface/if-index')
            # check if if_index is present
            if if_index is None:
                nas_if.log_err('Interface index not present in the interface state event')
                continue
            # Get Interface attributes
            if_obj_list = nas_if.nas_os_if_list(d={'if-index':if_index})
            if if_obj_list is None:
                nas_if.log_err('Failed to get Interface attributes by ifindex from NAS interface')
                continue
            admin_state = _get_obj_attr_value(obj, 'if/interfaces-state/interface/admin-status')
            if admin_state != None:
                # This is admin state change event
                try:
                    set_media_transceiver(if_obj_list[0])
                except:
                    nas_if.log_err("Unable to set media transceiver for if_index {}".format(str(if_index)))

            if _led_control == True:
                oper_state = _get_obj_attr_value(obj, 'if/interfaces-state/interface/oper-status')
                if oper_state != None:
                    try:
                        set_interface_media_speed(if_obj_list[0])
                    except:
                        nas_if.log_err("Error in setting LED")
                continue

class interfaceMonitorThread(threading.Thread):
    def __init__(self, threadID, name):
        threading.Thread.__init__(self)
        self.threadId = threadID
        self.name = name
    def run(self):
        monitor_interface_event()
    def __str__(self):
        return ' %s %d ' %(self.name, self.threadID)

def subscribe_events():
    while cps.enabled(nas_comm.yang.get_value('fp_key', 'keys_id'))  == False:
        #wait for front panel port objects to be ready
        nas_if.log_err('Media or front panel port object is not yet ready')
        time.sleep(1)

    if_thread = interfaceMonitorThread(1, "Interface event Monitoring Thread")
    if_thread.daemon = True
    if_thread.start()
