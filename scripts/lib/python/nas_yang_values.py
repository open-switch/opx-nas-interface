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
import cps
import nas_os_if_utils as nas_if

global yang_values
yang_values = None

def _read_config(filename):
    try:
        return json.load(open(filename))
    except:
        return None

class YangValues(object):
    ''' Yang Values Object'''
    def __init__(self):
        global yang_values
        if yang_values is None:
            yang_values = _read_config('/etc/opx/media/yang_values.json')

            extra_values = {

                'keys_id' : {
                    'breakout_key'        : cps.key_from_name('target', 'base-if-phy/breakout'),
                    'fp_key'              : cps.key_from_name('target', 'base-if-phy/front-panel-port'),
                    'npu_lane_key'        : cps.key_from_name('target', 'base-if-phy/hardware-port'),
                    'set_intf_key'        : cps.key_from_name('target', 'dell-base-if-cmn/set-interface'),
                    'get_mac_key'         : cps.key_from_name('target', 'dell-base-if-cmn/get-mac-address'),
                    'set_bridge_key'      : cps.key_from_name('target', 'bridge-domain/set-bridge'),
                    'physical_key'        : cps.key_from_name('target', 'base-if-phy/physical'),
                    'logical_if_key'      : cps.key_from_name('target', 'dell-base-if-cmn/if/interfaces/interface'),
                    'obs_logical_if_key'  : cps.key_from_name('observed', 'dell-base-if-cmn/if/interfaces/interface'),
                    'media_key'           : cps.key_from_name('observed', 'base-pas/media'),
                    'logical_if_state_key': cps.key_from_name('observed', 'dell-base-if-cmn/if/interfaces-state/interface'),
                    'media_info_key'      : cps.key_from_name('observed', 'base-media/media-info'),
                    'switch_key'          : cps.key_from_name('observed', 'base-switch/switching-entities/switching-entity')
                },

                'attr_name' : {
                    'npu_id'                    : 'base-if-phy/if/interfaces/interface/npu-id',
                    'port_id'                   : 'base-if-phy/if/interfaces/interface/port-id',
                    'fp_port'                   : 'base-if-phy/hardware-port/front-panel-port',
                    'subport_id'                : 'base-if-phy/hardware-port/subport-id',
                    'if_index'                  : 'dell-base-if-cmn/if/interfaces/interface/if-index',
                    'if_name'                   : 'if/interfaces/interface/name',
                    'intf_type'                 : 'if/interfaces/interface/type',
                    'phy_addr'                  : 'dell-if/if/interfaces/interface/phys-address',
                    'speed'                     : 'dell-if/if/interfaces/interface/speed',
                    'negotiation'               : 'dell-if/if/interfaces/interface/negotiation',
                    'auto_neg'                  : 'dell-if/if/interfaces/interface/auto-negotiation',
                    'duplex'                    : 'dell-if/if/interfaces/interface/duplex',
                    'hw_profile'                : 'base-if-phy/if/interfaces/interface/hw-profile',
                    'vlan_id'                   : 'base-if-vlan/if/interfaces/interface/id',
                    'fec_mode'                  : 'dell-if/if/interfaces/interface/fec',
                    'media_type'                : 'base-if-phy/if/interfaces/interface/phy-media',
                    'op'                        : 'dell-base-if-cmn/set-interface/input/operation',
                    'fanout_config'             : '/etc/opx/dn_nas_fanout_init_config.xml',
                    'supported_autoneg'         : 'base-if-phy/if/interfaces/interface/supported-autoneg',
                    'def_vlan_mode'             : 'dell-if/if/interfaces/vlan-globals/scaled-vlan',
                    'retcode'                   : 'cps/object-group/return-code',
                    'def_vlan_mode_attr_name'   : 'dell-if/if/interfaces/vlan-globals/scaled-vlan',
                    'def_vlan_id_attr_name'     : 'dell-if/if/interfaces/vlan-globals/default-vlan-id',
                    'vn_untagged_vlan_attr_name': 'dell-if/if/interfaces/vlan-globals/vn-untagged-vlan',
                    'retstr'                    : 'cps/object-group/return-string'
                },

                'if_type_map': {
                    'ianaift:ethernetCsmacd'  : 'front-panel',
                    'ianaift:fibreChannel'    : 'front-panel',
                    'ianaift:l2vlan'          : 'vlan',
                    'ianaift:ieee8023adLag'   : 'lag',
                    'ianaift:softwareLoopback': 'loopback',
                    'base-if:vxlan'           : 'vxlan',
                    'base-if:bridge'          : 'bridge',
                    'base-if:management'      : 'management',
                    'base-if:macvlan'         : 'macvlan',
                    'base-if:vlanSubInterface': 'vlanSubInterface',
                    'base-if:virtualNetwork'  : 'virtual-network'
                },

                'eth-to-fc-speed': {
                    self.get_value('10g', 'yang-speed'): self.get_value('8gfc', 'yang-speed'),
                    self.get_value('20g', 'yang-speed'): self.get_value('16gfc', 'yang-speed'),
                    self.get_value('25g', 'yang-speed'): self.get_value('16gfc', 'yang-speed'),
                    self.get_value('50g', 'yang-speed'): self.get_value('32gfc', 'yang-speed'),
                    self.get_value('40g', 'yang-speed'): self.get_value('32gfc', 'yang-speed')
                },

                'fc-to-ether-speed': {
                    self.get_value('8gfc', 'yang-speed') : self.get_value('10g', 'yang-speed'),
                    self.get_value('16gfc', 'yang-speed'): self.get_value('25g', 'yang-speed')
                },

                'yang-breakout-port-speed' : {
                    '100gx1'  : (self.get_value('1x1', 'yang-breakout-mode'), self.get_value('100g', 'yang-speed')),
                    '50gx2'   : (self.get_value('2x1', 'yang-breakout-mode'), self.get_value('50g', 'yang-speed')),
                    '40gx1'   : (self.get_value('1x1', 'yang-breakout-mode'), self.get_value('40g', 'yang-speed')),
                    '25gx4'   : (self.get_value('4x1', 'yang-breakout-mode'), self.get_value('25g', 'yang-speed')),
                    '10gx4'   : (self.get_value('4x1', 'yang-breakout-mode'), self.get_value('10g', 'yang-speed')),
                    'disabled': (self.get_value('disabled', 'yang-breakout-mode'), self.get_value('0m', 'yang-speed'))
                },

                'breakout-to-hwp-count' : {
                    self.get_value('1x1', 'yang-breakout-mode')     : 4,
                    self.get_value('2x1', 'yang-breakout-mode')     : 2,
                    self.get_value('4x1', 'yang-breakout-mode')     : 1,
                    self.get_value('4x4', 'yang-breakout-mode')     : 1,
                    self.get_value('2x4', 'yang-breakout-mode')     : 2,
                    self.get_value('disabled', 'yang-breakout-mode'): 0
                },

                'breakout-to-phy-fp-port-count' : {
                    self.get_value('1x1', 'yang-breakout-mode')     : (1,1),
                    self.get_value('2x1', 'yang-breakout-mode')     : (2,1),
                    self.get_value('4x1', 'yang-breakout-mode')     : (4,1),
                    self.get_value('4x4', 'yang-breakout-mode')     : (4,4),
                    self.get_value('2x4', 'yang-breakout-mode')     : (2,4),
                    self.get_value('2x2', 'yang-breakout-mode')     : (2,2),
                    self.get_value('4x2', 'yang-breakout-mode')     : (4,2),
                    self.get_value('8x2', 'yang-breakout-mode')     : (8,2),
                    self.get_value('disabled', 'yang-breakout-mode'): (0,0)
                },

                'breakout-to-skip-port' : {
                    self.get_value('1x1', 'yang-breakout-mode')     : 3,
                    self.get_value('2x1', 'yang-breakout-mode')     : 1,
                    self.get_value('4x1', 'yang-breakout-mode')     : 0,
                    self.get_value('4x4', 'yang-breakout-mode')     : 0,
                    self.get_value('2x4', 'yang-breakout-mode')     : 1,
                    self.get_value('2x2', 'yang-breakout-mode')     : 3,
                    self.get_value('4x2', 'yang-breakout-mode')     : 1,
                    self.get_value('8x2', 'yang-breakout-mode')     : 0,
                    self.get_value('disabled', 'yang-breakout-mode'): 4
                },

                'breakout-to-subport-lane-map' : {
                    self.get_value('1x1', 'yang-breakout-mode'): {0: (0, False), 1: (0, True)},
                    self.get_value('4x4', 'yang-breakout-mode'): {0: 0,1: 0},
                    self.get_value('2x4', 'yang-breakout-mode'): {0: 0},
                    self.get_value('2x1', 'yang-breakout-mode'): {1: 0, 3: 2, 2: 2},
                    self.get_value('4x1', 'yang-breakout-mode'): {1: 0, 2: 1, 3: 2, 4: 3}
                },

                'ddqsfp-2-qsfp-brmode' : {
                    self.get_value('8x2', 'yang-breakout-mode'): self.get_value('4x1', 'yang-breakout-mode'),
                    self.get_value('4x2', 'yang-breakout-mode'): self.get_value('2x1', 'yang-breakout-mode'),
                    self.get_value('2x2', 'yang-breakout-mode'): self.get_value('1x1', 'yang-breakout-mode')
                },

                'ietf-type-2-phy-mode' : {
                    'ianaift:ethernetCsmacd': self.get_value('ether', 'yang-phy-mode'),
                    'ianaift:fibreChannel'  : self.get_value('fc', 'yang-phy-mode')
                },

                'mbps-to-yang-speed' : {
                    0     : 0,  # 0 mbps
                    10    : 1,  # 10 mbps
                    100   : 2,  # 100 mbps
                    1000  : 3,  # 1gbps
                    10000 : 4,  # 10 gbps
                    25000 : 5,  # 25 gbps
                    40000 : 6,  # 40 gbps
                    100000: 7,  # 100 gbps
                    'auto': 8,  # default speed
                    20000 : 9,  # 20 gbps
                    50000 : 10, # 50 gbps
                    200000: 11, # 200 gbps
                    400000: 12, # 400 gbps
                    4000  : 13, # 4 gfc
                    8000  : 14, # 8 gfc
                    16000 : 15, # 16 gfc
                    32000 : 16  # 32 gfc
                },

                'yang-to-mbps-speed' : {
                    0 : 0,      # 0 mbps
                    1 : 10,     # 10 mbps
                    2 : 100,    # 100 mbps
                    3 : 1000,   # 1 gbps
                    4 : 10000,  # 10 gbps
                    5 : 25000,  # 25 gbps
                    6 : 40000,  # 40 gbps
                    7 : 100000, # 100 gbps
                    8 : 'auto', # default speed
                    9 : 20000,  # 20 gbps
                    10: 50000,  # 50 gbps
                    11: 200000, # 200 gbps
                    12: 400000, # 400 gbps
                    13: 4000,   # 4gfc
                    14: 8000,   # 8 gfc
                    15: 16000,  # 16 gfc
                    16: 32000   # 32 gfc
                },

                'yang-to-breakout-mode' : {
                    1 : "disabled",
                    2 : "4x1",
                    3 : "2x1",
                    4 : "1x1",
                    5 : "8x2",
                    6 : "2x2",
                    7 : "4x4",
                    8 : "2x4",
                    9 : "4x2",
                    10: "unknown",
                    11: "no-breakout",
                    12: "8x1"
                },

                'yang-autoneg-support' : {
                    'not-supported' : 1,
                    'on-supported'  : 2,
                    'off-supported' : 3,
                    'both-supported': 4
                }

            }
            yang_values.update(extra_values)

    def inv(self, tbl):
        return dict(zip(tbl.values(), tbl.keys()))

    def get_tbl(self, tbl=None):
        if tbl is None:
            return yang_values
        if tbl in yang_values:
            return yang_values[tbl]
        return None

    def get_key(self, key, tbl):
        global yang_values
        if tbl not in yang_values:
            nas_if.log_info('No yang table ' + str(tbl))
            return None
        tbl = self.inv(yang_values[tbl])
        if key in tbl:
            return tbl[key]
        return None

    def get_value(self, key, tbl):
        global yang_values
        if tbl not in yang_values:
            #nas_if.log_info('No yang table ' + str(tbl))
            return None
        tbl = yang_values[tbl]
        if key in tbl:
            return tbl[key]
        return None
