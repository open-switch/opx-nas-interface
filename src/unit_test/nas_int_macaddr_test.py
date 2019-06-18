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
from nas_mac_addr_utils import get_alloc_mac_addr_params, if_get_mac_addr
import cps_utils
import cps_object
import nas_common_header as nas_comm
import nas_os_if_utils as nas_if

fp_cache = nas_if.FpPortCache()

def check_interface_mac_addr_get(if_type, fp_port = None, vlan_id = None, lag_id = None):
    cps_obj = cps_object.CPSObject(module = 'dell-base-if-cmn/get-mac-address')
    if if_type == 'front-panel':
        if fp_port is None:
            print 'Front panel port ID is required for front-panel type'
            return False
        print 'checking mac address service for front-panel-port %d' % fp_port
        cps_obj.add_attr(nas_comm.yang.get_value('fp_port', 'attr_name'), fp_port)
        cps_obj.add_attr(nas_comm.yang.get_value('intf_type', 'attr_name'),
                         'ianaift:ethernetCsmacd')
    elif if_type == 'vlan':
        if vlan_id is None:
            print 'VLAN ID is required for vlan type'
            return False
        print 'checking mac address service for vlan %d' % vlan_id
        cps_obj.add_attr(nas_comm.yang.get_value('vlan_id', 'attr_name'), vlan_id)
        cps_obj.add_attr(nas_comm.yang.get_value('intf_type', 'attr_name'),
                         'ianaift:l2vlan')
    elif if_type == 'lag':
        if lag_id is None:
            print 'LAG ID is required for lag type'
            return False
        print 'checking mac address service for lag %d' % lag_id
        lag_name = 'bo%d' % lag_id
        cps_obj.add_attr(nas_comm.yang.get_value('if_name', 'attr_name'), lag_name)
        cps_obj.add_attr(nas_comm.yang.get_value('intf_type', 'attr_name'),
                         'ianaift:ieee8023adLag')
    elif if_type == 'virtual-network':
        cps_obj.add_attr(nas_comm.yang.get_value('intf_type', 'attr_name'),
                         'base-if:virtualNetwork')
    else:
        print 'Invalid interface type: %s' % if_type
        return False
    param_list = get_alloc_mac_addr_params(if_type, cps_obj, fp_cache)
    assert param_list is not None
    if if_type == 'front-panel':
        assert 'fp_mac_offset' in param_list
    elif if_type == 'vlan':
        assert 'vlan_id' in param_list
        assert param_list['vlan_id'] == vlan_id
    elif if_type == 'lag':
        assert 'lag_id' in param_list
        assert param_list['lag_id'] == lag_id
    mac_addr_info = if_get_mac_addr(**param_list)
    assert mac_addr_info is not None
    mac_addr, _ = mac_addr_info

    tr = cps_utils.CPSTransaction([('rpc', cps_obj.get())])
    ret_list = tr.commit()
    assert ret_list != False
    assert len(ret_list) == 1
    ret_obj = cps_object.CPSObject(obj = ret_list[0]['change'])
    ret_mac_addr = ret_obj.get_attr_data('dell-if/if/interfaces/interface/phys-address')
    assert mac_addr == ret_mac_addr

    print 'mac address: %s' % mac_addr
    return True

def check_application_mac_addr_get(appl_name, type_name, offset = 0):
    cps_obj = cps_object.CPSObject(module = 'dell-base-if-cmn/get-mac-address')
    cps_obj.add_attr('dell-base-if-cmn/get-mac-address/input/application', appl_name)
    cps_obj.add_attr('dell-base-if-cmn/get-mac-address/input/type', type_name)
    cps_obj.add_attr('dell-base-if-cmn/get-mac-address/input/offset', offset)
    tr = cps_utils.CPSTransaction([('rpc', cps_obj.get())])
    ret_list = tr.commit()
    assert ret_list != False
    assert len(ret_list) == 1
    ret_obj = cps_object.CPSObject(obj = ret_list[0]['change'])
    ret_mac_addr = ret_obj.get_attr_data('dell-if/if/interfaces/interface/phys-address')

    print 'mac address for application %s type %s offset %d: %s' % (appl_name, type_name, offset, ret_mac_addr)
    return True

def test_fp_port_mac_addr_get():
    for port_id in range(32):
        assert check_interface_mac_addr_get('front-panel', fp_port = port_id + 1)

def test_vlan_mac_addr_get():
    for vlan_id in range(100, 110):
        assert check_interface_mac_addr_get('vlan', vlan_id = vlan_id)

def test_lag_mac_addr_get():
    for lag_id in range(1, 10):
        assert check_interface_mac_addr_get('lag', lag_id = lag_id)
def test_virt_network_mac_addr_get():
    assert check_interface_mac_addr_get('virtual-network')

def test_fc_mac_addr_get():
    assert check_application_mac_addr_get('fc', 'fcf')
    assert check_application_mac_addr_get('vrf', 'routing')
