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
import nas_front_panel_map as fp
import nas_os_if_utils as nas_if
import nas_phy_port_utils as port_utils
import nas_common_header as nas_comm
import logging

import time
import os.path

current_profile = 'base-switch/switching-entities/switching-entity/switch-profile'

def _breakout_i_attr(t):
    return 'base-if-phy/breakout/input/' + t


def _fp_attr(t):
    return 'base-if-phy/front-panel-port/' + t


def _lane_attr(t):
    return 'base-if-phy/hardware-port/' + t

# Create Breakout capability object for the front panel port
def create_and_add_fp_caps(fp_port, resp):
    br_modes = fp_port.get_breakout_caps()
    hw_speeds = fp_port.get_hwport_speed_caps()
    hwp_count = len(fp_port.get_hwports())
    phy_mode = nas_comm.yang.get_value('ether', 'yang-phy-mode') # FP port does not support FC capability directly. it is on the port group
    for mode in br_modes:
        skip_ports = nas_comm.yang.get_tbl('breakout-to-skip-port')[mode]
        for speed in hw_speeds:
            phy_speed = fp.get_phy_npu_port_speed(mode, speed * hwp_count)
            if fp.verify_npu_supported_speed(phy_speed) == False:
                nas_if.log_info("create_and_add_fp_caps: fp port %s doesn't support yang speed %s " % (str(fp_port.id),str(phy_speed)))
# breakout mode and this speed is excluded from cps show
                continue
            cps_obj = cps_object.CPSObject(module='base-if-phy/front-panel-port/br-cap',
                                           qual='target',
                data={_fp_attr('br-cap/phy-mode'):phy_mode,
                    _fp_attr('br-cap/breakout-mode'):mode,
                    _fp_attr('br-cap/port-speed'):phy_speed,
                    _fp_attr('br-cap/skip-ports'):skip_ports,
                    _fp_attr('front-panel-port'):fp_port.id })
            resp.append(cps_obj.get())
            cps_obj = None

# Add Breakout capability to front panel port as child list
def add_fp_caps_to_fp_obj(fp_port, fp_obj):
    br_modes = fp_port.get_breakout_caps()
    hw_speeds = fp_port.get_hwport_speed_caps()
    hwp_count = len(fp_port.get_hwports())
    phy_mode = nas_comm.yang.get_value('ether', 'yang-phy-mode') # FP port does not support FC capability directly. it is on the port group
    cap_index = 0
    cap_list = {}
    for mode in br_modes:
        skip_ports = nas_comm.yang.get_tbl('breakout-to-skip-port')[mode]
        for speed in hw_speeds:
            phy_speed = fp.get_phy_npu_port_speed(mode, speed * hwp_count)
            if fp.verify_npu_supported_speed(phy_speed) == False:
                nas_if.log_info("create_and_add_fp_caps: fp port %s doesn't support yang speed %s " % (
                               str(fp_port.id), str(phy_speed)))
                # breakout mode and this speed is excluded from cps show
                continue
            cap_list[str(cap_index)] = {'phy-mode':phy_mode,
                                        'breakout-mode':mode,
                                        'port-speed':phy_speed,
                                        'skip-ports':skip_ports}
            cap_index += 1
    fp_obj.add_attr('br-cap', cap_list)

# Generate FP ports object list
def gen_fp_port_list(obj, resp):

    for npu in fp.get_npu_list():

        for p in npu.ports:

            port = npu.ports[p]

            if not obj.key_compare({_fp_attr('front-panel-port'): port.id}):
                continue

            media_id = port.media_id

            if not obj.key_compare({_fp_attr('media-id'): media_id}):
                continue

            elem = cps_object.CPSObject(module='base-if-phy/front-panel-port',
                                        data={
                                        'npu-id': npu.id,
                                        'front-panel-port': port.id,
                                        'control-port': port.control_port(),
                                        'port': port.hwports,
                                        'default-name': port.name,
                                        'media-id': media_id,
                                        'mac-offset':port.mac_offset,
                                        'port-speed':port.get_port_speed(),
                                        'default-phy-mode':port.get_def_phy_mode(),
                                        'default-breakout-mode':port.get_def_breakout(),
                                        'default-port-speed':port.get_default_phy_port_speed(),
                                        'profile-type':port.get_profile_type()
                                        })
            if port.is_pg_member() == False:
                breakout_mode = port.get_breakout_mode()
                elem.add_attr('breakout-mode',breakout_mode)
                add_fp_caps_to_fp_obj(port, elem)
            resp.append(elem.get())
            if port.is_pg_member() == False:
                create_and_add_fp_caps(port, resp)
            elif port.is_pg_member() == True:
                elem.add_attr('port-group',port.get_port_group_id())

# Generate HW port obj list
def _gen_npu_lanes(obj, resp):

    for npu in fp.get_npu_list():
        key_dict = {_lane_attr('npu-id'): npu.id
                     }

        if not obj.key_compare(key_dict):
            continue

        for p in npu.ports:
            port = npu.ports[p]

            for h in port.hwports:
                if not obj.key_compare({_lane_attr('hw-port'): h}):
                    continue

                elem = cps_object.CPSObject(module='base-if-phy/hardware-port',
                                            data= {
                                            'npu-id': npu.id,
                                            'hw-port': h,
                                            'front-panel-port': port.id,
                                            'hw-control-port':
                                                port.control_port(),
                                            'subport-id': port.lane(h),
                                            'fanout-mode': port.get_breakout_mode()
                                            })
                nas_if.log_info(str(elem.get()))
                resp.append(elem.get())

# publish FP object update
def send_fp_event(port):
    handle = cps.event_connect()
    fp_port_obj = cps_object.CPSObject(module='base-if-phy/front-panel-port',
                                data={
                                'npu-id':port.npu,
                                'front-panel-port': port.id,
                                'control-port': port.control_port(),
                                'port': port.hwports,
                                'default-name': port.name,
                                'breakout-mode':port.get_breakout_mode(),
                                'port-speed':port.get_port_speed()
                                })
    cps.event_send(handle,fp_port_obj.get())

# Set FP port config
def set_fp_port_config(fr_port, br_mode, phy_port_speed, phy_mode):
    nas_if.log_info('Front-panel-port %d config request: br_mode %s speed %s phy_mode %s' % (
                    fr_port, br_mode, phy_port_speed, phy_mode))

    fp_port_obj = fp.find_front_panel_port(fr_port)
    if fp_port_obj is None:
        nas_if.log_err('Front-panel-port %d not found' % fr_port)
        return False

    npu = fp_port_obj.npu
    if phy_mode == None:
        phy_mode = nas_comm.yang.get_value('ether', 'yang-phy-mode')


    #Check if breakout mode is same as current breakout mode
    if (br_mode == fp_port_obj.get_breakout_mode() and
        fp_port_obj.get_phy_mode() == phy_mode and
        fp_port_obj.get_port_speed() == phy_port_speed):
        nas_if.log_info('FP config is same as current config')
        return True

    #check if breakout mode is supported by the FP port   ( not now)
    # TODO

    port_list = port_utils.get_phy_port_list()
    #convert FP port to Hwport list ( control port)
    hwports = fp_port_obj.hwports[:]
    deleted_phy_ports = {}
    for hwport in hwports:
        phy_port_obj = port_utils.get_phy_port_by_hw_port(port_list, npu, hwport)
        if phy_port_obj== None:
            continue
        if port_utils.cps_del_nas_port(phy_port_obj):
            port_utils.del_phy_port(port_list, phy_port_obj)
            deleted_phy_ports[hwport] = phy_port_obj
        else:
            nas_if.log_err('Failed to delete physical port')
            return False
    # Create new phy ports based on the new breakout mode
    created_phy_ports = {}
    if port_utils.create_nas_ports(npu, hwports, br_mode, phy_port_speed,phy_mode,
                                   fr_port, created_phy_ports) == True:
        for port in created_phy_ports:
            port_utils.add_phy_port(port_list, created_phy_ports[port])
    else:
        nas_if.log_err('Failed to create physical port, rollback to previous status')
        port_utils.rollback_port_add_del(port_list, created_phy_ports, deleted_phy_ports,
                                         fp_port_obj.get_port_speed(),
                                         fp_port_obj.get_phy_mode())
        return False

    #set new breakout mode and new port speed
    fp_port_obj.set_breakout_mode(br_mode)
    fp_port_obj.set_port_speed(phy_port_speed)
    send_fp_event(fp_port_obj)
    return True

# update FP to HW port mapping
# If new hwport list is empty then first delete all physical ports associated with the
#    existing hwports in the FP. Then update the FP object with empty hwport list.
# If new hwport list is not empty then create new physical ports based on the
#   config parameters then update the FP to HWp mapping and HWP to physical port mapping.
def set_fp_to_hwp_mapping(fr_port, br_mode, port_speed, phy_mode, hwports):
    # If hwports is empty then delete all phy ports under the FP and set the hwport list to NOne
    port_detail = fp.find_front_panel_port(fr_port)
    if port_detail == None:
        return False
    hwp_list = port_detail.get_hwports()
    port_list = port_utils.get_phy_port_list()
    if len(hwports) == 0:
        npu = port_detail.npu
        if hwp_list != None:
            for hwp in hwp_list:
                phy_obj = port_utils.get_phy_port_by_hw_port(port_list, npu, hwp)
                if phy_obj != None:
                    # delete the physical port
                    ret = port_utils.cps_del_nas_port(phy_obj)
                    if ret == False:
                        nas_if.log_err(' failed to delete Phy port')
                        return False
                    # Delete hwp to phy port mapping
                    port_utils.del_phy_port(port_list, phy_obj)
            hwports += hwp_list
        # set the hw port list to None
        port_detail.set_hwports(None)
        # set breakout mode to None
        port_detail.set_breakout_mode(None)
        return True
    else:
        # hwports is not empty. It means add the hwport to the fp port
        # create phy port corresponding to the hwport
        # check if hwport list is empty
        if hwp_list != None and len(hwp_list) > 0:
            # hw port list is not empty, return False
            print 'port list is not empty'
            return False
        port_detail.set_hwports(hwports)
        # Now create phy ports base on the new hw port list.
        ret = set_fp_port_config(fr_port, br_mode, port_speed, phy_mode)
        if not ret:
            # Rollback
            port_detail.set_hwports(None)
        return ret
    return True

# Handle breakout mode
def set_fp_rpc_cb(params):
    try:
        return set_fp_rpc_cb_int(params)
    except:
        logging.exception('breakout error: ')

def set_fp_rpc_cb_int(params):
    nas_if.log_info('received set breakout config command')

    obj = cps_object.CPSObject(obj=params['change'])

    fr_port = obj.get_attr_data(_breakout_i_attr('front-panel-port'))
    br_mode = nas_if.get_cps_attr(obj, _breakout_i_attr('breakout-mode'))
    phy_port_speed = nas_if.get_cps_attr(obj, _breakout_i_attr('port-speed'))
    if br_mode == None:
        nas_if.log_err('No breakout mode given in input')
        return False
    if phy_port_speed == None:
        nas_if.log_err('phy port speed not received in cps')
        return False
    # set the breakout mode
    return set_fp_port_config(fr_port, br_mode, phy_port_speed, None)

# Registration of front panel port and hw port object handler

# Breakout of FP port RPC handler
def set_fp_cb(methods, params):
    if params['operation'] == 'rpc':
        return set_fp_rpc_cb(params)
    else:
        nas_if.log_err('Only RPC action is supported')
        return False

# Get Call method of FP and HW port objects
def get_cb(methods, params):
    obj = cps_object.CPSObject(obj=params['filter'])
    resp = params['list']

    try:
        if obj.get_key() == nas_comm.yang.get_tbl('keys_id')['fp_key']:
            gen_fp_port_list(obj, resp)
        elif obj.get_key() == nas_comm.yang.get_tbl('keys_id')['npu_lane_key']:
            _gen_npu_lanes(obj, resp)
        else:
            return False
    except:
        logging.exception('Front Panel Port Get Error: ')

    return True

def nas_fp_cps_register(handle):

    d = {}
    d['get'] = get_cb
    d['transaction'] = set_fp_cb

    cps.obj_register(handle, nas_comm.yang.get_tbl('keys_id')['fp_key'], d)
    cps.obj_register(handle, nas_comm.yang.get_tbl('keys_id')['npu_lane_key'], d)
    cps.obj_register(handle, nas_comm.yang.get_tbl('keys_id')['breakout_key'], d)

def get_npu_port_from_fp(fp_port, sub_port):
    nas_if.log_info('Trying to get npu port based on front-panel %d subport %d' % (
                    fp_port, sub_port))
    port_obj = fp.find_front_panel_port(fp_port)
    if port_obj == None:
        raise ValueError('Front-panel-port %d not found in cache' % fp_port)
    br_mode = port_obj.get_breakout_mode()
    nas_if.log_info('Cached breakout mode of front panel port %s is %s' % (str(fp_port), str(br_mode)))
    lane_id = nas_comm.subport_to_lane(br_mode, sub_port)
    if lane_id == None:
        raise ValueError('Failed to get lane id from br_mode %d subport %d' % (
                         br_mode, sub_port))
    if isinstance(lane_id, tuple):
        lane_id, flag = lane_id
        nas_if.log_info('Get lane id %d with extended condition %s' % (lane_id, flag))
    npu_id = port_obj.npu
    hw_port = port_obj.hwports[lane_id]
    nas_if.log_info('Front panel port %d lane %d hw-port %d' % (fp_port, lane_id, hw_port))
    port_list = port_utils.get_phy_port_list()
    port_id = port_utils.hw_port_to_phy_port(port_list, npu_id, hw_port)
    if port_id == -1:
        raise ValueError('There is no physical mapped to hw_port %d for subport %d' % (
                         hw_port, sub_port))
    return (npu_id, port_id, hw_port)

def get_mac_offset_from_fp(fp_port, sub_port, fp_cache = None):
    nas_if.log_info('Trying to get npu port based on front-panel %d subport %d' % (
                    fp_port, sub_port))
    if fp_cache is None:
        # Use local front-panel-port db
        port_obj = fp.find_front_panel_port(fp_port)
        if port_obj is None:
            raise ValueError('Front-panel-port %d not found in cache' % fp_port)
        br_mode = port_obj.get_breakout_mode()
        mac_offset = port_obj.mac_offset
    else:
        cps_port_obj = fp_cache.get(fp_port)
        if cps_port_obj is None:
            raise ValueError('Front-panel-port %d not found in cps cache' % fp_port)
        br_mode = nas_if.get_cps_attr(cps_port_obj, _fp_attr('breakout-mode'))
        mac_offset = nas_if.get_cps_attr(cps_port_obj, _fp_attr('mac-offset'))
        if br_mode is None or mac_offset is None:
            raise ValueError('Mandatory attributes not found in cps object')

    nas_if.log_info('Cached breakout mode of front panel port %d is %d' % (fp_port, br_mode))
    lane_id = nas_comm.subport_to_lane(br_mode, sub_port)
    if lane_id is None:
        raise ValueError('Failed to get lane id from br_mode %d subport %d' % (
                         br_mode, sub_port))
    if isinstance(lane_id, tuple):
        lane_id, flag = lane_id
        nas_if.log_info('Get lane id %d with extended condition %s' % (lane_id, flag))

    return mac_offset + lane_id

def init_profile():

     while cps.enabled(nas_comm.yang.get_tbl('keys_id')['switch_key']) is False:
        nas_if.log_err('Switch profile service not yet ready')
        time.sleep(1)

     l = []
     obj = cps_object.CPSObject(module='base-switch/switching-entities/switching-entity',
                                qual='observed',data={'switch-id':0})
     if not cps.get([obj.get()], l):
         nas_if.log_info('Get profile : CPS GET FAILED')
         return False
     switch_obj = cps_object.CPSObject(obj = l[0])
     try:
        profile = switch_obj.get_attr_data(current_profile)
     except:
        nas_if.log_info("Current profile missing in CPS get")
        return False
     nas_if.log_info('Get profile returned ' +str(profile))
     profile = '/etc/opx/' + profile + '-base_port_physical_mapping_table.xml'
     if os.path.isfile(profile):
        fp.init(profile)
        return True
     else:
        nas_if.log_err('Profile file missing' + str(profile))
        return False

def init():
    if init_profile() == False:
        nas_if.log_info('Using original file for profile')
        fp.init('/etc/opx/base_port_physical_mapping_table.xml')

