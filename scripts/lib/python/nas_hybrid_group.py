#
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
#

import cps
import cps_object
import nas_front_panel_map as fp
import nas_os_if_utils as nas_if
import nas_fp_port_utils as fp_utils
import nas_hybrid_group_utils as hg_utils
import nas_port_group_utils as pg_utils
import nas_common_header as nas_comm
import logging


# Helper Methods
#Apply CPS Configuration to HG Object
def apply_cps_config_to_hg(obj, hg_obj):

    '''Method to configure a Hybrid Group'''

    #Configure Profile Mode
    rollback_list = []
    prev_profile_mode = hg_obj.get_hybrid_group_profile_mode()
    cur_profile_mode = nas_if.get_cps_attr(obj, hg_utils.hg_attr('profile'))
    if cur_profile_mode is not None and prev_profile_mode != cur_profile_mode:
        hg_obj.set_hybrid_group_profile_mode(cur_profile_mode)

        # Configure Default Breakout for profile mode
        port_list = hg_obj.get_fp_ports()
        if port_list is not None:
            for fpp_num in port_list:

                port = fp.find_front_panel_port(fpp_num)

                # Previous configuration
                prev_br_mode = port.get_breakout_mode()
                prev_port_speed = port.get_port_speed()
                prev_phy_mode = port.get_phy_mode()
                prev_port_profile_name = port.get_profile_type()

                # Current configuration
                hybrid_profile = port.get_hybrid_profile()
                cur_port_profile_name = str(hybrid_profile.get_port_profile(cur_profile_mode))
                port.apply_port_profile(fp.get_port_profile(cur_port_profile_name))
                cur_phy_mode = port.get_def_phy_mode()
                cur_br_mode = port.get_def_breakout()
                cur_port_speed = port.get_default_phy_port_speed()

                rollback_list.append((fpp_num, prev_br_mode, prev_port_speed, prev_phy_mode, prev_port_profile_name))

                # Apply configuration
                rc = fp_utils.set_fp_port_config(fpp_num, cur_br_mode, cur_port_speed, cur_phy_mode)

                if rc is False or fpp_num not in hg_obj.get_fp_ports():
                    nas_if.log_err('failed to config fp_port %d, start rollback' % fpp_num)

                    # Rollback
                    for (fpp, br_mode, port_speed, phy_mode, port_profile_name) in rollback_list:
                        nas_if.log_info('rollback port %d config' % fpp)
                        port = fp.find_front_panel_port(fpp)
                        fp_utils.set_fp_port_config(fpp, br_mode, port_speed, phy_mode)
                        port.apply_port_profile(fp.get_port_profile(port_profile_name))
                    hg_obj.set_hybrid_group_profile_mode(prev_profile_mode)
                    return False

                hwp_speed = pg_utils.get_hwp_speed(cur_br_mode, cur_port_speed, cur_phy_mode)
                port.set_hwp_speed(hwp_speed)


    # Configure User Specified Breakout for profile mode
    port_list = nas_if.get_cps_attr(obj, hg_utils.hg_attr('port'))
    if port_list is not None:
        for fpp_idx in port_list:

            fpp_num = int(port_list[fpp_idx]['port-id'])
            port = fp.find_front_panel_port(fpp_num)

            # Current configuration
            cur_phy_mode = port_list[fpp_idx]['phy-mode']
            cur_br_mode = port_list[fpp_idx]['breakout-mode']
            cur_port_speed = port_list[fpp_idx]['port-speed']

            # Apply configuration
            rc = fp_utils.set_fp_port_config(fpp_num, cur_br_mode, cur_port_speed, cur_phy_mode)

            if rc is False or fpp_num not in hg_obj.get_fp_ports():
                nas_if.log_err('failed to config fp_port %d, start rollback' % fpp_num)

                # Rollback
                for (fpp, br_mode, port_speed, phy_mode, port_profile_name) in rollback_list:
                    nas_if.log_info('rollback port %d config' % fpp)
                    port = fp.find_front_panel_port(fpp)
                    fp_utils.set_fp_port_config(fpp, br_mode, port_speed, phy_mode)
                    port.apply_port_profile(fp.get_port_profile(port_profile_name))
                hg_obj.set_hybrid_group_profile_mode(prev_profile_mode)
                return False

            hwp_speed = pg_utils.get_hwp_speed(cur_br_mode, cur_port_speed, cur_phy_mode)
            port.set_hwp_speed(hwp_speed)
    return True


#Create Hybrid Group State Object
def create_hg_state_cps_obj(hg_obj):

    '''Helper Method to generate Hybrid Group State'''
    if hg_obj is None:
        nas_if.log_err("Hybrid Group is None")
        return None

    obj = cps_object.CPSObject(module='base-pg/dell-pg/port-groups-state/hybrid-group-state',
                               qual='observed',
                               data={hg_utils.hg_state_attr('id'): hg_obj.name,
                                     hg_utils.hg_state_attr('default-profile'): hg_obj.get_hybrid_group_default_profile_mode(),
                                     hg_utils.base_hg_state_attr('front-panel-port'): hg_obj.get_fp_ports(),
                                     hg_utils.base_hg_state_attr('hwport-list'): hg_obj.get_hw_ports()})


    # Add description field for each Hybrid Group.
    for profile_mode_idx,profile_mode in enumerate(hg_obj.get_supported_hybrid_group_profile_modes()):

        profile_description = str(profile_mode)
        for fpp_idx,fpp_num in enumerate(hg_obj.get_fp_ports()):

            port = fp.find_front_panel_port(fpp_num)
            hybrid_profile = port.get_hybrid_profile()
            port_profile_name = str(hybrid_profile.get_port_profile(profile_mode))
            port_profile = fp.get_port_profile(port_profile_name)
            desc = (("; Port %d - %s ") % (fpp_num, port_profile.description))
            profile_description += desc

        obj.add_embed_attr([hg_utils.hg_state_attr('supported-profiles'), str(profile_mode_idx), 'profile-name'], str(profile_mode), 6)
        obj.add_embed_attr([hg_utils.hg_state_attr('supported-profiles'), str(profile_mode_idx), 'profile-description'], str(profile_description), 6)


    for fpp_idx,fpp_num in enumerate(hg_obj.get_fp_ports()):

        port = fp.find_front_panel_port(fpp_num)
        prev_port_profile_name = port.get_profile_type()
        hybrid_profile = port.get_hybrid_profile()

        for profile_mode_idx,profile_mode in enumerate(hybrid_profile.profile_modes):

            cur_port_profile_name = str(hybrid_profile.get_port_profile(profile_mode))
            port.apply_port_profile(fp.get_port_profile(cur_port_profile_name))

            obj.add_embed_attr([hg_utils.hg_state_attr('port'), str(fpp_idx), 'port-id'], str(fpp_num), 6)
            obj.add_embed_attr([hg_utils.hg_state_attr('port'), str(fpp_idx), 'profile', str(profile_mode_idx), 'name'], str(profile_mode), 8)
            obj.add_embed_attr([hg_utils.hg_state_attr('port'), str(fpp_idx), 'profile', str(profile_mode_idx), 'default-phy-mode'], str(port.get_def_phy_mode()), 8)
            obj.add_embed_attr([hg_utils.hg_state_attr('port'), str(fpp_idx), 'profile', str(profile_mode_idx), 'default-breakout-mode'], str(port.get_def_breakout()), 8)
            obj.add_embed_attr([hg_utils.hg_state_attr('port'), str(fpp_idx), 'profile', str(profile_mode_idx), 'default-port-speed'], str(port.get_default_phy_port_speed()), 8)

            cap_idx = 0
            for mode in port.get_breakout_caps():
                for hw_speed in port.get_hwport_speed_caps():

                    phy_npu_speed = fp.get_phy_npu_port_speed(mode, (hw_speed * len(hg_obj.get_hw_ports())) / len(hg_obj.get_fp_ports()))
                    if False is fp.verify_npu_supported_speed(phy_npu_speed):
                        continue

                    obj.add_embed_attr([hg_utils.hg_state_attr('port'), str(fpp_idx), 'profile', str(profile_mode_idx), 'br-cap', str(cap_idx), 'phy-mode'], str(nas_comm.yang.get_value('ether', 'yang-phy-mode')), 10)
                    obj.add_embed_attr([hg_utils.hg_state_attr('port'), str(fpp_idx), 'profile', str(profile_mode_idx), 'br-cap', str(cap_idx), 'breakout-mode'], str(mode), 10)
                    obj.add_embed_attr([hg_utils.hg_state_attr('port'), str(fpp_idx), 'profile', str(profile_mode_idx), 'br-cap', str(cap_idx), 'port-speed'], str(phy_npu_speed), 10)
                    obj.add_embed_attr([hg_utils.hg_state_attr('port'), str(fpp_idx), 'profile', str(profile_mode_idx), 'br-cap', str(cap_idx), 'skip-ports'], str(nas_comm.yang.get_tbl('breakout-to-skip-port')[mode]), 10)
                    cap_idx += 1

        port.apply_port_profile(fp.get_port_profile(prev_port_profile_name))
    return obj


#Create Hybrid Group Object
def create_hg_cps_obj(hg_obj):

    '''Method to create a Hybrid Group object'''
    if hg_obj is None:
        return None
    obj = cps_object.CPSObject(module='base-pg/dell-pg/port-groups/hybrid-group',
                                   qual='target',
                                   data={hg_utils.hg_attr('id'): hg_obj.name,
                                         hg_utils.hg_attr('profile'): hg_obj.get_hybrid_group_profile_mode()})
    for fpp_idx,fpp_num in enumerate(hg_obj.get_fp_ports()):
        port = fp.find_front_panel_port(fpp_num)
        obj.add_embed_attr([hg_utils.hg_attr('port'), str(fpp_idx), 'port-id'], str(fpp_num), 6)
        obj.add_embed_attr([hg_utils.hg_attr('port'), str(fpp_idx), 'phy-mode'], port.get_phy_mode(), 6)
        obj.add_embed_attr([hg_utils.hg_attr('port'), str(fpp_idx), 'breakout-mode'], port.get_breakout_mode(), 6)
        obj.add_embed_attr([hg_utils.hg_attr('port'), str(fpp_idx), 'port-speed'], port.get_port_speed(), 6)
    return obj


def _service_set_hg(hg_name, obj, resp):

    '''Method to Service Hybrid Group Set Request'''
    hg_list = fp.get_hybrid_group_list()

    if hg_name is None or hg_name not in hg_list:
        nas_if.log_err('Error in reading Hybrid Group Name')
        return False

    if True is apply_cps_config_to_hg(obj, hg_list[hg_name]):
        resp = create_hg_cps_obj(hg_list[hg_name]).get()
        return True

    return False


def _set_hg_hdlr(methods, params):

    '''CPS Set Helper for Hybrid Group'''
    obj = cps_object.CPSObject(obj=params['change'])
    resp = params['change']

    if obj.get_key() == hg_utils.hg_key:
        hg_name = nas_if.get_cps_attr(obj, hg_utils.hg_attr('id'))
        return _service_set_hg(hg_name, obj, resp)

    nas_if.log_err("Key Error: Hybrid Group Key issue")
    return False


def set_hg_hdlr(methods, params):

    '''CPS Set Handler for Hybrid Group'''
    ret = False
    try:
        nas_if.log_info("CPS Set Handler for Hybrid Group Invoked")
        ret= _set_hg_hdlr(methods, params)
    except:
        logging.exception('Error: CPS Set for Hybrid Group Failed')
    return ret


def _service_get_hg(hg_name, resp):

    '''Method to Service Hybrid Group Get Request'''
    hg_list = fp.get_hybrid_group_list()

    if hg_list is None:
        nas_if.log_err("Hybrid Group list is None")
        return False

    if hg_name != None and hg_name in hg_list:
        # Add One
        o = create_hg_cps_obj(hg_list[hg_name])
        if None is o:
            nas_if.log_err("Failed to add %s to Hybrid Group list" % str(hg_name))
        resp.append(o.get())
        return True
    else:
        #Add All
        for name in hg_list:
            o = create_hg_cps_obj(hg_list[name])
            if None is o:
                nas_if.log_err("Failed to add %s to Hybrid Group list" % str(name))
            resp.append(o.get())
        return True


def _get_hg_hdlr(methods, params):

    '''CPS Get Helper for Hybrid Group'''
    obj = cps_object.CPSObject(obj=params['filter'])
    resp = params['list']

    if obj.get_key() == hg_utils.hg_key:
        hg_name = nas_if.get_cps_attr(obj, hg_utils.hg_attr('id'))
        return _service_get_hg(hg_name, resp)

    nas_if.log_err("Key Error: Hybrid Group Key issue")
    return False


def get_hg_hdlr(methods, params):

    '''CPS Get Handler for Hybrid Group'''
    ret = False
    try:
        nas_if.log_info("CPS Get Handler for Hybrid Group Invoked")
        ret = _get_hg_hdlr(methods, params)
    except:
        logging.exception('Error: CPS Get for Hybrid Group Failed')
    return ret


def _service_get_hg_state(hg_name, resp):

    '''Method to Service Hybrid Group State Get Request'''
    hg_list = fp.get_hybrid_group_list()

    if hg_list is None:
        nas_if.log_err("Hybrid Group list is None")
        return False

    if hg_name != None and hg_name in hg_list:
        #Add One
        o = create_hg_state_cps_obj(hg_list[hg_name])
        if None is o:
            nas_if.log_err("Failed to add %s to Hybrid Group State list" % str(hg_name))
        resp.append(o.get())
        return True
    else:
        #Add All
        for name in hg_list:
            o = create_hg_state_cps_obj(hg_list[name])
            if None is o:
                nas_if.log_err("Failed to add %s to Hybrid Group State list" % str(name))
            resp.append(o.get())
        return True


def _get_hg_state_hdlr(methods, params):

    '''Helper Method to get Hybrid Group State'''
    obj = cps_object.CPSObject(obj=params['filter'])
    resp = params['list']

    if obj.get_key() == hg_utils.hg_state_key:
        hg_name = nas_if.get_cps_attr(obj, hg_utils.hg_state_attr('id'))
        return _service_get_hg_state(hg_name, resp)

    nas_if.log_err("Key Error: Hybrid Group State Key issue")
    return False


def get_hg_state_hdlr(methods, params):

    '''CPS Get Handler for Hybrid Group State'''
    ret = False
    try:
        nas_if.log_info("CPS Get Handler for Hybrid Group State Invoked")
        ret = _get_hg_state_hdlr(methods, params)
    except:
        logging.exception('Error: CPS Get for Hybrid Group State Failed')
    return ret


def nas_hg_cps_register(handle):

    '''CPS Registration for Hybrid Group State and Hybrid Group State'''
    d = {}
    d['get'] = get_hg_state_hdlr
    cps.obj_register(handle, hg_utils.hg_state_key, d)
    nas_if.log_info("CPS Registeration for Hybrid Group State Succeeded")

    d = {}
    d['get'] = get_hg_hdlr
    d['transaction'] = set_hg_hdlr
    cps.obj_register(handle, hg_utils.hg_key, d)
    nas_if.log_info("CPS Registeration for Hybrid Group Succeeded")
