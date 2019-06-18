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


import xml.etree.ElementTree as ET
import copy
import nas_common_header as nas_comm

NPU = {}

portProfile_list = {}
_def_port_profile = None
supported_npu_speeds = []

# speed yang enum
def verify_npu_supported_speed(speed):
    # suported speed is Null then return true else verify the speed
    if len(supported_npu_speeds) == 0 or speed in supported_npu_speeds:
        return True
    else:
        return False

def get_fc_speed_frm_npu_speed(_yang_speed):
    if _yang_speed not in nas_comm.yang.get_tbl('eth-to-fc-speed'):
        return 0
    return nas_comm.yang.get_value(_yang_speed, 'eth-to-fc-speed')

def get_phy_npu_port_speed(breakout, fp_speed):
    if breakout is nas_comm.yang.get_value('disabled', 'yang-breakout-mode') and fp_speed is 0:
        return 0
    elif breakout is not nas_comm.yang.get_value('disabled', 'yang-breakout-mode') and fp_speed is not 0:
        if breakout not in nas_comm.yang.get_tbl('breakout-to-phy-fp-port-count'):
            print 'invalid breakout mode %d' % breakout
            return 0 # error case
        pp_count,fp_count = nas_comm.yang.get_value(breakout, 'breakout-to-phy-fp-port-count')
        speed = 0
        if pp_count is not 0:
            speed = ((fp_speed * fp_count) / pp_count)
        return (nas_comm.yang.get_value(speed, 'mbps-to-yang-speed'))
    return None

def get_phy_port_speed(breakout, phy_mode, fp_speed):
    npu_port_speed = get_phy_npu_port_speed(breakout, fp_speed)
    if npu_port_speed == 0:
        return 0
    if phy_mode == nas_comm.yang.get_value('fc', 'yang-phy-mode'):
        if get_fc_speed_frm_npu_speed(npu_port_speed) == 0:
            return 0;
    else:
        return npu_port_speed

def get_max_port_speed(port):
    g = None

    if port.is_pg_member() is True:
        pg_id = port.get_port_group_id()
        pg_list = get_port_group_list()
        if pg_id in pg_list:
            g = pg_list[pg_id]

    if port.is_hg_member() is True:
        hg_id = port.get_hybrid_group_id()
        hg_list = get_hybrid_group_list()
        if hg_id in hg_list:
            g = hg_list[hg_id]

    if g is None:
        fp_speed = port.hwp_speed * len(port.hwports)
        phy_mode = port.phy_mode
    else:
        fp_speed = (g.hwp_speed * len(g.hw_ports)) / len(g.fp_ports)
        phy_mode = g.phy_mode

    fp_speed = nas_comm.yang.get_value(fp_speed, 'mbps-to-yang-speed')
    if phy_mode == nas_comm.yang.get_value('fc', 'yang-phy-mode'):
        fp_speed = get_fc_speed_frm_npu_speed(fp_speed)
    return fp_speed

# Class PortProfile contains template for port capabilities such as supported breakout modes,
# Physical mode, and supported Hw port speed. It also contains platforms specifc
# default values and current value.
# This class is inherited by front panel port and port group objects based on the profile type
# it belongs to.

class PortProfile(object):
    def __init__(self, profile=None, supported_autoneg=None, name=None, breakout_caps=None, speed_caps=None, phy_mode_caps=None, def_speed=None, def_br=None, def_phy=None, description="None"):
        if profile != None and isinstance(profile, PortProfile):
            self.copy(profile)
        else:
            self.profile_name = name
            # supported capabilities options
            self.breakout_caps = breakout_caps[:]
            self.hwport_speed_caps = speed_caps[:]
            self.phy_mode_caps = phy_mode_caps[:]
            # default values
            self.def_hwport_speed = def_speed
            self.def_breakout = def_br
            if def_phy is None:
                self.def_phy_mode = nas_comm.yang.get_value('ether', 'yang-phy-mode')
            else:
                self.def_phy_mode = def_phy

            #Setup supported autoneg
            self.supported_autoneg = nas_comm.yang.get_value('both-supported', 'yang-autoneg-support')
            if supported_autoneg is not None:
                self.supported_autoneg = supported_autoneg

            # current configuration
            self.breakout =   self.def_breakout
            self.hwp_speed =  self.def_hwport_speed
            self.phy_mode =   self.def_phy_mode
            self.phy_port_speed =  None
            self.def_phy_port_speed = None
            self.fc_caps = []
            self.hybrid_group_profile_mode = None
            self.hybrid_group_default_profile_mode = None
            self.supported_hybrid_group_profile_modes = []
            self.description = description

    def copy(self,profile):
        self.profile_name = profile.profile_name
        self.supported_autoneg = profile.supported_autoneg
        self.breakout_caps = profile.breakout_caps[:]
        self.hwport_speed_caps = profile.hwport_speed_caps[:]
        self.phy_mode_caps = profile.phy_mode_caps[:]
        self.def_hwport_speed = profile.def_hwport_speed
        self.def_breakout = profile.def_breakout
        self.def_phy_mode = profile.def_phy_mode
        self.breakout = profile.def_breakout
        self.hwp_speed = profile.def_hwport_speed
        self.phy_mode = nas_comm.yang.get_value('ether', 'yang-phy-mode')
        self.phy_port_speed =  None
        self.def_phy_port_speed = None
        self.fc_caps = profile.fc_caps[:]

    def apply(self,profile):
        self.profile_name = profile.profile_name
        self.supported_autoneg = profile.supported_autoneg
        self.breakout_caps = profile.breakout_caps[:]
        self.hwport_speed_caps = profile.hwport_speed_caps[:]
        self.phy_mode_caps = profile.phy_mode_caps[:]
        self.def_hwport_speed = profile.def_hwport_speed
        self.def_breakout = profile.def_breakout
        self.def_phy_mode = profile.def_phy_mode
        self.hwp_speed = profile.def_hwport_speed
        self.phy_mode = nas_comm.yang.get_value('ether', 'yang-phy-mode')
        self.def_phy_port_speed = None
        self.fc_caps = profile.fc_caps[:]

    def get_profile_type(self):
        return self.profile_name

    def get_breakout_caps(self):
        return self.breakout_caps[:]

    def set_breakout_caps(self, br_caps):
        self.breakout_caps = br_caps[:]

    def get_hwport_speed_caps(self):
        return self.hwport_speed_caps[:]

    def get_fc_caps(self):
        return self.fc_caps[:]

    def set_fc_caps(self, fc_caps):
        self.fc_caps = fc_caps[:]

    def get_def_hwport_speed(self):
        return self.def_hwport_speed

    def get_def_breakout(self):
        return self.def_breakout

    def get_def_phy_mode(self):
        return self.def_phy_mode

    def get_supported_phy_mode_caps(self):
        return self.phy_mode_caps[:]

    def is_fc_supported(self):
        return nas_comm.yang.get_value('fc', 'yang-phy-mode') in self.phy_mode_caps

    def get_hwp_speed(self):
        return self.hwp_speed

    def get_breakout_mode(self):
        return self.breakout

    def set_breakout_mode(self, breakout):
        self.breakout = breakout

    def set_hwp_speed(self, speed):
        self.hwp_speed = speed

    def get_phy_mode(self):
        return self.phy_mode

    def set_phy_mode(self, mode):
        self.phy_mode = mode

    def set_port_speed(self, speed):
        self.phy_port_speed = speed

    def get_port_speed(self):
        return self.phy_port_speed

    def get_supported_autoneg(self):
        return self.supported_autoneg

    def get_default_phy_port_speed(self):
        return self.def_phy_port_speed

    def get_supported_hybrid_group_profile_modes(self):
        return self.supported_hybrid_group_profile_modes

    def set_supported_hybrid_group_profile_modes(self, supported_hybrid_group_profile_modes):
        self.supported_hybrid_group_profile_modes = supported_hybrid_group_profile_modes

    def get_hybrid_group_profile_mode(self):
        return self.hybrid_group_profile_mode

    def set_hybrid_group_profile_mode(self, hybrid_group_profile_mode):
        self.hybrid_group_profile_mode = hybrid_group_profile_mode

    def get_hybrid_group_default_profile_mode(self):
        return self.hybrid_group_default_profile_mode

    def set_hybrid_group_default_profile_mode(self, hybrid_group_default_profile_mode):
        self.hybrid_group_default_profile_mode = hybrid_group_default_profile_mode

    def show(self):
        print('Profile name %s' % self.profile_name)
        print('Phy modes   %s, ' % str((self.phy_mode_caps)))
        print('Breakout capabilities' + str(self.breakout_caps))
        print('speed capabilities' + str(self.hwport_speed_caps))
        print('default speed ' + str(self.def_hwport_speed))
        print('default breakout ' + str(self.def_breakout))
        print('default Phy mode ' + str(self.def_phy_mode))
        print('Phy port speed ' + str(self.phy_port_speed))
        print('Default phy port speed' +str(self.def_phy_port_speed))
        print('----------------------------------------')


def default_port_profile_init():
    speed_caps = [10000]
    supported_autoneg = nas_comm.yang.get_value('both-supported', 'yang-autoneg-support')
    breakout_caps = [nas_comm.yang.get_value('1x1', 'yang-breakout-mode')]
    phy_mode_caps = [nas_comm.yang.get_value('ether', 'yang-phy-mode')]
    def_speed = 10000
    def_br = nas_comm.yang.get_value('1x1', 'yang-breakout-mode')
    default_port_profile = PortProfile(None, supported_autoneg, 'base_default', breakout_caps, speed_caps, phy_mode_caps, def_speed, def_br)
    return default_port_profile

def show_profile(pr):
    print('Profile name %s' % pr.name)
    print('Phy modes   %s, ' % str((pr.phy_mode_caps)))
    print('Breakout capabilities' + str(pr.breakout_caps))
    print('speed capabilities' + str(pr.hwport_speeds))
    print('default speed ' + str(pr.def_hwport_speed))
    print('default breakout ' + str(pr.def_breakout))
    print('default Phy mode ' + str(pr.def_phy_mode))
    print('----------------------------------------')
def process_Profile_fc_cap_cfg(fc_cap_node):
    _fc_cap = []
    for cap in fc_cap_node.findall('cap'):
        fc_br_cap = {}
        _br_str = cap.get('breakout')
        _hwp_speed = cap.get('hwport_speed')
        _phy_fc_speed = cap.get('phy_fc_speed')
        fc_br_cap = {'breakout'    : nas_comm.yang.get_value(_br_str, 'yang-breakout-mode'),
                     'hwp_speed'   : int(_hwp_speed),
                     'phy_fc_speed': int(_phy_fc_speed)
                    }
        _fc_cap.append(fc_br_cap)
    return _fc_cap

def process_portProfile_cfg(root):
    global _def_port_profile
    global portProfile_list
    global supported_npu_speeds
    g_default = root.find('global')
    if g_default is None:
        _def_port_profile = default_port_profile_init()
        return
    _g_fc = g_default.get('fc_enabled')
    def_profile = g_default.get('default_profile')
    for profile in g_default:
        if profile.tag == 'hybrid_profile':
            continue
        if profile.tag == 'npu-supported-speeds':
            for speed in profile.findall('Supported_speed'):
                _yang_speed = nas_comm.yang.get_value(int(speed.get('value')), 'mbps-to-yang-speed')
                supported_npu_speeds.append(_yang_speed)
            continue
        name  = profile.get('name')
        phy_mode_caps = [nas_comm.yang.get_value('ether', 'yang-phy-mode')]
        _fc = profile.get('fc_enabled')
        if _fc is None:
            _fc = _g_fc
        if _fc == 'true':
            phy_mode_caps.append(nas_comm.yang.get_value('fc', 'yang-phy-mode'))

        def_hwport_speed = int(profile.get('default_hwport_speed'))

        supported_autoneg = profile.get('supported_autoneg')
        if supported_autoneg is not None and \
           supported_autoneg in nas_comm.yang.get_tbl('yang-autoneg-support'):
            supported_autoneg = nas_comm.yang.get_value(supported_autoneg, 'yang-autoneg-support')
        else:
            supported_autoneg = nas_comm.yang.get_value('both-supported', 'yang-autoneg-support')

        def_breakout = nas_comm.yang.get_value(profile.get('default_breakout'), 'yang-breakout-mode')
        br_cap = []
        hwp_speed_cap = []
        ds = "None"

        # Read profile description
        for dscr in profile.findall('profile_description'):
            ds = dscr.get('value')
        # Read breakout capabilities
        for br in profile.findall('breakout_cap'):
            _br_str = br.get('value')
            br_cap.append(nas_comm.yang.get_value(_br_str, 'yang-breakout-mode'))
        # Read HWport Lane speed capabilities
        for speed in profile.findall('hwport_speed_cap'):
            hwp_speed_cap.append(int(speed.get('value')))
        # Read FC br capability if present otherwise it is based on hwport speed and breakout cap
        fc_caps = []
        _fc_cap_node = profile.find('fc_cap')
        if _fc_cap_node is not None:
            fc_caps = process_Profile_fc_cap_cfg(_fc_cap_node)

        portProfile_list[name] = PortProfile(None, supported_autoneg, name, br_cap, hwp_speed_cap,phy_mode_caps,
                                             def_hwport_speed,def_breakout, description=ds)
        portProfile_list[name].set_fc_caps(fc_caps)
        fc_caps = []
    if def_profile != None and portProfile_list[def_profile] != None:
        _def_port_profile = portProfile_list[def_profile]
    else:
        _def_port_profile = default_port_profile_init()


def get_default_port_profile():
    return _def_port_profile

def get_port_profile(p_name):
    if p_name != None:
        if p_name in portProfile_list:
            return portProfile_list[p_name]
    return get_default_port_profile()

def get_hybrid_profile(hybrid_profile_name):
    if hybrid_profile_name != None:
        if hybrid_profile_name in hybrid_profile_list:
            return hybrid_profile_list[hybrid_profile_name]
    return None

class Port(PortProfile):

    def __init__(self, npu, name, port, media_id, mac_offset, port_profile, hybrid_profile=None):
        super(Port, self).__init__(port_profile)
        self.npu = npu
        self.name = name
        self.id = port
        self.media_id = media_id
        self.mac_offset = mac_offset
        self.port_group_id = None
        self.hybrid_group_id = None
        self.hwports = []
        self.hybrid_profile = hybrid_profile

    def get_hwports(self):
        if self.hwports is None:
            return None
        return self.hwports[:]

    def add_hwport(self, hwport):
        self.hwports.append(int(hwport))

    def set_hwports(self, hwports):
        if hwports is None:
            self.hwports = []
        else:
            self.hwports = hwports[:]

    def get_port_group_id(self):
        return self.port_group_id

    def set_port_group_id(self, pg_id):
        self.port_group_id = pg_id

    def get_hybrid_group_id(self):
        return self.hybrid_group_id

    def set_hybrid_group_id(self, hg_id):
        self.hybrid_group_id = hg_id

    def get_hybrid_profile(self):
        return self.hybrid_profile

    def set_hybrid_profile(self, hybrid_profile):
        self.hybrid_profile = hybrid_profile

    def is_hg_member(self):
        return not (self.hybrid_group_id is None)

    def is_pg_member(self):
        return not (self.port_group_id is None)

    def control_port(self):
        if self.hwports is None or len(self.hwports) == 0:
            return None
        else:
            return self.hwports[0]

    def lane(self, hwport):
        return self.hwports.index(hwport)

    def set_phy_port_speed(self):
        self.phy_port_speed = get_phy_port_speed(self.breakout, self.phy_mode,
                                    self.hwp_speed * len(self.hwports))

    def set_default_phy_port_speed(self):
        self.def_phy_port_speed = get_phy_port_speed(self.def_breakout, self.def_phy_mode,
                                    self.def_hwport_speed * len(self.hwports))

    def apply_port_profile(self, port_profile):
        self.apply(port_profile)
        self.set_default_phy_port_speed()
        return True

    def show(self):
        print "Name: " + self.name
        print "ID:   " + str(self.id)
        print "Media: " + str(self.media_id)
        print "MAC Offset:" + str(self.mac_offset)
        print self.hwports
        print "Ctrl Port: " + str(self.control_port())
        print " Profile Info :--"
        super(Port, self).show()

class Npu:

    def __init__(self, npu):
        self.ports = {}
        self.id = npu

    def get_port(self, port):
        if port not in self.ports:
            return None
        return self.ports[port]

    def create_port(self, name, port, media_id, mac_offset, port_profile, hybrid_profile=None):
        if port not in self.ports:
            self.ports[port] = Port(
                self.id,
                name,
                port,
                media_id,
                mac_offset,
                port_profile,
                hybrid_profile)
        return self.ports[port]

    def port_count(self):
        return len(self.ports)

    def show(self):
        print "NPU: " + self.id
        for p in self.ports:
            self.ports[p].show()

    def port_from_hwport(self, hwport):
        for k in self.ports:
            if self.ports[k].hwports != None and hwport in self.ports[k].hwports:
                return self.ports[k]

    def port_from_media_id(self, media_id):
        for k in self.ports:
            if media_id == self.ports[k].media_id:
                return self.ports[k]


def get_npu(npu):
    if npu not in NPU:
        NPU[npu] = Npu(npu)
    return NPU[npu]


def total_ports():
    _len = 0
    for i in NPU:
        _len += NPU[i].port_count()

    return _len


def get_npu_list():
    l = []
    for i in NPU:
        l.append(NPU[i])
    return l


class NasPortDetail:

    def __init__(self):
        self.name = ""

def find_front_panel_port(port):
    for (npu_id, npu) in NPU.items():
        p = npu.get_port(port)
        if p !=  None:
            return p
    return None

def find_port_by_hwport(npu, hwport):
    npu = get_npu(npu)
    if npu is None:
        return None
    p = npu.port_from_hwport(hwport)
    pd = NasPortDetail()

    pd.name = p.name
    pd.lane = p.lane(hwport)
    pd.port = p.id
    pd.hwport = hwport
    pd.media_id = p.media_id
    pd.port_group_id = p.port_group_id
    return pd


def find_port_by_media_id(npu, media_id):
    npu = get_npu(npu)
    if npu is None:
        return None
    p = npu.port_from_media_id(media_id)
    pd = NasPortDetail()

    pd.name = p.name
    pd.port = p.id
    pd.hwports = p.hwports
    return pd


def find_port_by_name_and_lane(name, lane):
    for i in NPU:
        for p in NPU[i].ports:
            if NPU[i].ports[p].name == name:
                p = NPU[i].ports[p]

                pd = NasPortDetail()
                pd.name = p.name
                pd.lane = lane
                pd.port = p.id
                pd.hwport = p.hwports[lane]
                return pd
    return None

def find_mac_offset_by_name_lane(name, lane):
    for i in NPU:
        for p in NPU[i].ports:
            if NPU[i].ports[p].name == name:
                _mac_offset =  NPU[i].ports[p].mac_offset + lane
    return _mac_offset

def is_qsfp28_cap_supported(fp_port_id):
    fp_details = find_front_panel_port(fp_port_id)
    def_phy_speed = fp_details.get_default_phy_port_speed()
    if def_phy_speed == nas_comm.yang.get_value('100g', 'yang-speed'):
        return True
    return False

def set_fp_hybrid_group_id(port, hg_id):
    port_detail = find_front_panel_port(port)
    if port_detail != None:
        port_detail.set_hybrid_group_id(hg_id)

def set_fp_port_group_id(port, pg_id):
    port_detail = find_front_panel_port(port)
    if port_detail != None:
        port_detail.set_port_group_id(pg_id)

class HybridProfile(object):
    def __init__(self, name, profile_modes):
        self.name = name
        self.profile_modes = profile_modes

    def get_port_profile(self, profile_mode):
        if profile_mode in self.profile_modes:
            return self.profile_modes[profile_mode][0]
        return None

# Port Group details is read from the platform specific config file.
# Properties in the port group is common to all front panel ports in a
# port group.
class PortGroup(PortProfile):
    def __init__(self, name, port_profile, fp_ports, hw_ports, is_hybrid=False, hybrid_group_default_profile_mode=None, supported_hybrid_group_profile_modes=[]):
        super(PortGroup, self).__init__(port_profile)
        self.name = name
        self.fp_ports = fp_ports[:]
        self.hw_ports = hw_ports[:]
        self.set_phy_port_speed()
        self.is_hybrid = is_hybrid
        self.set_hybrid_group_default_profile_mode(hybrid_group_default_profile_mode)
        self.set_hybrid_group_profile_mode(hybrid_group_default_profile_mode)
        self.set_supported_hybrid_group_profile_modes(supported_hybrid_group_profile_modes)

    def set_phy_port_speed(self):
        self.phy_port_speed = get_phy_port_speed(self.breakout, self.phy_mode,
                                    (self.hwp_speed * len(self.hw_ports)) / len(self.fp_ports))

    def set_default_phy_port_speed(self):
        self.def_phy_port_speed = get_phy_port_speed(self.def_breakout, self.def_phy_mode,
                                    self.def_hwport_speed * len(self.hw_ports))

    def get_fp_ports(self):
        return self.fp_ports[:]

    def get_hw_ports(self):
        return self.hw_ports[:]
    def get_lane(self,hwport):
        return self.hw_ports.index(hwport)
    def apply_port_profile(self, port_profile):
        self.apply(port_profile)
        self.set_default_phy_port_speed()
        return True

    def show(self):
        print "Name: " + self.name
        print ('Front Panel Port List %s' % str(self.fp_ports))
        print ('Hardware Port List %s' % str(self.hw_ports))
        super(PortGroup, self).show()


def process_frontPanelPort_cfg(root):
    npu = None
    port = None
    for i in root.findall('front-panel-port'):
        npu = None
        port = None
        _port = int(i.get('id'))
        _name = i.get('name')
        _npu = int(i.get('npu'))
        _phy_media = int(i.get('phy_media'))
        _mac_offset = int(i.get('mac_offset'))
        _profile_name = i.get('profile_type')
        port_profile = get_port_profile(_profile_name)

        if npu is None:
            npu = get_npu(_npu)

        if port is None:
            port = npu.get_port(_port)
            if port is None:
                port = npu.create_port(_name, _port, _phy_media, _mac_offset, port_profile)

        for e in i:
            _hwport = int(e.get('hwport'))
            port.add_hwport(_hwport)
        # Set physical port speed based on the breakout mode, phy mode and per pport hwp ports and hwp speed
        port.set_phy_port_speed()
        port.set_default_phy_port_speed()

port_group_list = {}

def get_port_group_list():
    return port_group_list

def process_portGroup_cfg(root):
    global port_group_list
    # Read all port group configuration
    _port_group = root.find('port-group')
    if _port_group is None:
        # the platform does not have port groups
        return
    for pg in _port_group.findall('pg'):
        _profile_name = pg.get('profile_type')
        port_profile = get_port_profile(_profile_name)
        name = pg.get('name')
        fp_ports = []
        hw_ports = []
        for e in pg:
            if e.get('fpport') != None:
                fp_port = int(e.get('fpport'))
                set_fp_port_group_id(fp_port, name)
                fp_ports.append(fp_port)
            if e.get('hwport') != None:
                hw_ports.append(int(e.get('hwport')))
        port_group_list[name] = PortGroup(name, port_profile, fp_ports, hw_ports)
        port_group_list[name].set_phy_port_speed()
        port_group_list[name].set_default_phy_port_speed()

hybrid_group_list = {}

def get_hybrid_group_list():
    return hybrid_group_list

def process_hybridGroup_cfg(root):
    global hybrid_group_list
    # Read all hybrid group configuration
    _hybrid_group = root.find('hybrid-group')
    if _hybrid_group is None:
        # the platform does not have hybrid groups
        return
    for hg in _hybrid_group.findall('hg'):
        _profile_name = hg.get('profile_type')
        port_profile = get_port_profile(_profile_name)
        profile_mode = hg.get('profile_mode')
        name = hg.get('name')
        fp_ports = []
        hw_ports = []
        supported_profile_modes = []
        for e in hg:
            if e.get('fpport') != None:
                fp_port = int(e.get('fpport'))
                set_fp_hybrid_group_id(fp_port, name)
                fp_ports.append(fp_port)
            if e.get('hwport') != None:
                hw_ports.append(int(e.get('hwport')))
            if e.get('supported_profiles') != None:
                supported_profile_modes.append(e.get('supported_profiles'))
            if e.get('hybrid_profile_type') != None:
                port = find_front_panel_port(fp_port)
                port.set_hybrid_profile(get_hybrid_profile(e.get('hybrid_profile_type')))
        hybrid_group_list[name] = PortGroup(name, port_profile, fp_ports, hw_ports, True, profile_mode, supported_profile_modes)
        hybrid_group_list[name].set_phy_port_speed()
        hybrid_group_list[name].set_default_phy_port_speed()

hybrid_profile_list = {}

def get_hybrid_profile_list():
    return hybrid_profile_list

def process_hybridProfile_cfg(root):
    global hybrid_profile_list
    # Read all hybrid group configuration
    _global = root.find('global')
    if _global is None:
        return
    for hybrid_profile in _global.findall('hybrid_profile'):
        hybrid_profile_name = hybrid_profile.get('name')
        hybrid_profile_profile_modes = {}

        for e in hybrid_profile.findall('profile_mode'):
            profile_mode = e.get('name')
            hybrid_profile_profile_modes[profile_mode] = []
            if profile_mode != None:

                for f in e.findall('port_profile'):
                    port_profile = f.get('name')
                    hybrid_profile_profile_modes[profile_mode].append(port_profile)

        hybrid_profile_list[hybrid_profile_name] = HybridProfile(hybrid_profile_name, hybrid_profile_profile_modes)


def process_file(root):
    process_portProfile_cfg(root)
    process_frontPanelPort_cfg(root)
    process_hybridProfile_cfg(root)
    process_portGroup_cfg(root)
    process_hybridGroup_cfg(root)


def init(filename):
    cfg = ET.parse(filename)
    root = cfg.getroot()
    process_file(root)


def show_all_profiles(profile_list):
    for name in profile_list:
        pr = profile_list[name]
        print ("Profile name is %s" % name)
        pr.show()

def show_all_fp_ports():
    global NPU
    for i in NPU:
        for p in NPU[i].ports:
            if p != None:
                NPU[i].ports[p].show()

def show_all_port_groups():
    global port_group_list
    for name in port_group_list:
        pg = port_group_list[name]
        pg.show()

