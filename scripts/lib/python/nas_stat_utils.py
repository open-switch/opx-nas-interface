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

import bytearray_utils as ba
import cps_utils

stat_key_string = ["dell-base-if-cmn/if/interfaces-state/interface/statistics",
                   "dell-base-if-cmn/dell-if/clear-counters/input"]

filters = []

ether_stats = [
'rx_bytes',
'tx_bytes',
'rx_no_errors',
'tx_no_errors',
'tx_total_collision',
'rx_undersize_packets',
'rx_jabbers',
'rx_fragments',
'rx_discards',
'rx_mcast_packets',
'rx_bcast_packets',
'rx_oversize_packets',
'tx_oversize_packets',
'rx_64_byte_packets',
'rx_65_to_127_byte_packets',
'rx_128_to_255_byte_packets',
'rx_256_to_511_byte_packets',
'rx_512_to_1023_byte_packets',
'rx_1024_to_1518_byte_packets',
'rx_1519_to_2047_byte_packets',
'rx_2048_to_4095_byte_packets',
'rx_4096_to_9216_byte_packets',
'tx_64_byte_packets',
'tx_65_to_127_byte_packets',
'tx_128_to_255_byte_packets',
'tx_256_to_511_byte_packets',
'tx_512_to_1023_byte_packets',
'tx_1024_to_1518_byte_packets',
'tx_1519_to_2047_byte_packets',
'tx_2048_to_4095_byte_packets',
'tx_4096_to_9216_byte_packets',
'crc_align_errors',
]

ether_stats_oid_mapping = {
'rx_bytes':                           'if/interfaces-state/interface/statistics/in-octets',
'tx_bytes':                           'if/interfaces-state/interface/statistics/out-octets',
'rx_no_errors':                       'dell-if/if/interfaces-state/interface/statistics/ether-rx-no-errors',
'tx_no_errors':                       'dell-if/if/interfaces-state/interface/statistics/ether-tx-no-errors',
'tx_total_collision':                 'dell-if/if/interfaces-state/interface/statistics/ether-collisions',
'rx_undersize_packets':               'dell-if/if/interfaces-state/interface/statistics/ether-undersize-pkts',
'rx_jabbers':                         'dell-if/if/interfaces-state/interface/statistics/ether-jabbers',
'rx_fragments':                       'dell-if/if/interfaces-state/interface/statistics/ether-fragments',
'rx_discards':                        'dell-if/if/interfaces-state/interface/statistics/ether-drop-events',
'rx_mcast_packets':                   'dell-if/if/interfaces-state/interface/statistics/ether-multicast-pkts',
'rx_bcast_packets':                   'dell-if/if/interfaces-state/interface/statistics/ether-broadcast-pkts',
'rx_oversize_packets':                'dell-if/if/interfaces-state/interface/statistics/ether-rx-oversize-pkts',
'tx_oversize_packets':                'dell-if/if/interfaces-state/interface/statistics/ether-tx-oversize-pkts',
'rx_64_byte_packets':                 'dell-if/if/interfaces-state/interface/statistics/ether-in-pkts-64-octets',
'rx_65_to_127_byte_packets':          'dell-if/if/interfaces-state/interface/statistics/ether-in-pkts-65-to-127-octets',
'rx_128_to_255_byte_packets':         'dell-if/if/interfaces-state/interface/statistics/ether-in-pkts-128-to-255-octets',
'rx_256_to_511_byte_packets':         'dell-if/if/interfaces-state/interface/statistics/ether-in-pkts-256-to-511-octets',
'rx_512_to_1023_byte_packets':        'dell-if/if/interfaces-state/interface/statistics/ether-in-pkts-512-to-1023-octets',
'rx_1024_to_1518_byte_packets':       'dell-if/if/interfaces-state/interface/statistics/ether-in-pkts-1024-to-1518-octets',
'rx_1519_to_2047_byte_packets':       'dell-if/if/interfaces-state/interface/statistics/ether-in-pkts-1519-to-2047-octets',
'rx_2048_to_4095_byte_packets':       'dell-if/if/interfaces-state/interface/statistics/ether-in-pkts-2048-to-4095-octets',
'rx_4096_to_9216_byte_packets':       'dell-if/if/interfaces-state/interface/statistics/ether-in-pkts-4096-to-9216-octets',
'tx_64_byte_packets':                 'dell-if/if/interfaces-state/interface/statistics/ether-out-pkts-64-octets',
'tx_65_to_127_byte_packets':          'dell-if/if/interfaces-state/interface/statistics/ether-out-pkts-65-to-127-octets',
'tx_128_to_255_byte_packets':         'dell-if/if/interfaces-state/interface/statistics/ether-out-pkts-128-to-255-octets',
'tx_256_to_511_byte_packets':         'dell-if/if/interfaces-state/interface/statistics/ether-out-pkts-256-to-511-octets',
'tx_512_to_1023_byte_packets':        'dell-if/if/interfaces-state/interface/statistics/ether-out-pkts-512-to-1023-octets',
'tx_1024_to_1518_byte_packets':       'dell-if/if/interfaces-state/interface/statistics/ether-out-pkts-1024-to-1518-octets',
'tx_1519_to_2047_byte_packets':       'dell-if/if/interfaces-state/interface/statistics/ether-out-pkts-1519-to-2047-octets',
'tx_2048_to_4095_byte_packets':       'dell-if/if/interfaces-state/interface/statistics/ether-out-pkts-2048-to-4095-octets',
'tx_4096_to_9216_byte_packets':       'dell-if/if/interfaces-state/interface/statistics/ether-out-pkts-4096-to-9216-octets',
'crc_align_errors':                   'dell-if/if/interfaces-state/interface/statistics/ether-crc-align-errors',
}
def get_stat_keys():
    return stat_key_string

def print_ether_stats(obj):
    print "Ether statistics:" + "\n"
    for stat in ether_stats:
        oid = ether_stats_oid_mapping[stat]
        print '    ' + stat + ': ' + str(ba.from_ba(obj['data'][oid], "uint64_t"))
    val1 = ba.from_ba(obj['data']['if/interfaces-state/interface/statistics/out-discards'], "uint64_t")
    val2 = ba.from_ba(obj['data']['if/interfaces-state/interface/statistics/in-discards'], "uint64_t")
    print '    buffer-overrun errors: ', (val1+val2)
    print "\n\n"

def print_stats(obj):
    if len(filters) != 0:
        for item in filters:
            if item in obj:
                print '{:40s} {:d}'.format(item.split("/")[-1], ba.from_ba(obj[item], "uint64_t"))
    else:
        for item in obj:
            print item + ':  ' + str(ba.from_ba(obj[item], "uint64_t"))
    print "\n\n"

def add_filters(stat_filters):
    for i in stat_filters:
        filters.append(i)

cps_utils.add_print_function(stat_key_string[0], print_stats)
cps_utils.add_print_function(stat_key_string[1], print_stats)
