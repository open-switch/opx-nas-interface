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


import xml.etree.ElementTree as ET

NPU = {}


class Port:

    def __init__(self, npu, name, port, media_id, mac_offset):
        self.npu = npu
        self.name = name
        self.id = port
        self.media_id = media_id
        self.mac_offset = mac_offset
        self.hwports = []

    def add_hwport(self, hwport):
        self.hwports.append(int(hwport))

    def control_port(self):
        return min(self.hwports)

    def lane(self, hwport):
        return self.hwports.index(hwport)

    def show(self):
        print "Name: " + self.name
        print "ID:   " + self.id
        print "Media:" + self.media_id
        print "MAC Offset:" + self.mac_offset
        print self.hwports
        print "Ctrl Port: " + str(self.control_port())


class Npu:

    def __init__(self, npu):
        self.ports = {}
        self.id = npu

    def get_port(self, port):
        if port not in self.ports:
            return None
        return self.ports[port]

    def create_port(self, name, port, media_id, mac_offset):
        if port not in self.ports:
            self.ports[port] = Port(
                self.id,
                name,
                port,
                media_id,
                mac_offset)
        return self.ports[port]

    def port_count(self):
        return len(self.ports)

    def show(self):
        print "NPU: " + self.id
        for p in self.ports:
            self.ports[p].show()

    def port_from_hwport(self, hwport):
        for k in self.ports:
            if hwport in self.ports[k].hwports:
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

#TODO remove it  later
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



def process_file(root):
    npu = None
    port = None
    for i in root.findall('front-panel-port'):
        _port = int(i.get('id'))
        _name = i.get('name')
        _npu = int(i.get('npu'))
        _phy_media = int(i.get('phy_media'))
        _mac_offset = int(i.get('mac_offset'))
        if npu is None:
            npu = get_npu(_npu)

        if port is None:
            port = npu.get_port(_port)
            if port is None:
                port = npu.create_port(_name, _port, _phy_media, _mac_offset)

        for e in i:
            _hwport = int(e.get('hwport'))
            port.add_hwport(_hwport)
        npu = None
        port = None


def init(filename):
    cfg = ET.parse(filename)
    root = cfg.getroot()
    process_file(root)
