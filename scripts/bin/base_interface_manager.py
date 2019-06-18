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
import nas_front_panel_map as fp
import nas_os_if_utils as nas_if
import nas_if_handler_lib as if_lib
import nas_mac_addr_utils as ma
import nas_phy_media as media
import nas_fp_led_utils as fp_led
import nas_if_lpbk as if_lpbk
import nas_if_vtep as if_vtep
import nas_if_vlan_subintf as if_vlan_subintf
import nas_if_bridge as if_bridge
import nas_if_macvlan as if_macvlan
import nas_if_config_obj as if_config
import nas_phy_port_utils as port_utils
import nas_fp_port_utils as fp_utils
import nas_port_group_utils as pg_utils
import nas_hybrid_group as hg_utils
import nas_common_header as nas_comm
import nas_media_monitor as media_monitor
import nas_interface_monitor as interface_monitor

import logging
import time
import copy
import systemd.daemon

port_list = None
fp_identification_led_control = None

def _get_if_media_obj(fp_port):
    ''' Method to get media object from PAS '''

    if fp_port is None:
        nas_if.log_err("Wrong fp port or None")
    try:
        media_id = _get_media_id_from_fp_port(fp_port)
        media_info = media.get_media_info(media_id)
        media_obj = cps_object.CPSObject(obj=media_info[0])
        return media_obj
    except:
        return None


def _if_init_config(obj, config):
    ''' Method to initialize interface config
    Below rule is applied to process speed, duplex, auto-neg and FEC attritube in cps object
    1. if attribute is in input cps object, use it and do not change anything
    2. else, if attribute is in config, add it to cps object
    3. else, add "auto" to cps object '''

    speed = nas_if.get_cps_attr(obj, nas_comm.yang.get_value('speed', 'attr_name'))
    _auto_speed = nas_comm.yang.get_value('auto', 'yang-speed')
    if speed is None:
        cfg_speed = config.get_speed()
        if cfg_speed is None:
            cfg_speed = _auto_speed
        obj.add_attr(nas_comm.yang.get_value('speed', 'attr_name'), cfg_speed)

    duplex = nas_if.get_cps_attr(obj, nas_comm.yang.get_value('duplex', 'attr_name'))
    _auto_duplex = nas_comm.yang.get_value('auto', 'yang-duplex')
    if duplex is None:
        cfg_duplex = config.get_duplex()
        if cfg_duplex is None:
            cfg_duplex = _auto_duplex
        obj.add_attr(nas_comm.yang.get_value('duplex', 'attr_name'), cfg_duplex)

    ng = nas_if.get_cps_attr(obj, nas_comm.yang.get_value('negotiation', 'attr_name'))
    _auto_negotiation = nas_comm.yang.get_value('auto', 'yang-autoneg')
    if ng is None:
        cfg_ng = config.get_negotiation()
        if cfg_ng is None:
            cfg_ng = _auto_negotiation
        obj.add_attr(nas_comm.yang.get_value('negotiation', 'attr_name'), cfg_ng)

    fec = nas_if.get_cps_attr(obj, nas_comm.yang.get_value('fec_mode', 'attr_name'))
    _auto_fec = nas_comm.yang.get_value('auto', 'yang-fec')
    if fec is None:
        cfg_fec = config.get_fec_mode()
        if cfg_fec is None:
            if_cfg_speed = config.get_cfg_speed()
            if if_cfg_speed != None and nas_comm.is_fec_supported(_auto_fec, if_cfg_speed):
                cfg_fec = _auto_fec
        if cfg_fec != None:
            obj.add_attr(nas_comm.yang.get_value('fec_mode', 'attr_name'), cfg_fec)


def _fp_identification_led_handle(cps_obj):
    ret = True
    if fp_identification_led_control:
        try:
            led_val = nas_if.get_cps_attr(cps_obj, 'base-if-phy/if/interfaces/interface/identification-led')
            if led_val != None:
                cps_obj.del_attr('base-if-phy/if/interfaces/interface/identification-led')
                name = cps_obj.get_attr_data('if/interfaces/interface/name')
                ret = fp_led.identification_led_set(name, led_val)
        except:
            pass
    return ret

# Interface config is cached here in case of front panel port. It does add default attributes
# like speed, FEC, autoneg , duplex etc.
def _if_update_config(op, obj):

    if_name = None
    npu_id = None
    port_id = None
    negotiation = None
    speed = None
    duplex = None

    try:
        if_name = obj.get_attr_data(nas_comm.yang.get_value('if_name', 'attr_name'))
    except:
        nas_if.log_info("Interface name not present in the object")
        return True

    nas_if.log_info('update config for %s: op %s' % (if_name, op))

    npu_port_found = True
    try:
        npu_id = obj.get_attr_data(nas_comm.yang.get_value('npu_id', 'attr_name'))
        port_id = obj.get_attr_data(nas_comm.yang.get_value('port_id', 'attr_name'))
    except:
        npu_port_found = False

    if op == 'create':
        # if config is cached only for physical interface
        if_type = if_config.get_intf_type(obj)
        if if_type != 'front-panel':
            return True

        ietf_intf_type = nas_if.get_cps_attr(obj,nas_comm.yang.get_value('intf_type', 'attr_name'))
        config = if_config.IF_CONFIG(if_name, ietf_intf_type)

        if if_config.if_config_add(if_name, config) == False:
            nas_if.log_err(' interface config already present for ' + str(if_name))
        nas_if.log_info(' interface config added successfully for ' + str(if_name))

        if (nas_if.get_cps_attr(obj, nas_comm.yang.get_value('negotiation', 'attr_name')) is None):
            obj.add_attr(nas_comm.yang.get_value('negotiation', 'attr_name'), nas_comm.yang.get_value('auto', 'yang-autoneg'))

        if (nas_if.get_cps_attr(obj, nas_comm.yang.get_value('speed', 'attr_name')) is None):
            obj.add_attr(nas_comm.yang.get_value('speed', 'attr_name'), nas_comm.yang.get_value('auto', 'yang-speed'))

    config = if_config.if_config_get(if_name)
    if config is None:
        nas_if.log_info(' interface not present in if config list' + str(if_name))
        return True
    if npu_port_found:
        #NPU port attribute only found in create or associate/disassociate request
        nas_if.log_info(' set npu %s and port %s to if config' % (str(npu_id), str(port_id)))
        config.set_npu_port(npu_id, port_id)

    if op == 'set' or op == 'create':
        force_update = False

        negotiation = nas_if.get_attr_from_obj_if_present(obj, 'negotiation', 'yang-autoneg', 'attr_name')
        speed = nas_if.get_attr_from_obj_if_present(obj, 'speed', 'yang-speed', 'attr_name')
        duplex = nas_if.get_attr_from_obj_if_present(obj, 'duplex', 'yang-duplex', 'attr_name')
        fec = nas_if.get_attr_from_obj_if_present(obj, 'fec_mode', 'yang-fec', 'attr_name')
        if npu_port_found:
            if npu_id != None or port_id != None:
                #for create or assiociate request
                fp_port = nas_if.get_cps_attr(obj, nas_comm.yang.get_value('fp_port', 'attr_name'))
                subport_id = nas_if.get_cps_attr(obj, nas_comm.yang.get_value('subport_id', 'attr_name'))
                config.set_fp_port(fp_port)
                config.set_subport_id(subport_id)

                fp_obj = fp.find_front_panel_port(fp_port)
                if fp_obj != None:
                    config.set_cfg_speed(fp_obj.get_port_speed())
                    config.set_breakout_mode(fp_obj.get_breakout_mode())
                    obj.add_attr(nas_comm.yang.get_value('supported_autoneg', 'attr_name'), fp_obj.get_supported_autoneg())
                else:
                    nas_if.log_err('Unable to find front panel object for port %d' % fp_port)

                #read media type from PAS
                try:
                    media_obj = _get_if_media_obj(fp_port)
                except:
                    media_obj = None

                media_monitor.process_media_event(npu_id, port_id, media_obj)

                if if_lib.check_if_media_supported(config) != True or if_lib.check_if_media_support_phy_mode(config) != True:
                    config.set_is_media_supported(False)
                    if negotiation is not None:
                        config.set_negotiation(negotiation)
                    if fec is not None and nas_comm.is_fec_supported(fec, config.get_cfg_speed()) is True:
                        config.set_fec_mode(fec)
                    if duplex is not None:
                        config.set_duplex(duplex)
                    if speed is not None:
                        if speed != nas_comm.yang.get_value('auto', 'yang-speed') and if_lib.verify_intf_supported_speed(config, speed) is True:
                            config.set_speed(speed)
                    return True

                obj.add_attr(nas_comm.yang.get_value('media_type', 'attr_name'), config.get_media_cable_type())
                config.set_is_media_supported(True)
                _if_init_config(obj, config)
                if op != 'create':
                    # for the case of interface associate, force attribute update
                    force_update = True
            else:
                #for disassociate request
                nas_if.log_info(' reset breakout_mode and media type in if config')
                config.set_breakout_mode(None)
                config.set_media_obj(None)

        if not(_fp_identification_led_handle(obj)):
            nas_if.log_err('Setting identification led failed')
            return False

        if config.get_is_media_supported() == False:
            nas_if.log_info('media type not supported')
            # Do not do any further processing based on the media connected
            if negotiation is not None:
                config.set_negotiation(negotiation)
            if fec is not None and nas_comm.is_fec_supported(fec, config.get_cfg_speed()) is True:
                config.set_fec_mode(fec)
            if duplex is not None:
                config.set_duplex(duplex)
            if speed is not None:
                if speed != nas_comm.yang.get_value('auto', 'yang-speed') and if_lib.verify_intf_supported_speed(config, speed) is True:
                    config.set_speed(speed)
            return True

        # update the new speed, duplex and autoneg in the config. If
        # autoneg, speed or duplex is auto then fetch default value and replace in the cps object
        # do not set auto speed in case of breakout mode.
        # In case of ethernet mode, set default speed only in case of breakout mode 1x1
        # in case of FC mode, set the default speed of the media.
        if speed != None and (force_update or speed != config.get_speed()):
            if if_lib.set_if_speed(speed, config, obj) == False:
                nas_if.log_err('failed to set speed')
                return False

        if duplex != None and (force_update or duplex != config.get_duplex()):
            if_lib.set_if_duplex(duplex, config, obj)
        # in case of negotiation, add autoneg attribute to on or off or default autoneg for eth mode interfaces
        intf_phy_mode = nas_comm.yang.get_value(config.get_ietf_intf_type(), 'ietf-type-2-phy-mode')

        if intf_phy_mode is not nas_comm.yang.get_value('fc','yang-phy-mode'):
            if negotiation != None and (force_update or negotiation != config.get_negotiation()):
                if_lib.set_if_autoneg(negotiation, config, obj)
        else:
            if (nas_if.get_cps_attr(obj, nas_comm.yang.get_value('negotiation', 'attr_name')) is not None):
                obj.del_attr(nas_comm.yang.get_value('auto_neg','attr_name'))

        if ((intf_phy_mode == nas_comm.yang.get_value('ether','yang-phy-mode')) and
            (op == 'create' or (fec != None and
                              (force_update or fec != config.get_fec_mode())))):
            if if_lib.set_if_fec(fec, config, obj, op) == False:
                if op == 'create':
                    obj.del_attr(nas_comm.yang.get_value('fec_mode','attr_name'))
                    nas_if.log_info("Failed to set FEC")
                else:
                    nas_if.log_err("Failed to set FEC")
                    return False

        if op == 'create':
            if_lib.set_if_hw_profile(config, obj)

        config.show()

    if op == 'delete':
        # remove the interface entry from the config
        if_config.if_config_del(if_name)
    return True

def _get_media_id_from_fp_port(fp_port):
    for npu in fp.get_npu_list():
        for p in npu.ports:
            port = npu.ports[p]
            if fp_port == port.id:
                return port.media_id
    return None

def _get_op_id(cps_obj):
    op_id_to_name_map = {1: 'create', 2: 'delete', 3: 'set'}
    op_id = None
    try:
        op_id = cps_obj.get_attr_data(nas_comm.yang.get_value('op', 'attr_name'))
    except ValueError:
        nas_if.log_err('No operation attribute in object')
        return None
    if not op_id in op_id_to_name_map:
        nas_if.log_err('Invalid operation type '+str(op_id))
        return None
    return op_id_to_name_map[op_id]

def _handle_loopback_intf(cps_obj, params):
    op = _get_op_id(cps_obj)
    if op is None:
        return False
    if op == 'set':
        return if_lpbk.set_loopback_interface(cps_obj)
    if op == 'create':
        return if_lpbk.create_loopback_interface(cps_obj, params)
    if op == 'delete':
        return if_lpbk.delete_loopback_interface(cps_obj)

def _handle_macvlan_intf(cps_obj, params):
    op = _get_op_id(cps_obj)
    if op is None:
        return False

    if op == 'create':
        return if_macvlan.create_macvlan_interface(cps_obj)
    if op == 'set':
        return if_macvlan.set_macvlan_interface(cps_obj)
    if op == 'delete':
        return if_macvlan.delete_macvlan_interface(cps_obj)

def _handle_global_vlan_config(obj):
    nas_if.log_info('Updating global default vlan container')
    in_obj = copy.deepcopy(obj)
    in_obj.set_key(cps.key_from_name('target', 'dell-base-if-cmn/if/interfaces'))
    in_obj.root_path = 'dell-base-if-cmn/if/interfaces' + '/'
    obj = in_obj.get()
    if nas_comm.yang.get_value('op', 'attr_name') in obj['data']:
        del obj['data'][nas_comm.yang.get_value('op', 'attr_name')]
    upd = ('set', obj)
    _ret = cps_utils.CPSTransaction([upd]).commit()
    if not _ret:
        nas_if.log_err('Failed to update global default vlan scale/id')
        return False
    return True

def set_intf_rpc_cb(methods, params):
    if params['operation'] != 'rpc':
        nas_if.log_err('Operation '+str(params['operation'])+' not supported')
        return False
    cps_obj = cps_object.CPSObject(obj = params['change'])

    try:
        if nas_comm.yang.get_value('def_vlan_mode_attr_name', 'attr_name') in params['change']['data'] or \
           nas_comm.yang.get_value('vn_untagged_vlan_attr_name', 'attr_name') in params['change']['data'] or \
           nas_comm.yang.get_value('def_vlan_id_attr_name', 'attr_name') in params['change']['data']:
            return _handle_global_vlan_config(cps_obj)
    except:
        pass

    nas_if.log_info(" cps object : %s" % str(params['change']))
    if_type = if_config.get_intf_type(cps_obj)
    if if_type == 'vxlan':
        return if_vtep.handle_vtep_intf(cps_obj, params)
    elif if_type == 'vlanSubInterface':
        return if_vlan_subintf.handle_vlan_sub_intf(cps_obj, params)
    elif if_type == 'management':
        return False
    elif if_type == 'macvlan':
        return _handle_macvlan_intf(cps_obj, params)

    op = _get_op_id(cps_obj)
    if op is None:
        return False

    member_port = None
    try:
        member_port = cps_obj.get_attr_data('dell-if/if/interfaces/interface/member-ports/name')
    except ValueError:
        member_port = None

    have_fp_attr = True
    front_panel_port = None
    try:
        front_panel_port = cps_obj.get_attr_data(nas_comm.yang.get_value('fp_port', 'attr_name'))
    except ValueError:
        have_fp_attr = False

    if_name = nas_if.get_cps_attr(cps_obj, nas_comm.yang.get_value('if_name', 'attr_name'))
    nas_if.log_info('Logical interface configuration: op %s if_name %s if_type %s' % (
                     op, if_name if if_name != None else '-', if_type))
    if ((op == 'create' or (op == 'set' and have_fp_attr == True and front_panel_port is not None))
        and member_port is None):
        if op == 'set' and if_type is None:
            # For set operation, if front_panel_port is given, if_type should be front-panel
            if_type = 'front-panel'
        nas_if.log_info('Interface MAC address setup for type %s' % if_type)
        try:
            mac_addr = cps_obj.get_attr_data(nas_comm.yang.get_value('phy_addr', 'attr_name'))
        except ValueError:
            mac_addr = None
        if mac_addr is None:
            nas_if.log_info('No mac address given in input object, get assigned mac address')
            try:
                mac_addr_info = ma.get_intf_mac_addr(if_type, cps_obj)
            except Exception:
                logging.exception('Failed to get params')
                return False
            if mac_addr_info is not None:
                mac_addr, _ = mac_addr_info
                if len(mac_addr) > 0:
                    nas_if.log_info('Assigned mac address: %s' % mac_addr)
                    cps_obj.add_attr(nas_comm.yang.get_value('phy_addr', 'attr_name'), mac_addr)

    if op == 'set' or op == 'create':
        if have_fp_attr == True:
            subport_id = 0
            try:
                subport_id = cps_obj.get_attr_data(nas_comm.yang.get_value('subport_id', 'attr_name'))
            except ValueError:
                # use default value if no attribute found in object
                pass
            if front_panel_port is None:
                npu_id = None
                port_id = None
            else:
                try:
                    (npu_id, port_id, hw_port) = fp_utils.get_npu_port_from_fp(
                                                            front_panel_port, subport_id)
                except ValueError as inst:
                    nas_if.log_info(inst.args[0])
                    return False
                nas_if.log_info('Front panel port %d, NPU port %d' % (front_panel_port, port_id))
                in_phy_mode = if_config.get_intf_phy_mode(cps_obj)
                if in_phy_mode != None:
                    phy_mode = port_utils.hw_port_to_phy_mode(port_list, npu_id, hw_port)
                    if in_phy_mode != phy_mode:
                        nas_if.log_err('Input PHY mode %d mis-match with physical port mode %d' % (
                                       in_phy_mode, phy_mode))
                        return False
            cps_obj.add_attr(nas_comm.yang.get_value('npu_id', 'attr_name'), npu_id)
            cps_obj.add_attr(nas_comm.yang.get_value('port_id', 'attr_name'), port_id)


        try:
            if _if_update_config(op, cps_obj) == False:
                params['change'] = cps_obj.get()
                nas_if.log_err( "Interface update config failed during set or create ")
                return False
        except Exception:
            nas_if.log_err( "Interface update config failed during set or create ")
            logging.exception('Error:')

    module_name = nas_if.get_if_key()
    in_obj = copy.deepcopy(cps_obj)
    in_obj.set_key(cps.key_from_name('target', module_name))
    in_obj.root_path = module_name + '/'
    obj = in_obj.get()
    if nas_comm.yang.get_value('op', 'attr_name') in obj['data']:
        del obj['data'][nas_comm.yang.get_value('op', 'attr_name')]
    upd = (op, obj)
    trans = cps_utils.CPSTransaction([upd])
    ret_data = trans.commit()
    nas_if.log_info(" cps object : %s" % str(obj))
    if ret_data == False:
        nas_if.log_err('Failed to commit request')
        ret_data = trans.get_objects()
        if len(ret_data) > 0 and 'change' in ret_data[0]:
            ret_obj = cps_object.CPSObject(obj = ret_data[0]['change'])
            try:
                ret_code = ret_obj.get_attr_data(nas_comm.yang.get_value('retcode', 'attr_name'))
                ret_str = ret_obj.get_attr_data(nas_comm.yang.get_value('retstr', 'attr_name'))
            except ValueError:
                nas_if.log_info('Return code and string not found from returned object')
                return False
            cps_obj.add_attr(nas_comm.yang.get_value('retcode', 'attr_name'), ret_code)
            cps_obj.add_attr(nas_comm.yang.get_value('retstr', 'attr_name'), ret_str)
            params['change'] = cps_obj.get()
        return False
    if op == 'delete':
        try:
            _if_update_config(op, in_obj)
        except:
            nas_if.log_err('update config failed for delete operation')
            logging.exception('Error:')
        return True
    if len(ret_data) == 0 or not 'change' in ret_data[0]:
        nas_if.log_err('Invalid return object from cps request')
        return False
    if (op == 'create' and member_port is None):
        ret_obj = cps_object.CPSObject(obj = ret_data[0]['change'])
        try:
            ifindex = ret_obj.get_attr_data(nas_comm.yang.get_value('if_index', 'attr_name'))
        except ValueError:
            nas_if.log_err('Ifindex not found from returned object')
            return False
        cps_obj.add_attr(nas_comm.yang.get_value('if_index', 'attr_name'), ifindex)

    params['change'] = cps_obj.get()
    return True

def set_intf_cb(methods, params):
    rc = False
    media_monitor.mutex.acquire()
    try:
        rc = set_intf_rpc_cb(methods, params)
    except Exception:
        logging.exception('logical interface error')
    media_monitor.mutex.release()
    return rc

def sigterm_hdlr(signum, frame):
    global shutdwn
    shutdwn = True

if __name__ == '__main__':

    shutdwn = False

    # Install signal handlers.
    import signal
    signal.signal(signal.SIGTERM, sigterm_hdlr)

    # Wait for base MAC address to be ready. the script will wait until
    # chassis object is registered.
    srv_chk_cnt = 0
    srv_chk_rate = 10
    chassis_key = cps.key_from_name('observed','base-pas/chassis')
    while cps.enabled(chassis_key)  == False:
        #wait for chassis object to be ready
        if srv_chk_cnt % srv_chk_rate == 0:
            nas_if.log_err('Create Interface: Base MAC address is not yet ready')
        time.sleep(1)
        srv_chk_cnt += 1
    nas_if.log_info('Base MAC address service is ready after checking %d times' % srv_chk_cnt)
    fp_utils.init()
    srv_chk_cnt = 0
    while cps.enabled(nas_comm.yang.get_value('physical_key', 'keys_id'))  == False:
        if srv_chk_cnt % srv_chk_rate == 0:
            nas_if.log_info('Create Interface: Physical port service is not ready')
        time.sleep(1)
        srv_chk_cnt += 1
    nas_if.log_info('Physical port service is ready after checking %d times' % srv_chk_cnt)

    port_list = port_utils.get_phy_port_list()

    port_utils.phy_port_cache_init(port_list)

    handle = cps.obj_init()

    # Register Front Panel Port and HW port object handler
    fp_utils.nas_fp_cps_register(handle)

    # Register for Port Group handler
    pg_utils.nas_pg_cps_register(handle)

    # Register Bridge handler
    if_bridge.nas_bridge_cps_regster(handle)

    # Register for Hybrid Group handler
    hg_utils.nas_hg_cps_register(handle)

    # Register Logical Interface handler
    d = {}
    d['transaction'] = set_intf_cb
    cps.obj_register(handle, nas_comm.yang.get_value('set_intf_key', 'keys_id'), d)

    fp_identification_led_control = fp_led.identification_led_control_get()

    media_monitor.subscribe_events()
    interface_monitor.subscribe_events()

    # Initialization complete
    # Notify systemd: Daemon is ready
    systemd.daemon.notify("READY=1")

    # Wait until a signal is received
    while False == shutdwn:
        signal.pause()

    systemd.daemon.notify("STOPPING=1")
    #Cleanup code here

    # No need to specifically call sys.exit(0).
    # That's the default behavior in Python.
