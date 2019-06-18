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

class AttachVlanUnitTest:

    def _validate_output(self,given_op,expected_op=set(),should_fail=False):
        rc = False
        if len(expected_op)==0:
            if "Success" in given_op:
                print "Success"
                rc = True
        else:
            l = given_op.split(" ")
            for i in l:
                t = i.split(',')
                s = set()
                for item in t:
                    if item:
                        s.add(item.splitlines()[0])

                if s == expected_op:
                    print "Expected Output",expected_op
                    print "Test Case Output", s
                    rc = True
                    break
        if should_fail:
            return rc == False

        return rc == True

    def _run_cmds(self,in_cmd,ignore_err=False,expected_op=set(),should_fail=False):
        proc = subprocess.Popen(in_cmd.split(),stdout=subprocess.PIPE)
        out,err = proc.communicate()
        print out
        return self._validate_output(out,expected_op,should_fail)

    def _setup_ut(self):

        _init_cmds = ["cps_config_vlan.py --add --id 200 --vlantype 1 -t"
                      " --port e101-005-0,e101-006-0,e101-007-0,e101-008-0,e101-009-0,e101-010-0",
                      "brctl addbr vn200",
                      "ip link add vtep200 type vxlan id 200 local 10.1.1.1 dstport 4789",
                      "brctl addif vn200 vtep200",
                      "cps_config_lag.py --create --lname bo40",
                      "cps_config_lag.py --add --lname bo40 --port e101-019-0,e101-020-0",
                      "cps_config_lag.py --create --lname bo41",
                      "cps_config_lag.py --add --lname bo41 --port e101-021-0",
                      "cps_config_vlan.py --addport --name br200 -t --port bo40",
                      "cps_config_vlan.py --add --id 300 --vlantype 1"
                     ]
        for i in _init_cmds:
            self._run_cmds(i,True)


    def _cleanup_ut(self):
        _cleanup_cmds = ["cps_config_vlan.py  --del --name br200",
                         "brctl delif vn200 vtep200",
                         "brctl delbr vn200",
                         "ip link del vtep200",
                         "cps_config_lag.py --remove --lname bo40 --port e101-019-0,e101-020-0",
                         "cps_config_lag.py --remove --lname bo41 --port e101-021-0",
                         "cps_config_lag.py --delete --lname bo40",
                         "cps_config_lag.py --delete --lname bo41",
                         "cps_config_vlan.py --del --name br300"
                         ]
        for i in _cleanup_cmds:
            self._run_cmds(i,True)

    def __init__(self):
        self._setup_ut()
        self.set_script_path = "/usr/bin/cps_set_oid.py "
        self.get_script_path = "/usr/bin/cps_get_oid.py "

        self.set_test_ids = [ "Attach VLAN",
                              "Add port to attached VLAN",
                              "Remove port from attached VLAN",
                              "Add LAG to attached VLAN",
                              "Remove LAG from attached VLAN",
                              "Attach Invalid VLAN"

                            ]

        self.set_test_cases = {
            "Attach VLAN" : " target/dell-base-if-cmn/set-interface dell-base-if-cmn/set-interface/input/operation=3"
                            " if/interfaces/interface/name=br200 if/interfaces/interface/type=ianaift:l2vlan"
                            " dell-if/if/interfaces/interface/parent-bridge=vn200 -oper action" ,
            "Attach VLAN Output" : set(),

           "Add port to attached VLAN" : "/usr/bin/cps_config_vlan.py --addport --name br200 -t --port e101-011-0",
           "Add port to attached VLAN Output" : set(),

           "Remove port from attached VLAN" : "/usr/bin/cps_config_vlan.py --delport --name br200 -t --port e101-011-0",
           "Remove port from attached VLAN Output" : set(),

           "Add LAG to attached VLAN" : "/usr/bin/cps_config_vlan.py --addport --name br200 -t --port bo41",
           "Add LAG to attached VLAN Output" : set(),

           "Remove LAG from attached VLAN" : "/usr/bin/cps_config_vlan.py --delport --name br200 -t --port bo41",
           "Remove LAG from attached VLAN Output" : set(),

           "Attach Invalid VLAN" : " target/dell-base-if-cmn/set-interface dell-base-if-cmn/set-interface/input/operation=3"
                            " if/interfaces/interface/name=b3200 if/interfaces/interface/type=ianaift:l2vlan"
                            " dell-if/if/interfaces/interface/parent-bridge=vn200 -oper action" ,
           "Attach Invalid VLAN Output" : set(),
           "Attach Invalid VLAN Should Fail" : True,
        }

        self.get_test_cases = {
            "Attach VLAN" : "target/bridge-domain/bridge name=vn200",
            "Attach VLAN Output" : set(["e101-005-0.200","e101-006-0.200","e101-007-0.200","e101-008-0.200",
                                        "e101-009-0.200","e101-010-0.200","bo40.200","vtep200"]),

            "Add port to attached VLAN" : "target/bridge-domain/bridge name=vn200",
            "Add port to attached VLAN Output" : set(["e101-005-0.200","e101-006-0.200","e101-007-0.200","e101-008-0.200","e101-009-0.200",
                                                        "e101-010-0.200","e101-011-0.200","bo40.200","vtep200"]),

            "Remove port from attached VLAN" : "target/bridge-domain/bridge name=vn200",
            "Remove port from attached VLAN Output" : set(["e101-005-0.200","e101-006-0.200","e101-007-0.200",
                                                           "e101-008-0.200","e101-009-0.200","e101-010-0.200",
                                                           "bo40.200","vtep200"]),

            "Add LAG to attached VLAN" : "target/bridge-domain/bridge name=vn200",
            "Add LAG to attached VLAN Output" : set(["e101-005-0.200","e101-006-0.200","e101-007-0.200","e101-008-0.200","e101-009-0.200",
                                                        "e101-010-0.200","bo40.200","bo41.200","vtep200"]),

            "Remove LAG from attached VLAN" : "target/bridge-domain/bridge name=vn200",
            "Remove LAG from attached VLAN Output" : set(["e101-005-0.200","e101-006-0.200","e101-007-0.200","e101-008-0.200",
                                                          "e101-009-0.200","e101-010-0.200","bo40.200","vtep200"]),

            "Attach Invalid VLAN" : "target/bridge-domain/bridge name=vn200",
            "Attach Invalid VLAN Output" : set(["e101-005-0.200","e101-006-0.200","e101-007-0.200","e101-008-0.200",
                                                          "e101-009-0.200","e101-010-0.200","bo40.200","vtep200"]),


        }


    def _run_test_case(self):
        tc_passed = 0
        tc_failed = 0
        for tc in self.set_test_ids:
            print "===================================================================="
            print "Running Set Test Case " + tc
            path = ""
            if "cps" in self.set_test_cases[tc]:
                path = self.set_test_cases[tc]
            else:
                path = self.set_script_path + self.set_test_cases[tc]

            should_fail = False
            should_fail_key = tc + " Should Fail"

            if should_fail_key in self.set_test_cases:
                should_fail = self.set_test_cases[should_fail_key]

            if self._run_cmds(path,False,self.set_test_cases[tc + " Output"],should_fail):
                print "Passed Set Test Case " + tc
                tc_passed = tc_passed + 1
            else:
                print "Failed Set Test Case " + tc
                tc_failed = tc_failed + 1

            print "===================================================================="
            path = ""
            if "cps" in self.get_test_cases[tc]:
                path = self.get_test_cases[tc]
            else:
                path = self.get_script_path + self.get_test_cases[tc]

            should_fail = False

            if should_fail_key in self.get_test_cases:
                should_fail = self.get_test_cases[should_fail_key]

            if self._run_cmds(path,False,self.get_test_cases[tc + " Output"],should_fail):
                print "Passed Get Test Case " + tc
                tc_passed = tc_passed + 1
            else:
                print "Failed Get Test Case " + tc
                tc_failed = tc_failed + 1

        print "===================================================================="


        print "Passed Test Cases ", tc_passed
        print "Failed Test Cases ", tc_failed
        self._cleanup_ut()
        if tc_failed:
            return False
        return True



def test_stat_get_set():
    test = AttachVlanUnitTest()
    assert test._run_test_case() == True
