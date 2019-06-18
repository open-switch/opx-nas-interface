#!/usr/bin/python
# Copyright (c) 2019 Dell Inc.
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
import nas_common_header as nas_comm

# if_config is caching interface specific configuration.
intf_list = {}


class IF_CONFIG:
    def __init__ (self, if_name, ietf_intf_type):
        self.name = if_name
        self.ietf_intf_type = ietf_intf_type
        self.breakout_mode = None
        self.negotiation = None
        self.speed = None
        self.enabled = None  # new element
        self.cfg_speed = None
        self.duplex = None
        self.fec = None
        self.npu = None
        self.port = None
        self.hw_profile = None
        self.media_obj = None
        self.media_supported = True

    def get_npu_port(self):
        return(self.npu, self.port)

    def set_npu_port(self, npu, port):
        self.npu = npu
        self.port = port

    def get_media_obj(self):
        return self.media_obj

    def set_media_obj(self, media_obj):
        if media_obj is None:
            return False
        self.media_obj = media_obj

    def get_media_cable_type(self):
        try:
            return self.media_obj.get_attr_data('base-pas/media/cable-type')
        except:
            return None

    def get_media_category(self):
        try:
            return self.media_obj.get_attr_data('base-pas/media/category')
        except:
            return None

    def get_is_media_supported(self):
        return (self.media_supported)

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

    def get_hw_profile(self):
        return(self.hw_profile)

    def set_hw_profile(self, hw_profile):
        self.hw_profile = hw_profile

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
                     ('Hw Profile', 'hw_profile'),
                     ('Breakout', 'breakout_mode'),
                     ('Front Panel Port', 'fp_port'),
                     ('Subport ID', 'subport_id'),
                     ('Media Support', 'media_supported'),
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
    try:
        obj_if_type = cps_obj.get_attr_data('if/interfaces/interface/type')
    except:
        return None
    if not obj_if_type in nas_comm.yang.get_tbl('if_type_map'):
        nas_if.log_err('Unknown if type: '+str(obj_if_type))
        return None
    return nas_comm.yang.get_value(obj_if_type, 'if_type_map')

def get_intf_phy_mode(cps_obj):
    try:
        obj_if_type = cps_obj.get_attr_data('if/interfaces/interface/type')
    except:
        return None
    if obj_if_type not in nas_comm.yang.get_tbl('ietf-type-2-phy-mode'):
        return None
    return nas_comm.yang.get_value(obj_if_type, 'ietf-type-2-phy-mode')
