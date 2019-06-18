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
import cps_object
import cps_utils
import cps
import nas_hybrid_group_utils as hg_utils
import nas_common_header as nas_comm
import nas_os_if_utils as nas_if

def prRed(prt): print("\033[91m {}\033[00m".format(prt))


def prGreen(prt): print("\033[95m {}\033[00m".format(prt))


def exec_shell(cmd):
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
    (out, err) = proc.communicate()
    return out


def _create_intf(intf_name):
    exec_shell("nas_intf_set.py -o create -t ether " + str(intf_name))


def _delete_intf(intf_name):
    exec_shell("nas_intf_set.py -o delete " + str(intf_name))


def _print_hg_details(hybrid_group_name):
    ret = exec_shell("opx-config-hybrid-group get hybrid-group --name " + str(hybrid_group_name))
    prGreen(ret)


def _check_hg_support_on_platform():
    resp = hg_utils.get_hg()
    if len(resp) == 0:
        prRed("Hybrid Group is not supported on this platform. Aborting Test")
        return False
    return True


def _configure_hg_profile(hybrid_group_name, profile_name):
    exec_shell("opx-config-hybrid-group set hybrid-group profile --name " + str(hybrid_group_name) + " --profile " + str(profile_name))

def _check_hg_profile(hybrid_group_name, profile_name):
    #Get Hybrid Group Object
    resp = hg_utils.get_hg(str(hybrid_group_name))
    assert len(resp) == 1
    for o in resp:
        assert o is not None
    obj = cps_object.CPSObject(obj=o)

    #Check if name is correct
    hg_name = nas_if.get_cps_attr(obj, hg_utils.hg_attr('id'))
    assert hg_name == str(hybrid_group_name)

    #Check profile Type
    hg_profile = nas_if.get_cps_attr(obj, hg_utils.hg_attr('profile'))
    assert hg_profile == str(profile_name)


def _configure_hg_port_breakout(hybrid_group_name, port_id, br_mode):
    exec_shell("opx-config-hybrid-group set hybrid-group port --name " + str(hybrid_group_name) + " --port-id " + str(port_id) + " --breakout " + str(br_mode))


def _check_hg_port_breakout(hybrid_group_name, port_id, br_mode):
    #Get Hybrid Group Object
    resp = hg_utils.get_hg(str(hybrid_group_name))
    assert len(resp) == 1
    for o in resp:
        assert o is not None
    obj = cps_object.CPSObject(obj=o)

    #Check if name is correct
    hg_name = nas_if.get_cps_attr(obj, hg_utils.hg_attr('id'))
    assert hg_name == str(hybrid_group_name)

    port_list = nas_if.get_cps_attr(obj, hg_utils.hg_attr('port'))
    for port_idx in port_list:
        port = port_list[port_idx]
        pr_id = port['port-id']
        if str(pr_id) == str(port_id):
            phy_mode = port['phy-mode']
            breakout_mode = port['breakout-mode']
            port_speed = port['port-speed']
            breakout_option = (breakout_mode, port_speed)
            breakout = nas_comm.yang.get_key(breakout_option, 'yang-breakout-port-speed')
            assert str(breakout) == str(br_mode)
            break


def cps_attr_get(cps_data, attr):
    try:
        return cps_utils.cps_attr_types_map.from_data(attr, cps_data[attr])
    except:
        return ""


def z9264_test_1():

    #Description
    prGreen("Test Description: CONFIGURE 'hybrid-group-32' to 'unrestricted' mode & check for default breakouts")

    #Test Prep
    if False == _check_hg_support_on_platform():
        return True
    _delete_intf("e101-063-0")
    _delete_intf("e101-064-0")

    #Apply Hybrid Group Profile as 'unrestricted' & Default Breakouts
    _configure_hg_profile("hybrid-group-32", "unrestricted")

    #Check Hybrid Group Profile as 'unrestricted' & Default Breakouts
    _check_hg_profile("hybrid-group-32", "unrestricted")
    _check_hg_port_breakout("hybrid-group-32", "63", "100gx1")
    _check_hg_port_breakout("hybrid-group-32", "64", "100gx1")
    _print_hg_details("hybrid-group-32")

    #Test Cleanup
    _create_intf("e101-063-0")
    _create_intf("e101-064-0")

    return True


def z9264_test_2():

    #Description
    prGreen("Test Description: CONFIGURE 'hybrid-group-32' to 'restricted' mode & check for default breakouts")

    #Test Prep
    if False == _check_hg_support_on_platform():
        return True
    _delete_intf("e101-063-0")
    _delete_intf("e101-064-0")

    #Apply Hybrid Group Profile as 'restricted' & Default Breakouts
    _configure_hg_profile("hybrid-group-32", "restricted")

    #Check Hybrid Group Profile as 'restricted' & Default Breakouts
    _check_hg_profile("hybrid-group-32", "restricted")
    _check_hg_port_breakout("hybrid-group-32", "63", "100gx1")
    _check_hg_port_breakout("hybrid-group-32", "64", "disabled")
    _print_hg_details("hybrid-group-32")

    #Test Cleanup
    _configure_hg_profile("hybrid-group-32", "unrestricted")
    _create_intf("e101-063-0")
    _create_intf("e101-064-0")

    return True


def z9264_test_3():

    #Description
    prGreen("Test Description: CONFIGURE 'hybrid-group-32' to 'restricted' mode, configure 25gx4 on Port 1, disabled on port 2")

    #Test Prep
    if False == _check_hg_support_on_platform():
        return True
    _delete_intf("e101-063-0")
    _delete_intf("e101-064-0")

    #Apply Hybrid Group Profile as 'restricted' & Default Breakouts
    _configure_hg_profile("hybrid-group-32", "restricted")

    #Check Hybrid Group Profile as 'restricted' & Default Breakouts
    _check_hg_profile("hybrid-group-32", "restricted")
    _check_hg_port_breakout("hybrid-group-32", "63", "100gx1")
    _check_hg_port_breakout("hybrid-group-32", "64", "disabled")

    #Configure 25gx4 Breakouts
    _configure_hg_port_breakout("hybrid-group-32", "63", "25gx4")
    _configure_hg_port_breakout("hybrid-group-32", "64", "disabled")

    #Check 25gx4 Breakouts
    _check_hg_port_breakout("hybrid-group-32", "63", "25gx4")
    _check_hg_port_breakout("hybrid-group-32", "64", "disabled")
    _print_hg_details("hybrid-group-32")

    #Test Cleanup
    _configure_hg_profile("hybrid-group-32", "unrestricted")
    _create_intf("e101-063-0")
    _create_intf("e101-064-0")

    return True


def z9264_test_4():

    #Description
    prGreen("Test Description: CONFIGURE 'hybrid-group-32' to 'restricted' mode, configure 10gx4 on Port 1, disabled on port 2")

    #Test Prep
    if False == _check_hg_support_on_platform():
        return True
    _delete_intf("e101-063-0")
    _delete_intf("e101-064-0")

    #Apply Hybrid Group Profile as 'restricted' & Default Breakouts
    _configure_hg_profile("hybrid-group-32", "restricted")

    #Check Hybrid Group Profile as 'restricted' & Default Breakouts
    _check_hg_profile("hybrid-group-32", "restricted")
    _check_hg_port_breakout("hybrid-group-32", "63", "100gx1")
    _check_hg_port_breakout("hybrid-group-32", "64", "disabled")

    #Configure 10gx4 Breakouts
    _configure_hg_port_breakout("hybrid-group-32", "63", "10gx4")
    _configure_hg_port_breakout("hybrid-group-32", "64", "disabled")

    #Check 10gx4 Breakouts
    _check_hg_port_breakout("hybrid-group-32", "63", "10gx4")
    _check_hg_port_breakout("hybrid-group-32", "64", "disabled")
    _print_hg_details("hybrid-group-32")

    #Test Cleanup
    _configure_hg_profile("hybrid-group-32", "unrestricted")
    _create_intf("e101-063-0")
    _create_intf("e101-064-0")

    return True


def z9264_test_5():

    #Description
    prGreen("Test Description: CONFIGURE 'hybrid-group-32' to 'unrestricted' mode, configure 50gx2 on port 1, 50gx2 on port 2")

    #Test Prep
    if False == _check_hg_support_on_platform():
        return True
    _delete_intf("e101-063-0")
    _delete_intf("e101-064-0")

    #Apply Hybrid Group Profile as 'unrestricted' & Default Breakouts
    _configure_hg_profile("hybrid-group-32", "unrestricted")

    #Check Hybrid Group Profile as 'unrestricted' & Default Breakouts
    _check_hg_profile("hybrid-group-32", "unrestricted")
    _check_hg_port_breakout("hybrid-group-32", "63", "100gx1")
    _check_hg_port_breakout("hybrid-group-32", "64", "100gx1")

    #Configure 10gx4 Breakouts
    _configure_hg_port_breakout("hybrid-group-32", "63", "50gx2")
    _configure_hg_port_breakout("hybrid-group-32", "64", "50gx2")

    #Check 10gx4 Breakouts
    _check_hg_port_breakout("hybrid-group-32", "63", "50gx2")
    _check_hg_port_breakout("hybrid-group-32", "64", "50gx2")
    _print_hg_details("hybrid-group-32")

    #Test Cleanup
    _configure_hg_profile("hybrid-group-32", "unrestricted")
    _configure_hg_port_breakout("hybrid-group-32", "63", "100gx1")
    _configure_hg_port_breakout("hybrid-group-32", "64", "100gx1")
    _create_intf("e101-063-0")
    _create_intf("e101-064-0")

    return True


#Platform Z9264 Test Suite
z9264_tests = {
    1: z9264_test_1,
    2: z9264_test_2,
    3: z9264_test_3,
    4: z9264_test_4,
    5: z9264_test_5,
}


#All Test Suites
test_suites = {
    "Z9264F-ON": z9264_tests,
}


def test_all():
    #Get platform type from PAS and run appropriate test suite
    d = []
    cps.get([cps.key_from_name('observed','base-pas/chassis')], d)
    product_name = cps_attr_get(d[0]['data'], 'base-pas/chassis/product-name')

    if product_name not in test_suites:
        prGreen("Hybrid Group is not supported on platform " + str(product_name) + ", Bypass the test for this platform")
        return True

    tests = test_suites[product_name]

    idx = 1
    while(idx <= len(tests)):
        tests[idx]()
        #raw_input("Press Enter to continue...")
        idx += 1


if __name__ == '__main__':
    test_all()

