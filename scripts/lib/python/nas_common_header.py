#!/usr/bin/python
# Copyright (c) 2016 Dell Inc.
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

keys_id = {
    'breakout_key' :        cps.key_from_name('target', 'base-if-phy/breakout'),
    'fp_key' :              cps.key_from_name('target', 'base-if-phy/front-panel-port'),
    'npu_lane_key' :        cps.key_from_name('target', 'base-if-phy/hardware-port'),
    'set_intf_key' :        cps.key_from_name('target', 'dell-base-if-cmn/set-interface'),
    'get_mac_key' :         cps.key_from_name('target', 'dell-base-if-cmn/get-mac-address'),
    'media_key' :           cps.key_from_name('observed', 'base-pas/media'),
    'physical_key' :        cps.key_from_name('target', 'base-if-phy/physical'),
    'logical_if_key' :      cps.key_from_name('target', 'dell-base-if-cmn/if/interfaces/interface'),
    'logical_if_state_key': cps.key_from_name('observed', 'dell-base-if-cmn/if/interfaces-state/interface'),
    'media_info_key' :      cps.key_from_name('observed', 'base-media/media-info'),
    'switch_key' :          cps.key_from_name('observed', 'base-switch/switching-entities/switching-entity')
}

attr_name = {
    'npu_id' :        'base-if-phy/if/interfaces/interface/npu-id',
    'port_id' :       'base-if-phy/if/interfaces/interface/port-id',
    'fp_port' :       'base-if-phy/hardware-port/front-panel-port',
    'subport_id' :    'base-if-phy/hardware-port/subport-id',
    'if_index' :      'dell-base-if-cmn/if/interfaces/interface/if-index',
    'if_name' :       'if/interfaces/interface/name',
    'intf_type':      'if/interfaces/interface/type',
    'phy_addr' :      'dell-if/if/interfaces/interface/phys-address',
    'speed' :         'dell-if/if/interfaces/interface/speed',
    'negotiation':    'dell-if/if/interfaces/interface/negotiation',
    'auto_neg' :      'dell-if/if/interfaces/interface/auto-negotiation',
    'duplex' :        'dell-if/if/interfaces/interface/duplex',
    'media_type' :    'base-if-phy/if/interfaces/interface/phy-media',
    'vlan_id' :       'base-if-vlan/if/interfaces/interface/id',
    'op' :            'dell-base-if-cmn/set-interface/input/operation',
    'fanout_config' : '/etc/opx/dn_nas_fanout_init_config.xml'
}

yang_speed = {
    '0'         : 0,      # 0 Mbps
    '10Mbps'    : 1,      # 10Mbps
    '100Mbps'   : 2,      # 100 Mbps
    '1G'        : 3,      # 1Gbps
    '10G'       : 4,      # 10Gbps
    '25G'       : 5,      # 25 Gbps
    '40G'       : 6,      # 40Gbps
    '100G'      : 7,      # 100Gbps
    '20G'       : 9,      # 20Gbps
    '50G'       : 10,     # 50Gbps
    '4GFC'      : 13,     # 4Gbps for FC port
    '8GFC'      : 14,     # 8Gbps for FC port
    '16GFC'     : 15,     # 16Gbps for FC port
    '32GFC'     : 16,     # 16Gbps for FC port
    'auto'      : 8       # default speed
}

eth_to_fc_speed = { yang_speed['10G']:yang_speed['8GFC'],
                    yang_speed['20G']: yang_speed['16GFC'],
                    yang_speed['25G']: yang_speed['16GFC'],
                    yang_speed['50G']: yang_speed['32GFC'],
                    yang_speed['40G']: yang_speed['32GFC'],
                    }

fc_to_ether_speed = {
            yang_speed['8GFC']: yang_speed['10G'],
            yang_speed['16GFC']: yang_speed['25G'],
            }
def get_fc_speed(speed):
    if speed in eth_to_fc_speed:
        return eth_to_fc_speed[speed]
    else:
        return None

# returns Eth speed for Hwp from FC speed
def get_hwp_fc_to_eth_speed(fc_speed):
    if fc_speed in fc_to_ether_speed:
        return fc_to_ether_speed[fc_speed]
    else:
        return None

yang_autoneg = {
    'auto' :  1,    # default value for negotiation
    'on' :    2,
    'off' :   3
}

yang_duplex = {
    'auto' : 3,      #auto - default value for duplex
    'full' : 1,
    'half' : 2
}

yang_breakout = {
    '1x1' :  4,
    '2x1' :  3,
    '4x1' :  2,
    '8x2' :  5,
    '2x2' :  6,
    '4x4' :  7,
    '2x4' :  8,
    '4x2' :  9,
}

# Mapping between breakout mode and number of allocated hw ports for each physical port.
breakout_to_hwp_count = {
        yang_breakout['1x1']:4,
        yang_breakout['2x1']:2,
        yang_breakout['4x1']:1,
        yang_breakout['4x4']:1,
        yang_breakout['2x4']:2
        }

# Mapping between breakout mode and physical ports count per front panel port, or
# physical and front panel ports count per port group.
breakout_to_phy_fp_port_count = {
        yang_breakout['1x1']:(1,1),
        yang_breakout['2x1']:(2,1),
        yang_breakout['4x1']:(4,1),
        yang_breakout['4x4']:(4,4),
        yang_breakout['2x4']:(2,4),
        yang_breakout['2x2']:(2,2),
        yang_breakout['4x2']:(4,2),
        yang_breakout['8x2']:(8,2),
        }

# mapping between breakout mode to the number of ports skipped to get the next physical port
breakout_to_skip_port = {
        yang_breakout['1x1']:3,
        yang_breakout['2x1']:1,
        yang_breakout['4x1']:0,
        yang_breakout['4x4']:0,
        yang_breakout['2x4']:1,
        yang_breakout['2x2']:3,
        yang_breakout['4x2']:1,
        yang_breakout['8x2']:0,
        }

# mapping between breakout mode to subport to lane id mapping
breakout_to_subport_lane_map = {
        # for 1x1 mode, extra flag in tuple is used to indicate if this mode
        # is 40G breakout for 100G(QSFP28) port
        yang_breakout['1x1']: {0: (0, False), 1: (0, True)},
        yang_breakout['4x4']: {0: 0},
        yang_breakout['2x4']: {0: 0},
        yang_breakout['2x1']: {1: 0, 3: 2, 2: 2},
        yang_breakout['4x1']: {1: 0, 2: 1, 3: 2, 4: 3},
        }

def subport_to_lane(br_mode, subport_id):
    if br_mode not in breakout_to_subport_lane_map:
        return None
    subport_to_lane_map = breakout_to_subport_lane_map[br_mode]
    if subport_id not in subport_to_lane_map:
        return None
    return subport_to_lane_map[subport_id]

def lane_to_subport(br_mode, lane_id, qsfp28_40g_mode = False):
    if br_mode not in breakout_to_subport_lane_map:
        return None
    subport_to_lane_map = breakout_to_subport_lane_map[br_mode]
    subport_id = None
    for sp, lane in subport_to_lane_map.items():
        flag = False
        if isinstance(lane, tuple):
            lane, flag = lane
        if lane_id == lane and flag == qsfp28_40g_mode:
            subport_id = sp
            break
    return subport_id

# conversion of breakout mode from DDQSFP group to the breakout mode applied on each FP port inthe DDQSFP group.
ddqsfp_2_qsfp_brmode = {yang_breakout['8x2']:yang_breakout['4x1'],
                        yang_breakout['4x2']:yang_breakout['2x1'],
                        yang_breakout['2x2']:yang_breakout['1x1'],
                        }

yang_phy_mode = {
        'ether' : 1,
        'fc' : 2
}

ietf_type_2_phy_mode = {
        'ianaift:ethernetCsmacd': yang_phy_mode['ether'],
        'ianaift:fibreChannel': yang_phy_mode['fc']
        }

speed_map = {
    'speed_0'         :  0,     # 0 Mbps
    'speed_10Mbps'    : 10,     # 10Mbps
    'speed_100Mbps'   : 100,    # 100 Mbps
    'speed_1G'        : 1000,   # 1Gbps
    'speed_10G'       : 10000,  # 10Gbps
    'speed_20G'       : 20000,  # 20Gbps
    'speed_25G'       : 25000,  # 25 Gbps
    'speed_40G'       : 40000,  # 40Gbps
    'speed_50G'       : 50000,  # 50Gbps
    'speed_100G'      : 100000, # 100Gbps
    'speed_auto'      : 'auto'  # default speed
}

mbps_to_yang_speed = {
        0: 0,      # 0 Mbps
        10: 1,     # 10 Mbps
        100: 2,    # 100 Mbps
        1000: 3,   # 1GBPS
        10000: 4,  # 10 GBPS
        25000: 5,  # 25 GBPS
        40000: 6,  # 40GBps
        100000: 7, # 100Gbps
        'auto': 8, # default speed
        20000: 9,  # 20 GBPS
        50000: 10, # 50 GBPS
        200000: 11,# 200 GBPS
        400000: 12,# 400 GBPS
        4000: 13,  # 4 GFC
        8000: 14,  # 8 GFC
        16000: 15,  # 16 GFC
        32000: 16  # 32 GFC
        }
yang_to_mbps_speed = {
        0: 0,      # 0 Mbps
        1: 10,     # 10Mbps
        2: 100,    # 100 Mbps
        3: 1000,   # 1Gbps
        4: 10000,  # 10Gbps
        5: 25000,  # 25 Gbps
        6: 40000,  # 40Gbps
        7: 100000, # 100Gbps
        8: 'auto',  # default speed
        9: 20000, # 20 Gbps
       10: 50000, # 50 Gbps
       11: 200000, # 200 Gbps
       12: 400000, # 400 Gbps
       13: 4000,    # 4GFC
       14: 8000,    # 8 GFC
       15: 16000,  # 16 GFC
       16: 32000  # 32 GFC
        }

breakout_allowed_speed = {
    '1x1' : ('40G', '100G'),
    '2x1' : ('20G', '50G'),
    '4x1' : ('10G', '25G')
}

yang_fec_mode = {
    'AUTO'     : 1,
    'OFF'      : 2,
    'CL91-RS'  : 3,
    'CL74-FC'  : 4,
    'CL108-RS' : 5
}

fec_mode_to_yang = {
    1: 'AUTO',
    2: 'OFF',
    3: 'CL91-RS',
    4: 'CL74-FC',
    5: 'CL108-RS'
}

def is_fec_supported(fec_mode, if_speed):
    if ((if_speed == yang_speed['25G'] or
         if_speed == yang_speed['50G'] or
         if_speed == yang_speed['100G']) and
        (fec_mode == yang_fec_mode['AUTO'] or
         fec_mode == yang_fec_mode['OFF'])):
        # AUTO and OFF modes are supported by FEC enabled port
        return True

    if ((if_speed == yang_speed['25G'] or
         if_speed == yang_speed['50G']) and
        (fec_mode == yang_fec_mode['CL74-FC'] or
         fec_mode == yang_fec_mode['CL108-RS'])):
        # 25G/50G port supports CL74 and CL108 modes
        return True

    if (if_speed == yang_speed['100G'] and
        fec_mode == yang_fec_mode['CL91-RS']):
        # 100G port supports CL91 modes
        return True

    return False

def is_key_valid(dict, key):
    return key in dict

def get_value(dict, key):
    return dict[key]
