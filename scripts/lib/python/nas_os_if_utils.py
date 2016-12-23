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
import cps_utils
import cps_object
import bytearray_utils
import event_log as ev

nas_os_if_keys = {'interface': 'dell-base-if-cmn/if/interfaces/interface',
        'interface-state': 'dell-base-if-cmn/if/interfaces-state/interface',
        'physical': 'base-if-phy/physical',
        'hardware-port' : 'base-if-phy/hardware-port',
        'front-panel-port' : 'base-if-phy/front-panel-port' }

default_chassis_id = 1
default_slot_id = 1
_g_if_type = "ianaift:ethernetCsmacd"
_g_cpu_if_type = "base-if:cpu"

# TODO default values should go to a common file and ideally it should be generated from yang file  just like C headers
_yang_auto_speed = 8  # default value for auto speed
_yang_auto_neg = 1    # default value for negotiation
_yang_auto_dup = 3      #auto - default value for duplex

def log_err(msg):
    ev.logging("INTERFACE",ev.ERR,"INTERFACE-HANDLER","","",0,msg)

def log_info(msg):
    ev.logging("INTERFACE",ev.INFO,"INTERFACE-HANDLER","","",0,msg)


def get_if_key():
    return nas_os_if_keys['interface']

def get_if_state_key():
    return nas_os_if_keys['interface-state']

def get_physical_key():
    return nas_os_if_keys['physical']

def get_fp_key():
    return nas_os_if_keys['front-panel-port']

def get_hw_key():
    return nas_os_if_keys['hardware-port']

cps_utils.add_attr_type('base-if-phy/front-panel-port/default-name', 'string')
cps_utils.add_attr_type('if/interfaces/interface/name', 'string')
cps_utils.add_attr_type('dell-if/if/interfaces/interface/phys-address', 'string')


def set_obj_val(obj, key, val):
    obj.add_attr(key, val)


def make_if_obj(d={}):
    return cps_object.CPSObject(module=get_if_key(), data=d)

def make_if_state_obj(d={}):
    return cps_object.CPSObject(module=get_if_state_key(),qual='observed', data=d)


def make_phy_obj(d={}):
    return cps_object.CPSObject(module=get_physical_key(), data=d)


def make_hwport_obj(d={}):
    return cps_object.CPSObject(module=get_hw_key(), data=d)


def make_fp_obj(d={}):
    return cps_object.CPSObject(module=get_fp_key(), data=d)


def nas_os_cpu_if(d={}):
    l = []
    filt = make_if_obj(d)
    set_obj_val(filt, 'if/interfaces/interface/type', _g_cpu_if_type)

    if cps.get([filt.get()], l):
        return l
    return None

def nas_os_if_list(d={}):
    l = []
    filt = make_if_obj(d)
    set_obj_val(filt, 'if/interfaces/interface/type', _g_if_type)

    if cps.get([filt.get()], l):
        return l
    return None

def nas_os_if_state_list(d={}):
    l = []
    filt = make_if_state_obj(d)
    set_obj_val(filt, 'if/interfaces-state/interface/type', _g_if_type)

    if cps.get([filt.get()], l):
        return l
    return None


def nas_os_phy_list(d={}):
    l = []
    filt = make_phy_obj(d)

    if cps.get([filt.get()], l):
        return l
    return None

def nas_os_hwport_list(d={}):
    l = []
    filt = make_hwport_obj(d)

    if cps.get([filt.get()], l):
        return l
    return None

def nas_os_fp_list(d={}):
    l = []
    filt = make_fp_obj(d)

    if cps.get([filt.get()], l):
        return l

    return None


def name_to_ifindex(ifname):
    ifs = nas_os_if_list()
    if ifs == None:
        return None
    for intf in ifs:
        obj = cps_object.CPSObject(obj=intf)
        name = obj.get_attr_data('if/interfaces/interface/name')
        if name == ifname:
            return obj.get_attr_data('dell-base-if-cmn/if/interfaces/interface/if-index')

    return None


def ifindex_to_name(ifindex):
    intf = nas_os_if_list({'if-index': ifindex})
    if intf:
        obj = cps_object.CPSObject(obj=intf[0])
        return obj.get_attr_data('if/interfaces/interface/name')
    return None


def get_default_mtu():
    return 1532

def get_base_mac_address():
    obj = cps_object.CPSObject(module='base-pas/chassis', qual="observed")
    chassis = []
    cps.get([obj.get()], chassis)

    base_mac_address = chassis[0]['data'][
        'base-pas/chassis/base_mac_addresses']
    return base_mac_address

class IfComponentCache(object):

    def _list(self, type):
        obj = cps_object.CPSObject(module=type)
        l = []
        cps.get([obj.get()], l)
        return l

    def print_keys(self):
        for k in self.m:
            print k

    def __init__(self):
        self.m = {}

    def exists(self, name):
        return name in self.m

    def get(self, name):
        if not self.exists(name):
            return None
        return self.m[name]

    def len(self):
        return len(self.m)


class FpPortCache(IfComponentCache):

    def make_media_key(self, media):
        return 'media-%d' % (media)

    def __init__(self):
        IfComponentCache.__init__(self)
        l = self._list(get_fp_key())
        for e in l:
            obj = cps_object.CPSObject(obj=e)
            self.m[obj.get_attr_data('default-name')] = obj
            self.m[obj.get_attr_data('front-panel-port')] = obj
            self.m[self.make_media_key(
                   obj.get_attr_data('media-id'))] = obj

    def get_by_media_id(self, media_id):
        return self.get(self.make_media_key(media_id))


class PhyPortCache(IfComponentCache):

    def get_phy_port_cache_keys(self, npu, port):
        return "port-%d-%d" % (npu, port)

    def get_phy_port_cache_hw_keys(self, npu, port):
        return "hw-%d-%d" % (npu, port)

    def __init__(self):
        IfComponentCache.__init__(self)
        self.l = self._list(get_physical_key())
        self.m = {}

        for i in self.l:
            ph = cps_object.CPSObject(obj=i)
            self.m[
                self.get_phy_port_cache_hw_keys(
                    ph.get_attr_data('npu-id'),
                    ph.get_attr_data('hardware-port-id'))
            ] = ph
            self.m[self.get_phy_port_cache_keys(ph.get_attr_data('npu-id'),
                                                ph.get_attr_data('port-id'))] = ph

    def get_by_port(self, npu, port):
        return self.get(self.get_phy_port_cache_keys(npu, port))

    def get_by_hw_port(self, npu, port):
        return self.get(self.get_phy_port_cache_hw_keys(npu, port))

    def get_port_list(self):
        return self.l


class IfCache(IfComponentCache):

    def get_phy_port_cache_keys(self, npu, port):
        return "port-%d-%d" % (npu, port)

    def __init__(self):
        IfComponentCache.__init__(self)
        l = nas_os_if_list()
        for i in l:
            try:
                obj = cps_object.CPSObject(obj=i)
                self.m[obj.get_attr_data('if/interfaces/interface/name')] = obj
                self.m[self.get_phy_port_cache_keys(
                    obj.get_attr_data('base-if-phy/if/interfaces/interface/npu-id'),
                    obj.get_attr_data('base-if-phy/if/interfaces/interface/port-id')) ] = obj
            except:
                continue

    def get_by_port(self, npu, port):
        return self.get(self.get_phy_port_cache_keys(npu, port))


def get_interface_name(chassis, slot, port, lane):
    return 'e%d%02d-%03d-%d' % (chassis, slot, port, lane)

def make_interface_from_phy_port(obj):
    npu = obj.get_attr_data('npu-id')
    hw_port_id = obj.get_attr_data('hardware-port-id')

    l = []
    elem = cps_object.CPSObject(module=get_hw_key(), data={
        'npu-id': npu,
        'hw-port': hw_port_id
    })
    cps.get([elem.get()], l)
    if len(l) == 0:
        log_err("Invalid port specified... ")
        log_err(str(elem.get()))
        raise Exception("Invalid port - no matching hardware-port")

    elem = cps_object.CPSObject(obj=l[0])

    chassis_id = default_chassis_id
    slot_id = default_slot_id
    fp_port_id = elem.get_attr_data('front-panel-port')
    lane = elem.get_attr_data('subport-id')
    mode = elem.get_attr_data('fanout-mode')
    _subport = lane
    if mode != 4:  # 1x1
        _subport += 1
    name = get_interface_name(chassis_id, slot_id, fp_port_id, _subport)

    # extract the port number from the phy port strcture (based on the hwid)
    port = obj.get_attr_data('port-id')

    # setting default MTU size during interface creation.
    _mtu = get_default_mtu()
    ifobj = cps_object.CPSObject(module='dell-base-if-cmn/set-interface', data={
        'dell-base-if-cmn/set-interface/input/operation': 1,
        'if/interfaces/interface/name': name,
        'base-if-phy/hardware-port/front-panel-port':fp_port_id,
        'base-if-phy/hardware-port/subport-id':_subport,
        'base-if-phy/if/interfaces/interface/npu-id': npu,
        'base-if-phy/if/interfaces/interface/port-id': port,
        'dell-if/if/interfaces/interface/mtu': _mtu,
        'dell-if/if/interfaces/interface/negotiation':_yang_auto_neg,
        'dell-if/if/interfaces/interface/speed':_yang_auto_speed,
        'dell-if/if/interfaces/interface/duplex':_yang_auto_dup,
        'if/interfaces/interface/type':_g_if_type})

    return ifobj

def create_interface_set_obj(if_name, attr_list):
    ifobj = cps_object.CPSObject(module='dell-base-if-cmn/set-interface', data = {})
    ifobj.add_attr('dell-base-if-cmn/set-interface/input/operation',3)
    ifobj.add_attr('if/interfaces/interface/name',if_name)
    for attr in attr_list:
        ifobj.add_attr(attr, attr_list[attr])
    return ifobj


def physical_ports_for_front_panel_port(fp_obj):
    # replace with object get attr data
    ports = fp_obj.get()['data']['base-if-phy/front-panel-port/port']

    npu = fp_obj.get_attr_data('npu-id')

    l = []
    phy_port_list = []
    phy_port_cache = PhyPortCache()

    for i in ports:
        _port = cps_object.types.from_data(
            'base-if-phy/front-panel-port/port',
            i)
        phy_port = phy_port_cache.get_by_hw_port(npu, _port)
        if phy_port is None:
            continue
        phy_port_list.append(phy_port)

    return phy_port_list

class npuPort:
    def __init__(self):
        self.npu = 0
        self.port = 0
def get_phy_port_from_if_index(if_index):
    l = nas_os_if_list(d={'if-index':if_index})
    if l == None:
        return None

    phy_port = npuPort()
    obj = cps_object.CPSObject(obj=l[0])
    phy_port.npu = obj.get_attr_data('npu-id')
    phy_port.port = obj.get_attr_data('port-id')
    return phy_port

class hwPort:
    def __init__(self):
        self.npu = 0
        self.hw_port = 0

def get_hwport_from_phy_port(npu, port):
    port = nas_os_phy_list(d={'npu-id':npu,'port-id':port})
    if port == None:
        return None
    port_obj = cps_object.CPSObject(obj=port[0])

    hwport_details = hwPort()
    hwport_details.hw_port = port_obj.get_attr_data('hardware-port-id')
    hwport_details.npu = port_obj.get_attr_data('npu-id')
    return hwport_details

def get_fp_from_hw_port(npu, hw_port):
    hwport_list = nas_os_hwport_list(d={'npu-id':npu, 'hw-port':hw_port})
    if hwport_list == None:
        return None
    hwport_obj = cps_object.CPSObject(obj=hwport_list[0])
    fp_port = hwport_obj.get_attr_data('front-panel-port')
    return fp_port


def get_media_id_from_fp_port(fp_port):
    fp_port_list = nas_os_fp_list()
    media_id = None
    for fp in fp_port_list:
        obj = cps_object.CPSObject(obj=fp)
        if fp_port == obj.get_attr_data('front-panel-port'):
            media_id =  obj.get_attr_data('media-id')
            break
    return media_id

admin_status_to_str = {
        1: 'UP',
        2: 'DOWN',
        3: 'TESTING'
        }
def admin_status_str_get(admin_status):
    if admin_status <= len(admin_status_to_str):
        return admin_status_to_str[admin_status]
    else:
        return 'UNKNOWN'

oper_status_to_str = {
        1: 'UP',
        2: 'DOWN',
        3: 'TESTING',
        4: 'UNKNOWN',
        5: 'DORMANT',
        6: 'NOT PRESENT',
        7: 'LOWER LAYER DOWN'
        }

def oper_status_str_get(oper_status):
    if oper_status <= len(oper_status_to_str):
        return oper_status_to_str[oper_status]
    else:
        return 'UNKNOWN'


# functions for mapping speed, duplex and autneg to/from yang value


to_yang_speed_map = {
        0: 0,      # 0 Mbps
        10: 1,     # 10 Mbps
        100: 2,    # 100 Mbps
        1000: 3,   # 1GBPS
        10000: 4,  # 10 GBPS
        25000: 5,  # 25 GBPS
        40000: 6,  # 40GBps
        100000: 7, # 100Gbps
        'auto': 8  # deafault speed
        }
from_yang_speed_map = {
        0: 0,      # 0 Mbps
        1: 10,     # 10Mbps
        2: 100,    # 100 Mbps
        3: 1000,   # 1Gbps
        4: 10000,  # 10Gbps
        5: 25000,  # 25 Gbps
        6: 40000,  # 40Gbps
        7: 100000, # 100Gbps
        8: 'auto'  # default speed
        }

def to_yang_speed(speed):
    if speed in to_yang_speed_map.keys():
        return(to_yang_speed_map[speed])
    else:
        return(False)

def from_yang_speed(speed):
    if speed in from_yang_speed_map.keys():
        return(from_yang_speed_map[speed])
    else:
        return(False)

def to_yang_autoneg(autoneg):
    if autoneg == "on":
        return(True)
    else:
        return(False)

def from_yang_autoneg(autoneg):
    if autoneg == True:
        return("on")
    else:
        return("off")

to_yang_negotiation_map = {
         'auto': 1,
         'on' : 2,
         'off': 3
         }
from_yang_negotiation_map = {
        1: 'auto',
        2: 'on',
        3: 'off'
        }

def to_yang_negotiation(neg):
    if neg in to_yang_negotiation_map:
        return (to_yang_negotiation_map[neg])
    return(None)

def from_yang_negotiation(neg):
    if neg in from_yang_negotiation_map:
        return (from_yang_negotiation_map[neg])
    return(None)

to_yang_duplex_map = {
         'full' : 1,
         'half': 2,
         'auto': 3
         }
from_yang_duplex_map = {
        1: 'full',
        2: 'half',
        3: 'auto'
        }

def to_yang_duplex(dup):
    if dup in to_yang_duplex_map:
        return (to_yang_duplex_map[dup])
    return(None)

def from_yang_duplex(dup):
    if dup in from_yang_duplex_map:
        return (from_yang_duplex_map[dup])
    return(None)

# Set speed , duplex and autoneg
def nas_set_interface_attribute(if_name, speed, duplex, autoneg):
    if if_name == None:
        log_err('not a valid interface')
        return False
    _data = {}
    if speed != None:
        _data['dell-if/if/interfaces/interface/speed']= speed
    if duplex != None:
        _data['dell-if/if/interfaces/interface/duplex']= duplex
    if autoneg != None:
        _data['dell-if/if/interfaces/interface/negotiation']= autoneg
    intf = create_interface_set_obj(if_name, _data)
    intf.add_attr('if/interfaces/interface/type', _g_if_type)
    ch = {'operation':'rpc', 'change':intf.get()}
    if cps.transaction([ch]):
        return(True)
    else:
        return(False)

def get_media_id_from_phy_port(npu, port):
    hwp = get_hwport_from_phy_port(npu, port)
    fp_port =  get_fp_from_hw_port(npu, hwp.hw_port)
    media_id = get_media_id_from_fp_port(fp_port)
    return media_id


def get_media_id_from_if_index(if_index):
    phy_port = get_phy_port_from_if_index(if_index)
    if phy_port == None:
        log_err('not a physical port')
        return False
    hwp = get_hwport_from_phy_port(phy_port.npu, phy_port.port)
    fp_port =  get_fp_from_hw_port(phy_port.npu, hwp.hw_port)
    media_id = get_media_id_from_fp_port(fp_port)
    return media_id

def get_front_port_from_name(if_name, check_if = True):
    if check_if:
        if_index = name_to_ifindex(if_name)
        if if_index == None:
            log_err('Invalid interface name '+str(if_name))
            return None
    try:
        (unit_slot, port, sub_port) = if_name.split('-')
    except ValueError:
        log_err('Interface '+str(if_name)+' is not front port')
        return None
    try:
        front_port = int(port)
        subport_id = int(sub_port)
    except ValueError:
        log_err('Interface '+str(if_name)+' is not front port')
        return None
    return (front_port, subport_id)


def get_breakoutCap_currentMode_mode(if_index):
    phy_port = get_phy_port_from_if_index(if_index)
    return (get_port_breakoutCap_currentMode_mode(phy_port.npu, phy_port.port))

def get_port_breakoutCap_currentMode_mode(npu, port):
    port = nas_os_phy_list(d={'npu-id':npu,'port-id':port})
    if port == None:
        return None
    port_obj = cps_object.CPSObject(obj=port[0])
    breakout_cap = port_obj.get_attr_data('breakout-capabilities')
    current_mode = port_obj.get_attr_data('fanout-mode')
    return (breakout_cap,current_mode)

def get_cps_attr(obj,attr_name):
    attr = None
    try:
        attr = obj.get_attr_data(attr_name)
    except:
        return None
    return attr


