#
# Copyright (c) 2018 Dell Inc.
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
import event_log as ev
import nas_front_panel_map as fp
import nas_os_if_utils as nas_if
import nas_fp_port_utils as fp_utils
import nas_common_header as nas_comm
import logging
import sys


_port_group_state_key = cps.key_from_name('observed', 'base-pg/dell-pg/port-groups-state/port-group-state')
_port_group_key = cps.key_from_name('target', 'base-pg/dell-pg/port-groups/port-group')
port_group_state_id_attr = 'dell-pg/port-groups/port-group'

def pg_state_attr(t):
    return 'dell-pg/port-groups-state/port-group-state/' + t

def pg_attr(t):
    return 'dell-pg/port-groups/port-group/' + t

def base_pg_attr(t):
    return 'base-pg/' + pg_attr(t)

def base_pg_state_attr(t):
    return 'base-pg/' + pg_state_attr(t)



# Return Phy port Speed( Yang enum value)  based on Breakout mode , hwport speed( in Mbps), Phy mode

# Returns hwp port speed in the Ethernet Mbps ( 10000 Mbps or 25000 Mbps)
# Phy_speed in yang enum value
# fp breakout mode
# Phy mode Fc/Ether
def get_hwp_speed(breakout, phy_speed, phy_mode):
    hwp_count =  nas_comm.yang.get_value(breakout, 'breakout-to-hwp-count')
    _phy_speed_mbps = nas_if.from_yang_speed(phy_speed)
    if _phy_speed_mbps is None or hwp_count is None or hwp_count == 0:
        return None
    _hwp_speed_mbps = _phy_speed_mbps/hwp_count
    if phy_mode  == nas_comm.yang.get_value('fc', 'yang-phy-mode'):
        hwp_speed_eth =  nas_comm.get_hwp_fc_to_eth_speed(nas_if.to_yang_speed(_hwp_speed_mbps))
        return nas_if.from_yang_speed(hwp_speed_eth)
    else:
        return _hwp_speed_mbps

def add_fc_br_cap_objs(pg, fc_caps, resp):
    phy_mode =  nas_comm.yang.get_value('fc', 'yang-phy-mode')
    hwp_count = len(pg.get_hw_ports())
    fp_count = len(pg.get_fp_ports())
    for cap in fc_caps:
        mode = cap['breakout']
        skip_ports =  nas_comm.yang.get_value(mode, 'breakout-to-skip-port')
        hw_speed = cap['hwp_speed']
        phy_npu_speed = fp.get_phy_npu_port_speed(mode, (hw_speed * hwp_count) / fp_count)
        if fp.verify_npu_supported_speed(phy_npu_speed) == False:
#           don't add this entry of speed or breakout
            continue
        phy_fc_speed = cap['phy_fc_speed']
        phy_speed = nas_comm.yang.get_value(phy_fc_speed, 'mbps-to-yang-speed')
        cps_obj = cps_object.CPSObject(module='base-pg/dell-pg/port-groups-state/port-group-state/br-cap',
                                       qual='observed',
            data={pg_state_attr('br-cap/phy-mode'):phy_mode,
                pg_state_attr('br-cap/breakout-mode'):mode,
                pg_state_attr('br-cap/port-speed'):phy_speed,
                pg_state_attr('br-cap/skip-ports'):skip_ports,
                pg_state_attr('id'):pg.name })
        resp.append(cps_obj.get())
        cps_obj = None


# Create port group capability object for  port group object get request.
# Each Capability object includes breakout mode, phy mode and phy port speed
def create_and_add_pg_caps(pg, phy_mode, resp):
    if phy_mode == nas_comm.yang.get_value('fc', 'yang-phy-mode'):
        fc_caps = pg.get_fc_caps()
        if fc_caps is not None and len(fc_caps) != 0:
            add_fc_br_cap_objs(pg, fc_caps, resp)
            return
    br_modes = pg.get_breakout_caps()
    hwp_speeds = pg.get_hwport_speed_caps()
    hwp_count = len(pg.get_hw_ports())
    fp_count = len(pg.get_fp_ports())
    for mode in br_modes:
        skip_ports =  nas_comm.yang.get_value(mode, 'breakout-to-skip-port')
        for hw_speed in hwp_speeds:
            phy_npu_speed = fp.get_phy_npu_port_speed(mode, (hw_speed * hwp_count) / fp_count)
            if fp.verify_npu_supported_speed(phy_npu_speed) == False:
#               don't add this entry of speed or breakout
                continue

            phy_speed = phy_npu_speed
            if phy_mode == nas_comm.yang.get_value('fc', 'yang-phy-mode'):
                phy_speed = fp.get_fc_speed_frm_npu_speed(phy_npu_speed)
                if phy_speed == 0:
                    continue

            cps_obj = cps_object.CPSObject(module='base-pg/dell-pg/port-groups-state/port-group-state/br-cap',
                                           qual='observed',
                data={pg_state_attr('br-cap/phy-mode'):phy_mode,
                    pg_state_attr('br-cap/breakout-mode'):mode,
                    pg_state_attr('br-cap/port-speed'):phy_speed,
                    pg_state_attr('br-cap/skip-ports'):skip_ports,
                    pg_state_attr('id'):pg.name })
            resp.append(cps_obj.get())
            cps_obj = None
# Add FC Specific Breakout mode in the port group CPS object is present in the port group
def _append_fc_br_caps_to_pg_obj(pg, phy_mode, cap_list, cap_index):
    hwp_count = len(pg.get_hw_ports())
    fp_count = len(pg.get_fp_ports())
    fc_caps = pg.get_fc_caps()
    for cap in fc_caps:
        mode = cap['breakout']
        skip_ports =  nas_comm.yang.get_tbl('breakout-to-skip-port')[mode]
        hw_speed = cap['hwp_speed']
        phy_npu_speed = fp.get_phy_npu_port_speed(mode, (hw_speed * hwp_count) / fp_count)
        if fp.verify_npu_supported_speed(phy_npu_speed) == False:
#           don't add this entry of speed or breakout
            continue
        phy_fc_speed = cap['phy_fc_speed']
        phy_speed = nas_comm.yang.get_value(phy_fc_speed, 'mbps-to-yang-speed')
        cap_list[str(cap_index)] = {'phy-mode':phy_mode,
                                    'breakout-mode':mode,
                                    'port-speed':phy_speed,
                                    'skip-ports':skip_ports}
        cap_index += 1
    return cap_index


def add_pg_caps_to_pg_obj(pg, phy_mode, cap_list, cap_index = 0):
    if phy_mode == nas_comm.yang.get_value('fc', 'yang-phy-mode'):
        fc_caps = pg.get_fc_caps()
        if fc_caps is not None and len(fc_caps) != 0:
            return _append_fc_br_caps_to_pg_obj(pg, phy_mode, cap_list, cap_index)
    br_modes = pg.get_breakout_caps()
    hwp_speeds = pg.get_hwport_speed_caps()
    hwp_count = len(pg.get_hw_ports())
    fp_count = len(pg.get_fp_ports())
    for mode in br_modes:
        skip_ports =  nas_comm.yang.get_tbl('breakout-to-skip-port')[mode]
        for hw_speed in hwp_speeds:
            phy_npu_speed = fp.get_phy_npu_port_speed(mode, (hw_speed * hwp_count) / fp_count)
            if fp.verify_npu_supported_speed(phy_npu_speed) == False:
                # don't add this entry of spped or beakout
                continue

            phy_speed = phy_npu_speed
            if phy_mode == nas_comm.yang.get_value('fc', 'yang-phy-mode'):
                phy_speed = fp.get_fc_speed_frm_npu_speed(phy_npu_speed)
                if phy_speed == 0:
                    continue
            cap_list[str(cap_index)] = {'phy-mode':phy_mode,
                                        'breakout-mode':mode,
                                        'port-speed':phy_speed,
                                        'skip-ports':skip_ports}
            cap_index += 1

    return cap_index

# Add capability object for the port group in the cps get request
def add_all_pg_caps(pg, resp):
    if pg == None:
        return False
    create_and_add_pg_caps(pg, nas_comm.yang.get_value('ether', 'yang-phy-mode'), resp)
    if pg.is_fc_supported():
        create_and_add_pg_caps(pg, nas_comm.yang.get_value('fc', 'yang-phy-mode'), resp)

# Add capability to port group as child list in the cps get request
def add_all_pg_caps_to_pg_obj(pg, pg_obj):
    if pg == None:
        return False
    cap_list = {}
    cap_index = add_pg_caps_to_pg_obj(pg, nas_comm.yang.get_value('ether', 'yang-phy-mode'), cap_list)
    if pg.is_fc_supported():
        add_pg_caps_to_pg_obj(pg, nas_comm.yang.get_value('fc', 'yang-phy-mode'), cap_list, cap_index)
    pg_obj.add_attr(pg_state_attr('br-cap'), cap_list)

# Get request for port group state object
def create_and_add_pg_state_obj(pg, resp):
    if pg == None:
        return False
    cps_obj = cps_object.CPSObject(module='base-pg/dell-pg/port-groups-state/port-group-state', qual='observed',
            data={base_pg_state_attr('front-panel-port'):pg.get_fp_ports(),
                  base_pg_state_attr('hwport-list'):pg.get_hw_ports(),
                  pg_state_attr('default-phy-mode'):pg.get_def_phy_mode(),
                  pg_state_attr('default-breakout-mode'):pg.get_def_breakout(),
                  pg_state_attr('default-port-speed'):pg.get_default_phy_port_speed(),
                  pg_state_attr('id'):pg.name })
    add_all_pg_caps_to_pg_obj(pg, cps_obj)
    resp.append(cps_obj.get())
    add_all_pg_caps(pg,resp)

    return True

def gen_port_group_state_list(obj, resp):
    pg_name = nas_if.get_cps_attr(obj,pg_state_attr('id'))
    pg_list = fp.get_port_group_list()
    if pg_name != None:
        pg = pg_list[pg_name]
        return create_and_add_pg_state_obj(pg, resp)
    else:
        # return all port groups
        for name in pg_list:
            ret = create_and_add_pg_state_obj(pg_list[name], resp)
        return True

def create_pg_cps_obj(pg):
    if pg == None:
        return None
    cps_obj = cps_object.CPSObject(module='base-pg/dell-pg/port-groups/port-group', qual='target',
            data={pg_attr('id'):pg.name,
                pg_attr('phy-mode'):pg.get_phy_mode(),
                pg_attr('breakout-mode'):pg.get_breakout_mode(),
                pg_attr('port-speed'):pg.get_port_speed()
                  })
    return cps_obj

def gen_port_group_list(obj, resp):
    pg_name = nas_if.get_cps_attr(obj,pg_attr('id'))
    pg_list = fp.get_port_group_list()
    if pg_name != None:
        pg = pg_list[pg_name]
        obj = create_pg_cps_obj(pg)
        if obj == None:
            return False
        resp.append(obj.get())

    else:
        # return all port groups
        for name in pg_list:
            obj = create_pg_cps_obj(pg_list[name])
            if obj != None:
                resp.append(obj.get())
        return True

def get_pg_cb(methods, params):
    print " Get port group callback "
    try:
        ret= get_int_pg_cb(methods, params)
    except:
        logging.exception('error:')
    return ret

def get_int_pg_cb(methods, params):
    obj = cps_object.CPSObject(obj=params['filter'])
    resp = params['list']

    if obj.get_key() == _port_group_key:
        gen_port_group_list(obj, resp)
    else:
        return False
    return True


# Set config handler for unified SFP port group type.
def set_sfp_port_group_config(pg, br_mode, port_speed, phy_mode):
    # Get the list of front port and HW Ports
    fp_list = pg.get_fp_ports()
    hwp_list = pg.get_hw_ports()
    fp_list.sort() # make sure that fp is in the sorted order in the list
    # set the HW port mapping to 0 fpr each fp
    rollback_list = []
    for port in fp_list:
        hwports = []
        ret = fp_utils.set_fp_to_hwp_mapping(port, br_mode, port_speed, phy_mode, hwports)
        if ret == False:
            # rollback
            nas_if.log_err('failed to clean hw_port list for port %d, start rollback' % port)
            for port, hwports in rollback_list:
                    nas_if.log_info('add back hw_port list %s to port %d' % (hwports, port))
                    fp_utils.set_fp_to_hwp_mapping(port, pg.get_breakout_mode(), pg.get_port_speed(),
                                                   pg.get_phy_mode(), hwports)
            return False
        rollback_list.insert(0, (port, hwports))
    # set the HW port mapping based on the breakout mode

    # re-verify this case
    hwp_count =  nas_comm.yang.get_tbl('breakout-to-hwp-count')[br_mode]
    hwp_list.sort()
    hwp_list.reverse()  # so that we can pop out from the end
    first_fp_port = fp_list[0]
    nas_if.log_info('br_mode %s hwport list  %s fp_list %s' % (str(br_mode), str(hwp_list), str(fp_list)))
    for port in fp_list:
        port_offset = port - first_fp_port
        if port_offset not in sfp_pg_br_to_fp_map[br_mode]:
            continue # skip this fp port if not valid in this breakout mode
        if len(hwp_list) < hwp_count:
            nas_if.log_err(' some problem in setting fp port to hwp mapping')
            return False
        hwports = []
        i =0
        while i < hwp_count:
            hwports.append(hwp_list.pop())
            i = i + 1
        ret = fp_utils.set_fp_to_hwp_mapping(port, br_mode, port_speed, phy_mode, hwports)
        if ret == False:
            # rollback
            nas_if.log_err('failed to add hw_port list %s to port %d, start rollback' %
                           (hwports, port))
            for port, hwports in rollback_list:
                    if len(hwports) == 0:
                        nas_if.log_info('clean hw_port list for port %d' % port)
                        fp_utils.set_fp_to_hwp_mapping(port, br_mode, port_speed, phy_mode,
                                                       hwports)
                    else:
                        nas_if.log_info('add back hw_port list %s to port %d' % (hwports, port))
                        fp_utils.set_fp_to_hwp_mapping(port, pg.get_breakout_mode(), pg.get_port_speed(),
                                                       pg.get_phy_mode(), hwports)
            return False
        rollback_list.insert(0, (port, []))
    return True

# Set breakout mode to DDQSFP28 port group or QSFP28 port group
def set_qsfp28_port_group_config(pg, fp_br_mode, port_speed, phy_mode):
    fp_list = pg.get_fp_ports()
    if fp_list == None:
        print 'FP list is empty'
        return False

    rollback_list = []
    for fp_port in fp_list:
        ret = fp_utils.set_fp_port_config(fp_port, fp_br_mode, port_speed, phy_mode)
        if ret == False:
            # Rollback
            nas_if.log_err('failed to config fp_port %d, start rollback' % fp_port)
            for fp_port in rollback_list:
                nas_if.log_info('rollback port %d config' % fp_port)
                fp_utils.set_fp_port_config(fp_port, pg.get_breakout_mode(),
                                            pg.get_port_speed(), pg.get_phy_mode())
            return False
        rollback_list.append(fp_port)
    return True

# Generic port group config handler
def set_port_group_config(pg, phy_mode, br_mode, port_speed):
    nas_if.log_info('config port group %s' % pg.name)

    fp_list = pg.get_fp_ports()
    pg_type = pg.get_profile_type()
    fp_br_mode = br_mode
    if pg_type == "unified_sfp":
        set_sfp_port_group_config(pg, fp_br_mode, port_speed, phy_mode)
    # If any of the value is None then skip it
    elif pg_type == 'ethernet_sfp28' or pg_type == 'unified_qsfp28' or pg_type == 'ethernet_qsfp' or pg_type == 'unified_qsfp':
        rollback_list = []
        for port in fp_list:
            ret = fp_utils.set_fp_port_config(port, fp_br_mode, port_speed, phy_mode)
            if ret == False:
                nas_if.log_err('failed to config fp_port %d, start rollback' % port)
                for port in rollback_list:
                    nas_if.log_info('rollback port %d config' % port)
                    fp_utils.set_fp_port_config(port, pg.get_breakout_mode(),
                                                pg.get_port_speed(), pg.get_phy_mode())
                return False
            rollback_list.append(port)
    elif pg_type == 'ethernet_ddqsfp28':
        fp_br_mode = nas_comm.yang.get_value(br_mode, 'ddqsfp-2-qsfp-brmode')
        ret = set_qsfp28_port_group_config(pg, fp_br_mode, port_speed, phy_mode)
    elif pg_type == 'ethernet_qsfp28':
        ret = set_qsfp28_port_group_config(pg, fp_br_mode, port_speed, phy_mode)
    else:
        nas_if.log_err('Unknown port group type %s' % pg_type)
        return False

    if phy_mode != None:
        pg.set_phy_mode(phy_mode)
    if br_mode != None:
        pg.set_breakout_mode(br_mode)
    if port_speed != None:
        pg.set_port_speed(port_speed)
    hwp_speed = get_hwp_speed(fp_br_mode, port_speed, phy_mode)
    pg.set_hwp_speed(hwp_speed)
    return True

def set_int_pg_cb(methods, params):
    nas_if.log_info( "set port group config")
    obj = cps_object.CPSObject(obj=params['change'])
    if obj.get_key() == _port_group_key:
        pg_name = nas_if.get_cps_attr(obj,pg_attr('id'))
        if pg_name == None:
            nas_if.log_err('port group name not present')
            return False
        phy_mode = nas_if.get_cps_attr(obj,pg_attr('phy-mode'))
        br_mode = nas_if.get_cps_attr(obj,pg_attr('breakout-mode'))
        port_speed = nas_if.get_cps_attr(obj,pg_attr('port-speed'))
        pg_list = fp.get_port_group_list()
        ret = set_port_group_config(pg_list[pg_name], phy_mode, br_mode, port_speed)
        if ret == True:
            obj = create_pg_cps_obj(pg_list[pg_name])
            params['change'] = obj.get()
    else:
        return False

    return True

def set_pg_cb(methods, params):
    try:
        ret= set_int_pg_cb(methods, params)
    except:
        logging.exception('error:')
    return ret

def get_pg_state_cb(methods, params):
    try:
        ret= get_int_pg_state_cb(methods, params)
    except:
        logging.exception('error:')
    return ret


def get_int_pg_state_cb(methods, params):
    obj = cps_object.CPSObject(obj=params['filter'])
    resp = params['list']

    if obj.get_key() == _port_group_state_key:
        gen_port_group_state_list(obj, resp)
    else:
        return False

    return True


def nas_pg_cps_register(handle):
    d = {}
    d['get'] = get_pg_state_cb
    cps.obj_register(handle, _port_group_state_key, d)

    d = {}
    d['get'] = get_pg_cb
    d['transaction'] = set_pg_cb
    cps.obj_register(handle, _port_group_key, d)

