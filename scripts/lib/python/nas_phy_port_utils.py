
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
import cps_utils
import nas_os_if_utils as nas_if
import nas_common_header as nas_comm

# list of all phy port object indexed by phy port and hwport
g_port_list = {}

def get_phy_port_list():
    return g_port_list

def get_phy_port_cache_keys(npu, port):
    return "port-%d-%d" % (npu, port)


def get_phy_port_cache_hw_keys(npu, port):
    return "hw-%d-%d" % (npu, port)

def get_first_hw_port(port_obj):
    hwport_list = port_obj.get_attr_data('hardware-port-list')
    return hwport_list[0]

def phy_port_cache_init(port_list):
    if port_list:
        nas_if.log_err(' port list is not empty')
        return False
    l = []
    if not cps.get([cps_object.CPSObject(module='base-if-phy/physical').get()], l):
        return False

    for i in l:
        port_obj = cps_object.CPSObject(obj=i)
        npu = port_obj.get_attr_data('npu-id')
        port_list[get_phy_port_cache_hw_keys(npu, get_first_hw_port(port_obj))] = port_obj
        port_list[get_phy_port_cache_keys(npu, port_obj.get_attr_data('port-id'))] = port_obj
    return True


def get_phy_port_by_phy_port(port_list, npu,port):
    if get_phy_port_cache_keys(npu, port) in port_list:
        return port_list[get_phy_port_cache_keys(npu,port)]
    else:
        return None

def get_phy_port_by_hw_port(port_list, npu,port):
    if get_phy_port_cache_hw_keys(npu, port) in port_list:
        return port_list[get_phy_port_cache_hw_keys(npu,port)]
    else:
        return None

def add_phy_port(port_list, port_obj):
    npu = port_obj.get_attr_data('npu-id')
    port_list[get_phy_port_cache_hw_keys(npu, get_first_hw_port(port_obj))] = port_obj
    port_list[get_phy_port_cache_keys(npu, port_obj.get_attr_data('port-id'))] = port_obj
    return

def del_phy_port(port_list, port_obj):
    npu = port_obj.get_attr_data('npu-id')
    del port_list[get_phy_port_cache_hw_keys(npu, get_first_hw_port(port_obj))]
    del port_list[get_phy_port_cache_keys(npu, port_obj.get_attr_data('port-id'))]
    return
def hw_port_to_phy_port(port_list, npu, hwport):
    port_obj = get_phy_port_by_hw_port(port_list, npu, hwport)
    if port_obj != None:
        return(port_obj.get_attr_data('port-id'))
    else:
        return -1
def hw_port_to_phy_mode(port_list, npu, hwport):
    port_obj = get_phy_port_by_hw_port(port_list, npu, hwport)
    if port_obj != None:
        return(port_obj.get_attr_data('phy-mode'))
    else:
        return -1

def phy_port_to_first_hwport(port_list, npu, port):
    port_obj = get_phy_port_by_phy_port(port_list, npu,port)
    if port_obj != None:
        return(get_first_hw_port(port_obj))
    else:
        return -1

# obtain the supported speed list for an npu, port
def phy_port_supported_speed_get(npu, port):
    supported_speed = []
    port_obj = get_phy_port_by_hw_port(g_port_list, npu, port)
    if port_obj != None:
        supported_speed = port_obj.get_attr_data('supported-speed')
    return supported_speed

def print_port_obj(port_obj):
    cps_utils.print_obj(port_obj.get())
    return

def print_port_list(port_list):
    for ph in port_list:
        print 'Key ' + str(ph)
        cps_utils.print_obj(port_list[ph].get())
    return
# Delete nas port
def cps_del_nas_port(port_obj):
    npu = port_obj.get_attr_data('npu-id')
    port = port_obj.get_attr_data('port-id')
    nas_if.log_info(' Delete physical port %d ' % port)
    obj = cps_object.CPSObject(module='base-if-phy/physical',qual='target',data={'npu-id':npu,'port-id':port})
    ch = {'operation':'delete', 'change':obj.get()}
    return cps.transaction([ch])


def cps_create_nas_port(npu, hwports, speed, phy_mode, fr_port = None):
    nas_if.log_info(' Create physical port hwports %s speed %d phy mode %d' % (str(hwports), speed, phy_mode))
    attr_list = {'npu-id':npu,'hardware-port-list':hwports,'speed':speed, 'phy-mode': phy_mode}
    if fr_port != None:
        attr_list['front-panel-number'] = fr_port
    phy_obj = cps_object.CPSObject(module='base-if-phy/physical',qual='target', data=attr_list)
    data = cps_utils.CPSTransaction([('create', phy_obj.get())]).commit()
    if data == False:
        nas_if.log_err('Failed to create Phy port %s ' % str(hwports))
        return None
    port_obj  = cps_object.CPSObject(obj = data[0]['change'])
    return port_obj

# Create npu ports (physical ports ) based on the breakout mode and port speed and returns newly created npu ports
def create_nas_ports(npu, hwports, br_mode,speed, phy_mode, fr_port, created_phy_ports):
    if br_mode not in nas_comm.yang.get_tbl('breakout-to-hwp-count'):
        nas_if.log_err('unsupported breakout mode %d' % br_mode)
        return False

    hwp_count = nas_comm.yang.get_tbl('breakout-to-hwp-count')[br_mode]
    hwports.reverse()

    while len(hwports) != 0 and hwp_count != 0:
        i =0
        hw_list = []
        if len(hwports) < hwp_count:
            nas_if.log_err('some problem in creating hwport len %s ' % str(hwports))
            return False
        # Get hw_ports_list for creating new phy port
        while  i < hwp_count:
            hw_list.append(hwports.pop())
            i = i + 1
        # Send request to create phy port with hwlist and port speed
        obj = cps_create_nas_port(npu,hw_list,speed, phy_mode, fr_port)
        if obj == None:
            return False
        created_phy_ports[hw_list[0]] = obj
    return True

# Rollback Created and deleted  ports
def rollback_port_add_del(port_list,created_phy_ports, deleted_phy_ports, speed, phy_mode):
    for port,port_obj in created_phy_ports.items():
        npu = port_obj.get_attr_data('npu-id')
        cps_del_nas_port(port_obj)
        if port_list[get_phy_port_cache_keys(npu, port)]:
            del_phy_port(port_list, port_obj)

    for port, port_obj in deleted_phy_ports.items():
        npu = port_obj.get_attr_data('npu-id')
        hwports = port_obj.get_attr_data('hardware-port-list')
        new_port_obj = cps_create_nas_port(npu, hwports, speed, phy_mode)
        if new_port_obj != None:
            add_phy_port(port_list, new_port_obj)
        else:
            nas_if.log_err('Rollback failed')
