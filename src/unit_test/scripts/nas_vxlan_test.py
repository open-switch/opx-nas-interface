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
import os
import subprocess
import pytest
import nas_vxlan_utils as vxlan_utils

def prRed(prt): print("\033[91m {}\033[00m".format(prt))
def prGreen(prt): print("\033[95m {}\033[00m".format(prt))

def exec_shell(cmd):
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
    (out, err) = proc.communicate()
    prGreen(out)
    return out

def test_1():
    #PRE-TEST Prep
    exec_shell('brctl delbr br400')
    exec_shell('ip link delete dev vtep500')
    #Create a VTEP 500
    prRed("Test Description: Create vtep500")
    assert vxlan_utils.create_vtep('vtep500', 500, '10.1.1.1', 'ipv4') == True
    exec_shell('ip link show dev vtep500')

def test_2():
    #Add a remote endpoint(1.1.1.1) to vtep500
    prRed("Test Description: Add remote endpoint(1.1.1.1) to vtep500")
    re = vxlan_utils.RemoteEndpoint('1.1.1.1', 'ipv4', 1, "00:00:00:00:00:11")
    remote_endpoints = [re]
    assert vxlan_utils.add_remote_endpoint('vtep500', remote_endpoints) == True
    exec_shell('bridge fdb | grep vtep500')

def test_3():
    #PRE-TEST Prep
    exec_shell('ip link delete dev vtep600')
    #Create a VTEP 600 with remote endpoints
    prRed("Test Description: Create vtep600 with Remote endpoint(1.1.1.2)")
    re = vxlan_utils.RemoteEndpoint('1.1.1.2', 'ipv4', 1, "00:00:00:00:00:22")
    remote_endpoints = [re]
    assert vxlan_utils.create_vtep_with_remote_endpoints('vtep600', 600, '10.1.1.2', 'ipv4', remote_endpoints) == True
    exec_shell('ip link show dev vtep600')

def test_4():
    #Remove vtep500 remote endpoint(1.1.1.2) (1.1.1.2 doesn't exist)
    prRed("Test Description: Remove remote endpoint(1.1.1.2) from vtep500")
    re = vxlan_utils.RemoteEndpoint('1.1.1.2', 'ipv4', 1, "00:00:00:00:00:22")
    remote_endpoints = [re]
    assert vxlan_utils.remove_remote_endpoint('vtep500', remote_endpoints) == False
    exec_shell('bridge fdb | grep vtep500')

def test_5():
    #Remove vtep500 remote endpoint(1.1.1.1)
    prRed("Test Description: Remove remote endpoint(1.1.1.1) from vtep500")
    re = vxlan_utils.RemoteEndpoint('1.1.1.1', 'ipv4', 1, "00:00:00:00:00:11")
    remote_endpoints = [re]
    assert vxlan_utils.remove_remote_endpoint('vtep500', remote_endpoints) == True
    exec_shell('bridge fdb | grep vtep500')

def test_6():
    #PRE-TEST Prep
    exec_shell('ip link delete dev e101-010-0.200')
    exec_shell('ip link delete dev e101-011-0.200')
    #Create a Bridge br400 with tagged access ports(e101-010-0.200, e101-011-0.200) and tunnel endpoints(vtep500, vtep600)
    prRed("Test Description: Create bridge:br400; Tagged ports: e101-010-0.200, e101-011-0.200; VTEPs: vtep500, vtep600")
    vxlan_utils.create_vlan_subintf('e101-010-0', 200)
    vxlan_utils.create_vlan_subintf('e101-011-0', 200)
    tap1 = 'e101-010-0.200'
    tap2 = 'e101-011-0.200'
    tagged_access_ports = [tap1, tap2]
    member_interfaces = [] + tagged_access_ports + ['vtep500', 'vtep600']
    assert vxlan_utils.create_bridge_interface('br400', member_interfaces) == True
    exec_shell('brctl show')

def test_7():
    #PRE-TEST Prep
    exec_shell('ip link delete dev e101-012-0.300')
    exec_shell('ip link delete dev e101-013-0.300')
    #Add Tagged access ports e101-012-0.300, e101-013-0.300 to Bridge br400
    prRed("Test Description: Add Tagged ports to bridge br400: e101-012-0.300, e101-013-0.300")
    vxlan_utils.create_vlan_subintf('e101-012-0', 300)
    vxlan_utils.create_vlan_subintf('e101-013-0', 300)
    tap1 = 'e101-012-0.300'
    tap2 = 'e101-013-0.300'
    tagged_access_ports = [tap1, tap2]
    assert vxlan_utils.add_tagged_access_ports_to_bridge_interface('br400', tagged_access_ports) == True
    exec_shell('brctl show')

def test_8():
    #Remove Tagged access ports e101-010-0.200, e101-012-0.300 to Bridge br400
    prRed("Test Description: Remove tagged access ports: e101-010-0.200, e101-012-0.300")
    tap1 = 'e101-010-0.200'
    tap2 = 'e101-012-0.300'
    tagged_access_ports = [tap1, tap2]
    assert vxlan_utils.remove_tagged_access_ports_to_bridge_interface('br400', tagged_access_ports) == True
    exec_shell('brctl show')

def test_9():
    #Remove tunnel Endpoints vtep500, vtep600 from Bridge
    prRed("Test Description: Remove vtep500, vtep600 from br400")
    assert vxlan_utils.remove_vteps_from_bridge_interface('br400', ['vtep500', 'vtep600']) == True
    exec_shell('brctl show')

def test_10():
    #Add tunnel Endpoints vtep500, vtep600 to Bridge
    prRed("Test Description: Add VTEPS: vtep500, vtep600 to br400")
    assert vxlan_utils.add_vteps_to_bridge_interface('br400', ['vtep500', 'vtep600']) == True
    exec_shell('brctl show')

def test_11():
    #Remove tunnel Endpoints vtep700, vtep600 from Bridge
    prRed("Test Description: Remove vtep700, vtep600 from br400")
    assert vxlan_utils.remove_vteps_from_bridge_interface('br400', ['vtep700', 'vtep600']) == False
    exec_shell('brctl show')

def test_12():
    prRed("Test Description: Delete br400")
    assert vxlan_utils.delete_bridge_interface('br400') == True
    exec_shell('brctl show')
    exec_shell('ip link show e101-010-0.200')
    exec_shell('ip link show e101-011-0.200')
    exec_shell('ip link show e101-012-0.300')
    exec_shell('ip link show e101-013-0.300')

def test_13():
    prRed("Test Description: Delete vtep500")
    assert vxlan_utils.delete_vtep('vtep500') == True
    exec_shell('ip link show dev vtep500')

def test_14():
    prRed("Test Description: Delete vtep600")
    assert vxlan_utils.delete_vtep('vtep600') == True
    exec_shell('ip link show dev vtep600')

def test_15():
    prRed("Test Description: Delete e101-010-0.200, e101-011-0.200, e101-012-0.300, e101-013-0.300")
    assert vxlan_utils.delete_vlan_subintf('e101-010-0', 200) == True
    assert vxlan_utils.delete_vlan_subintf('e101-011-0', 200) == True
    assert vxlan_utils.delete_vlan_subintf('e101-012-0', 300) == True
    assert vxlan_utils.delete_vlan_subintf('e101-013-0', 300) == True
    exec_shell('ip link show e101-010-0.200')
    exec_shell('ip link show e101-011-0.200')
    exec_shell('ip link show e101-012-0.300')
    exec_shell('ip link show e101-013-0.300')

tests = {
    1: test_1,
    2: test_2,
    3: test_3,
    4: test_4,
    5: test_5,
    6: test_6,
    7: test_7,
    8: test_8,
    9: test_9,
    10:test_10,
    11:test_11,
    12:test_12,
    13:test_13,
    14:test_14,
    15:test_15,
}

if __name__ == '__main__':
    idx = 1
    while(idx <= len(tests)):
        tests[idx]()
        raw_input("Press Enter to continue...")
        idx += 1
