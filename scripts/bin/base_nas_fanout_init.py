#!/usr/bin/python

# Copyright (c) 2018 Dell Inc.
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
import event_log as ev
from time import sleep
import os
import subprocess
import shlex
import nas_os_if_utils as nas_if

fanout_config_file = "/etc/opx/startup/config-interfaces"
nocreate_file = "/etc/opx/nas_if_nocreate"

def config_fanout():
    """parse the fanout_config_file, which has the configs for
       fanout interfaces
    """

    if not os.path.isfile(fanout_config_file):
        nas_if.log_err(str(fanout_config_file)+" does not exist")

    with open(fanout_config_file, 'r') as f:
        for line in f:
            command = line.strip('\t\n\r')
            if command.startswith('#') or len(command) == 0:
                continue
            try:
                p = subprocess.Popen(shlex.split(command),stdout=subprocess.PIPE, stderr=subprocess.PIPE,shell=False, universal_newlines=True)
                (stdoutdata, stderrdata) = p.communicate()
            except Exception as e:
                nas_if.log_err('File: "%s", line: "%s" - EXCEPTION: %s' % (fanout_config_file,line,e))


if __name__ == '__main__':

    if os.path.isfile(nocreate_file):
        sys.exit(0)

    # Wait till interface service is ready
    while nas_if.nas_os_if_list() == None:
        sleep(1)

    config_fanout()
