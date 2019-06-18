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
 * filename: tap_main.c
 */


#include "swp_util_tap.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define TAP_ALLOC (5)

int main() {

    swp_util_tap_descr *taps = swp_util_alloc_descr(TAP_ALLOC);

    int ix = 0;
    int mx = TAP_ALLOC;
    for ( ; ix < mx ; ++ix ) {

        swp_util_tap_descr_init_wname(taps[ix],"swp",&ix,&ix,&ix,NULL,5);
        if (swp_util_alloc_tap(taps[ix],SWP_UTIL_TYPE_TAP,1)<0) {
            printf("Failed to create tap... %s",
                    swp_util_tap_descr_get_name(taps[ix]));
        } else {
            swp_util_print_tap(taps[ix]);
        }
    }
    int pktid = 0;

    while (1==1) {
        fd_set rds;
        FD_ZERO(&rds);

        int max_fd = swp_util_tap_fd_set_add(taps,TAP_ALLOC,&rds);
        int rc = 0;
        struct timeval t;
        t.tv_sec = 10;
        t.tv_usec = 0;
        if ((rc=select(max_fd+1,&rds,NULL,NULL,&t))) {
            if (rc<0 || errno==EINTR) continue;
            if (rc==0) continue;
            ix = 0;
            for ( ; ix < mx ; ++ix ) {
                while (1) {
                    int fd = swp_util_tap_fd_locate_from_set(taps[ix],&rds);
                    if (fd<0) break;
                    FD_CLR(fd,&rds);
                    char buff[10000]; //please don't expect this to work in realtime :) allocate buffer

                    int len = read(fd,buff,sizeof(buff));
                    //if (len<0) handle eintr...
                    if (len >0) {
                        //printf packet
                        printf("pkt fd==%d, %d",fd,pktid++);
                    }
                }
            }
        }
    }

    return 0;
}
