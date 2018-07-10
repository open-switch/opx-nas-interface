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
import nas_front_panel_map as fp
import nas_os_if_utils as nas_if
import nas_mac_addr_utils as ma
import nas_phy_media as media
import nas_fp_led_utils as fp_led
import nas_if_lpbk as if_lpbk
import nas_if_macvlan as if_macvlan
import nas_if_config_obj as if_config
import nas_phy_port_utils as port_utils
import nas_fp_port_utils as fp_utils
import nas_port_group_utils as pg_utils
import nas_hybrid_group as hg_utils
import nas_common_header as nas_comm

import logging
import time
import copy
import systemd.daemon

npu_attr_name = 'base-if-phy/if/interfaces/interface/npu-id'
port_attr_name = 'base-if-phy/if/interfaces/interface/port-id'
supported_autoneg = 'base-if-phy/if/interfaces/interface/supported-autoneg'
fp_port_attr_name = 'base-if-phy/hardware-port/front-panel-port'
subport_attr_name = 'base-if-phy/hardware-port/subport-id'
ifindex_attr_name = 'dell-base-if-cmn/if/interfaces/interface/if-index'
ifname_attr_name = 'if/interfaces/interface/name'
mac_attr_name = 'dell-if/if/interfaces/interface/phys-address'
speed_attr_name = 'dell-if/if/interfaces/interface/speed'
negotiation_attr_name = 'dell-if/if/interfaces/interface/negotiation'
autoneg_attr_name = 'dell-if/if/interfaces/interface/auto-negotiation'
duplex_attr_name = 'dell-if/if/interfaces/interface/duplex'
hwprofile_attr_name = 'base-if-phy/if/interfaces/interface/hw-profile'
media_type_attr_name = 'base-if-phy/if/interfaces/interface/phy-media'
vlan_id_attr_name = 'base-if-vlan/if/interfaces/interface/id'
fec_mode_attr_name = 'dell-if/if/interfaces/interface/fec'
op_attr_name = 'dell-base-if-cmn/set-interface/input/operation'
def_vlan_mode_attr_name = 'dell-if/if/interfaces/vlan-globals/scaled-vlan'
retcode_attr_name = 'cps/object-group/return-code'
retstr_attr_name = 'cps/object-group/return-string'

_yang_auto_speed = 8  # default value for auto speed
_yang_40g_speed = 6  # default value for auto speed
_yang_auto_neg = 1    # default value for negotiation
_yang_neg_on =2
_yang_neg_off =3
_yang_auto_dup = 3      #auto - default value for duplex
_yang_dup_full = 1
_yang_dup_half = 2

_yang_breakout_1x1 = 4
_yang_breakout_2x1 = 3
_yang_breakout_4x1 = 2
_yang_breakout_4x4 = 7

port_list = None
fp_identification_led_control = None

def _get_if_media_type(fp_port):
    if fp_port == None:
        nas_if.log_err("Wrong fp port or None")
    media_type = 0
    try:
        media_id = _get_media_id_from_fp_port(fp_port)
        if media_id == 0:
            return media_type
        media_info = media.get_media_info(media_id)
        media_obj = cps_object.CPSObject(obj=media_info[0])
        media_type = media_obj.get_attr_data('type')
        return media_type
    except:
        return None

# Verify if the speed is supported by the interface
def _verify_intf_supported_speed(config, speed):
    # if it is ethernet type then check if the speed is supported globally
    intf_phy_mode = nas_comm.get_value(nas_comm.ietf_type_2_phy_mode, config.get_ietf_intf_type())
    if intf_phy_mode == nas_comm.get_value(nas_comm.yang_phy_mode, 'ether'):
        if  fp.verify_npu_supported_speed(speed) == False:
            nas_if.log_err('Configured speed not supported %s' % str(speed))
            return False
    return True

def _add_default_speed(config, cps_obj):
    # fetch default speed
    media_type = config.get_media_type()
    if media_type is None:
        return
    nas_if.log_info("set default speed for media type " + str(media_type))
    speed = media.get_default_media_setting(media_type, 'speed')
    try:
        if (cps_obj.get_attr_data(speed_attr_name)):
            cps_obj.del_attr(speed_attr_name)
    except ValueError:
        # ignore exception
        pass
    if speed is None:
        nas_if.log_info('default speed setting not found')
        return
    if fp.is_qsfp28_cap_supported(config.get_fp_port()) == True:
        if config.get_ietf_intf_type() !=  'ianaift:fibreChannel':
            nas_if.log_info('Do not push default speed in case of QSFP28 port')
            return
    if _verify_intf_supported_speed(config, speed) == False:
        nas_if.log_err('Media based default speed not supported %s' % str(speed))
        return
    cps_obj.add_attr(speed_attr_name, speed)
    nas_if.log_info("default speed is " + str(speed))

def _add_default_autoneg(media_type, cps_obj):
    if media_type == None:
        return
    # fetch default autoneg
    nas_if.log_info("set default autoneg for media type " + str(media_type))
    autoneg = media.get_default_media_setting(media_type, 'autoneg')
    if autoneg is None:
        nas_if.log_info('default auto-negotiation setting not found')
        return
    nas_if.log_info("default autoneg is " + str(autoneg))
    cps_obj.add_attr(autoneg_attr_name, autoneg)

def _add_default_duplex(media_type, cps_obj):
    if media_type == None:
        return
    # fetch default duplex
    nas_if.log_info("set default duplex for media type " + str(media_type))
    duplex = media.get_default_media_setting(media_type, 'duplex')
    if duplex is None:
        nas_if.log_info('default duplex setting not found')
        duplex = _yang_dup_full
    cps_obj.add_attr(duplex_attr_name, duplex)
    nas_if.log_info("default Duplex is " + str(duplex))

# Interface Speed is pushed down to hardware based on media connected and if it is configured AUTO(default).
# Based on the port capability (QSFP+ or QSFP28) and mode configured(ethernet or FC), connected physical media
# may not be supported and media based default speed will not be pushed down to the NPU/Hardware.
# If user set something other than AUTO( default) then it will be passed down without checking connected media.
# in case of ethernet fanout mode, default speed is skipped.
def _set_speed(speed, config, obj):
    intf_phy_mode = nas_comm.get_value(nas_comm.ietf_type_2_phy_mode, config.get_ietf_intf_type())
    breakout_mode = config.get_breakout_mode()
    nas_if.log_info("breakout mode %s and speed %s " % (str(breakout_mode), str(speed)))
    if speed != _yang_auto_speed:
        if _verify_intf_supported_speed(config, speed) == False:
            return False
    config.set_speed(speed)
    if speed == _yang_auto_speed:
        if intf_phy_mode == nas_comm.get_value(nas_comm.yang_phy_mode, 'fc') or breakout_mode == _yang_breakout_1x1 or breakout_mode == _yang_breakout_4x4 or media.is_sfp_media_type(config.get_media_type()):
            # TODO default speed in breakout mode is not supported  yet
            # TODO it may cause issue in port group ports. Verify the usecases.
            _add_default_speed(config, obj)
    return True

def _set_autoneg(negotiation, config, obj):
    nas_if.log_info ('negotiation is ' + str(negotiation))
    config.set_negotiation(negotiation)
    if negotiation == _yang_auto_neg:
        media_type = config.get_media_type()
        _add_default_autoneg(media_type, obj)
    else:
        autoneg = False
        if negotiation == _yang_neg_on:
            autoneg = True
        obj.add_attr(autoneg_attr_name, autoneg)
    return

def _set_hw_profile(config, obj):
    media_type = config.get_media_type()
    if media_type is None:
        return
    hw_profile = media.get_default_media_setting(media_type, 'hw-profile')
    if hw_profile is None:
        return
    if config.get_hw_profile() is None or \
       config.get_hw_profile() != hw_profile:
        obj.add_attr(hwprofile_attr_name, hw_profile)
        config.set_hw_profile(hw_profile)

def _set_duplex(duplex, config, obj):
    nas_if.log_info("duplex is " + str(duplex))
    config.set_duplex(duplex)
    if duplex == _yang_auto_dup:
        media_type = config.get_media_type()
        _add_default_duplex(media_type, obj)
    else:
        obj.add_attr(duplex_attr_name, duplex)
    return

def _get_default_fec_mode(media_type,if_speed):
    """
    Auto option will configure default FEC. Default FEC for 100G is CL91
    for all types of media. Default option for 25G/50G for 25/50G CR is CL108
     and other 25/50g is OFF for all types of media.
    """
    fec_mode = None
    media_str = media.get_media_str(media_type)
    if if_speed == nas_comm.get_value(nas_comm.yang_speed, '100G'):
        fec_mode = nas_comm.get_value(nas_comm.yang_fec_mode, 'CL91-RS')
    elif (if_speed == nas_comm.get_value(nas_comm.yang_speed, '25G') or
             if_speed == nas_comm.get_value(nas_comm.yang_speed, '50G')):
        if(media_str in media.fec_25g_cl108_support):
            fec_mode = nas_comm.get_value(nas_comm.yang_fec_mode, 'CL108-RS')
        else:
            fec_mode = nas_comm.get_value(nas_comm.yang_fec_mode, 'OFF')

    return fec_mode

def _set_fec_mode(fec_mode, config, obj, op):
    if_cfg_speed = config.get_cfg_speed()
    if fec_mode != None:
        if not nas_comm.is_fec_supported(fec_mode, if_cfg_speed):
            _str_err = ('FEC mode %s is not supported with speed %s'
                        %(nas_comm.get_value(nas_comm.fec_mode_to_yang,fec_mode),
                        nas_comm.get_value(nas_comm.yang_to_mbps_speed,if_cfg_speed)))
            nas_if.log_err(_str_err)
            obj.set_error_string(1,_str_err)
            return False
    else:
        if (op == 'create' and
            nas_comm.is_fec_supported(nas_comm.get_value(nas_comm.yang_fec_mode, 'AUTO'), if_cfg_speed)):
            # set default FEC mode as auto to newly created interface
            fec_mode = nas_comm.get_value(nas_comm.yang_fec_mode, 'AUTO')
        else:
            return True

    nas_if.log_info('set FEC mode %d' % fec_mode)
    config.set_fec_mode(fec_mode)
    if fec_mode == nas_comm.get_value(nas_comm.yang_fec_mode, 'AUTO'):
        media_type = config.get_media_type()
        fec_mode = _get_default_fec_mode(media_type,if_cfg_speed)
    obj.add_attr(fec_mode_attr_name, fec_mode)
    return True

# check if media supported. Right now ,
# check only if QSFP28 is plugged in a QSFP+ slot
def check_if_media_supported(media_type, config):
    supported = True   # by default media is supported

    # Check if Ethernet QSFP28 plugged-in the QSFP+ slot
    if media.is_qsfp28_media_type(media_type):
        #check if front port supports QSFP28 media
        if fp.is_qsfp28_cap_supported(config.get_fp_port()) == False:
            nas_if.log_err('Connected media QSFP28 is not support in this port')
            supported = False
    return(supported)

# Check if physical Mode is supported by the media plugged-in
def check_if_media_support_phy_mode(media_type, config):
    intf_phy_mode = nas_comm.get_value(nas_comm.ietf_type_2_phy_mode, config.get_ietf_intf_type())

    # Make sure that connected media supports  the configured intf phy mode.
    supported_phy_modes = media.get_default_media_setting(media_type, 'supported-phy-mode')
    if intf_phy_mode not in supported_phy_modes:
        nas_if.log_err('Connected media does not support configured phy mode')
        return(False)
    return(True)


# fetch media type from Media event object and then add corresponding default speed/autoneg
def if_handle_set_media_type(op, obj):
    nas_if.log_info('media obj: %s' % str(obj.get()))
    if_name = None
    try:
        npu = obj.get_attr_data(npu_attr_name)
        port = obj.get_attr_data(port_attr_name)
        media_type = obj.get_attr_data(media_type_attr_name)
    except ValueError:
        nas_if.log_info('missing npu,port or media type or non physical port cps obj request')
        nas_if.log_obj(obj.get())
        return True
    # find npu, port in the _if_config
    if_name = if_config.if_config_get_by_npu_port(npu, port)
    if if_name == None:
        nas_if.log_err("No interface present for the npu "+str(npu)+ "and port " +str(port))
        return False
    nas_if.log_info( "if name is " +str(if_name))
    config = if_config.if_config_get(if_name)
    config.set_media_type(media_type)

    if check_if_media_supported(media_type, config) != True:
        config.set_is_media_supported(False)
        obj.del_attr(media_type_attr_name)
        return False

    if check_if_media_support_phy_mode(media_type, config) != True:
        config.set_is_media_supported(False)
        return False

    # Initialize default speed, hw_profile and negotiation to auto if not initialized
    if config.get_speed() is None:
        config.set_speed(_yang_auto_speed)
    if config.get_negotiation() is None:
        config.set_negotiation(_yang_auto_neg)

    config.set_is_media_supported(True)

    intf_phy_mode = nas_comm.get_value(nas_comm.ietf_type_2_phy_mode, config.get_ietf_intf_type())

    obj.add_attr(ifname_attr_name, if_name)
    # set the default speed if the speed is configured to auto it is in non-breakout mode
    if config.get_speed() == _yang_auto_speed:
        if config.get_breakout_mode() == _yang_breakout_1x1 or config.get_breakout_mode() == _yang_breakout_4x4 or intf_phy_mode == nas_comm.get_value(nas_comm.yang_phy_mode, 'fc'):
            _add_default_speed(config, obj)
    if config.get_negotiation() == _yang_auto_neg:
        _add_default_autoneg(media_type, obj)
    if config.get_duplex() == _yang_auto_dup:
        _add_default_duplex(media_type, obj)
    if config.get_fec_mode() == nas_comm.get_value(nas_comm.yang_fec_mode, 'AUTO'):
        cfg_speed = config.get_cfg_speed()
        fec_cfg = _get_default_fec_mode(media_type, cfg_speed)
        if(fec_cfg is not None):
            obj.add_attr(fec_mode_attr_name,fec_cfg)

    _set_hw_profile(config, obj)

    # delete npu port attribute because NAS use them as flag for interface association
    obj.del_attr(npu_attr_name)
    obj.del_attr(port_attr_name)

    nas_if.log_info("media type setting is successful for " +str(if_name))
    config.show()

    return True

def _if_init_config(obj, config):
    # Below rule is applied to process speed, duplex, auto-neg and FEC attritube in cps object
    # 1. if attribute is in input cps object, use it and do not change anything
    # 2. else, if attribute is in config, add it to cps object
    # 3. else, add "auto" to cps object
    speed = nas_if.get_cps_attr(obj, speed_attr_name)
    if speed is None:
        cfg_speed = config.get_speed()
        if cfg_speed is None:
            cfg_speed = _yang_auto_speed
        obj.add_attr(speed_attr_name, cfg_speed)
    duplex = nas_if.get_cps_attr(obj, duplex_attr_name)
    if duplex is None:
        cfg_duplex = config.get_duplex()
        if cfg_duplex is None:
            cfg_duplex = _yang_auto_dup
        obj.add_attr(duplex_attr_name, cfg_duplex)
    ng = nas_if.get_cps_attr(obj, negotiation_attr_name)
    if ng is None:
        cfg_ng = config.get_negotiation()
        if cfg_ng is None:
            cfg_ng = _yang_auto_neg
        obj.add_attr(negotiation_attr_name, cfg_ng)

    fec = nas_if.get_cps_attr(obj, fec_mode_attr_name)
    if fec is None:
        cfg_fec = config.get_fec_mode()
        if cfg_fec is None:
            auto_fec = nas_comm.get_value(nas_comm.yang_fec_mode, 'AUTO')
            if_cfg_speed = config.get_cfg_speed()
            if if_cfg_speed != None and nas_comm.is_fec_supported(auto_fec, if_cfg_speed):
                cfg_fec = auto_fec
        if cfg_fec != None:
            obj.add_attr(fec_mode_attr_name, cfg_fec)


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
    media_type = None
    breakout_mode = None

    try:
        if_name = obj.get_attr_data(ifname_attr_name)
    except:
        # check for media_type change event obj
        nas_if.log_info('process media event, op %s' % op)
        return(if_handle_set_media_type(op, obj))

    nas_if.log_info('update config for %s: op %s' % (if_name, op))

    npu_port_found = True
    try:
        npu_id = obj.get_attr_data(npu_attr_name)
        port_id = obj.get_attr_data(port_attr_name)
    except:
        npu_port_found = False

    if op == 'create':
        # if config is cached only for physical interface
        if_type = if_config.get_intf_type(obj)
        if if_type != 'front-panel':
            return True

        ietf_intf_type = nas_if.get_cps_attr(obj,nas_comm.get_value(nas_comm.attr_name,'intf_type'))
        config = if_config.IF_CONFIG(if_name, ietf_intf_type)

        if if_config.if_config_add(if_name, config) == False:
            nas_if.log_err(' interface config already present for ' + str(if_name))
        nas_if.log_info(' interface config added successfully for ' + str(if_name))

        if (nas_if.get_cps_attr(obj, negotiation_attr_name) is None):
            obj.add_attr(negotiation_attr_name, _yang_auto_neg)

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
        if npu_port_found:
            if npu_id != None or port_id != None:
                #for create or assiociate request
                fp_port = nas_if.get_cps_attr(obj, fp_port_attr_name)
                subport_id = nas_if.get_cps_attr(obj, subport_attr_name)
                config.set_fp_port(fp_port)
                config.set_subport_id(subport_id)

                fp_obj = fp.find_front_panel_port(fp_port)
                if fp_obj != None:
                    config.set_cfg_speed(fp_obj.get_port_speed())
                    config.set_breakout_mode(fp_obj.get_breakout_mode())
                    obj.add_attr(supported_autoneg, fp_obj.get_supported_autoneg())
                else:
                    nas_if.log_err('Unable to find front panel object for port %d' % fp_port)

                #read media type from PAS
                media_type = _get_if_media_type(fp_port)
                nas_if.log_info(' set media_type %s to if config for fp %d' % (
                                str(media_type), fp_port));
                config.set_media_type(media_type)

                if check_if_media_supported(media_type, config) != True:
                    config.set_is_media_supported(False)
                    nas_if.log_err(' Plugged-in media is not supported for ' + str(if_name))
                    return True

                if check_if_media_support_phy_mode(media_type, config) != True:
                    config.set_is_media_supported(False)
                    nas_if.log_err(' Plugged-in media does not support configured phy mode for %s' %
                                   if_name)
                    return True

                config.set_is_media_supported(True)
                obj.add_attr(media_type_attr_name, media_type)
                _if_init_config(obj, config)
                if op != 'create':
                    # for the case of interface associate, force attribute update
                    force_update = True
            else:
                #for disassociate request
                nas_if.log_info(' reset breakout_mode and media type in if config')
                config.set_breakout_mode(None)
                config.set_media_type(None)

        if not(_fp_identification_led_handle(obj)):
            nas_if.log_err('Setting identification led failed')
            return False

        if config.get_is_media_supported() == False:
            nas_if.log_info('media type not supported')
            # Do not do any further processing based on the media connected
            return True

        negotiation = nas_if.get_cps_attr(obj, negotiation_attr_name)
        speed = nas_if.get_cps_attr(obj, speed_attr_name)
        duplex = nas_if.get_cps_attr(obj, duplex_attr_name)
        # update the new speed, duplex and autoneg in the config. If
        # autoneg, speed or duplex is auto then fetch default value and replace in the cps object
        # do not set auto speed in case of breakout mode.
        # In case of ethernet mode, set default speed only in case of breakout mode 1x1
        # in case of FC mode, set the default speed of the media.
        if speed != None and (force_update or speed != config.get_speed()):
            if _set_speed(speed, config, obj) == False:
                nas_if.log_err('failed to set speed')
                return False

        # in case of negotiation, add autoneg attribute to on or off or default autoneg
        # if negotiation== auto
        if negotiation != None and (force_update or negotiation != config.get_negotiation()):
            _set_autoneg(negotiation, config, obj)

        if duplex != None and (force_update or duplex != config.get_duplex()):
            _set_duplex(duplex, config, obj)

        fec_mode = nas_if.get_cps_attr(obj, fec_mode_attr_name)
        if op == 'create' or (fec_mode != None and
                              (force_update or fec_mode != config.get_fec_mode())):
            if _set_fec_mode(fec_mode, config, obj, op) != True:
                nas_if.log_err('Failed to set FEC mode %d to interface %s' % (fec_mode, if_name))
                return False

        if op == 'create':
            _set_hw_profile(config, obj)

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

def get_alloc_mac_addr_params(if_type, cps_obj):
    global port_list
    ret_list = {'if_type': if_type}
    if if_type == 'front-panel':
        port_id = None
        try:
            port_id = cps_obj.get_attr_data(port_attr_name)
        except ValueError:
            pass
        if port_id is None:
            front_panel_port = None
            subport_id = 0
            try:
                front_panel_port = cps_obj.get_attr_data(fp_port_attr_name)
                subport_id = cps_obj.get_attr_data(subport_attr_name)
            except ValueError:
                pass

            if front_panel_port is None:
                nas_if.log_info('Create virtual interface without mac address assigned')
                return None

            try:
                (npu_id, port_id, hw_port) = fp_utils.get_npu_port_from_fp(front_panel_port, subport_id)
            except ValueError as inst:
                nas_if.log_info(inst.args[0])
                return None
        else:
            try:
                npu_id = cps_obj.get_attr_data(npu_attr_name)
            except ValueError:
                nas_if.log_err('Input object does not contain npu id attribute')
                return None
            hw_port = port_utils.phy_port_to_first_hwport(port_list, npu_id, port_id)
            if hw_port == -1:
                nas_if.log_err('Physical port object not found')
                return None
        npu = fp.get_npu(npu_id)
        if hw_port is None or npu is None:
            nas_if.log_err('No hardware port id or npu object for front panel port')
            return None
        p = npu.port_from_hwport(hw_port)
        lane = p.lane(hw_port)
        mac_offset = p.mac_offset + lane
        ret_list['fp_mac_offset'] = mac_offset
    elif if_type == 'vlan':
        try:
            vlan_id = cps_obj.get_attr_data(vlan_id_attr_name)
        except ValueError:
            nas_if.log_err('Input object does not contain VLAN id attribute')
            return None
        ret_list['vlan_id'] = vlan_id
    elif if_type == 'lag':
        try:
            lag_name = cps_obj.get_attr_data(ifname_attr_name)
        except ValueError:
            nas_if.log_err('Input object does not contain name attribute')
            return None
        lag_id = nas_if.get_lag_id_from_name(lag_name)
        ret_list['lag_id'] = lag_id
    else:
        nas_if.log_err('Unknown interface type %s' % if_type)
        return None
    return ret_list

def _get_op_id(cps_obj):
    op_id_to_name_map = {1: 'create', 2: 'delete', 3: 'set'}
    op_id = None
    try:
        op_id = cps_obj.get_attr_data(op_attr_name)
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
    nas_if.log_err('Updating global default vlan mode')
    in_obj = copy.deepcopy(obj)
    in_obj.set_key(cps.key_from_name('target', 'dell-base-if-cmn/if/interfaces'))
    in_obj.root_path = 'dell-base-if-cmn/if/interfaces' + '/'
    obj = in_obj.get()
    print obj
    if op_attr_name in obj['data']:
        del obj['data'][op_attr_name]
    upd = ('set', obj)
    _ret = cps_utils.CPSTransaction([upd]).commit()
    if not _ret:
        nas_if.log_err('Failed to update global default vlan mode')
        return False
    return True

def set_intf_rpc_cb(methods, params):
    if params['operation'] != 'rpc':
        nas_if.log_err('Operation '+str(params['operation'])+' not supported')
        return False
    cps_obj = cps_object.CPSObject(obj = params['change'])

    try:
        if def_vlan_mode_attr_name in params['change']['data']:
            return _handle_global_vlan_config(cps_obj)
    except:
        pass

    if_type = if_config.get_intf_type(cps_obj)
    if if_type == 'loopback':
        return _handle_loopback_intf(cps_obj, params)
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
        front_panel_port = cps_obj.get_attr_data(fp_port_attr_name)
    except ValueError:
        have_fp_attr = False

    if_name = nas_if.get_cps_attr(cps_obj, ifname_attr_name)
    nas_if.log_info('Logical interface configuration: op %s if_name %s if_type %s' % (
                     op, if_name if if_name != None else '-', if_type))
    if ((op == 'create' or (op == 'set' and have_fp_attr == True and front_panel_port is not None))
        and member_port is None):
        if op == 'set' and if_type is None:
            # For set operation, if front_panel_port is given, if_type should be front-panel
            if_type = 'front-panel'
        nas_if.log_info('Interface MAC address setup for type %s' % if_type)
        try:
            mac_addr = cps_obj.get_attr_data(mac_attr_name)
        except ValueError:
            mac_addr = None
        if mac_addr is None:
            nas_if.log_info('No mac address given in input object, get assigned mac address')
            try:
                param_list = get_alloc_mac_addr_params(if_type, cps_obj)
            except Exception:
                logging.exception('Failed to get params')
                return False
            if param_list != None:
                try:
                    mac_addr = ma.if_get_mac_addr(**param_list)
                except:
                    logging.exception('Failed to get mac address')
                    return False
                if mac_addr is None:
                    nas_if.log_err('Failed to get mac address')
                    return False
                if len(mac_addr) > 0:
                    nas_if.log_info('Assigned mac address: %s' % mac_addr)
                    cps_obj.add_attr(mac_attr_name, mac_addr)

    if op == 'set' or op == 'create':
        if have_fp_attr == True:
            subport_id = 0
            try:
                subport_id = cps_obj.get_attr_data(subport_attr_name)
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
            cps_obj.add_attr(npu_attr_name, npu_id)
            cps_obj.add_attr(port_attr_name, port_id)

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
    if op_attr_name in obj['data']:
        del obj['data'][op_attr_name]
    upd = (op, obj)
    trans = cps_utils.CPSTransaction([upd])
    ret_data = trans.commit()
    if ret_data == False:
        nas_if.log_err('Failed to commit request')
        ret_data = trans.get_objects()
        if len(ret_data) > 0 and 'change' in ret_data[0]:
            ret_obj = cps_object.CPSObject(obj = ret_data[0]['change'])
            try:
                ret_code = ret_obj.get_attr_data(retcode_attr_name)
                ret_str = ret_obj.get_attr_data(retstr_attr_name)
            except ValueError:
                nas_if.log_info('Return code and string not found from returned object')
                return False
            cps_obj.add_attr(retcode_attr_name, ret_code)
            cps_obj.add_attr(retstr_attr_name, ret_str)
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
            ifindex = ret_obj.get_attr_data(ifindex_attr_name)
        except ValueError:
            nas_if.log_err('Ifindex not found from returned object')
            return False
        cps_obj.add_attr(ifindex_attr_name, ifindex)

    params['change'] = cps_obj.get()
    return True

def set_intf_cb(methods, params):
    try:
        return set_intf_rpc_cb(methods, params)
    except:
        logging.exception('logical interface error')

def get_mac_cb(methods, params):
    if params['operation'] != 'rpc':
        nas_if.log_err('Operation %s not supported' % params['operation'])
        return False
    cps_obj = cps_object.CPSObject(obj = params['change'])

    if_type = if_config.get_intf_type(cps_obj)
    if if_type == 'loopback' or if_type == 'management':
        nas_if.log_err('Interface type %s not supported' % if_type)
        return False
    param_list = get_alloc_mac_addr_params(if_type, cps_obj)
    if param_list is None:
        nas_if.log_err('No enough attributes in input object to get mac address')
        return False

    mac_addr = ma.if_get_mac_addr(**param_list)
    if mac_addr is None or len(mac_addr) == 0:
        nas_if.log_err('Failed to get mac address')
        return False
    cps_obj.add_attr(mac_attr_name, mac_addr)

    params['change'] = cps_obj.get()
    return True

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
    chassis_key = cps.key_from_name('observed','base-pas/chassis')
    while cps.enabled(chassis_key)  == False:
        #wait for chassis object to be ready
        nas_if.log_err('Create Interface: Base MAC address is not yet ready')
        time.sleep(1)
    fp_utils.init()
    while cps.enabled(nas_comm.get_value(nas_comm.keys_id, 'physical_key'))  == False:
        nas_if.log_info('Create Interface: Physical port service is not ready')
        time.sleep(1)

    port_list = port_utils.get_phy_port_list()

    port_utils.phy_port_cache_init(port_list)


    handle = cps.obj_init()

    # Register Front Panel POrt and HW port object handler
    fp_utils.nas_fp_cps_register(handle)

    # Register for Port Group handler
    pg_utils.nas_pg_cps_register(handle)

    # Register for Hybrid Group handler
    hg_utils.nas_hg_cps_register(handle)

    # Register Logical Interface handler
    d = {}
    d['transaction'] = set_intf_cb
    cps.obj_register(handle, nas_comm.get_value(nas_comm.keys_id, 'set_intf_key'), d)

    # Register MAc address allocation handler
    get_mac_hdl = cps.obj_init()

    d = {}
    d['transaction'] = get_mac_cb
    cps.obj_register(get_mac_hdl, nas_comm.get_value(nas_comm.keys_id, 'get_mac_key'), d)

    fp_identification_led_control = fp_led.identification_led_control_get()

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
