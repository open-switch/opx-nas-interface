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
import cps_object
import cps_utils
import nas_os_if_utils as nas_if
import bytearray_utils as ba
import event_log as ev


import xml.etree.ElementTree as ET
import nas_phy_media as media

import time

_media_key = cps.key_from_name('observed', 'base-media/media-info')

MEDIA_LIST = {}

class PHY_Media:
    def  __init__(self, media_type, media_str, speed, autoneg, duplex):
        self.name = media_str
        self.id = media_type
        self.speed = speed
        self.autoneg = autoneg
        self.duplex = duplex
        self.supported_speed = []

    def add_supported_speed(self, speed):
        self.supported_speed.append(int(speed))

    def show(self):
        nas_if.log_info("Name "  + str(self.name))
        if self.speed != None:
            nas_if.log_info("speed "  + str(self.speed))
        if self.autoneg != None:
            nas_if.log_info("autoneg "  + str(self.autoneg))
        if self.duplex != None:
            nas_if.log_info("duplex "  + str(self.duplex))
        nas_if.log_info("supported speed :" + str(self.supported_speed))


def get_media_info(media_type):
        if media_type not in MEDIA_LIST:
            return None
        return MEDIA_LIST[media_type]
def add_media_info(media_type, media_str, speed, autoneg, duplex):
    if media_type not in MEDIA_LIST:
        MEDIA_LIST[media_type] = PHY_Media(media_type, media_str, speed, autoneg, duplex)
    return MEDIA_LIST[media_type]

def process_file(root):
    for i in root.findall('media-type'):
        _media_str = i.get('id')
        _media_type = media.get_media_type_from_str(_media_str)
        _speed = None
        _autoneg = None
        _duplex = "full"  # Default value for duplex state is always full state
        _speed_str = i.get('speed')
        _autoneg_str = i.get('autoneg')
        _duplex_str = i.get('duplex')
        if _speed_str != 'None':
            _speed = int(_speed_str)
        if _autoneg_str != 'None':
            _autoneg = _autoneg_str
        if _duplex_str != 'None':
            _duplex = _duplex_str
        if _media_type not in MEDIA_LIST:
            media_info = add_media_info(_media_type, _media_str, _speed, _autoneg, _duplex)
            if media_info is not None:
                for e in i:
                    _supported_speed = int(e.get('supported-speed'))
                    media_info.add_supported_speed(_supported_speed)
                media_info.show()


def init(filename):
    cfg = ET.parse(filename)
    root = cfg.getroot()
    process_file(root)

def _media_attr(attr):
    return 'base-media/media-info/' + t


def _gen_media_list(media_obj, resp):

    _media_type = media_obj.get_attr_data('media-type')

    # Fetch media-type information
    _m_info = get_media_info(_media_type)
    _m_info.show()
    if _m_info == None:
        nas_if.log_err("Null Media info")
        return
    _data = {}
    _data['media-type'] = _media_type
    if _m_info.speed != None:
        _data['speed'] = nas_if.to_yang_speed(_m_info.speed)
    if _m_info.autoneg != None:
        _data['autoneg'] = nas_if.to_yang_autoneg(_m_info.autoneg)
    if _m_info.duplex != None:
        _data['duplex'] = nas_if.to_yang_duplex(_m_info.duplex)

    elem = cps_object.CPSObject(module='base-media/media-info', data= _data)
    resp.append(elem.get())


def get_cb(methods, params):
    _media_obj = cps_object.CPSObject(obj=params['filter'])
    resp = params['list']

    if _media_obj.get_key() == _media_key:
        _gen_media_list(_media_obj, resp)
    else:
        return False
    return True

if __name__ == '__main__':
    init('/etc/opx/phy_media_default_npu_setting.xml')

    handle = cps.obj_init()

    d = {}
    d['get'] = get_cb

    cps.obj_register(handle, _media_key, d)
    while True:
        time.sleep(1)



