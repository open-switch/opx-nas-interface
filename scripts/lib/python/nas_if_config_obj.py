#!/usr/bin/python
# Copyright (c) 2015 Dell Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License. You may obtain
# a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
# LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
# FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
#
# See the Apache Version 2.0 License for specific language governing
# permissions and limitations under the License.

import cps
import cps_object
import nas_os_if_utils as nas_if
from nas_common_header import *


# if_config is caching interface specific configuration.
intf_list = {}


class IF_CONFIG:
    def __init__ (self, if_name, ietf_intf_type):
        self.name = if_name
        self.ietf_intf_type = ietf_intf_type

        self.media_type = None
        self.breakout_mode = None
        self.negotiation = None
        self.speed = None
        self.cfg_speed = None
        self.duplex = None
        self.fec = None
        self.npu = None
        self.port = None

        self.media_supported = True

    def get_npu_port(self):
        return(self.npu, self.port)

    def set_npu_port(self, npu, port):
        self.npu = npu
        self.port = port

    def get_media_type(self):
        return(self.media_type)

    def get_is_media_supported(self):
        return (self.media_supported)

    def set_media_type(self, media_type):
        self.media_type = media_type

    def set_is_media_supported(self, supported):
        self.media_supported = supported

    def get_speed(self):
        return(self.speed)

    def set_speed(self, speed):
        self.speed = speed

    def get_cfg_speed(self):
        return(self.cfg_speed)

    def set_cfg_speed(self, speed):
        self.cfg_speed = speed

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

    def set_breakout_mode(self, breakout_mode):
        self.breakout_mode = breakout_mode

    def get_ietf_intf_type(self):
        return(self.ietf_intf_type)

    def get_fp_port(self):
        return(self.fp_port)

    def set_fp_port(self, port):
        self.fp_port = port

    def get_subport_id(self):
        return(self.subport_id)

    def set_subport_id(self, sp_id):
        self.subport_id = sp_id

    def show(self):
        attr_list = [('If Name', 'name'),
                     ('If Type', 'ietf_intf_type'),
                     ('Speed', 'speed'),
                     ('Config speed', 'cfg_speed'),
                     ('Negotiation', 'negotiation'),
                     ('Duplex', 'duplex'),
                     ('NPU ID', 'npu'),
                     ('Port ID', 'port'),
                     ('Media', 'media_type'),
                     ('Breakout', 'breakout_mode'),
                     ('Front Panel Port', 'fp_port'),
                     ('Subport ID', 'subport_id'),
                     ('FEC mode', 'fec')]
        for attr_desc, attr_name in attr_list:
            if attr_name not in self.__dict__ or self.__dict__[attr_name] == None:
                attr_val = '-'
            else:
                attr_val = str(self.__dict__[attr_name])
            nas_if.log_info('%-17s: %s' % (attr_desc, attr_val))

    def get_fec_mode(self):
        return(self.fec)

    def set_fec_mode(self, fec):
        self.fec = fec

def if_config_get_by_npu_port(npu, port):
    for if_name in intf_list:
        if (npu, port) == intf_list[if_name].get_npu_port():
            return if_name
    return None

def if_config_get(if_name):
    if if_name in intf_list:
        return intf_list[if_name]
    return None

def if_config_del(if_name):
    if if_name in intf_list:
        del intf_list[if_name]
        return True
    return False

def if_config_add(if_name, config_obj):
    if if_name not in intf_list:
        intf_list[if_name] = config_obj
    else:
        return False
    return True

def get_intf_type(cps_obj):
    if_type_map = {'ianaift:ethernetCsmacd': 'front-panel',
                   'ianaift:fibreChannel': 'front-panel',
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

def get_intf_phy_mode(cps_obj):
    try:
        obj_if_type = cps_obj.get_attr_data('if/interfaces/interface/type')
    except:
        return None
    if not is_key_valid(ietf_type_2_phy_mode, obj_if_type):
        return None
    return get_value(ietf_type_2_phy_mode, obj_if_type)
