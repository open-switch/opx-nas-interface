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
import cps_utils

import nas_os_if_utils as nas_if
import nas_front_panel_map as fp
import cps_object
import event_log as ev
import time
import bytearray_utils as ba

cps_utils.add_attr_type('base-pas/media/port', 'uint8_t')
cps_utils.add_attr_type('base-pas/media/slot', 'uint8_t')
cps_utils.add_attr_type('base-pas/media/present', 'uint8_t')
cps_utils.add_attr_type('base-pas/media/type', 'uint32_t')

def get_all_media_info():
    obj = cps_object.CPSObject(module='base-pas/media', qual='observed')
    media_list = []
    cps.get([obj.get()], media_list)
    return media_list

def get_media_info(media_id):
    obj = cps_object.CPSObject(
        module='base-pas/media',
        qual='observed',
        data={'slot': 1,
              'port': media_id})
    media_list = []
    cps.get([obj.get()], media_list)
    return media_list

def get_media_channel_info(media_id):
    obj = cps_object.CPSObject(
        module='base-pas/media-channel',
        qual='observed',
        data={'slot': 1,
              'port': media_id})
    media_channel_list = []
    cps.get([obj.get()], media_channel_list)
    return media_channel_list

def media_led_set(slot, media_id, channel, speed):
    media_channel = cps_object.CPSObject(module='base-pas/media-channel', qual='target', data=
                                         {'slot': slot, 'port': media_id, 'channel': channel, 'speed': speed})
    ch = {'operation': 'set', 'change': media_channel.get()}
    nas_if.log_info("set speed for media Id : "+str(media_id)+" channel "+str(channel)+" speed "+str(speed))
    cps.transaction([ch])

def media_transceiver_set(slot, media_id, channel, enable):
    media_channel = cps_object.CPSObject(module='base-pas/media-channel', qual='target', data=
                                         {'slot': slot, 'port': media_id, 'state': enable})
    if channel != None:
        media_channel.add_attr('channel', channel)
    ch = {'operation': 'set', 'change': media_channel.get()}
    cps.transaction([ch])

def led_control_get():
    media = cps_object.CPSObject(module='base-pas/media-config',qual='observed',data={'slot':1})
    l = []
    cps.get([media.get()],l)
    media_config = cps_object.CPSObject(obj=l[0])
    led_control = media_config.get_attr_data('led-control')
    return led_control
