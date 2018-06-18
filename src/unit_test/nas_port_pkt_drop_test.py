import subprocess
import pytest
import cps
import cps_object

test_intf = 'e101-002-0'
test_vlan_id = 100

def run_command(cmd, response = None):
    prt = subprocess.Popen(
        cmd,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    if response is not None:
        for line in prt.stdout.readlines():
            respose.append(line.rstrip())
    retval = prt.wait()
    return retval

def get_intf_tagging_mode(if_name):
    cps_obj = cps_object.CPSObject(module = 'dell-base-if-cmn/if/interfaces/interface',
                                   data = {'if/interfaces/interface/name': if_name})
    resp = []
    assert cps.get([cps_obj.get()], resp)
    assert len(resp) != 0
    ret_obj = cps_object.CPSObject(obj = resp[0])
    try:
        mode = ret_obj.get_attr_data('base-if-phy/if/interfaces/interface/tagging-mode')
    except ValueError:
        mode = None
    return mode

def test_packet_drop_status():
    # create vlan, delete port from default vlan 1 and add untagged port to new vlan
    assert run_command('cps_config_vlan.py --add --id %d --vlantype 1' % test_vlan_id) == 0
    assert run_command('cps_config_vlan.py --delport --name br1 --port %s' % test_intf) == 0
    assert run_command('cps_config_vlan.py --addport --name br%d --port %s' % (test_vlan_id, test_intf)) == 0
    # no drop enabled
    assert get_intf_tagging_mode(test_intf) == 3
    # delete untagged port from vlan
    assert run_command('cps_config_vlan.py --delport --name br%d --port %s' % (test_vlan_id, test_intf)) == 0
    # untagged drop should be enabled
    assert get_intf_tagging_mode(test_intf) == 2
    # add tagged port to vlan
    assert run_command('cps_config_vlan.py --addport --name br%d -t --port %s' % (test_vlan_id, test_intf)) == 0
    # untagged drop status should not change
    assert get_intf_tagging_mode(test_intf) == 2
    assert run_command('cps_config_vlan.py --delport --name br%d -t --port %s' % (test_vlan_id, test_intf)) == 0
    assert get_intf_tagging_mode(test_intf) == 2
    # add port back to default vlan 1
    assert run_command('cps_config_vlan.py --addport --name br1 --port %s' % test_intf) == 0
    assert get_intf_tagging_mode(test_intf) == 3

    # delete vlan
    assert run_command('cps_config_vlan.py --del --name br%d' % test_vlan_id) == 0
