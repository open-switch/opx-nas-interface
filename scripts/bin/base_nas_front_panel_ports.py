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

import cps
import cps_object
import cps_utils
import event_log as ev
import nas_front_panel_map as fp
import nas_os_if_utils as nas_if
import nas_mac_addr_utils as ma
import nas_phy_media as media
import bytearray_utils as ba
import dn_base_ip_tool
import ifindex_utils

import sys
import time
import xml.etree.ElementTree as ET
from nas_front_panel_map import NPU
import copy

_breakout_key = cps.key_from_name('target', 'base-if-phy/breakout')
_fp_key = cps.key_from_name('target', 'base-if-phy/front-panel-port')
_npu_lane_key = cps.key_from_name('target', 'base-if-phy/hardware-port')
_set_intf_key = cps.key_from_name('target', 'dell-base-if-cmn/set-interface')
_get_mac_key = cps.key_from_name('target', 'dell-base-if-cmn/get-mac-address')

npu_attr_name = 'base-if-phy/if/interfaces/interface/npu-id'
port_attr_name = 'base-if-phy/if/interfaces/interface/port-id'
fp_port_attr_name = 'base-if-phy/hardware-port/front-panel-port'
subport_attr_name = 'base-if-phy/hardware-port/subport-id'
ifindex_attr_name = 'dell-base-if-cmn/if/interfaces/interface/if-index'
ifname_attr_name = 'if/interfaces/interface/name'
mac_attr_name = 'dell-if/if/interfaces/interface/phys-address'
speed_attr_name = 'dell-if/if/interfaces/interface/speed'
negotiation_attr_name = 'dell-if/if/interfaces/interface/negotiation'
autoneg_attr_name = 'dell-if/if/interfaces/interface/auto-negotiation'
duplex_attr_name = 'dell-if/if/interfaces/interface/duplex'
media_type_attr_name = 'base-if-phy/if/interfaces/interface/phy-media'
vlan_id_attr_name = 'base-if-vlan/if/interfaces/interface/id'
op_attr_name = 'dell-base-if-cmn/set-interface/input/operation'

_yang_auto_speed = 8  # default value for auto speed
_yang_auto_neg = 1    # default value for negotiation
_yang_neg_on =2
_yang_neg_off =3
_yang_auto_dup = 3      #auto - default value for duplex
_yang_dup_full = 1
_yang_dup_half = 2

_yang_breakout_1x1 = 4
_yang_breakout_4x1 = 2

def _breakout_i_attr(t):
    return 'base-if-phy/breakout/input/' + t


def _fp_attr(t):
    return 'base-if-phy/front-panel-port/' + t


def _lane_attr(t):
    return 'base-if-phy/hardware-port/' + t

class IF_CONFIG:
    def __init__ (self, if_name, npu, port, media_type, breakout_mode):
        self.name = if_name
        self.npu = npu
        self.port = port
        self.negotiation = None
        self.speed = None
        self.duplex = None

        self.media_type = media_type
        self.breakout_mode = breakout_mode

    def get_npu_port(self):
        return(self.npu, self.port)

    def get_media_type(self):
        return(self.media_type)

    def set_media_type(self, media_type):
        self.media_type = media_type

    def get_speed(self):
        return(self.speed)

    def set_speed(self, speed):
        self.speed = speed

    def get_negotiation(self):
        return(self.negotiation)

    def set_negotiation(self, negotiation):
        self.negotiation = negotiation

    def get_duplex(self):
        return(self.duplex)

    def set_duplex(self, duplex):
        self.duplex = duplex

    def get_breakout_mode(self):
        return(self.breakout_mode)

    def show(self):
        nas_if.log_info( "If Name:  " + str(self.name))
        nas_if.log_info( "Speed: " + str(self.speed))
        nas_if.log_info( "negotiation: " +  str(self.negotiation))
        nas_if.log_info( "Duplex: " + str(self.duplex))
        nas_if.log_info( "npu: " + str(self.npu))
        nas_if.log_info( "port: " + str(self.port))

# if_config is caching autoneg speed configuration.
_if_config = {}

def find_if_config_by_npu_port(npu, port):
    for if_name in _if_config:
        if (npu, port) == _if_config[if_name].get_npu_port():
            return if_name
    return None

def _if_config_get(if_name):
    if if_name in _if_config:
        return _if_config[if_name]
    return None

def _if_config_add_interface(if_name, config_obj):
    if if_name not in _if_config:
        _if_config[if_name] = config_obj
    else:
        nas_if.log_err("_if_config interface already exists")
    return

def _get_if_media_type(fp_port):
    media_type = 0
    try:
        media_id = _get_media_id_from_fp_port(fp_port)
        if media_id == 0:
            return media_type
        media_info = media.get_media_info(media_id)
        media_obj = cps_object.CPSObject(obj=media_info[0])
        media_type = media_obj.get_attr_data('type')
        return media_type
    except:
        return None

def _add_default_speed(media_type, cps_obj):
    # fetch default speed
    speed = media.get_default_media_setting(media_type, 'speed')
    if speed == None:
        return
    cps_obj.add_attr(speed_attr_name, speed)
    nas_if.log_info("default speed is " + str(speed))

def _add_default_autoneg(media_type, cps_obj):
    # fetch default autoneg
    autoneg = media.get_default_media_setting(media_type, 'autoneg')
    if autoneg == None:
        return
    nas_if.log_info("default autoneg is " + str(autoneg))
    cps_obj.add_attr(autoneg_attr_name, autoneg)

def _add_default_duplex(media_type, cps_obj):
    # fetch default duplex
    duplex = media.get_default_media_setting(media_type, 'duplex')
    if duplex == None:
        duplex = _yang_dup_full
    cps_obj.add_attr(duplex_attr_name, duplex)
    nas_if.log_info("default Duplex is " + str(duplex))

def _set_autoneg(negotiation, config, obj):
    config.set_negotiation(negotiation)
    if negotiation == _yang_auto_neg:
        media = config.get_media_type()
        _add_default_autoneg(media, obj)
    else:
        autoneg = False
        if negotiation == _yang_neg_on:
            autoneg = True
        obj.add_attr(autoneg_attr_name, autoneg)
    return

def _set_duplex(duplex, config, obj):
    config.set_duplex(duplex)
    if duplex == _yang_auto_dup:
        media = config.get_media_type()
        _add_default_duplex(media, obj)
    else:
        obj.add_attr(duplex_attr_name, duplex)
    return

# fetch media type from Media event object and then add corresponding default speed/autoneg
def if_handle_set_media_type(op, obj):
    if_name = None
    try:
        npu = obj.get_attr_data(npu_attr_name)
        port = obj.get_attr_data(port_attr_name)
        media_type = obj.get_attr_data(media_type_attr_name)
    except:
        nas_if.log_err('missing npu,port or media type in set media type request')
        return
    # find npu, port in the _if_config
    if_name = find_if_config_by_npu_port(npu, port)
    if if_name == None:
        nas_if.log_err("No interface present for the npu "+str(npu)+ "and port " +str(port))
        return
    nas_if.log_info( "if name is " +str(if_name))
    config = _if_config[if_name]
    config.set_media_type(media_type)
    obj.add_attr(ifname_attr_name, if_name)
    # set the default speed if the speed is configured to auto it is in non-breakout mode
    if config.get_breakout_mode() == _yang_breakout_1x1 and config.get_speed() == _yang_auto_speed:
        _add_default_speed(media_type, obj)
    if config.get_negotiation() == _yang_auto_neg:
        _add_default_autoneg(media_type, obj)
    if config.get_duplex() == _yang_auto_dup:
        _add_default_duplex(media_type, obj)

    nas_if.log_info("media type setting is successful for " +str(if_name))
    config.show()

def _if_update_config(op, obj):

    if_name = None
    npu_id = None
    port_id = None
    negotiation = None
    speed = None
    duplex = None
    media_type = None
    breakout_mode = None
    breakout_cap = None

    nas_if.log_info("update config for " +str(op))
    try:
        if_name = obj.get_attr_data(ifname_attr_name)
    except:
        # check for media_type change event obj
        nas_if.log_err('process media event')
        if_handle_set_media_type(op, obj)
        return
    if op == 'create':
        if_type = _get_intf_type(obj)
        if if_type != 'front-panel':
            return
        try:
            npu_id = obj.get_attr_data(npu_attr_name)
            port_id = obj.get_attr_data(port_attr_name)
        except:
            nas_if.log_err(' update config: Unable to get npu or port from the obj ' +str(if_name))
            return
        #read media type from PAS
        try:
            (breakout_cap, breakout_mode) = nas_if.get_port_breakoutCap_currentMode_mode(npu_id, port_id)
        except:
            nas_if.log_err(' unable to get breakout mode ' + str(if_name))
            return

        fp_port = nas_if.get_cps_attr(obj, fp_port_attr_name)
        media_type = _get_if_media_type(fp_port)

        config = IF_CONFIG(if_name, npu_id, port_id, media_type, breakout_mode)
        _if_config_add_interface(if_name, config)
        nas_if.log_info(' interface config updated successfully for ' + str(if_name))
    if op == 'set' or op == 'create':
        negotiation = nas_if.get_cps_attr(obj, negotiation_attr_name)
        speed = nas_if.get_cps_attr(obj, speed_attr_name)
        duplex = nas_if.get_cps_attr(obj, duplex_attr_name)
        # update the new speed, duplex and autoneg in the config. If
        # autoneg, speed or duplex is auto then fetch default value and replace in the cps object
        # do not set auto speed in case of breakout mode.
        config = _if_config_get(if_name)
        if config == None:
            nas_if.log_err(' interface not present in config')
            return
        breakout_mode = config.get_breakout_mode()
        nas_if.log_info("breakout mode " + str(breakout_mode))
        if speed != None and speed != config.get_speed():
            nas_if.log_info("speed " + str(speed))
            if speed == _yang_auto_speed and breakout_mode == _yang_breakout_1x1:
                nas_if.log_info("set default speed media type " + str(config.get_media_type()))
                # TODO default speed in breakout mode is not supported  yet
                _add_default_speed(config.get_media_type(), obj)
            config.set_speed(speed)

        # in case of negotiation, add autoneg attribute to on or off or default autoneg if negotiation== auto
        if negotiation != None and negotiation != config.get_negotiation():
            _set_autoneg(negotiation, config, obj)
            nas_if.log_info ('negotiation is ' + str(negotiation))

        if duplex != None and duplex != config.get_duplex():
            _set_duplex(duplex, config, obj)
            nas_if.log_info("duplex is " + str(duplex))
        config.show()

    if op == 'delete':
        # remove the interface entry from the config
        del _if_config[if_name]


def _get_media_id_from_fp_port(fp_port):
    for npu in fp.get_npu_list():
        for p in npu.ports:
            port = npu.ports[p]
            if fp_port == port.id:
                return port.media_id
    return None


def _gen_fp_port_list(obj, resp):

    for npu in fp.get_npu_list():

        for p in npu.ports:

            port = npu.ports[p]

            if not obj.key_compare({_fp_attr('front-panel-port'): port}):
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
                                        })

            resp.append(elem.get())


def get_phy_port_cache_keys(npu, port):
    return "port-%d-%d" % (npu, port)


def get_phy_port_cache_hw_keys(npu, port):
    return "hw-%d-%d" % (npu, port)


def get_phy_port_cache():
    l = []
    m = {}
    if not cps.get([cps_object.CPSObject(module='base-if-phy/physical').get()], l):
        return

    for i in l:
        ph = cps_object.CPSObject(obj=i)
        m[get_phy_port_cache_hw_keys(ph.get_attr_data('npu-id'),
                                     ph.get_attr_data('hardware-port-id'))] = ph
        m[get_phy_port_cache_keys(ph.get_attr_data('npu-id'),
                                  ph.get_attr_data('port-id'))] = ph
    return m


def _gen_npu_lanes(obj, resp):
    m = get_phy_port_cache()

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

                key = get_phy_port_cache_hw_keys(npu.id, h)

                pm = 4  # port mode 4 is disabled
                if key in m:
                    pm = m[key].get_attr_data('fanout-mode')

                elem = cps_object.CPSObject(module='base-if-phy/hardware-port',
                                            data= {
                                            'npu-id': npu.id,
                                            'hw-port': h,
                                            'front-panel-port': port.id,
                                            'hw-control-port':
                                                port.control_port(),
                                            'subport-id': port.lane(h),
                                            'fanout-mode': pm
                                            })
                nas_if.log_info(str(elem.get()))
                resp.append(elem.get())


def get_cb(methods, params):
    obj = cps_object.CPSObject(obj=params['filter'])
    resp = params['list']

    if obj.get_key() == _fp_key:
        _gen_fp_port_list(obj, resp)
    elif obj.get_key() == _npu_lane_key:
        _gen_npu_lanes(obj, resp)
    else:
        return False

    return True


def hw_port_to_nas_port(ports, npu, hwport):
    ph = get_phy_port_cache_hw_keys(npu, hwport)
    if ph in ports:
            return ports[ph].get_attr_data('port-id')
    return -1


def set_cb(methods, params):
    obj = cps_object.CPSObject(obj=params['change'])

    if params['operation'] != 'rpc':
        return False

    fr_port = obj.get_attr_data(_breakout_i_attr('front-panel-port'))
    mode = obj.get_attr_data(_breakout_i_attr('breakout-mode'))

    port_obj = fp.find_front_panel_port(fr_port)
    if port_obj is None:
        return False

    m = get_phy_port_cache()

    npu = port_obj.npu

    control_port = hw_port_to_nas_port(
        m,
        npu,
        port_obj.control_port())
    if control_port == -1:
        return False

    port_list = []

    if mode == 2:  # breakout - 1->4
        port_list.append(control_port)
    if mode == 4:  # breakin 4->1
        for i in port_obj.hwports:
            port_list.append(hw_port_to_nas_port(m, npu, i))

    for i in port_list:
        if i == -1:
            nas_if.log_err("Invalid port list detected.. not able to complete operation ")
            nas_if.log_err(port_list)
            return False

    breakout_req = cps_object.CPSObject(module='base-if-phy/set-breakout-mode',
                                        data={
                                        'base-if-phy/set-breakout-mode/input/breakout-mode':
                                        mode,
                                        'base-if-phy/set-breakout-mode/input/npu-id':
                                            npu,
                                        'base-if-phy/set-breakout-mode/input/port-id':
                                        control_port,
                                        'base-if-phy/set-breakout-mode/input/effected-port':
                                        port_list
                                        })

    tr = cps_utils.CPSTransaction([('rpc', breakout_req.get())])
    if tr.commit() == False:
        return False

    return True

def get_lag_id_from_name(lag_name):
    idx = 0
    while idx < len(lag_name):
        if lag_name[idx].isdigit():
            break
        idx += 1
    if idx >= len(lag_name):
        lag_id = 0
    else:
        lag_id_str = lag_name[idx:]
        if lag_id_str.isdigit():
            lag_id = int(lag_id_str)
        else:
            lag_id = 0
    return lag_id

def get_alloc_mac_addr_params(if_type, cps_obj):
    ret_list = {'if_type': if_type}
    if if_type == 'front-panel':
        port_id = None
        try:
            port_id = cps_obj.get_attr_data(port_attr_name)
        except ValueError:
            pass
        if port_id == None:
            front_panel_port = None
            subport_id = None
            try:
                front_panel_port = cps_obj.get_attr_data(fp_port_attr_name)
                subport_id = cps_obj.get_attr_data(subport_attr_name)
            except ValueError:
                pass
            if front_panel_port == None or subport_id == None:
                try:
                    if_name = cps_obj.get_attr_data(ifname_attr_name)
                except ValueError:
                    nas_if.log_err('Failed to read interface name')
                    return None
                front_port = nas_if.get_front_port_from_name(if_name, False)
                if front_port == None:
                    nas_if.log_err('Failed to parse front port from interface name %s' % if_name)
                    return None
                front_panel_port = front_port[0]
                subport_id = front_port[1]
                cps_obj.add_attr(fp_port_attr_name, front_panel_port)
                cps_obj.add_attr(subport_attr_name, subport_id)
            port_obj = fp.find_front_panel_port(front_panel_port)
            if port_obj == None:
                nas_if.log_err('Invalid front panel port id %d' % front_panel_port)
                return None
            if subport_id > len(port_obj.hwports):
                nas_if.log_err('Invalid subport id %d' % subport_id)
                return None
            if subport_id > 0:
                subport_id -= 1
            npu_id = port_obj.npu
            hw_port = port_obj.hwports[subport_id]
            m = get_phy_port_cache()
            port_id = hw_port_to_nas_port(m, npu_id, hw_port)
            if port_id == -1:
                nas_if.log_err('There is no physical mapped to hw_port %d' % hw_port)
                return None
            cps_obj.add_attr(npu_attr_name, npu_id)
            cps_obj.add_attr(port_attr_name, port_id)
        else:
            try:
                npu_id = cps_obj.get_attr_data(npu_attr_name)
            except ValueError:
                nas_if.log_err('Input object does not contain npu id attribute')
                return None
            m = get_phy_port_cache()
            ph_key = get_phy_port_cache_keys(npu_id, port_id)
            if not ph_key in m:
                nas_if.log_err('Physical port object not found')
                return None
            hw_port = m[ph_key].get_attr_data('hardware-port-id')
        npu = fp.get_npu(npu_id)
        if hw_port == None or npu == None:
            nas_if.log_err('No hardware port id or npu object for front panel port')
            return None
        p = npu.port_from_hwport(hw_port)
        lane = p.lane(hw_port)
        mac_offset = p.mac_offset + lane
        ret_list['fp_mac_offset'] = mac_offset
    elif if_type == 'vlan':
        try:
            vlan_id = cps_obj.get_attr_data(vlan_id_attr_name)
        except ValueError:
            nas_if.log_err('Input object does not contain VLAN id attribute')
            return None
        ret_list['vlan_id'] = vlan_id
    elif if_type == 'lag':
        try:
            lag_name = cps_obj.get_attr_data(ifname_attr_name)
        except ValueError:
            nas_if.log_err('Input object does not contain name attribute')
            return None
        lag_id = get_lag_id_from_name(lag_name)
        ret_list['lag_id'] = lag_id
    else:
        nas_if.log_err('Unknown interface type %s' % if_type)
        return None
    return ret_list

def _get_intf_type(cps_obj):
    if_type_map = {'ianaift:ethernetCsmacd': 'front-panel',
                   'ianaift:l2vlan': 'vlan',
                   'ianaift:ieee8023adLag': 'lag',
                   'ianaift:softwareLoopback': 'loopback',
                   'base-if:management': 'management'}
    try:
        obj_if_type = cps_obj.get_attr_data('if/interfaces/interface/type')
    except:
        return None
    if not obj_if_type in if_type_map:
        nas_if.log_err('Unknown if type: '+str(obj_if_type))
        return None
    return if_type_map[obj_if_type]

def _get_op_id(cps_obj):
    op_id_to_name_map = {1: 'create', 2: 'delete', 3: 'set'}
    op_id = None
    try:
        op_id = cps_obj.get_attr_data(op_attr_name)
    except ValueError:
        nas_if.log_err('No operation attribute in object')
        return None
    if not op_id in op_id_to_name_map:
        nas_if.log_err('Invalid operation type '+str(op_id))
        return None
    return op_id_to_name_map[op_id]

def  _set_loopback_interface(obj):
    name = None
    try:
        name = obj.get_attr_data('if/interfaces/interface/name')
    except:
        pass
    if name is None:
        nas_if.log_err("Failed to create interface without name")
        return False

    try:
        mtu = obj.get_attr_data('dell-if/if/interfaces/interface/mtu')
        if dn_base_ip_tool.set_if_mtu(name, mtu) == False:
            nas_if.log_err("Failed to execute request..." + str(res))
            return False
    except:
        pass

    try:
        state = obj.get_attr_data('if/interfaces/interface/enabled')
        if dn_base_ip_tool.set_if_state(name, state) == False:
            nas_if.log_err(('Failed to set the interface state.', name, state))
            return False
    except:
        pass
    try:
        mac_str = obj.get_attr_data('dell-if/if/interfaces/interface/phys-address')
        if dn_base_ip_tool.set_if_mac(name, mac_str) == False:
            nas_if.log_err(('Failed to set the interface mac.' +str(name)+' , ' +str(mac_str)))
            return False
    except:
        pass

    return True

def _create_loopback_interface(obj, params):
    name = None
    mac_str = None
    mac = None
    try:
        name = obj.get_attr_data('if/interfaces/interface/name')
        mac_str = obj.get_attr_data('dell-if/if/interfaces/interface/phys-address')
    except:
        pass
    if name is None:
        nas_if.log_err("Failed to create interface without name")
        return False
    nas_if.log_info("interface name is" +str(name))
    lst = dn_base_ip_tool.get_if_details(name)
    if (len(lst)) > 0:
        nas_if.log_err("Interface already exists" +str(name))
        return False
    if mac_str is None:
        mac_str = ma.get_offset_mac_addr(get_base_mac_addr(), 0)
    rc = dn_base_ip_tool.create_loopback_if(name, mac=mac_str)
    if rc:
        nas_if.log_info("loopback interface is created" +str(name))
        rc = _set_loopback_interface(obj)
        if_index = ifindex_utils.if_nametoindex(name)
        obj.add_attr(ifindex_attr_name, if_index)
        params['change'] = obj.get()
    return rc

def _delete_loopback_interface(obj):
    name = None
    try:
        name = obj.get_attr_data('if/interfaces/interface/name')
    except:
        return False
    return dn_base_ip_tool.delete_if(name)

def _handle_loopback_intf(cps_obj, params):
    op = _get_op_id(cps_obj)
    if op == None:
        return False

    if op == 'set':
        return _set_loopback_interface(cps_obj)
    if op == 'create':
        return _create_loopback_interface(cps_obj, params)
    if op == 'delete':
        return _delete_loopback_interface(cps_obj)

def set_intf_cb(methods, params):
    if params['operation'] != 'rpc':
        nas_if.log_err('Operation '+str(params['operation'])+' not supported')
        return False
    cps_obj = cps_object.CPSObject(obj = params['change'])

    if_type = _get_intf_type(cps_obj)
    if if_type == 'loopback':
        return _handle_loopback_intf(cps_obj, params)
    elif if_type == 'management':
        return False

    op = _get_op_id(cps_obj)
    if op == None:
        return False

    member_port = None
    try:
        member_port = cps_obj.get_attr_data('dell-if/if/interfaces/interface/member-ports/name')
    except ValueError:
        member_port = None

    if (op == 'create' and member_port == None):
        nas_if.log_info('Create interface, type: %s' % if_type)
        mac_addr = None
        try:
            mac_addr = cps_obj.get_attr_data(mac_attr_name)
        except ValueError:
            pass
        if mac_addr == None:
            nas_if.log_info('No mac address given in input object, get assigned mac address')
            param_list = get_alloc_mac_addr_params(if_type, cps_obj)
            if param_list != None:
                mac_addr = ma.if_get_mac_addr(**param_list)
                if mac_addr == None:
                    nas_if.log_err('Failed to get mac address')
                    return False
                if len(mac_addr) > 0:
                    nas_if.log_info('Assigned mac address: %s' % mac_addr)
                    cps_obj.add_attr(mac_attr_name, mac_addr)

    if op == 'set' or op == 'create':
        try:
            _if_update_config(op, cps_obj)
        except:
            nas_if.log_err( "update config failed during set or create ")
            pass
    module_name = nas_if.get_if_key()
    in_obj = copy.deepcopy(cps_obj)
    in_obj.set_key(cps.key_from_name('target', module_name))
    in_obj.root_path = module_name + '/'
    obj = in_obj.get()
    if op_attr_name in obj['data']:
        del obj['data'][op_attr_name]
    upd = (op, obj)
    ret_data = cps_utils.CPSTransaction([upd]).commit()
    if ret_data == False:
        nas_if.log_err('Failed to commit request')
        return False
    if op == 'delete':
        try:
            _if_update_config(op, in_obj)
        except:
            nas_if.log_err('update config failed for delete operation')
            pass
        return True
    if len(ret_data) == 0 or not 'change' in ret_data[0]:
        nas_if.log_err('Invalid return object from cps request')
        return False
    if (op == 'create' and member_port == None):
        ret_obj = cps_object.CPSObject(obj = ret_data[0]['change'])
        try:
            ifindex = ret_obj.get_attr_data(ifindex_attr_name)
        except ValueError:
            nas_if.log_err('Ifindex not found from returned object')
            return False
        cps_obj.add_attr(ifindex_attr_name, ifindex)

    params['change'] = cps_obj.get()
    return True

def get_mac_cb(methods, params):
    if params['operation'] != 'rpc':
        nas_if.log_err('Operation %s not supported' % params['operation'])
        return False
    cps_obj = cps_object.CPSObject(obj = params['change'])

    if_type = _get_intf_type(cps_obj)
    if if_type == 'loopback' or if_type == 'management':
        nas_if.log_err('Interface type %s not supported' % if_type)
        return False
    param_list = get_alloc_mac_addr_params(if_type, cps_obj)
    if param_list == None:
        nas_if.log_err('No enough attributes in input object to get mac address')
        return False

    mac_addr = ma.if_get_mac_addr(**param_list)
    if mac_addr == None or len(mac_addr) == 0:
        nas_if.log_err('Failed to get mac address')
        return False
    cps_obj.add_attr(mac_attr_name, mac_addr)

    params['change'] = cps_obj.get()
    return True

if __name__ == '__main__':
    # Wait for base MAC address to be ready. the script will wait until
    # chassis object is registered.
    chassis_key = cps.key_from_name('observed','base-pas/chassis')
    while cps.enabled(chassis_key)  == False:
        #wait for chassis object to be ready
        nas_if.log_err('Create Interface: Base MAC address is not yet ready')
        time.sleep(1)
    fp.init('/etc/opx/base_port_physical_mapping_table.xml')

    handle = cps.obj_init()

    d = {}
    d['get'] = get_cb
    d['transaction'] = set_cb

    cps.obj_register(handle, _fp_key, d)
    cps.obj_register(handle, _npu_lane_key, d)
    cps.obj_register(handle, _breakout_key, d)

    d = {}
    d['transaction'] = set_intf_cb
    cps.obj_register(handle, _set_intf_key, d)

    get_mac_hdl = cps.obj_init()

    d = {}
    d['transaction'] = get_mac_cb
    cps.obj_register(get_mac_hdl, _get_mac_key, d)

    while True:
        time.sleep(1)
