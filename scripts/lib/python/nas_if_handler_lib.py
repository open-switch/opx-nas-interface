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

import nas_front_panel_map as fp
import nas_os_if_utils as nas_if
import nas_if_config_obj as if_config
import nas_common_header as nas_comm
import nas_media_config as media_config
import nas_phy_port_utils as port_utils


def verify_intf_supported_speed(config, speed):
    ''' Method to verify if speed is supported by the interface '''

    # if it is ethernet type then check if the speed is supported globally
    intf_phy_mode = nas_comm.yang.get_value(config.get_ietf_intf_type(), 'ietf-type-2-phy-mode')
    if intf_phy_mode == nas_comm.yang.get_value('ether', 'yang-phy-mode'):
        if  fp.verify_npu_supported_speed(speed) == False:
            nas_if.log_err('Configured speed not supported %s' % str(speed))
            return False
    return True


def set_if_autoneg(autoneg, config, obj):
    ''' Method to set Autoneg in CPS Object '''

    if config.get_media_obj() is None:
        return False

    nas_if.log_info('Application Configured AUTONEG: ' + str(nas_comm.yang.get_key(autoneg, 'yang-autoneg')))

    # Set in config object
    config.set_negotiation(autoneg)

    # Retrieve default setting
    if autoneg == nas_comm.yang.get_value('auto', 'yang-autoneg'):
        autoneg = media_config.Autoneg().get_setting(config.get_media_obj())

    # Add attribute to CPS object
    if autoneg is not None:
        autoneg = True if (autoneg == nas_comm.yang.get_value('on', 'yang-autoneg')) else False
        obj.add_attr(nas_comm.yang.get_value('auto_neg', 'attr_name'), autoneg)
        return True
    return False


def set_if_duplex(duplex, config, obj):
    ''' Method to set Duplex in CPS Object '''

    if config.get_media_obj() is None:
        return False

    nas_if.log_info("Application Configured DUPLEX: " + str(nas_comm.yang.get_key(duplex, 'yang-duplex')))
    config.set_duplex(duplex)

    # Retrieve default setting
    if duplex == nas_comm.yang.get_value('auto', 'yang-duplex'):
        duplex = media_config.Duplex().get_setting(config.get_media_obj())

    # Add attribute to CPS object
    if duplex is not None:
        obj.add_attr(nas_comm.yang.get_value('duplex', 'attr_name'), duplex)
        return True
    return False


def set_if_hw_profile(config, obj):
    ''' Method to set Hardware Profile in CPS Object '''

    if config.get_media_obj() is None:
        return False

    # Retrieve default setting
    hw_profile = media_config.HardwareProfile().get_setting(config.get_media_obj())
    if hw_profile == 0:
        hw_profile = None

    # Set in config object
    config.set_hw_profile(hw_profile)

    # Add attribute to CPS object
    if config.get_hw_profile() is not None:
        obj.add_attr(nas_comm.yang.get_value('hw_profile', 'attr_name'), hw_profile)
        return True
    return False


def set_if_fec(fec, config, obj, op=None):
    ''' Method to set FEC in CPS Object '''

    if config.get_media_obj() is None:
        return False

    if_cfg_speed = config.get_cfg_speed()
    if fec != None:
        if not nas_comm.is_fec_supported(fec, if_cfg_speed):
            _str_err = ('FEC mode %s is not supported with speed %s'
                        %(str(fec), nas_comm.yang.get_value(if_cfg_speed, 'yang-to-mbps-speed')))
            nas_if.log_err(_str_err)
            obj.set_error_string(1, _str_err)
            return False
    else:
        if (op == 'create' and nas_comm.is_fec_supported(nas_comm.yang.get_value('auto', 'yang-fec'), if_cfg_speed)):
            fec = nas_comm.yang.get_value('auto', 'yang-fec')
        else:
            return False

    nas_if.log_info("Application Configured FEC: " + str(nas_comm.yang.get_key(fec, 'yang-fec')))

    # Set in config object
    config.set_fec_mode(fec)

    # Retrieve default setting
    if fec == nas_comm.yang.get_value('auto', 'yang-fec'):
        fec = media_config.FEC().get_setting(config.get_media_obj())

    # Add attribute to CPS object
    if fec is not None:
        obj.add_attr(nas_comm.yang.get_value('fec_mode', 'attr_name'), fec)
        return True
    return False


def _get_min_speed(sp1, sp2):
    ''' Method to get minimum of sp1 and sp2 '''
    if sp1 not in nas_comm.yang.get_tbl('yang-to-mbps-speed') or sp2 not in nas_comm.yang.get_tbl('yang-to-mbps-speed'):
        return None
    speed = min(nas_comm.yang.get_value(sp1, 'yang-to-mbps-speed'), nas_comm.yang.get_value(sp2, 'yang-to-mbps-speed'))
    return nas_comm.yang.get_value(speed, 'mbps-to-yang-speed')


def _get_default_speed(config):
    ''' Method to retrieve minimum of port speed and media speed '''

    # Retrieve media default speed for mode as media_speed
    intf_phy_mode = nas_comm.yang.get_value(config.get_ietf_intf_type(), 'ietf-type-2-phy-mode')
    media_speed = media_config.Speed().get_setting(config.get_media_obj(), intf_phy_mode)

    # Retreive port default speed for mode as fp_speed. fp_speed is the max speed supported on the port
    fp_port = fp.find_front_panel_port(config.get_fp_port())
    fp_speed = fp_port.get_port_speed()

    # Return min(port_speed, media_speed)
    return _get_min_speed(media_speed, fp_speed)


def set_if_speed(speed, config, obj):
    ''' Method to set Speed in CPS Object '''

    # 1. Interface Speed is pushed down to hardware based on media connected and if it is configured AUTO(default).
    # 2. Based on the port capability (QSFP+ or QSFP28) and mode configured(ethernet or FC), connected physical media
    #    may not be supported and media based default speed will not be pushed down to the NPU/Hardware.
    # 3. If user set something other than AUTO( default ) then it will be passed down without checking connected media.
    # 4. In case of ethernet fanout mode, default speed is skipped.
    if config.get_media_obj() is None:
        return True

    nas_if.log_info("Application Configured Breakout Mode: " + str(config.get_breakout_mode()))
    nas_if.log_info("Application Configured Speed: " +  str(speed))
    if speed != nas_comm.yang.get_value('auto', 'yang-speed') and verify_intf_supported_speed(config, speed) is False:
        return False

    # Set in config object
    config.set_speed(speed)

    # Retrieve default setting
    if speed == nas_comm.yang.get_value('auto', 'yang-speed'):
        try:
            obj.del_attr(nas_comm.yang.get_value('speed', 'attr_name'))
        except:
            pass
        speed = _get_default_speed(config)
        nas_if.log_info('Default Speed: ' + str(speed))

    try:
        (npu_id, port_id) = config.get_npu_port()
        nas_if.log_info('set_if_speed: Obtained npu %s and port %s from config for intf %s' 
                % (str(npu_id), str(port_id), str(config.name)))
    except ValueError:
        nas_if.log_err('Missing npu or port or non physical port cps obj request')
        nas_if.log_obj(obj.get())
        return False

    supported_speed_list = []
    supported_speed_list = port_utils.phy_port_supported_speed_get(npu_id, port_id)

    nas_if.log_info('supported-speed list: ' + str(supported_speed_list))
    # Add attribute to CPS object
    if speed is not None and verify_intf_supported_speed(config, speed) == True:
        if speed in supported_speed_list:
            obj.add_attr(nas_comm.yang.get_value('speed', 'attr_name'), speed)
            nas_if.log_info('Added speed %s into obj' % str(speed))
        else:
            nas_if.log_err('speed %s not added into the obj' % str(speed))

    config.get_media_obj().add_attr(nas_comm.yang.get_value('speed', 'attr_name'), speed)
    return True

def check_if_media_supported(config):
    ''' Method to check if media is supported in platform '''

    supported = True

    # Check if Ethernet QSFP28 plugged-in the QSFP+ slot
    category = config.get_media_category()
    if category is None:
        nas_if.log_info('No media present')
        return False

    if 'qsfp28' == nas_comm.yang.get_key(category, 'yang-category'):
        if fp.is_qsfp28_cap_supported(config.get_fp_port()) == False:
            nas_if.log_err('Connected media QSFP28 is not supported in this port')
            supported = False

    nas_if.log_info('Plugged-in media is supported')
    return(supported)


def check_if_media_support_phy_mode(config):
    ''' Method to check if physical Mode is supported by the media plugged-in '''

    intf_phy_mode = nas_comm.yang.get_value(config.get_ietf_intf_type(), 'ietf-type-2-phy-mode')
    # Make sure that connected media supports  the configured intf phy mode.
    supported_phy_modes = media_config.SupportedPhyMode().get_setting(config.get_media_obj())

    if intf_phy_mode not in supported_phy_modes:
        nas_if.log_info('Connected media doesn\'t support Eth or FC physical modes')
        return(False)

    return(True)


def set_media_setting(op, obj):
    ''' Method to add default media settings to CPS Object '''

    nas_if.log_info('media obj: %s' % str(obj.get()))
    if_name = None

    try:
        npu = obj.get_attr_data(nas_comm.yang.get_value('npu_id', 'attr_name'))
        port = obj.get_attr_data(nas_comm.yang.get_value('port_id', 'attr_name'))
    except ValueError:
        nas_if.log_info('missing npu or port or non physical port cps obj request')
        nas_if.log_obj(obj.get())
        return False

    # find npu, port in the _if_config
    if_name = if_config.if_config_get_by_npu_port(npu, port)
    if if_name is None:
        nas_if.log_err("No interface present for the npu "+str(npu)+ " and port " +str(port))
        return False
    nas_if.log_info( "if name is " +str(if_name))
    obj.add_attr(nas_comm.yang.get_value('if_name', 'attr_name'), if_name)
    config = if_config.if_config_get(if_name)

    if check_if_media_supported(config) != True or check_if_media_support_phy_mode(config) != True:
        config.set_is_media_supported(False)
        return False

    obj.add_attr(nas_comm.yang.get_value('media_type', 'attr_name'), config.get_media_cable_type())
    config.set_is_media_supported(True)

    _auto_speed = nas_comm.yang.get_value('auto', 'yang-speed')
    if config.get_speed() is None or config.get_speed() == _auto_speed:
        set_if_speed(_auto_speed, config, obj)

    _auto_negotiation = nas_comm.yang.get_value('auto', 'yang-autoneg')
    if config.get_negotiation() is None or config.get_negotiation() == _auto_negotiation:
        set_if_autoneg(_auto_negotiation, config, obj)

    _auto_fec = nas_comm.yang.get_value('auto', 'yang-fec')
    if config.get_fec_mode() is None or config.get_fec_mode() == _auto_fec:
        set_if_fec(_auto_fec, config, obj)

    _auto_duplex = nas_comm.yang.get_value('auto', 'yang-duplex')
    if config.get_duplex() is None or config.get_duplex() == _auto_duplex:
        set_if_duplex(_auto_duplex, config, obj)

    set_if_hw_profile(config, obj)

    # delete npu port attribute because NAS uses them as flag for interface association
    obj.del_attr(nas_comm.yang.get_value('npu_id', 'attr_name'))
    obj.del_attr(nas_comm.yang.get_value('port_id', 'attr_name'))

    nas_if.log_info("media type setting is successful for " +str(if_name))
    config.show()
    return True
