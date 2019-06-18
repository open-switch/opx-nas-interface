#!/usr/bin/python
# Copyright (c) 2019 Dell Inc.
#
# Licensed under the Apache License, Version 2.0 (the 'License'); you may
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

import threading
import time
import logging
import cps
import cps_object
import nas_phy_media as media
import nas_front_panel_map as fp
import nas_os_if_utils as nas_if
import nas_if_handler_lib as if_lib
import nas_fp_port_utils as fp_utils
import nas_if_config_obj as if_config
import nas_common_header as nas_comm
import bytearray_utils as ba
from threading import Thread,Lock

global mutex
mutex = Lock()

def process_media_event(npu, port, pas_media_obj):

    try:
        media_id               = pas_media_obj.get_attr_data('base-pas/media/port')
        display_str            = pas_media_obj.get_attr_data('base-pas/media/display-string')
    except:
        nas_if.log_info("Media event without enough attributes to determine default media settings")
        return

    if_name = if_config.if_config_get_by_npu_port(npu, port)
    if if_name is None:
        nas_if.log_err("Interface Name: None")
        return False

    config = if_config.if_config_get(if_name)

    port = fp.find_front_panel_port(config.fp_port)
    pas_media_obj.add_attr('dell-if/if/interfaces/interface/speed', config.get_speed())
    pas_media_obj.add_attr('base-if-phy/physical/speed', fp.get_max_port_speed(port))
    config.set_media_obj(pas_media_obj)
    return True

def set_intf_request(pas_media_obj):
    try:
        media_id    = pas_media_obj.get_attr_data('port')
        display_str = pas_media_obj.get_attr_data('base-pas/media/display-string')
    except:
        nas_if.log_info("Media String is not present in the media event")
        return

    # fetch FP info from media ID
    o = cps_object.CPSObject(module='base-if-phy/front-panel-port', data={'base-if-phy/front-panel-port/media-id': media_id})
    l = []
    fp_utils.gen_fp_port_list(o, l)
    if len(l) == 0:
        nas_if.log_err("No such port found... for media  "+str(media_id))
        return

    #fetching 2 front panel port object from 1 phy media id for QSFP28-DD ports
    port_list = []
    for fp_obj in l:
        obj = cps_object.CPSObject(obj=fp_obj)
        if nas_comm.yang.get_value('fp_key', 'keys_id') == obj.get_key():
            port_list = port_list + nas_if.physical_ports_for_front_panel_port(obj)

    if len(port_list) == 0:
        nas_if.log_err("There are no physical ports for front panel port ")
        nas_if.log_err(str(l[0]))
        return

    # create interface set RPC obj for each phy port in the list and send it
    for p in port_list:
        npu = p.get_attr_data('npu-id')
        port = p.get_attr_data('port-id')
        process_media_event(npu, port, pas_media_obj)
        hwport_list = p.get_attr_data('hardware-port-list')

        nas_if.log_info("send if obj for media id set for phy port "+str(port))
        ifobj = cps_object.CPSObject(module='dell-base-if-cmn/if/interfaces/interface', data={
        'base-if-phy/if/interfaces/interface/npu-id': npu,
        'base-if-phy/if/interfaces/interface/port-id': port,
        'if/interfaces/interface/type':"ianaift:ethernetCsmacd"})
        if if_lib.set_media_setting(None, ifobj) == False:
            return
        ch = {'operation': 'set', 'change': ifobj.get()}
        cps.transaction([ch])
        if_name = if_config.if_config_get_by_npu_port(npu, port)
        config= if_config.if_config_get(if_name)
        speed = config.get_speed()
        if_details = nas_if.nas_os_if_list(d={'if/interfaces/interface/name':if_name})
        enable = ba.from_ba(if_details[0]['data']['if/interfaces/interface/enabled'],"uint64_t")
        for hwport in hwport_list:
            fp_details = fp.find_port_by_hwport(npu, hwport)
            if fp_details.port_group_id is None:
                _lane = fp_details.lane
            else:
                pg_list = fp.get_port_group_list()
                pg_obj = pg_list[fp_details.port_group_id]
                if ((pg_obj.get_profile_type()) == "ethernet_ddqsfp28"):
                    _lane = pg_obj.get_lane(hwport)
                else:
                    _lane = fp_details.lane
            if speed is not None:
                media.media_led_set(1, fp_details.media_id, _lane, speed)
            else:
                nas_if.log_err("Error speed not present")
            media.media_transceiver_set(1, fp_details.media_id, _lane, enable)

def monitor_media_events():
    handle = cps.event_connect()
    cps.event_register(handle, nas_comm.yang.get_value('media_key', 'keys_id'))
    while True:
        o = cps.event_wait(handle)
        obj = cps_object.CPSObject(obj=o)
        nas_if.log_info("Media event received")
        global mutex
        mutex.acquire()
        try:
            set_intf_request(obj)
        except:
            logging.exception('logical interface media event failed')
        mutex.release()

class mediaMonitorThread(threading.Thread):
    def __init__(self, threadID, name):
        threading.Thread.__init__(self)
        self.threadId = threadID
        self.name = name
    def run(self):
        monitor_media_events()
    def __str__(self):
        return ' %s %d ' %(self.name, self.threadID)

def subscribe_events():

    while cps.enabled(nas_comm.yang.get_value('media_key', 'keys_id')) == False:
        #wait for media and front panel port objects to be ready
        nas_if.log_err('Media or front panel port object is not yet ready')
        time.sleep(1)

    media_thread = mediaMonitorThread(2, "Media event Monitoring Thread")
    media_thread.daemon = True
    media_thread.start()
