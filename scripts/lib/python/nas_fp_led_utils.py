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
import nas_if_config_obj as if_config
import nas_os_if_utils as nas_if


def identification_led_control_get():
    media = cps_object.CPSObject(module='base-pas/media-config',qual='observed',data={'slot':1})
    l = []
    try:
        if cps.get([media.get()],l):
            media_config = cps_object.CPSObject(obj=l[0])
            led_control = media_config.get_attr_data('identification-led-control')
            return led_control
    except:
        nas_if.log_err("Failed to get media config object")
        pass
    # return None when cps.get fails to get the media config object
    return None

def identification_led_set(name, state):
    config = if_config.if_config_get(name)
    port = config.get_fp_port()
    led_name = "Port " + str(port) + " Beacon"
    fp_beacon_led_obj =  cps_object.CPSObject(module='base-pas/led',qual='target',data=
                                              {'slot':1, 'entity-type':3, 'name':led_name,'on':state})
    ch = {'operation': 'set', 'change': fp_beacon_led_obj.get()}
    return cps.transaction([ch])

