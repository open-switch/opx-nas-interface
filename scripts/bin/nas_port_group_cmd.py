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

import nas_port_group_utils as pg_utils
import nas_common_header as nas_comm
import argparse
from subprocess import call
import nas_media_config as media_config

_pg_key = 'base-pg/dell-pg/port-groups/port-group'
_pg_state_key = 'base-pg/dell-pg/port-groups-state/port-group-state'

def show_port_group(args):
    if args.pg_name == 'all':
        call(["cps_get_oid.py", "target/", _pg_key ])
    else:
        call(["cps_get_oid.py", "target/", _pg_key, pg_utils.pg_attr('id') + '=' + args.pg_name])
        call(["cps_get_oid.py", "observed/", _pg_state_key, pg_utils.pg_state_attr('id') + '=' + args.pg_name])

def config_port_group(args):
    if args.pg_name == None or args.Breakout == None or args.PHY_Mode == None or args.Port_Speed == None:
       return False

    br_mode = nas_comm.yang.get_value(args.Breakout, 'yang-breakout-mode')
    phy_mode= nas_comm.yang.get_value(args.PHY_Mode, 'yang-phy-mode')
    port_speed = nas_comm.yang.get_value(args.Port_Speed, 'yang-speed')

    call(["cps_set_oid.py", "-qua", "target", "-oper", "set", _pg_key, pg_utils.pg_attr('id') + '=' + args.pg_name,
                                                      pg_utils.pg_attr('breakout-mode') + '=' + str(br_mode),
                                                      pg_utils.pg_attr('port-speed') + '=' + str(port_speed),
                                                      pg_utils.pg_attr('phy-mode') + '=' + str(phy_mode)])

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-s", "--Set", action="store_true", help=" Set Port group config")
    parser.add_argument("pg_name", help=" Port Group Name or all to get all port groups info")
    parser.add_argument("-b", "--Breakout", choices=nas_comm.yang.get_tbl('yang-breakout-mode'), default="1x1", help=" Breakout Mode")
    parser.add_argument("-p", "--PHY_Mode", choices=nas_comm.yang.get_tbl('yang-phy-mode'), default="ether", help=" Physical Layer Ethernet or Fibre channel")
    parser.add_argument("-S", "--Port_Speed", choices=nas_comm.yang.get_tbl('yang-speed'), default="40G", help=" Physical Port Speed")



    args = parser.parse_args()
    print args.Set
    print args.pg_name
    print args.Breakout
    print args.PHY_Mode
    print args.Port_Speed

    if args.Set == None:
        show_port_group(args)
    else:
        config_port_group(args)



