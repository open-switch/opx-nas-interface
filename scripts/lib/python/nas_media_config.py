#!/usr/bin/python
# Copyright (c) 2019 Dell Inc.
#
# Licensed under the Apache License, Version 2.0 (the 'License'); you may
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

import json
import nas_os_if_utils as nas_if
import nas_common_header as nas_comm

config_path = '/etc/opx/media/'

global json_config
json_config = {
    'config'  : {},
    'key'     : {},
    'profiles': {}
}

yang_type_cps_attr = {
    'display-string'           : 'base-pas/media/media-name',
    'category'                 : 'base-pas/media/category',
    'breakout-mode'            : 'base-pas/media/default-breakout-mode',
    'breakout-speed'           : 'base-pas/media/default-breakout-speed',
    'phy-mode'                 : 'base-pas/media/default-phy-mode',
    'cable-type'               : 'base-pas/media/cable-type',
    'cable-length-cm'          : 'base-pas/media/cable-length-cm',
    'ext-spec-compliance-code' : 'base-pas/media/ext-spec-compliance-code',
    'media-interface'          : 'base-pas/media/media-interface',
    'media-qualifier'          : 'base-pas/media/media-interface-qualifier',
    'port-max-speed'           : 'base-if-phy/physical/speed',
    'interface-speed'          : 'dell-if/if/interfaces/interface/speed'
}

def _read_attr(cps_obj, attr_name):
    try:
        return cps_obj.get_attr_data(attr_name)
    except:
        return None

def _read_config(filename):
    try:
        return json.load(open(filename))
    except:
        return None

def _get_setting(cps_obj, param_str, param_type):
    nas_if.log_info('Get ' + param_type + ' ' + param_str)
    err_str = ''

    global json_config

    fl = str(config_path + param_str + '/' + param_type +'/config.json')
    if fl not in json_config['config']:
        json_config['config'][fl] = _read_config(fl)['config']
    config = json_config['config'][fl]

    fl = str(config_path + param_str + '/' + param_type +'/config.json')
    if fl not in json_config['key']:
        json_config['key'][fl] = _read_config(fl)['key']
    key = json_config['key'][fl]

    fl = str(config_path + param_str + '/profiles.json')
    if fl not in json_config['profiles']:
        json_config['profiles'][fl] = _read_config(fl)
    profiles = json_config['profiles'][fl]

    for k in key:
        try:
            v = _read_attr(cps_obj, yang_type_cps_attr[str(k)])
            if str(k) in nas_comm.yang.get_tbl('index'):
                kp = nas_comm.yang.get_tbl('index')[str(k)]
                if nas_comm.yang.get_key(v, kp) is not None:
                    v = nas_comm.yang.get_key(v, kp)
                    err_str += str('__' + '(' + str(k) + ')' + str(v))
            if v not in config:
                for e in config:
                    if v in e:
                        v = e
                        break
            config = config[v]
        except:
            nas_if.log_info(str(k) + ': ' + str(v) + ' not found in ' + config_path + param_str + '/' + param_type + '/config.json file')
            if err_str != '':
                nas_if.log_err('Media: ' + err_str + ' not found in ' + config_path + param_str + '/' + param_type + '/config.json file')
            return None
    p = profiles[config[0]]

    if param_str in nas_comm.yang.get_tbl('index'):
        param_str = nas_comm.yang.get_tbl('index')[param_str]

    if nas_comm.yang.get_value(p, param_str) is not None:
        return nas_comm.yang.get_value(p, param_str)
    return p

class SupportedPhyMode(object):
    '''Supported Phy Mode Setting Object'''
    def get_setting(self, cps_obj):
        val = [_read_attr(cps_obj, 'base-pas/media/default-phy-mode')]
        nas_if.log_info('Default Media SUPPORTED-PHY-MODES: ' + str(val))
        return val

class Speed(object):
    '''Speed Setting Object'''
    def get_setting(self, cps_obj, phy_mode):
        val = _read_attr(cps_obj, 'base-pas/media/default-breakout-speed')
        nas_if.log_info('Default Media SPEED: ' + str(nas_comm.yang.get_key(val, 'yang-speed')))
        return val

class Duplex(object):
    '''Duplex Setting Object'''
    def get_setting(self, cps_obj):
        val = _get_setting(cps_obj, 'duplex', 'override')
        if val is None:
            val = _get_setting(cps_obj, 'duplex', 'default')
        nas_if.log_info('Default Media DUPLEX: ' + str(nas_comm.yang.get_key(val, 'yang-duplex')))
        return val

class Autoneg(object):
    '''Auto-negotiation Setting Object'''
    def get_setting(self, cps_obj):
        val = _get_setting(cps_obj, 'autoneg', 'override')
        if val is None:
            val = _get_setting(cps_obj, 'autoneg', 'default')
        nas_if.log_info('Default Media AUTONEG: ' + str(nas_comm.yang.get_key(val, 'yang-autoneg')))
        return val

class FEC(object):
    '''FEC Configuration Object'''
    def get_setting(self, cps_obj):
        val = _get_setting(cps_obj, 'fec', 'override')
        if val is None:
            val = _get_setting(cps_obj, 'fec', 'default')
        nas_if.log_info('Default Media FEC: ' + str(nas_comm.yang.get_key(val, 'yang-fec')))
        return val

class HardwareProfile(object):
    '''Hardware Profile Configuration Object'''
    def get_setting(self, cps_obj):
        val = _get_setting(cps_obj, 'hardware', 'override')
        if val is None:
            val = _get_setting(cps_obj, 'hardware', 'default')
        nas_if.log_info('Default HW-PROFILE: ' + str(val))
        return val
