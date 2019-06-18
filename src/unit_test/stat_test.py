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


import subprocess
import time
import cps_utils

class StatUnitTest:

    def _run_cmds(self,in_cmd,ignore_err=False):
        proc = subprocess.Popen(in_cmd.split(),stdout=subprocess.PIPE)
        out,err = proc.communicate()
        print out
        return True

    def _setup_ut(self):
        cps_utils.add_attr_type('tunnel/tunnel-stats/tunnels/remote-ip/addr','ipv4')
        cps_utils.add_attr_type('tunnel/tunnel-stats/tunnels/local-ip/addr','ipv4')
        cps_utils.add_attr_type('tunnel/clear-tunnel-stats/input/remote-ip/addr','ipv4')
        cps_utils.add_attr_type('tunnel/clear-tunnel-stats/input/local-ip/addr','ipv4')


        _init_cmds = ["ip link add  link e101-001-0 name e101-001-0.100 type vlan id 100",
                      "brctl addbr br100",
                      "ip link add vtep100 type vxlan id 100 local 10.1.1.1 dstport 4789",
                      "brctl addif br100 vtep100",
                      "brctl addif br100 e101-001-0.100"]
        for i in _init_cmds:
            self._run_cmds(i,True)
        time.sleep(0.5)
        self._run_cmds("bridge fdb add 00:00:00:00:00:00 dev vtep100 dst 10.1.1.2",True)

    def _cleanup_ut(self):
        _cleanup_cmds = ["bridge fdb del 00:00:00:00:00:00 dev vtep100",
                         "brctl delif br100 vtep100",
                         "brctl delif br100 e101-001-0.100",
                         "brctl delbr br100",
                         "ip link del e101-001-0.100",
                         "ip link del vtep100"
                         ]
        for i in _cleanup_cmds:
            self._run_cmds(i,True)

    def __init__(self):
        self._setup_ut()
        self.set_script_path = "/usr/bin/cps_set_oid.py "
        self.get_script_path = "/usr/bin/cps_get_oid.py "

        self.set_test_ids = [ "1D Bridge Stat Clear",
                              "VLAN Sub Intf Stat Clear",
                              "Tunnel Stat Clear"
                            ]

        self.get_test_ids = ["1D Bridge Stat Get",
                             "VLAN Sub Intf Stat Get",
                             "Tunnel Stat Get",
                            ]

        self.set_test_cases = {
            "1D Bridge Stat Clear" : " target/bridge-domain/clear-bridge-stats"
                                    " bridge-domain/clear-bridge-stats/input/name=br100 -oper action" ,

            "VLAN Sub Intf Stat Clear" : " target/dell-base-if-cmn/dell-if/clear-counters/input"
                        " dell-if/clear-counters/input/intf-choice/ifname-case/ifname=e101-001-0.100 -oper action",

            "Tunnel Stat Clear" : " target/tunnel/clear-tunnel-stats"
                                  " tunnel/clear-tunnel-stats/input/remote-ip/addr-family=2"
                                  " tunnel/clear-tunnel-stats/input/remote-ip/addr=10.1.1.2"
                                  " tunnel/clear-tunnel-stats/input/local-ip/addr-family=2"
                                  " tunnel/clear-tunnel-stats/input/local-ip/addr=10.1.1.1 -oper action"

                        }

        self.get_test_cases = {
            "1D Bridge Stat Get" : " observed/bridge-domain/bridge/stats"
                                    " bridge-domain/bridge/name=br100" ,

            "VLAN Sub Intf Stat Get" : " observed/dell-base-if-cmn/if/interfaces-state/interface/statistics"
                                       " if/interfaces-state/interface/name=e101-001-0.100"
                                       " if/interfaces-state/interface/type=base-if:vlanSubInterface",

            "Tunnel Stat Get" : " observed/tunnel/tunnel-stats"
                                  " tunnel/tunnel-stats/tunnels/remote-ip/addr-family=2"
                                  " tunnel/tunnel-stats/tunnels/remote-ip/addr=10.1.1.2"
                                  " tunnel/tunnel-stats/tunnels/local-ip/addr-family=2"
                                  " tunnel/tunnel-stats/tunnels/local-ip/addr=10.1.1.1"

                        }


    def _run_test_case(self):
        tc_passed = 0
        tc_failed = 0
        for tc in self.get_test_ids:
            print "===================================================================="
            print "Running Test Case " + tc
            if self._run_cmds(self.get_script_path + self.get_test_cases[tc]):
                print "Passed Test Case " + tc
                tc_passed = tc_passed + 1
            else:
                print "Failed Test Case " + tc
                tc_failed = tc_failed + 1

        print "===================================================================="

        for tc in self.set_test_ids:
            print "===================================================================="
            print "Running Test Case " + tc
            if self._run_cmds(self.set_script_path + self.set_test_cases[tc]):
                print "Passed Test Case " + tc
                tc_passed = tc_passed + 1
            else:
                print "Failed Test Case " + tc
                tc_failed = tc_failed + 1

        print "===================================================================="

        print "Passed Test Cases ", tc_passed
        print "Failed Test Cases ", tc_failed
        self._cleanup_ut()
        if tc_failed:
            return False
        return True



def test_stat_get_set():
    test = StatUnitTest()
    assert test._run_test_case() == True
