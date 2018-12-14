#!/usr/bin/python
# Copyright (c) 2018 Dell Inc.
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

import nas_fp_port_utils as fp_utils
import nas_front_panel_map as fp
import nas_port_group_utils as nas_pg
import nas_common_header as nas_comm
import nas_media_config as media_config
import nas_yang_values as yv


nas_os_if_keys = {'interface': 'dell-base-if-cmn/if/interfaces/interface',
        'interface-state': 'dell-base-if-cmn/if/interfaces-state/interface',
        'physical': 'base-if-phy/physical',
        'hardware-port' : 'base-if-phy/hardware-port',
        'front-panel-port' : 'base-if-phy/front-panel-port',
        'port-group-state' : 'base-pg/dell-pg/port-groups-state/port-group-state',
        'port-group' : 'base-pg/dell-pg/port-groups/port-group' }

default_chassis_id = 1
default_slot_id = 1
_g_if_eth_type = "ianaift:ethernetCsmacd"
_g_if_fc_type = "ianaift:fibreChannel"
_g_cpu_if_type = "base-if:cpu"

# TODO default values should go to a common file and ideally it should be generated from yang file  just like C headers
_yang_auto_speed = 8  # default value for auto speed
_yang_auto_neg = 1    # default value for negotiation
#_yang_auto_dup = 3      #auto - default value for duplex

def log_err(msg):
    ev.logging("INTERFACE",ev.ERR,"INTERFACE-HANDLER","","",0,msg)

def log_info(msg):
    ev.logging("INTERFACE",ev.INFO,"INTERFACE-HANDLER","","",0,msg)

# note: Enable log for DSAPI
def log_obj(cps_obj):
    try:
        cps_utils.log_obj(cps_obj, ev.INFO)
    except:
        log_info('CPS obj logging is not successful completely')
        pass

def get_if_key():
    return nas_os_if_keys['interface']

def get_if_state_key():
    return nas_os_if_keys['interface-state']

def get_pg_state_key():
    return nas_os_if_keys['port-group-state']

def get_pg_key():
    return nas_os_if_keys['port-group']

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

def make_pg_state_obj(d={}):
    return cps_object.CPSObject(module=get_pg_state_key(),qual='observed', data=d)

def make_pg_obj(d={}):
    return cps_object.CPSObject(module=get_pg_key(), data=d)

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
    # get FC and ether type interfaces both
    eth_list = []
    set_obj_val(filt, 'if/interfaces/interface/type', _g_if_eth_type)
    if not(cps.get([filt.get()], eth_list)):
        return None

    fc_list = []
    set_obj_val(filt, 'if/interfaces/interface/type', _g_if_fc_type)
    if not(cps.get([filt.get()], fc_list)):
        return None

    l = eth_list + fc_list
    return l

def nas_os_if_state_list(d={}):
    l = []
    filt = make_if_state_obj(d)
    set_obj_val(filt, 'if/interfaces-state/interface/type', _g_if_eth_type)

    if cps.get([filt.get()], l):
        return l
    return None

def nas_os_pg_state_list(d={}):
    l = []
    filt = make_pg_state_obj(d)
    cps_utils.print_obj(filt.get(),show_key=False)
    if cps.get([filt.get()], l):
        return l
    return None

def nas_os_pg_list(d={}):
    l = []
    filt = make_pg_obj(d)
    cps_utils.print_obj(filt.get(),show_key=False)
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
    intf = nas_os_if_list(d={'if/interfaces/interface/name':ifname})
    if intf:
        obj = cps_object.CPSObject(obj=intf[0])
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

    def __len__(self):
        return self.len()

    def __delitem__(self, name):
        if self.exists(name):
            del self.m[name]

    def __getitem__(self, name):
        return self.get(name)

    def __iter__(self):
        return iter(self.m)

    def items(self):
        for item in self.m.items():
            yield item

class FpPortCache(IfComponentCache):

    def make_media_key(self, media):
        return 'media-%d' % (media)

    def __init__(self):
        IfComponentCache.__init__(self)
        l = self._list(get_fp_key())
        for e in l:
            obj = cps_object.CPSObject(obj=e)

            if get_cps_attr(obj,'default-name') is None:
                continue
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

def is_40g_mode_on_100g_port(fp_obj):
    try:
        port_speed = fp_obj.get_attr_data('port-speed')
        def_port_speed = fp_obj.get_attr_data('default-port-speed')
    except ValueError:
        log_err('Unable to find supported-speed from cps object')
        return False
    # Checking if default port speed is 100g and port speed is 40g
    if (def_port_speed == nas_comm.yang.get_value('100g', 'yang-speed') and
        port_speed == nas_comm.yang.get_value('40g', 'yang-speed')):
        log_info('100G physical port was configured as 40G breakout mode')
        return True
    return False

def make_interface_from_phy_port(obj, mode = None, speed = None):
    npu = obj.get_attr_data('npu-id')
    hw_port_id = obj.get_attr_data('hardware-port-id')

    l = []
    elem = cps_object.CPSObject(module=get_hw_key(), data={
        'npu-id': npu,
        'hw-port': hw_port_id
    })
    cps.get([elem.get()], l)
    if len(l) == 0:
        log_err('No object found for hardware port %d' % hw_port_id)
        log_err(str(elem.get()))
        raise Exception('Invalid port %d - no matching hardware-port' % hw_port_id)

    elem = cps_object.CPSObject(obj=l[0])

    chassis_id = default_chassis_id
    slot_id = default_slot_id
    fp_port_id = elem.get_attr_data('front-panel-port')
    lane = elem.get_attr_data('subport-id')
    fp_obj = cps_object.CPSObject(module=get_fp_key(), data={
        'front-panel-port': fp_port_id
    })
    l=[]
    cps.get([fp_obj.get()],l)
    if len(l) == 0:
        log_err('No object found for hardware port %d' % hw_port_id)
        log_err(str(elem.get()))
        raise Exception('Invalid port %d - no matching hardware-port' % hw_port_id)

    fp_obj = cps_object.CPSObject(obj=l[0])
    if mode is None:
        mode = elem.get_attr_data('fanout-mode')
    if speed is None:
        speed = _yang_auto_speed
    _subport = nas_comm.lane_to_subport(mode, lane, is_40g_mode_on_100g_port(fp_obj))
    if _subport is None:
        raise Exception('Failed to get subport id from br_mode %d lane %d' % (
                        mode, lane))
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
        'dell-if/if/interfaces/interface/speed':speed,
        'dell-if/if/interfaces/interface/duplex': nas_comm.yang.get_value('auto', 'yang-duplex'),
        'if/interfaces/interface/type':_g_if_eth_type})

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
    fp_port = fp_obj.get_attr_data('front-panel-port')

    l = []
    phy_port_list = []
    phy_port_cache = PhyPortCache()

    for i in ports:
        try:
            _port = cps_object.types.from_data('base-if-phy/front-panel-port/port', i)
            print "_port",_port 
            phy_port = phy_port_cache.get_by_hw_port(npu, _port)
            if phy_port is None:
                continue
            phy_port.add_attr('front-panel-number', fp_port)
            phy_port_list.append(phy_port)
        except:
            log_err("Physical port get failed")
            pass

    return phy_port_list

class npuPort:
    def __init__(self):
        self.npu = 0
        self.port = 0
def get_phy_port_from_if_index(if_index):
    l = nas_os_if_list(d={'if-index':if_index})
    if l is None:
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
    if port is None or not port:
        return None
    port_obj = cps_object.CPSObject(obj=port[0])

    hwport_details = hwPort()
    hwport_details.hw_port = port_obj.get_attr_data('hardware-port-id')
    hwport_details.npu = port_obj.get_attr_data('npu-id')
    return hwport_details

def get_fp_from_hw_port(npu, hw_port):
    hwport_list = nas_os_hwport_list(d={'npu-id':npu, 'hw-port':hw_port})
    if hwport_list is None:
        return None
    hwport_obj = cps_object.CPSObject(obj=hwport_list[0])
    fp_port = hwport_obj.get_attr_data('front-panel-port')
    return fp_port

def get_subport_id_from_hw_port(npu, hw_port):
    hwport_list = nas_os_hwport_list(d={'npu-id':npu, 'hw-port':hw_port})
    if hwport_list is None:
        return None
    hwport_obj = cps_object.CPSObject(obj=hwport_list[0])
    subport_id = hwport_obj.get_attr_data('subport-id')
    fp_port = hwport_obj.get_attr_data('front-panel-port')
    return fp_port, subport_id

def get_media_id_from_fp_port(fp_port):
    fp_port_list = nas_os_fp_list()
    media_id = None
    for fp in fp_port_list:
        obj = cps_object.CPSObject(obj=fp)
        try:
            if fp_port == obj.get_attr_data('front-panel-port'):
                media_id =  obj.get_attr_data('media-id')
                break
        except Exception:
            pass
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
from_yang_speed_map = {
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
    if if_name is None:
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
    intf.add_attr('if/interfaces/interface/type', _g_if_eth_type)
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

def get_lag_id_from_name(lag_name):
    idx = 0
    while idx < len(lag_name):
        if lag_name[idx].isdigit():
            break
        idx += 1
    if idx >= len(lag_name):
        lag_id = 0
    else:
        lag_id_str = lag_name[idx:]
        if lag_id_str.isdigit():
            lag_id = int(lag_id_str)
        else:
            lag_id = 0
    return lag_id


def get_media_id_from_if_index(if_index):
    phy_port = get_phy_port_from_if_index(if_index)
    if phy_port is None:
        log_err('not a physical port')
        return False
    hwp = get_hwport_from_phy_port(phy_port.npu, phy_port.port)
    fp_port =  get_fp_from_hw_port(phy_port.npu, hwp.hw_port)
    media_id = get_media_id_from_fp_port(fp_port)
    return media_id

def get_front_port_from_name(if_name, check_if = True):
    if check_if:
        if_index = name_to_ifindex(if_name)
        if if_index is None:
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
    if phy_port is None:
        log_err('no physical port found for ifindex %d' % if_index)
        return None
    hwp = get_hwport_from_phy_port(phy_port.npu, phy_port.port)
    if hwp is None:
        log_err('no hw port found for npu %d port %d' % (phy_port.npu, phy_port.port))
        return None
    fp_port = get_fp_from_hw_port(phy_port.npu, hwp.hw_port)
    if fp_port is None:
        log_err('no front panel port found for hw port %d' % hwp.hw_port)
        return None
    return (get_port_breakoutCap_currentMode_mode(fp_port))

def get_pg_br_cap_list(pg_id):
    pg_list = nas_os_pg_state_list(d={'dell-pg/port-groups-state/port-group-state/id':pg_id})
    pg_port_obj = cps_object.CPSObject(obj=pg_list[0])
    br_cap_list = pg_port_obj.get_attr_data('br-cap')
    return br_cap_list

def get_port_breakoutCap_currentMode_mode(fp_port):
    fp_utils.init()
    fp_details = fp.find_front_panel_port(fp_port)
    if fp_details.is_pg_member() is True:
        pg_name_list=fp.get_port_group_list()
        pg_id = fp_details.get_port_group_id()	
        pg = pg_name_list[pg_id]
        print "Port-group profile type:", pg.get_profile_type()
        br_cap_list = get_pg_br_cap_list(pg_id) 
        pg_data_list = nas_os_pg_list(d={'dell-pg/port-groups/port-group/id':pg_id})
        obj=cps_object.CPSObject(obj=pg_data_list[0])
        current_mode = obj.get_attr_data(nas_pg.pg_attr('breakout-mode'))
        print "Current breakout-mode:", current_mode
        current_speed = obj.get_attr_data(nas_pg.pg_attr('port-speed'))
        print "Current port-speed:", current_speed
    else:            
        fp_list = nas_os_fp_list(d={'front-panel-port':fp_port})
        if fp_list is None or len(fp_list) == 0:
                log_err('failed to get object of front panel port %d' % fp_port)
                return None
        fp_port_obj = cps_object.CPSObject(obj=fp_list[0])
        br_cap_list = fp_port_obj.get_attr_data('br-cap')
        current_mode = fp_port_obj.get_attr_data('breakout-mode')
    breakout_cap = []

    for cap_items in br_cap_list.values():
        for cap_item_key in cap_items.keys():
            if 'breakout-mode' in cap_item_key:
                br_mode = cap_items[cap_item_key]
                if br_mode not in breakout_cap:
                    breakout_cap.append(br_mode)
                break

    return (breakout_cap,current_mode)

def get_cps_attr(obj,attr_name):
    attr = None
    try:
        attr = obj.get_attr_data(attr_name)
    except:
        return None
    return attr

