from nas_mac_addr_utils import get_alloc_mac_addr_params, if_get_mac_addr
import cps_utils
import cps_object
import nas_common_header as nas_comm
import nas_os_if_utils as nas_if

fp_cache = nas_if.FpPortCache()

def check_mac_addr_get(if_type, fp_port = None, vlan_id = None, lag_id = None):
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
    mac_addr = if_get_mac_addr(**param_list)
    assert mac_addr is not None

    tr = cps_utils.CPSTransaction([('rpc', cps_obj.get())])
    ret_list = tr.commit()
    assert ret_list != False
    assert len(ret_list) == 1
    ret_obj = cps_object.CPSObject(obj = ret_list[0]['change'])
    ret_mac_addr = ret_obj.get_attr_data('dell-if/if/interfaces/interface/phys-address')
    assert mac_addr == ret_mac_addr

    print 'mac address: %s' % mac_addr
    return True

def test_fp_port_mac_addr_get():
    for port_id in range(32):
        assert check_mac_addr_get('front-panel', fp_port = port_id + 1)

def test_vlan_mac_addr_get():
    for vlan_id in range(100, 110):
        assert check_mac_addr_get('vlan', vlan_id = vlan_id)

def test_lag_mac_addr_get():
    for lag_id in range(1, 10):
        assert check_mac_addr_get('lag', lag_id = lag_id)
