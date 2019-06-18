/*
 * Copyright (c) 2019 Dell Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED ON AN *AS IS* BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 * LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 * FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 * See the Apache Version 2.0 License for specific language governing
 * permissions and limitations under the License.
 */

/*
 * filename: interface_file_reader_unit_test.c
 */

/*
 * interface_file_reader_unit_test.c
 */


#include "hal_if_mapping.h"
#include "hal_interface_common.h"


#include <stdio.h>


void hal_if_register_details(hal_if_port_details_t * port_details) {
    printf("Registering... \n");
    printf("Faceplate:%d\nIF Name:%s\nNPU:%d\n,PORT:%d\nFanout:%d\nSubIF:%d\n\n",
            (int)port_details->faceplate_port,
            port_details->ifname,
            (int)port_details->npu,
            (int)port_details->port,
            (int)port_details->fanout,
            (int)port_details->sub_interface);
}

int main() {
    hal_load_interface_map("port_names.cfg");
    return 0;
}
