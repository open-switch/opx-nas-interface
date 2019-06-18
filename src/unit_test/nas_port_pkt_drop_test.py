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
import subprocess
import pytest
import cps
import cps_object

test_intf = 'e101-002-0'
test_vlan_id = 100

PORT_ACCEPT_UNTAGGED = 1
PORT_ACCEPT_TAGGED = 2
PORT_ACCEPT_BOTH = 3

def run_command(cmd, response = None):
    prt = subprocess.Popen(
        cmd,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    if response is not None:
        for line in prt.stdout.readlines():
            response.append(line.rstrip())
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

def test_default_drop_status():
    # get untagged members under default vlan
    untagged_intf_list = None
    resp = []
    assert run_command('cps_config_vlan.py --show --name br1', resp) == 0
    for ret_line in resp:
        tokens = ret_line.split('=')
        if len(tokens) < 2:
            continue
        if tokens[0].strip() == 'dell-if/if/interfaces/interface/untagged-ports':
            untagged_intf_list = tokens[1].strip().split(',')
            break
    if untagged_intf_list is None:
        print 'No untagged member for default VLAN'
        return
    for intf in untagged_intf_list:
        print 'Testing default drop status for interface %s' % intf
        assert get_intf_tagging_mode(intf) == PORT_ACCEPT_UNTAGGED

def test_vlan_packet_drop_status():
    print 'Testing drop status change for interface %s with VLAN %d' % (test_intf, test_vlan_id)
    # create vlan, delete port from default vlan 1 and add untagged port to new vlan
    assert run_command('cps_config_vlan.py --add --id %d --vlantype 1' % test_vlan_id) == 0
    assert run_command('cps_config_vlan.py --delport --name br1 --port %s' % test_intf) == 0
    assert run_command('cps_config_vlan.py --addport --name br%d --port %s' % (test_vlan_id, test_intf)) == 0
    # tagged packets will be dropped for untagged member port
    assert get_intf_tagging_mode(test_intf) == PORT_ACCEPT_UNTAGGED
    # delete untagged port from vlan
    assert run_command('cps_config_vlan.py --delport --name br%d --port %s' % (test_vlan_id, test_intf)) == 0
    # if port is not member of any vlan, it will not drop any kind of packets
    assert get_intf_tagging_mode(test_intf) == PORT_ACCEPT_BOTH
    # add tagged port to vlan
    assert run_command('cps_config_vlan.py --addport --name br%d -t --port %s' % (test_vlan_id, test_intf)) == 0
    # untagged packets will be dropped for tagged member port
    assert get_intf_tagging_mode(test_intf) == PORT_ACCEPT_TAGGED
    assert run_command('cps_config_vlan.py --delport --name br%d -t --port %s' % (test_vlan_id, test_intf)) == 0
    # port is removed from vlan as tagged member, it will not drop any kind of packets
    assert get_intf_tagging_mode(test_intf) == PORT_ACCEPT_BOTH
    # add port back to default vlan 1
    assert run_command('cps_config_vlan.py --addport --name br1 --port %s' % test_intf) == 0
    # port is restored to default status
    assert get_intf_tagging_mode(test_intf) == PORT_ACCEPT_UNTAGGED

    # delete vlan
    assert run_command('cps_config_vlan.py --del --name br%d' % test_vlan_id) == 0
