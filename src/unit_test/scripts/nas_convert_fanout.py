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

import sys
import getopt
import os
import cps


def usage():
    ''' Usage Method '''
    print '< Usage >'
    print 'nas_convert_fanout can be used to fanout set of interface(s)\n'
    print '-h, --help : Display the usage of the script'
    print '-i, --if   : List of space separted interface for fanout\n'
    print 'Example: nas_convert_fanout.py -i "e00-1 e00-2"'
    sys.exit(1)


def main(argv):
    ''' The main function will read the user input from the
        command line argument and  process the request  '''

    if_name = ''
    choice = ''

    try:
        opts, args = getopt.getopt(argv, "i:hl:d", ["if=", "help", "del"])

    except getopt.GetoptError:
        usage()

    for opt, arg in opts:

        if opt in ('-h', '--help'):
            choice = 'help'
            usage()

        elif opt in ('-i', '--if'):
            if_name = arg

    if if_name == '':
        usage()

    if_name_list = str.split(if_name)
    for name in if_name_list:
        del_cmd_str = '/usr/bin/nas_delete_interface.py ' + name
        set_cmd_str = ' /usr/bin/opx-config-fanout' + name + ' true'
        os.system(del_cmd_str)
        os.system(set_cmd_str)

    create_cmd_str = '/usr/bin/base_nas_create_interface.py'
    os.system(create_cmd_str)

# Calling the main method
if __name__ == "__main__":
    main(sys.argv[1:])
