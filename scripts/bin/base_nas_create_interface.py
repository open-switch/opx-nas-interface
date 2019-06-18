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
import nas_os_if_utils as nas_if
import nas_common_header as nas_comm

import time
import logging

front_panel_ports = None
port_cache = None
if_cache = None

def create_interface(obj):

    ifobj = nas_if.make_interface_from_phy_port(obj)

    if if_cache.exists(ifobj.get_attr_data('if/interfaces/interface/name')):
        nas_if.log_err("Already exists.... " + str(ifobj.get_attr_data('if/interfaces/interface/name')))
        return

    # create the object
    ch = {'operation': 'rpc', 'change': ifobj.get()}
    cps.transaction([ch])
    nas_if.log_info("Interface Created : " + str(ifobj.get_attr_data('if/interfaces/interface/name')))

if __name__ == '__main__':

    while cps.enabled(nas_comm.yang.get_tbl('keys_id')['physical_key']) == False:
        nas_if.log_err('physical port info  not ready ')
        time.sleep(1) #in seconds

    port_cache = nas_if.PhyPortCache()
    if port_cache.len() ==0:
        nas_if.log_err('physical port info  not present')

    while cps.enabled(nas_comm.yang.get_tbl('keys_id')['fp_key']) == False:
        nas_if.log_err('fetch front panel port info  not ready ')
        time.sleep(1) #in seconds

    front_panel_ports = nas_if.FpPortCache()
    if front_panel_ports.len() == 0:
        nas_if.log_err('front panel port info  not present')

    if_cache = nas_if.IfCache()

    while cps.enabled(nas_comm.yang.get_tbl('keys_id')['set_intf_key']) == False: 
        nas_if.log_err('Logical Interface handler not ready')
        time.sleep(1)

    # walk through the list of physical ports
    for port in port_cache.get_port_list():
        obj = cps_object.CPSObject(obj=port)
        try:
            create_interface(obj)
        except:
            logging.exception('Interface Creation failure:')
            pass
