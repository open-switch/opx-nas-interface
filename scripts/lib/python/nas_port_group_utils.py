
# CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
# LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
# FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
#
# See the Apache Version 2.0 License for specific language governing
# permissions and limitations under the License.

import cps
import cps_object
import cps_utils
import event_log as ev
import nas_front_panel_map as fp
import nas_os_if_utils as nas_if
import nas_fp_port_utils as fp_utils
from nas_common_header import *

import logging

import sys


yang_phy_mode_ether = 1
yang_phy_mode_fc = 2

_yang_breakout_1x1=4
_yang_breakout_2x1=3
_yang_breakout_4x1=2
_yang_breakout_8x2=5 # for DDQSFP28
_yang_breakout_2x2=6 # for DDQSFP28
_yang_breakout_4x2=9 # for DDQSFP28 not yet added inthe yang


_port_group_state_key = cps.key_from_name('observed', 'base-pg/dell-pg/port-groups-state/port-group-state')
_port_group_key = cps.key_from_name('target', 'base-pg/dell-pg/port-groups/port-group')
port_group_state_id_attr = 'dell-pg/port-groups/port-group'



def pg_state_attr(t):
    return 'dell-pg/port-groups-state/port-group-state/' + t

def pg_attr(t):
    return 'dell-pg/port-groups/port-group/' + t

def base_pg_attr(t):
    return 'base-pg/' + pg_attr(t)

def base_pg_state_attr(t):
    return 'base-pg/' + pg_state_attr(t)




# Return Phy port Speed( Yang enum value)  based on Breakout mode , hwport speed( in Mbps), Phy mode

# Returns hwp port speed in the Ethernet Mbps ( 10000 Mbps or 25000 Mbps)
# Phy_speed in yang enum value
# fp breakout mode
# Phy mode Fc/Ether
def get_hwp_speed(breakout, phy_speed, phy_mode):
    hwp_count =  breakout_to_hwp_count[breakout]
    _phy_speed_mbps = nas_if.from_yang_speed(phy_speed)
    if _phy_speed_mbps == None or hwp_count == None:
        return None
    _hwp_speed_mbps = _phy_speed_mbps/hwp_count
    if phy_mode  == yang_phy_mode_fc:
        hwp_speed_eth =  get_hwp_fc_to_eth_speed(nas_if.to_yang_speed(_hwp_speed_mbps))
        return nas_if.from_yang_speed(hwp_speed_eth)
    else:
        return _hwp_speed_mbps

# Create port group capability object for  port group object get request.
# Each Capability object includes breakout mode, phy mode and phy port speed
def create_and_add_pg_caps(pg, phy_mode, resp):
    br_modes = pg.get_breakout_caps()
    hwp_speeds = pg.get_hwport_speed_caps()
    hwp_count = len(pg.get_hw_ports())
    for mode in br_modes:
        skip_ports = breakout_to_skip_port[mode]
        for speed in hwp_speeds:
            phy_npu_speed = fp.get_phy_npu_port_speed(mode, speed, hwp_count)
            if fp.verify_npu_supported_speed(phy_npu_speed) == False:
#                nas_if.log_err("create_and_add_pg_caps: br_mode %d doesn't support speed %d " % (mode, phy_npu_speed))
#               don't add this entry of spped or beakout
                continue

            phy_speed = phy_npu_speed
            if phy_mode == get_value(yang_phy_mode, 'fc'):
                phy_speed = fp.get_fc_speed_frm_npu_speed(phy_npu_speed)
            cps_obj = cps_object.CPSObject(module='base-pg/dell-pg/port-groups-state/port-group-state/br-cap',
                                           qual='observed',
                data={pg_state_attr('br-cap/phy-mode'):phy_mode,
                    pg_state_attr('br-cap/breakout-mode'):mode,
                    pg_state_attr('br-cap/port-speed'):phy_speed,
                    pg_state_attr('br-cap/skip-ports'):skip_ports,
                    pg_state_attr('id'):pg.name })
            resp.append(cps_obj.get())
            cps_obj = None

# Add capability object for the port group in the cps get request
def add_all_pg_caps(pg, resp):
    if pg == None:
        return False
    create_and_add_pg_caps(pg, yang_phy_mode_ether, resp)
    if pg.is_fc_supported():
        create_and_add_pg_caps(pg, yang_phy_mode_fc, resp)

# Get request for port group state object
def create_and_add_pg_state_obj(pg, resp):
    if pg == None:
        return False
    cps_obj = cps_object.CPSObject(module='base-pg/dell-pg/port-groups-state/port-group-state', qual='observed',
            data={base_pg_state_attr('front-panel-port'):pg.get_fp_ports(),
                  base_pg_state_attr('hwport-list'):pg.get_hw_ports(),
                  pg_state_attr('id'):pg.name })
    resp.append(cps_obj.get())
    add_all_pg_caps(pg,resp)

    return True

def gen_port_group_state_list(obj, resp):
    pg_name = nas_if.get_cps_attr(obj,pg_state_attr('id'))
    pg_list = fp.get_port_group_list()
    if pg_name != None:
        pg = pg_list[pg_name]
        return create_and_add_pg_state_obj(pg, resp)
    else:
        # return all port groups
        for name in pg_list:
            ret = create_and_add_pg_state_obj(pg_list[name], resp)
        return True

def create_pg_cps_obj(pg):
    if pg == None:
        return None
    cps_obj = cps_object.CPSObject(module='base-pg/dell-pg/port-groups/port-group', qual='target',
            data={pg_attr('id'):pg.name,
                pg_attr('phy-mode'):pg.get_phy_mode(),
                pg_attr('breakout-mode'):pg.get_breakout_mode(),
                pg_attr('port-speed'):pg.get_port_speed()
                  })
    return cps_obj

def gen_port_group_list(obj, resp):
    pg_name = nas_if.get_cps_attr(obj,pg_attr('id'))
    pg_list = fp.get_port_group_list()
    if pg_name != None:
        pg = pg_list[pg_name]
        obj = create_pg_cps_obj(pg)
        if obj == None:
            return False
        resp.append(obj.get())

    else:
        # return all port groups
        for name in pg_list:
            obj = create_pg_cps_obj(pg_list[name])
            if obj != None:
                resp.append(obj.get())
        return True

def get_pg_cb(methods, params):
    print " Get port group callback "
    try:
        ret= get_int_pg_cb(methods, params)
    except:
        logging.exception('error:')
    return ret

def get_int_pg_cb(methods, params):
    obj = cps_object.CPSObject(obj=params['filter'])
    resp = params['list']

    if obj.get_key() == _port_group_key:
        gen_port_group_list(obj, resp)
    else:
        return False
    return True

# following is the mapping of breakout mode to valid front panel port offset
sfp_pg_br_to_fp_map = {
        yang_breakout['4x4']: [0, 1, 2, 3],
        yang_breakout['2x4']: [0, 2],
        }

# Set config handler for unified SFP port group type.
def set_sfp_port_group_config(pg, br_mode, port_speed, phy_mode):
    # Get the list of front port and HW Ports
    fp_list = pg.get_fp_ports()
    hwp_list = pg.get_hw_ports()
    fp_list.sort() # make sure that fp is in the sorted order in the list
    # set the HW port mapping to 0 fpr each fp
    for port in fp_list:
        ret = fp_utils.set_fp_to_hwp_mapping(port, br_mode, port_speed, phy_mode, None)
        if ret == False:
            # TODO rollback
            return False
    # set the HW port mapping based on the breakout mode

    # re-verify this case
    hwp_count = breakout_to_hwp_count[br_mode]
    hwp_list.sort()
    hwp_list.reverse()  # so that we can pop out from the end
    first_fp_port = fp_list[0]
    nas_if.log_info('br_mode %s hwport list  %s fp_list %s' % (str(br_mode), str(hwp_list), str(fp_list)))
    for port in fp_list:
        port_offset = port - first_fp_port
        if port_offset not in sfp_pg_br_to_fp_map[br_mode]:
            continue # skip this fp port if not valid in this breakout mode
        if len(hwp_list) < hwp_count:
            nas_if.log_err(' some problem in setting fp port to hwp mapping')
            return False
        hwports = []
        i =0
        while i < hwp_count:
            hwports.append(hwp_list.pop())
            i = i + 1
        ret = fp_utils.set_fp_to_hwp_mapping(port, br_mode, port_speed, phy_mode, hwports)
        if ret == False:
            # TODO rollback
            return False
    return True

# Set breakout mode to DDQSFP28 port group
def set_ddqsfp_port_group_config(pg, fp_br_mode, port_speed, phy_mode):
    fp_list = pg.get_fp_ports()
    if fp_list == None:
        print 'FP list is empty'
        return False

    for fp_port in fp_list:
        ret = fp_utils.set_fp_port_config(fp_port, fp_br_mode, port_speed, phy_mode)
        if ret == False:
            # Rollback
            return False
    return True

# Generic port group config handler
def set_port_group_config(pg, phy_mode, br_mode, port_speed):
    nas_if.log_info('config port group %s' % pg.name)

    fp_list = pg.get_fp_ports()
    pg_type = pg.get_profile_type()
    fp_br_mode = br_mode
    if pg_type == "unified_sfp":
        set_sfp_port_group_config(pg, fp_br_mode, port_speed, phy_mode)
    # If any of the value is None then skip it
    elif pg_type == 'unified_qsfp28' or pg_type == 'ethernet_qsfp' or pg_type == 'unified_qsfp':
        for port in fp_list:
            ret = fp_utils.set_fp_port_config(port, fp_br_mode, port_speed, phy_mode)
            if ret == False:
                return False
    elif pg_type == 'ethernet_ddqsfp28':
        fp_br_mode = ddqsfp_2_qsfp_brmode[br_mode]
        ret = set_ddqsfp_port_group_config(pg, fp_br_mode, port_speed, phy_mode)
    else:
        nas_if.log_err('Uknown port group type %s' % pg_type)
        return False

    if phy_mode != None:
        pg.set_phy_mode(phy_mode)
    if br_mode != None:
        pg.set_breakout_mode(br_mode)
    if port_speed != None:
        pg.set_port_speed(port_speed)
    hwp_speed = get_hwp_speed(fp_br_mode, port_speed, phy_mode)
    pg.set_hwp_speed(hwp_speed)
    return True

def set_int_pg_cb(methods, params):
    nas_if.log_info( "set port group config")
    obj = cps_object.CPSObject(obj=params['change'])
    if obj.get_key() == _port_group_key:
        pg_name = nas_if.get_cps_attr(obj,pg_attr('id'))
        if pg_name == None:
            nas_if.log_err('port group name not present')
            return False
        phy_mode = nas_if.get_cps_attr(obj,pg_attr('phy-mode'))
        br_mode = nas_if.get_cps_attr(obj,pg_attr('breakout-mode'))
        port_speed = nas_if.get_cps_attr(obj,pg_attr('port-speed'))
        pg_list = fp.get_port_group_list()
        ret = set_port_group_config(pg_list[pg_name], phy_mode, br_mode, port_speed)
        if ret == True:
            obj = create_pg_cps_obj(pg_list[pg_name])
            params['change'] = obj.get()
    else:
        return False

    return True

def set_pg_cb(methods, params):
    try:
        ret= set_int_pg_cb(methods, params)
    except:
        logging.exception('error:')
    return ret

def get_pg_state_cb(methods, params):
    try:
        ret= get_int_pg_state_cb(methods, params)
    except:
        logging.exception('error:')
    return ret


def get_int_pg_state_cb(methods, params):
    obj = cps_object.CPSObject(obj=params['filter'])
    resp = params['list']

    if obj.get_key() == _port_group_state_key:
        gen_port_group_state_list(obj, resp)
    else:
        return False

    return True


def nas_pg_cps_register(handle):
    d = {}
    d['get'] = get_pg_state_cb
    cps.obj_register(handle, _port_group_state_key, d)

    d = {}
    d['get'] = get_pg_cb
    d['transaction'] = set_pg_cb
    cps.obj_register(handle, _port_group_key, d)



