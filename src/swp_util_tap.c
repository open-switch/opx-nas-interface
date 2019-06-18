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
 * filename: swp_util_tap.c
 */



#include "swp_util_tap.h"
#include "std_assert.h"
#include "std_socket_tools.h"

#include <sys/socket.h>

#include <linux/if_tun.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <net/if.h>
#include "std_utils.h"


#define SWP_UTIL_TAP_NAME_MAX 17
#define SWP_UTIL_TAP_QUEUE_LEN_MAX 10
#define SWP_UTIL_TAP_TXQLEN 1000
/*
 * \brief Internally managed tap description structure.  Access to fields are unsupported
 */
typedef struct _swp_util_tap_descr {
    char devname[SWP_UTIL_TAP_NAME_MAX+1];
    int fds[SWP_UTIL_TAP_QUEUE_LEN_MAX];
    int max_fd;
    int queues;
    int type;
} s_swp_util_tap_descr;


#define MAX_OPT_STR_LEN 20

const char * swp_util_tap_descr_get_name(swp_util_tap_descr p) {
    return p->devname;
}

void swp_util_free_descrs(swp_util_tap_descr  taps) {
    free((void*)taps);
}

int swp_util_tap_descr_get_queue(swp_util_tap_descr p,int queue) {
    if ((p != NULL) && (queue < p->queues)) return p->fds[queue];
    return SWP_UTIL_INV_FD;
}

swp_util_tap_descr  swp_util_alloc_descr(void) {
    int size;
    void * p;
    size = sizeof(struct _swp_util_tap_descr);
    p = malloc(size);
    if (p!=NULL) memset(p,0,size);
    return (swp_util_tap_descr)p;
}

void swp_util_tap_descr_init_wname(swp_util_tap_descr p,
        const char *n, int *chas, int *inst, port_t *port, port_t *subi,
        int queues)
{
    swp_util_tap_descr_init(p);
    char opt_str[MAX_OPT_STR_LEN] = "%s";

    //strncat null terminates always
    if (chas!=NULL) strncat(opt_str,"%x",MAX_OPT_STR_LEN-strlen(opt_str));
    if (inst!=NULL) strncat(opt_str,"%d",MAX_OPT_STR_LEN-strlen(opt_str));
    if (port!=NULL) strncat(opt_str,"-%d",MAX_OPT_STR_LEN-strlen(opt_str));
    if (subi!=NULL) strncat(opt_str,"-%x",MAX_OPT_STR_LEN-strlen(opt_str));

    snprintf(p->devname,sizeof(p->devname)-1,
            opt_str,n,chas!=NULL ? *chas : 0, inst!=NULL ? *inst : 0,
                        port!=NULL? *port : 0, subi!=NULL ? *subi : 0 );
    p->queues = queues;
    p->type = SWP_UTIL_TYPE_UNDEF;
}


void swp_util_tap_descr_init(swp_util_tap_descr p) {
    memset(p->devname,0,sizeof(p->devname));
    size_t ix = 0;
    size_t mx = sizeof(p->fds)/sizeof(*(p->fds));
    for ( ; ix < mx ; ++ix ) p->fds[ix] =SWP_UTIL_INV_FD;
    p->queues = 1;
    p->max_fd = SWP_UTIL_INV_FD;
    p->type = SWP_UTIL_TYPE_UNDEF;
}


void swp_util_print_tap(swp_util_tap_descr tap) {
    int ix = 0;
    int mx = sizeof(tap->fds)/sizeof(*(tap->fds));
    EV_LOGGING (NAS_PKT_IO, DEBUG, "PKT-IO", "Dev : %s, Queues: %d,",
                     tap->devname,tap->queues);
    for ( ; ix < mx ; ++ix ) {
        EV_LOGGING (NAS_PKT_IO, DEBUG, "PKT-IO", "FD: %d", tap->fds[ix]);
    }
}

int swp_util_set_persist(swp_util_tap_descr tap, int how) {
    if ( (tap->fds[0]==SWP_UTIL_INV_FD) ||
         (ioctl(tap->fds[0],TUNSETPERSIST,how==0 ? 0 : 1)< 0) ) {
        EV_LOGGING (NAS_PKT_IO, ERR,"TAP-PERSIST", "Can't persist tap device %s",
                                tap->devname);
        return STD_ERR(INTERFACE,FAIL,0);
    }
    return STD_ERR_OK;
}

void swp_util_close_fds(swp_util_tap_descr tap) {
    int ix = 0;
    int mx = tap->queues;

    for ( ; ix < mx ; ++ix ) {
        if (tap->fds[ix]!=SWP_UTIL_INV_FD)
            close(tap->fds[ix]);
        tap->fds[ix]=SWP_UTIL_INV_FD;
    }
}

t_std_error swp_util_tap_operation(const char * name, int type, bool create) {

    struct ifreq ifr;

    if (type!=SWP_UTIL_TYPE_TAP) {
        return STD_ERR(INTERFACE,PARAM,type);
    }

    int fd=SWP_UTIL_INV_FD,err=0;

    if (strlen(name)>(sizeof(ifr.ifr_ifrn.ifrn_name)-1))
        return STD_ERR(INTERFACE,PARAM,strlen(name));

    memset(&ifr,0,sizeof(ifr));

    ifr.ifr_flags = IFF_TAP | IFF_MULTI_QUEUE | IFF_NO_PI;

    strncpy(ifr.ifr_ifrn.ifrn_name,name,
            sizeof(ifr.ifr_ifrn.ifrn_name)-1);

    if ((fd=open("/dev/net/tun",O_RDWR))<0) {
        EV_LOGGING (NAS_PKT_IO, ERR,"TAP-PERSIST", "Can't open /dev/net/tun");
        return STD_ERR(INTERFACE,FAIL,errno);
    }
    t_std_error rc = STD_ERR_OK;
    do {
        err = ioctl(fd,TUNSETIFF,(void*)&ifr);
        if (err) {
            EV_LOGGING (NAS_PKT_IO, ERR,"TAP-OP", "Can't associate tap device %s to FD err:%d",name,
                    err);
            rc = STD_ERR(INTERFACE,FAIL,err);
            break;
        }

        if(create){
            int tqlen_fd;
            if((tqlen_fd = socket(AF_INET, SOCK_DGRAM, 0))<0){
                EV_LOGGING (NAS_PKT_IO, ERR,"TAP-OP", "Can't open fd to set txqlen");
            }else{
                ifr.ifr_qlen = SWP_UTIL_TAP_TXQLEN;
                if ((err = ioctl(tqlen_fd, SIOCSIFTXQLEN, &ifr)) < 0) {
                    EV_LOGGING (NAS_PKT_IO, ERR,"TAP-OP", "Can't set txqlen for %s to %d FD err:%d",name,
                        SWP_UTIL_TAP_TXQLEN,err);
                }
                close(tqlen_fd);
            }
        }

        if (ioctl(fd,TUNSETPERSIST,(create) ? 1 : 0)< 0) {
            EV_LOGGING (NAS_PKT_IO, ERR,"TAP-OP", "Can't change persist state for %s to %d",
                    name,create);
            rc = STD_ERR(INTERFACE,FAIL,err);
        }
    } while (0);

    close(fd);
    return rc;
}


t_std_error swp_util_alloc_tap(swp_util_tap_descr tap, int type) {
    STD_ASSERT(tap!=NULL);

    struct ifreq ifr;

    if (type!=SWP_UTIL_TYPE_TAP) {
        return STD_ERR(INTERFACE,PARAM,type);
    }

    tap->type = type;

    int fd=SWP_UTIL_INV_FD,err=0,i;

    if (strlen(tap->devname)>(sizeof(ifr.ifr_ifrn.ifrn_name)-1)) {
        return STD_ERR(INTERFACE,PARAM,strlen(tap->devname));
    }

    memset(&ifr,0,sizeof(ifr));

    ifr.ifr_flags = IFF_TAP | IFF_MULTI_QUEUE | IFF_NO_PI;

    safestrncpy(ifr.ifr_ifrn.ifrn_name,tap->devname,
            sizeof(ifr.ifr_ifrn.ifrn_name));

    for ( i = 0; i < tap->queues ; ++i) {
        if ((fd=open("/dev/net/tun",O_RDWR))<0) {
            swp_util_close_fds(tap);
            EV_LOGGING (NAS_PKT_IO, ERR,"TAP-ALLOC", "Can't open /dev/net/tun ");
            return STD_ERR(INTERFACE,FAIL,err);
        }

        err = ioctl(fd,TUNSETIFF,(void*)&ifr);
        if (err) {
            EV_LOGGING (NAS_PKT_IO, ERR,"TAP-ALLOC", "Can't associate file descriptor for tap (%s:%s) :%d",
                    tap->devname,ifr.ifr_ifrn.ifrn_name,err);
            close(fd);
            swp_util_close_fds(tap);
            return STD_ERR(INTERFACE,FAIL,err);
        }
        tap->fds[i]=fd;
        err = std_sock_set_nonblock(fd, 1);
        if (err != STD_ERR_OK) {
            EV_LOGGING (NAS_PKT_IO, ERR,"TAP-ALLOC", "Can't set non-blocking to tap fd for interface:%s: %d",
                       ifr.ifr_ifrn.ifrn_name,err);
            close(fd);
            swp_util_close_fds(tap);
            return STD_ERR(INTERFACE,FAIL,err);
        }

        if (tap->max_fd < fd) tap->max_fd = fd;
    }

    return STD_ERR_OK;
}

hal_ifindex_t swp_init_tap_ifindex(swp_util_tap_descr p) {
    return if_nametoindex(p->devname);
}

int swp_util_tap_fd_set_add(register swp_util_tap_descr *p, register int descrs, register fd_set *set)
{
    register int dix = 0;
    register int dmx = descrs;
    register int fd_max = SWP_UTIL_INV_FD;
    for ( ; dix < dmx ; ++dix ) {
        if (p[dix]==NULL) continue;
        register int ix = 0;
        register int mx = p[dix]->queues;
        for ( ; ix < mx ; ++ix ) {
            if (p[dix]->fds[ix]==SWP_UTIL_INV_FD) continue;
            FD_SET(p[dix]->fds[ix],set);
        }
        if (fd_max < p[dix]->max_fd) fd_max = p[dix]->max_fd;
    }
    return fd_max;
}

int swp_util_tap_fd_locate_from_set(register swp_util_tap_descr p, register fd_set *set)
{
    register int ix = 0;
    register int mx = p->queues;
    for ( ; ix < mx ; ++ix ) {
        if (p->fds[ix]==SWP_UTIL_INV_FD) continue;
        if (FD_ISSET(p->fds[ix],set)) {
            return p->fds[ix];
        }
    }
    return SWP_UTIL_INV_FD;
}

bool swp_util_tap_is_fd_in_tap_fd_set(register swp_util_tap_descr p, int fd)
{
    register int ix = 0;
    register int mx = p->queues;
    for ( ; ix < mx ; ++ix ) {
        if (p->fds[ix]== fd) return true;
    }
    return false;
}
