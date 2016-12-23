/*
 * Copyright (c) 2016 Dell Inc.
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
 * nas_int_port.cpp
 *
 *  Created on: Jun 9, 2015
 */


#include "hal_if_mapping.h"
#include "hal_interface_common.h"
#include "dell-base-if-phy.h"

#include "swp_util_tap.h"

#include "std_error_codes.h"
#include "event_log.h"
#include "std_assert.h"
#include "std_rw_lock.h"
#include "std_time_tools.h"


#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <event2/event.h>
#include <signal.h>
#include <unordered_map>


#define MAX_QUEUE          1
/* num packets to read on fd event */
#define NAS_PKT_COUNT_TO_READ 10


//Lock for a interface structures
static std_rw_lock_t ports_lock = PTHREAD_RWLOCK_INITIALIZER;


class CNasPortDetails {
    bool _used = false;
    npu_id_t _npu = 0;
    port_t _port = 0;
    uint32_t _hwport = 0;
    swp_util_tap_descr _dscr=nullptr;
    IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t _link = IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_DOWN;
public:
    void init(npu_id_t npu, port_t port) {
        _npu = npu;
        _port=port;
    }

    IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t link_state() {
        return _link;
    }

    void set_link_state(IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t state) ;

    bool del();
    bool create(const char *name);
    hal_ifindex_t ifindex() ;
    bool valid() { return _used; }

    inline npu_id_t npu() { return _npu; }
    inline port_t port() { return _port; }
    inline swp_util_tap_descr tap() { return _dscr; }

    ~CNasPortDetails();

};

using NasPortList = std::vector<CNasPortDetails> ;
using NasListOfPortList = std::vector<NasPortList> ;

static NasListOfPortList _ports;

/* tap fd to event info details */
typedef std::unordered_map<int,struct event *> _fd_to_event_info_map_t;

typedef struct _nas_vif_pkt_tx_t {
    struct event_base *nas_evt_base; // Pointer to event base
    struct event *nas_signal_event;  // Pointer to our signal event
    hal_virt_pkt_transmit tx_cb;     // Pointer to packet tx callback function
    void *tx_buf; // Pointer to packet tx buffer
    unsigned int tx_buf_len; // packet tx buffer len
    _fd_to_event_info_map_t _tap_fd_to_event_info_map; //fd to event base info
} nas_vif_pkt_tx_t;

nas_vif_pkt_tx_t g_vif_pkt_tx; //global virtual interface packet tx information

void process_packets (evutil_socket_t fd, short evt, void *arg);

/* Add the tap fd to event info map */
t_std_error nas_add_fd_to_evt_info_map (int fd, struct event *fd_evt)
{
    g_vif_pkt_tx._tap_fd_to_event_info_map[fd] = fd_evt;
    return STD_ERR_OK;
}

/* Del the tap fd from event info map */
t_std_error nas_del_fd_from_evt_info_map (int fd)
{
    g_vif_pkt_tx._tap_fd_to_event_info_map.erase(fd);
    return STD_ERR_OK;
}

/* this function is called when tap interface becomes oper up.
 * On oper up, add the tap interface fd's to event for read event.
 * Also add the fd to event base info to the mapping table.
 */
static t_std_error tap_fd_register_with_evt (swp_util_tap_descr tap, CNasPortDetails *nas_port) {
    fd_set fds;
    struct event *nas_fd_ev = NULL;     // fd event struct

    FD_ZERO(&fds);

    /* get the fd(s) for this tap interface */
    swp_util_tap_fd_set_add(&tap, 1, &fds);

    while (true) {
        /* scan thru the fd(s) and add each fd to event */
        int fd = swp_util_tap_fd_locate_from_set(tap, &fds);
        if (fd < 0)
            break;
        FD_CLR(fd, &fds);

        auto it = g_vif_pkt_tx._tap_fd_to_event_info_map.find(fd);
        if (it != g_vif_pkt_tx._tap_fd_to_event_info_map.end())
        {
            /* fd already present in event info map */
            EV_LOGGING(INTERFACE,ERR,"TAP-UP",
                    "interface (%s) fd (%d) already present in event info map",
                    swp_util_tap_descr_get_name(tap),fd);
            continue;
        }

        // Setup the events for this fd
        nas_fd_ev = event_new(g_vif_pkt_tx.nas_evt_base, fd,
                              EV_READ | EV_PERSIST, process_packets, (void *)nas_port);

        if (!nas_fd_ev) {
            EV_LOGGING(INTERFACE,ERR,"TAP-TX", "NAS Packet read event create failed for interface (%s) fd (%d).",
                       swp_util_tap_descr_get_name(tap),fd);
            return STD_ERR(INTERFACE,FAIL,0);
        }

        if (event_add(nas_fd_ev, NULL) < 0) {
            EV_LOGGING(INTERFACE,ERR,"TAP-TX", "NAS Packet read event add failed for interface (%s) fd (%d).",
                       swp_util_tap_descr_get_name(tap),fd);
            event_free (nas_fd_ev);
            return STD_ERR(INTERFACE,FAIL,0);
        }

        /* fd is registered for events, add to evt info map */
        nas_add_fd_to_evt_info_map (fd, nas_fd_ev);
    }

    return STD_ERR_OK;
}


/* this function is called when tap interface is deleted or becomes oper down,
 * then delete the tap interface fd's from event poll.
 * Also delete the fd from event base mapping table.
 */
static t_std_error tap_fd_deregister_from_evt (swp_util_tap_descr tap) {
    fd_set fds;

    FD_ZERO(&fds);

    /* get the fd's for this tap interface */
    swp_util_tap_fd_set_add(&tap, 1, &fds);
    while (true) {
        /* scan thru the fd's and add each fd to event */
        int fd = swp_util_tap_fd_locate_from_set(tap, &fds);
        if (fd < 0)
            break;
        FD_CLR(fd, &fds);

        auto it = g_vif_pkt_tx._tap_fd_to_event_info_map.find(fd);
        if (it == g_vif_pkt_tx._tap_fd_to_event_info_map.end())
        {
            /* fd is not registered for events; simply continue to next fd */
            EV_LOGGING(INTERFACE,ERR,"TAP-UP",
                    "interface (%s) fd (%d) not present in event info map",
                    swp_util_tap_descr_get_name(tap),fd);
            continue;
        }
        struct event *nas_fd_ev = (struct event *)it->second;

        nas_del_fd_from_evt_info_map (fd);

        // delete events for this fd
        event_del(nas_fd_ev);
        event_free(nas_fd_ev);
    }

    return STD_ERR_OK;
}


static bool tap_link_down(swp_util_tap_descr tap) {
    //during link down, deregister fd from event poll before closing fd's.
    tap_fd_deregister_from_evt (tap);
    swp_util_close_fds(tap);
    return true;
}


static bool tap_link_up(swp_util_tap_descr tap, CNasPortDetails *npu) {
    //just incase... clean up
    //during cleanup, deregister fd from event poll if already registered
    tap_fd_deregister_from_evt (tap);
    swp_util_close_fds(tap);

    size_t retry = 0;
    const size_t MAX_RETRY = 12;
    bool success = false;
    for ( ; retry < MAX_RETRY ; ++retry ) {
        if (swp_util_alloc_tap(tap,SWP_UTIL_TYPE_TAP)!=STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"INT-LINK",
                    "Can not bring the link up %s had issues opening device (%d)",
                    swp_util_tap_descr_get_name(tap),retry);
            std_usleep(MILLI_TO_MICRO((1<<retry)));
            continue;
        }
        if (tap_fd_register_with_evt(tap, npu) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"INT-LINK",
                    "Can not bring the link up %s had issues in registering device fd with event handler (%d)",
                    swp_util_tap_descr_get_name(tap),retry);
            //on event reg failure, deregister fd from event poll for already registered fd's and retry
            tap_fd_deregister_from_evt (tap);
            swp_util_close_fds(tap);
            std_usleep(MILLI_TO_MICRO((1<<retry)));
            continue;
        }
        success = true;
        break;
    }
    if (!success) {
        return false;
    }
    EV_LOG(TRACE,INTERFACE,0,"INT-LINK", "Link up %s ",swp_util_tap_descr_get_name(tap));
    return true;
}


static void tap_delete(swp_util_tap_descr tap) {

    tap_link_down(tap);

    const char * name = swp_util_tap_descr_get_name(tap);

    if (swp_util_tap_operation(name,SWP_UTIL_TYPE_TAP,false)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"INTF-DEL", "Failed to delete linux interface %s",name);
    }

    swp_util_free_descrs(tap);
}

static swp_util_tap_descr tap_create(CNasPortDetails *npu, const char * name, uint_t queues) {
    swp_util_tap_descr tap = swp_util_alloc_descr();
    if (tap==nullptr) return tap;

    swp_util_tap_descr_init_wname(tap,name,NULL,NULL,NULL,NULL,queues);

    if (swp_util_tap_operation(name,SWP_UTIL_TYPE_TAP,true)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"INTF-DEL", "Failed to create linux interface %s",name);
    }
    return tap;
}

CNasPortDetails::~CNasPortDetails() {

}

bool CNasPortDetails::del() {
    _used = false;
    if (_dscr==nullptr) return true;
    set_link_state(IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_DOWN);

    tap_delete(_dscr);
    _dscr = nullptr;
    return true;
}

bool CNasPortDetails::create(const char *name) {
    if (_used) return true;
    _used = true;
    _dscr = tap_create(this,name,MAX_QUEUE);
    return true;
}

void CNasPortDetails::set_link_state(IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t state)  {
    if (_link==state) return;
    if (state == IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_UP) {
        if (tap_link_up(_dscr, this)) _link=state;
    }
    if (state == IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_DOWN) {
        tap_link_down(_dscr);
        _link = state;
    }
}

hal_ifindex_t CNasPortDetails::ifindex(){
    if (_dscr==nullptr) return -1;
    return swp_init_tap_ifindex(_dscr);
}


// Our signal handler function callback. Cleans up all the resources and
// breaks the event loop.
void nas_evt_signal_cb (evutil_socket_t sig_num, short evt, void *arg)
{
    nas_vif_pkt_tx_t *p_vif_pkt_tx = (nas_vif_pkt_tx_t *) arg;

    auto it = p_vif_pkt_tx->_tap_fd_to_event_info_map.begin();
    for (; it != p_vif_pkt_tx->_tap_fd_to_event_info_map.end();) {

        struct event *nas_fd_ev = (struct event *) it->second;

        it = p_vif_pkt_tx->_tap_fd_to_event_info_map.erase(it);

        // delete events for this fd
        event_del(nas_fd_ev);
        event_free(nas_fd_ev);
    }

    event_del (p_vif_pkt_tx->nas_signal_event);
    event_free (p_vif_pkt_tx->nas_signal_event);
    event_base_loopbreak(p_vif_pkt_tx->nas_evt_base);
}


/*
 * Callback function from event for read event from tap fd.
 * callback gives the context of the nas port information that
 * was registered during event_add.
*/
void process_packets (evutil_socket_t fd, short evt, void *arg)
{
    int pkt_len = 0;
    int pkt_count = 0;
    npu_id_t npu = 0;
    port_t port = 0;

    /* libevent takes care of blocking event_del for this event
     * when the callback function is still executing. Hence we don't
     * really need to validate if the nas port is still valid.
     * When NAS deletes the port for any reason, then it would have
     * done event_del and libevent guarentees that after event_del
     * the callback will not get called for that fd.
     */
    CNasPortDetails *details = (CNasPortDetails *) arg;

    npu = details->npu();
    port  = details->port();

    /* event is received in level-triggered mode,
     * so read data as required and w/o starving other ports
     */
    while (pkt_count < NAS_PKT_COUNT_TO_READ)
    {
        pkt_len = read(fd, g_vif_pkt_tx.tx_buf, g_vif_pkt_tx.tx_buf_len);

        if (pkt_len <=0)
        {
            /* no more data to read */
            break;
        }
        pkt_count++;
        /* send packet for transmission to registered callback function with registered packet buffer */
        g_vif_pkt_tx.tx_cb(npu,port,g_vif_pkt_tx.tx_buf,pkt_len);
    }
}


/* packet transmission from virtual interface is handled via libevent.
 * call to hal_virtual_interface_wait() trigger event dispatcher.
 */
extern "C" t_std_error hal_virtual_interface_wait (hal_virt_pkt_transmit fun,
                                                   void *data, unsigned int len)
{
    /* initialize global virtual interface packet tx information with given input params */
    g_vif_pkt_tx.nas_evt_base = NULL;
    g_vif_pkt_tx.nas_signal_event = NULL;

    g_vif_pkt_tx.tx_cb = fun;
    g_vif_pkt_tx.tx_buf = data;
    g_vif_pkt_tx.tx_buf_len = len;


    /* initialize event base */
    g_vif_pkt_tx.nas_evt_base = event_base_new();
    if (!g_vif_pkt_tx.nas_evt_base)
    {
        EV_LOGGING(INTERFACE,ERR,"TAP-TX", "NAS Packet event base initialization failed.");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    /* initialize event for sigint */
    g_vif_pkt_tx.nas_signal_event = event_new (g_vif_pkt_tx.nas_evt_base, SIGINT,
                    EV_SIGNAL | EV_PERSIST, nas_evt_signal_cb, (void *)&g_vif_pkt_tx);

    if (!g_vif_pkt_tx.nas_signal_event || event_add(g_vif_pkt_tx.nas_signal_event, NULL)<0) {
        EV_LOGGING(INTERFACE,ERR,"TAP-TX", "NAS Packet signal event initialization failed.");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    /* dispatch the event loop; to dispatch event loop there should be atleast
     * one active event. events for tap interfaces will get added from NAS
     * only after ports are oper up, so SIGINT event is registered as an event at start.
     */
    if (event_base_dispatch(g_vif_pkt_tx.nas_evt_base) != 0) {
        EV_LOGGING(INTERFACE,ERR,"TAP-TX", "NAS Packet event dispath failed...Aborting...");
        event_del (g_vif_pkt_tx.nas_signal_event);
        event_free (g_vif_pkt_tx.nas_signal_event);
        event_base_free (g_vif_pkt_tx.nas_evt_base);
        return STD_ERR(INTERFACE,FAIL,0);
    }
    return STD_ERR_OK;
}

extern "C" t_std_error hal_virtual_interace_send(npu_id_t npu, npu_port_t port, int queue,
        const void * data, unsigned int len) {
    int fd = -1;
    {
        std_rw_lock_read_guard l(&ports_lock);
        if (_ports[npu][port].link_state()!=IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_UP) {
            return STD_ERR_OK;
        }
        fd = swp_util_tap_descr_get_queue(_ports[npu][port].tap(), queue);
    }

    if (fd!=-1) {
        int l = write(fd,data,len);
        if (((int)len)!=l) return STD_ERR(INTERFACE,FAIL,errno);
    }
    return STD_ERR_OK;
}

extern "C" bool nas_int_port_used(npu_id_t npu, port_t port) {
    std_rw_lock_read_guard l(&ports_lock);
    return _ports[npu][port].valid();
}

extern "C" bool nas_int_port_ifindex (npu_id_t npu, port_t port, hal_ifindex_t *ifindex) {
    std_rw_lock_read_guard l(&ports_lock);
    if (!_ports[npu][port].valid()) {
        return false;
    }
    *ifindex = _ports[npu][port].ifindex ();
    return true;
}

extern "C" void nas_int_port_link_change(npu_id_t npu, port_t port,
                    IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t state) {
    std_rw_lock_write_guard l(&ports_lock);

    if (!_ports[npu][port].valid()) {
        EV_LOGGING(INTERFACE,ERR,"INT-STATE", "Interface state invalid - no matching port %d:%d",(int)npu,(int)port);
        return;
    }

    _ports[npu][port].set_link_state(state);
    EV_LOGGING(INTERFACE,INFO,"INT-STATE", "Interface state change %d:%d to %d",(int)npu,(int)port,(int)state);
}

extern "C" t_std_error nas_int_port_create(npu_id_t npu, port_t port, const char *name) {

    std_rw_lock_write_guard l(&ports_lock);


    //if created already... return error
    if (_ports[npu][port].valid()) return STD_ERR(INTERFACE,PARAM,0);

    _ports[npu][port].init(npu,port);

    if (!_ports[npu][port].create(name)) {
        EV_LOGGING(INTERFACE,ERR,"INT-CREATE", "Not created %d:%d:%s - error in create",
                (int)npu,(int)port,name);
        return STD_ERR(INTERFACE,FAIL,0);
    }

    interface_ctrl_t details;
    memset(&details,0,sizeof(details));
    details.if_index = _ports[npu][port].ifindex();
    strncpy(details.if_name,name,sizeof(details.if_name)-1);
    details.npu_id = npu;
    details.port_id = port;
    details.tap_id = (npu << 16) + port;
    details.int_type = nas_int_type_PORT;

    if (dn_hal_if_register(HAL_INTF_OP_REG,&details)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"INT-CREATE", "Not created %d:%d:%s - mapping error",
                        (int)npu,(int)port,name);
        _ports[npu][port].del();
        return STD_ERR(INTERFACE,FAIL,0);
    }

    EV_LOGGING(INTERFACE,INFO,"INT-CREATE", "Interface created %d:%d:%s - %d ",
            (int)npu,(int)port,name, details.if_index);

    return STD_ERR_OK;
}

extern "C" t_std_error nas_int_port_delete(npu_id_t npu, port_t port) {
    std_rw_lock_write_guard l(&ports_lock);

    interface_ctrl_t details;

    memset(&details,0,sizeof(details));
    details.if_index = _ports[npu][port].ifindex();
    details.q_type = HAL_INTF_INFO_FROM_IF;

    if (dn_hal_get_interface_info(&details)==STD_ERR_OK) {
        if (dn_hal_if_register(HAL_INTF_OP_DEREG,&details)!=STD_ERR_OK){
            EV_LOGGING(INTERFACE,ERR,"INT-DELETE", "Not deleted %d:%d: - mapping error",
                       (int)npu,(int)port);
            return STD_ERR(INTERFACE,FAIL,0);
        }
    }

    if (!_ports[npu][port].del()) {
        return STD_ERR(INTERFACE,FAIL,0);
    }
    return STD_ERR_OK;
}

extern "C" t_std_error nas_int_port_init(void) {

    std_rw_lock_write_guard l(&ports_lock);
    size_t npus = 1; //!@TODO get the maximum ports

    _ports.resize(npus);

    for ( size_t npu_ix = 0; npu_ix < npus ; ++npu_ix ) {
        size_t port_mx = ndi_max_npu_port_get(npu_ix)*4;
        _ports[npu_ix].resize(port_mx);
    }
    return STD_ERR_OK;
}

/* debug dump function for fd to event info table */
void nas_dbg_dump_tap_fd_to_nas_port_map_tbl ()
{
    printf("\rFD to event info table \r\n");
    /* port_lock is used for accessing _tap_fd_to_event_info_map */
    std_rw_lock_read_guard l(&ports_lock);

    for (auto &it: g_vif_pkt_tx._tap_fd_to_event_info_map) {
        printf ("\rFD: %d \r\n", it.first);
    }

    return;
}
