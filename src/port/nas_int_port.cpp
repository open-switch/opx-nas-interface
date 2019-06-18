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
 * nas_int_port.cpp
 *
 *  Created on: Jun 9, 2015
 */


#include "hal_if_mapping.h"
#include "hal_interface_common.h"
#include "cps_api_object_key.h"
#include "cps_api_operation.h"
#include "cps_class_map.h"
#include "nas_os_interface.h"
#include "dell-base-if.h"
#include "dell-base-if-phy.h"
#include "nas_int_port.h"
#include "nas_int_utils.h"

#include "swp_util_tap.h"

#include "std_error_codes.h"
#include "event_log.h"
#include "std_assert.h"
#include "std_rw_lock.h"
#include "std_mutex_lock.h"
#include "std_time_tools.h"
#include "std_ip_utils.h"
#include "std_mac_utils.h"

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <signal.h>
#include <unordered_map>



#define MAX_QUEUE          1
/* num packets to read on fd event */
#define NAS_PKT_COUNT_TO_READ 10
/* num packets to read from nflog fd */
#define NAS_NFLOG_PKT_COUNT_TO_READ 1
/* invalid port id to indicate virtual interface */
#define INVALID_PORT_ID    -1

//Lock for a interface structures
static std_rw_lock_t ports_lock = PTHREAD_RWLOCK_INITIALIZER;
static std_mutex_lock_create_static_init_fast(tap_fd_lock);

class CNasPortDetails {
private:
    bool _used = false;
    npu_id_t _npu = 0;
    port_t _port = 0;
    bool _mapped = false;
    swp_util_tap_descr _dscr=nullptr;
    IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t _link =
            IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_DOWN;
public:

    virtual bool create(const char *name);
    virtual void init(npu_id_t npu, port_t port) {
        _npu = npu;
        _port = port;
        _mapped = true;
    }
    virtual void init() {
        _mapped = false;
    }
    virtual bool del();

    IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t link_state() const {
        return _link;
    }

    virtual void set_link_state(IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t state);

    hal_ifindex_t ifindex () const;
    bool valid() const { return _used; }
    bool mapped() const { return _mapped; }

    inline npu_id_t npu() const { return _npu; }
    inline port_t port() const { return _port; }
    inline swp_util_tap_descr tap() const { return _dscr; }

    virtual ~CNasPortDetails();
};

class CNasDummyPort : public CNasPortDetails {
public:
    virtual bool create(const char *name) override {return true;}
    virtual void init(npu_id_t npu, port_t port) override {}
    virtual void init() override {}
    virtual bool del() override {return true;}
    virtual void set_link_state(IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t state) override {}
};

class NasPortList {
private:
    CNasPortDetails* _dummy_port = nullptr;
    std::unordered_map<std::string, CNasPortDetails> port_name_info_map;
    std::vector<std::vector<CNasPortDetails*>> port_id_info_list;
public:
    std::vector<CNasPortDetails*>& operator[](npu_id_t npu)
    {
        return port_id_info_list[npu];
    }
    void resize(size_t npus) {port_id_info_list.resize(npus);}
    void erase(std::string name) {port_name_info_map.erase(name);}
    CNasPortDetails* dummy_port() {return _dummy_port;}
    const CNasPortDetails& operator[](std::string name) const;
    CNasPortDetails& operator[](std::string name) {return port_name_info_map[name];}
    NasPortList();
    virtual ~NasPortList();
};

NasPortList::NasPortList()
{
    _dummy_port = new CNasDummyPort();
}

NasPortList::~NasPortList()
{
    if (_dummy_port != nullptr) {
        delete _dummy_port;
        _dummy_port = nullptr;
    }
}

const CNasPortDetails& NasPortList::operator[](std::string name) const
{
    auto iter = port_name_info_map.find(name);
    if (iter == port_name_info_map.end()) {
        return *_dummy_port;
    }
    return iter->second;
}

static NasPortList& _ports = *new NasPortList();

/* tap fd to event info details */
typedef std::unordered_map<int,struct event *> _fd_to_event_info_map_t;

typedef struct _nas_vif_pkt_tx_t {
    struct event_base *nas_evt_base;    // Pointer to event base
    struct event *nas_signal_event;     // Pointer to our signal event
    hal_virt_pkt_transmit egress_tx_cb; // Pointer to packet tx callback function
    // Pointer to packet tx to ingress pipeline callback function
    hal_virt_pkt_transmit_to_ingress_pipeline tx_to_ingress_fun;
    hal_virt_pkt_transmit_to_ingress_pipeline_hybrid tx_to_ingress_hybrid_fun;
    void *tx_buf;                      // Pointer to packet tx buffer
    unsigned int tx_buf_len;           // packet tx buffer len
    struct event *nas_nflog_fd_ev;     // nflog fd event struct
    int nas_nflog_fd;                  // fd for packet copy thru nflog
    _fd_to_event_info_map_t _tap_fd_to_event_info_map; //fd to event base info
} nas_vif_pkt_tx_t;

nas_vif_pkt_tx_t g_vif_pkt_tx; //global virtual interface packet tx information

static int     nas_nflog_pkts_tx_to_ingress_pipeline = 0;
static int     nas_nflog_pkts_tx_to_ingress_pipeline_dropped = 0;
static int     nas_nflog_pkts_tx_to_ingress_pipeline_hybrid = 0;
static int     nas_nflog_pkts_tx_to_ingress_pipeline_hybrid_dropped = 0;
uint8_t        dest_ipv4_from_last_pkt[4];
static struct timespec ts_last_pkt_sent = {0,0};
uint8_t        dest_ipv6_from_last_pkt[16];
static struct timespec ts_last_ns_pkt_sent = {0,0};

typedef struct _arp_header {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t operation;
    uint8_t  sender_hw_addr[6];
    uint8_t  sender_ip[4];
    uint8_t  target_hw_addr[6];
    uint8_t  target_ip[4];
} arp_header_t;

typedef struct _icmpv6_options_header {
    uint8_t type;
    uint8_t length;
    uint8_t link_layer_address[6];
}icmpv6_options_header_t;

typedef struct _icmpv6_header {
    uint8_t type;
    uint8_t code;
    uint16_t  checksum;
    uint32_t reserved;
    uint8_t target_address[16];
} icmpv6_header_t;

typedef struct _ipv6_header {
    uint32_t ver_class_flow;
    uint16_t  plen;
    uint8_t next_header;
    uint8_t  hop_limit;
    uint8_t  source_ipv6[16];
    uint8_t  target_ipv6[16];
} ipv6_header_t;

void process_packets (evutil_socket_t fd, short evt, void *arg);
void process_nflog_packets (evutil_socket_t fd, short evt, void *arg);

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
            EV_LOGGING(INTERFACE,INFO,"TAP-TX", "NAS Packet read event add failed for interface (%s) fd (%d).",
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
            EV_LOGGING(INTERFACE,INFO,"TAP-UP",
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

static void tap_fd_close(swp_util_tap_descr tap) {

    //make sure tap_fd access to be protected before closing it.
    //make sure tap_fd_lock is not taken before calling any event lib api's.
    std_mutex_simple_lock_guard l(&tap_fd_lock);
    swp_util_close_fds(tap);
    return;
}

static bool tap_link_down(swp_util_tap_descr tap) {
    //during link down, deregister fd from event poll before closing fd's.
    tap_fd_deregister_from_evt (tap);
    tap_fd_close(tap);
    return true;
}

static bool tap_link_up(swp_util_tap_descr tap, CNasPortDetails *npu) {
    //just incase... clean up
    //during cleanup, deregister fd from event poll if already registered
    tap_fd_deregister_from_evt (tap);
    tap_fd_close(tap);

    size_t retry = 0;
    const size_t MAX_RETRY = 12;
    bool success = false;
    for ( ; retry < MAX_RETRY ; ++retry ) {
        if (swp_util_alloc_tap(tap,SWP_UTIL_TYPE_TAP)!=STD_ERR_OK) {
            EV_LOGGING(INTERFACE,INFO,"INT-LINK",
                    "Can not bring the link up %s had issues opening device (%lu)",
                    swp_util_tap_descr_get_name(tap),retry);
            std_usleep(MILLI_TO_MICRO((1<<retry)));
            continue;
        }
        if (tap_fd_register_with_evt(tap, npu) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"INT-LINK",
                    "Can not bring the link up %s had issues in registering device fd with event handler (%lu)",
                    swp_util_tap_descr_get_name(tap),retry);
            //on event reg failure, deregister fd from event poll for already registered fd's and retry
            tap_fd_deregister_from_evt (tap);
            tap_fd_close(tap);
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
    if (_dscr == nullptr || _link == state) return;
    if (state == IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_UP) {
        if (tap_link_up(_dscr, this)) _link=state;
    }
    if (state == IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_DOWN) {
        tap_link_down(_dscr);
        _link = state;
    }
}

hal_ifindex_t CNasPortDetails::ifindex() const{
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
    //@@TODO de-init nas_nflog_fd
    event_del (g_vif_pkt_tx.nas_nflog_fd_ev);
    event_free (g_vif_pkt_tx.nas_nflog_fd_ev);
    g_vif_pkt_tx.nas_nflog_fd = -1;

    event_del (p_vif_pkt_tx->nas_signal_event);
    event_free (p_vif_pkt_tx->nas_signal_event);
    event_base_loopbreak(p_vif_pkt_tx->nas_evt_base);
}

int nas_process_payload_and_form_NS_packet(uint8_t *pkt_buf, nas_nflog_params_t *p_nas_nflog_params,
                                           interface_ctrl_t *intf_ctrl)
{
#define NSEC_PER_SEC 1000000000
#define ICMPV6_HDR 0x3A
#define ICMPV6_NS_PACKET 0x87

    int               pkt_len = 0;
    static const uint16_t          vlan_protocol = htons (0x8100);
    uint16_t          vlan_id = 0;
    struct timespec   ts_cur_pkt;
    struct timespec   ts_diff;
    unsigned char     dest_mac[6]={0x33,0x33,0x33,0x33,0x33,0x13};
    ipv6_header_t      *p_ipv6_header = NULL;
    uint8_t     src_mac[6]={0x00,0x00,0x00,0x00,0x00,0x00};
    icmpv6_header_t *p_icmpv6_header = NULL ;
    icmpv6_options_header_t *p_icmpv6_options_header = NULL;

    p_ipv6_header = (ipv6_header_t *)p_nas_nflog_params->payload;

    memcpy(&dest_mac[2],&(p_ipv6_header->target_ipv6[12]),4);

    if(p_ipv6_header->next_header != ICMPV6_HDR)
    {
        /* Non NS packet. return it here */
        return -1;
    }

    if(intf_ctrl->int_type == nas_int_type_VLAN)
    {
        vlan_id = htons (intf_ctrl->vlan_id);
    }
    if((intf_ctrl->l3_intf_info.if_index != 0) && (intf_ctrl->l3_intf_info.vrf_id != 0))
    {
        hal_vrf_id_t parent_vrf_id = intf_ctrl->l3_intf_info.vrf_id;
        hal_ifindex_t parent_if_index = intf_ctrl->l3_intf_info.if_index;
        if (parent_if_index != 0) {
            memset(intf_ctrl, 0, sizeof(interface_ctrl_t));
            intf_ctrl->q_type = HAL_INTF_INFO_FROM_IF;
            intf_ctrl->vrf_id = parent_vrf_id;
            intf_ctrl->if_index = parent_if_index;

            if ((dn_hal_get_interface_info(intf_ctrl)) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE,ERR,"TAP-TX", "Invalid VRF interface %d. ",
                           parent_if_index);
                return -1;
            }
        }
    }
    clock_gettime (CLOCK_MONOTONIC, &ts_cur_pkt);

    ts_diff.tv_nsec = (ts_cur_pkt.tv_nsec - ts_last_ns_pkt_sent.tv_nsec);
    ts_diff.tv_sec = (ts_cur_pkt.tv_sec - ts_last_ns_pkt_sent.tv_sec);

    while (ts_diff.tv_nsec >= NSEC_PER_SEC)
    {
        ts_diff.tv_nsec -= NSEC_PER_SEC;
        ++ts_diff.tv_sec;
    }
    while (ts_diff.tv_nsec < 0)
    {
        ts_diff.tv_nsec += NSEC_PER_SEC;
        --ts_diff.tv_nsec;
    }
    if ((memcmp (&dest_ipv6_from_last_pkt, &p_ipv6_header->target_ipv6, 16) == 0) &&
        (ts_diff.tv_sec < 1))
    {
        return 0;
    }

    ts_last_ns_pkt_sent = ts_cur_pkt;
    memcpy (&dest_ipv6_from_last_pkt, &p_ipv6_header->target_ipv6, 16);

    memcpy ((pkt_buf + pkt_len), &dest_mac, 6);
    pkt_len += 6;

    p_icmpv6_header = (icmpv6_header_t *)(p_nas_nflog_params->payload + sizeof(ipv6_header_t));
    if(p_icmpv6_header->type == ICMPV6_NS_PACKET)
    {
        p_icmpv6_options_header = (icmpv6_options_header_t *)(p_nas_nflog_params->payload + (sizeof(ipv6_header_t) + sizeof(icmpv6_header_t)));
        // Need to get the data from payload as much as possible.
        if(p_icmpv6_options_header->type == 0x1)
            memcpy (&src_mac, &p_icmpv6_options_header->link_layer_address, 6);
        else
            std_string_to_mac(&src_mac, intf_ctrl->mac_addr,sizeof(intf_ctrl->mac_addr));
    }
    else
    {
        std_string_to_mac(&src_mac, intf_ctrl->mac_addr,sizeof(intf_ctrl->mac_addr));
    }
    /* copy the ethernet header source-mac from interface structure. icmpv6 options wont be available in some NS packet */
    memcpy ((pkt_buf + pkt_len), &src_mac, 6);
    pkt_len += 6;


    if(vlan_id != 0 )
    {
        memcpy ((pkt_buf + pkt_len), &vlan_protocol, 2);
        pkt_len += 2;

        memcpy ((pkt_buf + pkt_len), &vlan_id, 2);
        pkt_len += 2;
    }
    memcpy ((pkt_buf + pkt_len), &p_nas_nflog_params->hw_protocol, 2);
    pkt_len += 2;

    memcpy ((pkt_buf + pkt_len), p_nas_nflog_params->payload, p_nas_nflog_params->payload_len);
    pkt_len += p_nas_nflog_params->payload_len;
    return pkt_len;
}

int nas_process_payload_and_form_packet (uint8_t *pkt_buf,
                                         nas_nflog_params_t *p_nas_nflog_params,
                                         interface_ctrl_t *intf_ctrl)
{
#define NSEC_PER_SEC 1000000000
#define ICMPV6_HDR 0x3A
#define ICMPV6_NS_PACKET 0x87
    int               pkt_len = 0;
    uint16_t          vlan_id = 0;
    static const uint16_t          vlan_protocol = htons (0x8100);
    static const uint16_t          arp_protocol = htons (0x0806);
    static const uint16_t          ip_protocol = htons (0x0800); //ipv4
    static const uint16_t          ip6_protocol = htons (0x86dd); //ipv6
    static const unsigned char     dest_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    arp_header_t      *p_arp_header = NULL;
    struct timespec   ts_cur_pkt;
    struct timespec   ts_diff;
    ipv6_header_t      *p_ipv6_header = NULL;
    icmpv6_header_t *p_icmpv6_header = NULL ;

    p_ipv6_header = (ipv6_header_t *)p_nas_nflog_params->payload;
    if((p_nas_nflog_params->hw_protocol == ip6_protocol) &&
       p_ipv6_header->next_header == ICMPV6_HDR)
    {
        p_icmpv6_header = (icmpv6_header_t *)(p_nas_nflog_params->payload + sizeof(ipv6_header_t));
        if(p_icmpv6_header->type == ICMPV6_NS_PACKET)
        {
            pkt_len = nas_process_payload_and_form_NS_packet (pkt_buf,p_nas_nflog_params, intf_ctrl);
            return pkt_len;
        }
        return -1;
    }

    /* reference arp header from nflog payload */
    p_arp_header = (arp_header_t *) p_nas_nflog_params->payload;

    if (!(p_nas_nflog_params->payload_len))
    {
        /* return, if payload length is not valid */
        return -1;
    }

    if ((p_nas_nflog_params->hw_protocol != arp_protocol) ||
        (p_arp_header->ptype != ip_protocol))
    {
        /* return, if its not IPv4 ARP */
        return -1;
    }

    clock_gettime (CLOCK_MONOTONIC, &ts_cur_pkt);

    ts_diff.tv_nsec = (ts_cur_pkt.tv_nsec - ts_last_pkt_sent.tv_nsec);
    ts_diff.tv_sec = (ts_cur_pkt.tv_sec - ts_last_pkt_sent.tv_sec);

    while (ts_diff.tv_nsec >= NSEC_PER_SEC)
    {
        ts_diff.tv_nsec -= NSEC_PER_SEC;
        ++ts_diff.tv_sec;
    }
    while (ts_diff.tv_nsec < 0)
    {
        ts_diff.tv_nsec += NSEC_PER_SEC;
        --ts_diff.tv_nsec;
    }

    /* For ARP requests, check the following and drop the packets by returning length as 0:
     * 1) check if the target-ip of this ARP request and previous ARP request is same
     * 2) check if the time elapsed between two ARP request packet for same destination
     *    less than a second,
     * if yes, then drop this packet as this could be a copy that Kernel replicated
     * for other VLAN member ports in the bridge.
     */
    if ((memcmp (&dest_ipv4_from_last_pkt, &p_arp_header->target_ip, 4) == 0) &&
        (ts_diff.tv_sec < 1))
    {
        return 0;
    }
    ts_last_pkt_sent = ts_cur_pkt;
    memcpy (&dest_ipv4_from_last_pkt, &p_arp_header->target_ip, 4);

    memcpy ((pkt_buf + pkt_len), &dest_mac, 6);
    pkt_len += 6;

    /* copy the ethernet header source-mac same as the ARP payload sender mac */
    memcpy ((pkt_buf + pkt_len), p_arp_header->sender_hw_addr, 6);
    pkt_len += 6;

    if(intf_ctrl->int_type == nas_int_type_VLAN)
    {
        vlan_id = htons (intf_ctrl->vlan_id);

        memcpy ((pkt_buf + pkt_len), &vlan_protocol, 2);
        pkt_len += 2;

        memcpy ((pkt_buf + pkt_len), &vlan_id, 2);
        pkt_len += 2;
    }
    memcpy ((pkt_buf + pkt_len), &p_nas_nflog_params->hw_protocol, 2);
    pkt_len += 2;

    memcpy ((pkt_buf + pkt_len), p_nas_nflog_params->payload, p_nas_nflog_params->payload_len);
    pkt_len += p_nas_nflog_params->payload_len;

    return pkt_len;
}


/*
 * Callback function from event for read event from nflog fd.
 * Here we are not interested in any of the context information.
 */
void process_nflog_packets (evutil_socket_t fd, short evt, void *arg)
{
    int pkt_len = 0;
    int pkt_count = 0;
    nas_nflog_params_t nflog_params;

    /* event is received in level-triggered mode,
     * so read data as required and w/o starving other ports
     */
    while (pkt_count < NAS_NFLOG_PKT_COUNT_TO_READ)
    {
        pkt_len = read(fd, g_vif_pkt_tx.tx_buf, g_vif_pkt_tx.tx_buf_len);

        if (pkt_len <=0)
        {
            /* no more data to read */
            break;
        }
        pkt_count++;

        nflog_params.out_ifindex = 0;
        nflog_params.payload_len = 0;

        nas_os_nl_get_nflog_params ((uint8_t *) g_vif_pkt_tx.tx_buf,
                                    pkt_len, &nflog_params);

        if (!(nflog_params.payload_len))
        {
            /* skip, if payload length is not valid */
            nas_nflog_pkts_tx_to_ingress_pipeline_dropped++;
            continue;
        }

        interface_ctrl_t  intf_ctrl;
        memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));
        intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
        intf_ctrl.if_index = nflog_params.out_ifindex;

        /* retrieve the VLAN id from interface index */
        if ((dn_hal_get_interface_info(&intf_ctrl)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"TAP-TX", "Processing payload failed. Invalid interface %d. ifInfo get failed",
                       nflog_params.out_ifindex);
            nas_nflog_pkts_tx_to_ingress_pipeline_dropped++;
            continue;
        }

        nas_int_type_t int_type = intf_ctrl.int_type;
        nas_bridge_id_t bridge_id = intf_ctrl.bridge_id;
        pkt_len = nas_process_payload_and_form_packet ((uint8_t *) g_vif_pkt_tx.tx_buf,
                                                       &nflog_params, &intf_ctrl);

        /* send packet for transmission to ingress pipeline processing
         * to the registered callback function with registered packet buffer
         */
        if (pkt_len > 0) {
            if(int_type == nas_int_type_DOT1D_BRIDGE) {
                nas_nflog_pkts_tx_to_ingress_pipeline_hybrid++;
                g_vif_pkt_tx.tx_to_ingress_hybrid_fun (g_vif_pkt_tx.tx_buf,pkt_len,
                                                       NDI_PACKET_TX_TYPE_PIPELINE_HYBRID_BRIDGE, bridge_id);
            } else {
                nas_nflog_pkts_tx_to_ingress_pipeline++;
                g_vif_pkt_tx.tx_to_ingress_fun (g_vif_pkt_tx.tx_buf,pkt_len);
            }
        } else if (pkt_len == 0) {
            if(int_type == nas_int_type_DOT1D_BRIDGE) {
                nas_nflog_pkts_tx_to_ingress_pipeline_hybrid_dropped++;
            } else {
                nas_nflog_pkts_tx_to_ingress_pipeline_dropped++;
            }
        }
    }
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

    swp_util_tap_descr tap = details->tap();

    /* event is received in level-triggered mode,
     * so read data as required and w/o starving other ports
     */
    while (pkt_count < NAS_PKT_COUNT_TO_READ)
    {
        {
            std_mutex_simple_lock_guard l(&tap_fd_lock);
            if (swp_util_tap_is_fd_in_tap_fd_set(tap, fd) == false)
            {
                EV_LOGGING(INTERFACE,ERR, "TAP-TX", "TAP fd closed already. "
                        "npu:%d, port:%d, fd:%d",
                        npu, port, fd);
                break;
            }

            pkt_len = read(fd, g_vif_pkt_tx.tx_buf, g_vif_pkt_tx.tx_buf_len);
        }
        if (pkt_len <=0)
        {
            /* no more data to read */
            break;
        }
        pkt_count++;
        /* send packet for transmission to registered callback function with registered packet buffer */
        g_vif_pkt_tx.egress_tx_cb(npu,port,g_vif_pkt_tx.tx_buf,pkt_len);
    }
}


/* packet transmission from virtual interface is handled via libevent.
 * call to hal_virtual_interface_wait() trigger event dispatcher.
 */
t_std_error hal_virtual_interface_wait (hal_virt_pkt_transmit tx_fun,
                                        hal_virt_pkt_transmit_to_ingress_pipeline tx_to_ingress_fun,
                                        hal_virt_pkt_transmit_to_ingress_pipeline_hybrid tx_to_ingress_hybrid_fun,
                                        void *data, unsigned int len)
{
    /* initialize global virtual interface packet tx information with given input params */
    g_vif_pkt_tx.nas_evt_base = NULL;
    g_vif_pkt_tx.nas_signal_event = NULL;
    g_vif_pkt_tx.nas_nflog_fd_ev = NULL;
    g_vif_pkt_tx.nas_nflog_fd = -1;

    g_vif_pkt_tx.egress_tx_cb = tx_fun;
    g_vif_pkt_tx.tx_to_ingress_fun = tx_to_ingress_fun;
    g_vif_pkt_tx.tx_to_ingress_hybrid_fun = tx_to_ingress_hybrid_fun;
    g_vif_pkt_tx.tx_buf = data;
    g_vif_pkt_tx.tx_buf_len = len;

    if (evthread_use_pthreads()) {
        EV_LOGGING (INTERFACE,ERR,"TAP-TX", "NAS Packet event lock initialization failed.");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    /* initialize event base */
    g_vif_pkt_tx.nas_evt_base = event_base_new();
    if (!g_vif_pkt_tx.nas_evt_base)
    {
        EV_LOGGING(INTERFACE,ERR,"TAP-TX", "NAS Packet event base initialization failed.");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    g_vif_pkt_tx.nas_nflog_fd = nas_os_nl_nflog_init ();
    if (g_vif_pkt_tx.nas_nflog_fd == -1)
    {
        EV_LOGGING(INTERFACE,ERR,"TAP-TX", "NAS NFLOG Packet read initialization failed.");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    // Setup the events for nflog fd
    g_vif_pkt_tx.nas_nflog_fd_ev = event_new(g_vif_pkt_tx.nas_evt_base, g_vif_pkt_tx.nas_nflog_fd,
                          EV_READ | EV_PERSIST, process_nflog_packets, NULL);

    //@@TODO de-init nas_nflog_fd on failure
    if (!g_vif_pkt_tx.nas_nflog_fd_ev) {
        EV_LOGGING(INTERFACE,ERR,"TAP-TX", "NAS Packet read event create failed for nflog fd (%d).",
                   g_vif_pkt_tx.nas_nflog_fd);
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (event_add(g_vif_pkt_tx.nas_nflog_fd_ev, NULL) < 0) {
        EV_LOGGING(INTERFACE,ERR,"TAP-TX", "NAS Packet read event add failed for nflog fd (%d).",
                   g_vif_pkt_tx.nas_nflog_fd);
        event_free (g_vif_pkt_tx.nas_nflog_fd_ev);
        return STD_ERR(INTERFACE,FAIL,0);
    }


    /* initialize event for sigint */
    g_vif_pkt_tx.nas_signal_event = event_new (g_vif_pkt_tx.nas_evt_base, SIGINT,
                    EV_SIGNAL | EV_PERSIST, nas_evt_signal_cb, (void *)&g_vif_pkt_tx);

    if (!g_vif_pkt_tx.nas_signal_event || event_add(g_vif_pkt_tx.nas_signal_event, NULL)<0) {
        EV_LOGGING(INTERFACE,ERR,"TAP-TX", "NAS Packet signal event initialization failed.");
        event_del (g_vif_pkt_tx.nas_nflog_fd_ev);
        event_free (g_vif_pkt_tx.nas_nflog_fd_ev);
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
        //@@TODO de-init nas_nflog_fd on failure
        event_del (g_vif_pkt_tx.nas_nflog_fd_ev);
        event_free (g_vif_pkt_tx.nas_nflog_fd_ev);
        event_base_free (g_vif_pkt_tx.nas_evt_base);
        return STD_ERR(INTERFACE,FAIL,0);
    }
    return STD_ERR_OK;
}

t_std_error hal_virtual_interface_send(npu_id_t npu, npu_port_t port, int queue,
                                       const void * data, unsigned int len) {
    int fd = -1;
    {
        std_rw_lock_read_guard l(&ports_lock);
        if (_ports[npu][port]->link_state()!=IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_UP) {
            return STD_ERR_OK;
        }
        fd = swp_util_tap_descr_get_queue(_ports[npu][port]->tap(), queue);
    }

    if (fd!=-1) {
        int l = write(fd,data,len);
        if (((int)len)!=l) return STD_ERR(INTERFACE,FAIL,errno);
    }
    return STD_ERR_OK;
}

static bool nas_int_port_mapped(const char* name)
{
    const auto& ports = _ports;
    if (!ports[name].valid()) {
        return false;
    }

    return ports[name].mapped();
}

static bool nas_int_port_used_int(const char* name, npu_id_t npu, port_t port,
                                  bool check_port)
{
    if (name != nullptr) {
        const auto& ports = _ports;
        if (!ports[name].valid()) {
            return false;
        }
        if (check_port) {
            return (ports[name].npu() == npu &&
                    ports[name].port() == port);
        }
    } else if (check_port) {
        return _ports[npu][port]->valid();
    }

    return true;
}

bool nas_int_port_id_used(npu_id_t npu, port_t port) {
    std_rw_lock_read_guard l(&ports_lock);
    return nas_int_port_used_int(nullptr, npu, port, true);
}

bool nas_int_port_name_used(const char *name) {
    std_rw_lock_read_guard l(&ports_lock);
    return nas_int_port_used_int(name, 0, 0, false);
}

bool nas_int_port_ifindex (npu_id_t npu, port_t port, hal_ifindex_t *ifindex) {
    std_rw_lock_read_guard l(&ports_lock);
    if (!nas_int_port_used_int(nullptr, npu, port, true)) {
        return false;
    }
    *ifindex = _ports[npu][port]->ifindex ();
    return true;
}

void nas_int_port_link_change(npu_id_t npu, port_t port,
                              IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t state) {
    std_rw_lock_write_guard l(&ports_lock);

    if (!nas_int_port_used_int(nullptr, npu, port, true)) {
        EV_LOGGING(INTERFACE,ERR,"INT-STATE", "Interface state invalid - no matching port %d:%d",(int)npu,(int)port);
        return;
    }

    _ports[npu][port]->set_link_state(state);
    EV_LOGGING(INTERFACE,INFO,"INT-STATE", "Interface state change %d:%d to %d",(int)npu,(int)port,(int)state);
}

static t_std_error nas_int_port_create_int(npu_id_t npu, port_t port, const char *name,
                                           nas_int_type_t type,
                                           bool mapped) {

    std_rw_lock_write_guard l(&ports_lock);


    //if created already... return error
    if (nas_int_port_used_int(name, 0, 0, false)) {
        EV_LOGGING(INTERFACE,ERR,"INT-CREATE", "Not created %s - interface exist",
                   name);
        return STD_ERR(INTERFACE,PARAM,0);
    }

    if (mapped) {
        _ports[name].init(npu, port);
    } else {
        _ports[name].init();
    }
    _ports[name].create(name);
    if (mapped) {
        _ports[npu][port] = &_ports[name];
    }

    interface_ctrl_t details;
    memset(&details,0,sizeof(details));
    details.if_index = _ports[name].ifindex();
    strncpy(details.if_name,name,sizeof(details.if_name)-1);
    if (mapped) {
        details.npu_id = npu;
        details.port_id = port;
        details.tap_id = (npu << 16) + port;
    } else {
        details.port_id = static_cast<npu_port_t>(INVALID_PORT_ID);
    }
    details.int_type = type;
    details.port_mapped = mapped;
    details.desc = nullptr;

    if (dn_hal_if_register(HAL_INTF_OP_REG,&details)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"INT-CREATE", "Not created %d:%d:%s - mapping error",
                        (int)npu,(int)port,name);
        if (mapped) {
            _ports[npu][port] = _ports.dummy_port();
        }
        _ports.erase(name);
        return STD_ERR(INTERFACE,FAIL,0);
    }

    EV_LOGGING(INTERFACE,INFO,"INT-CREATE", "Interface created %d:%d:%s - %d ",
            (int)npu,(int)port,name, details.if_index);

    return STD_ERR_OK;
}

t_std_error nas_int_port_create_mapped(npu_id_t npu, port_t port, const char *name,
                                       nas_int_type_t type) {
    return nas_int_port_create_int(npu, port, name, type, true);
}

t_std_error nas_int_port_create_unmapped(const char *name, nas_int_type_t type) {
    return nas_int_port_create_int(0, 0, name, type, false);
}

t_std_error nas_int_port_delete(const char *name) {
    std_rw_lock_write_guard l(&ports_lock);

    interface_ctrl_t details;

    const auto& ports = _ports;
    const auto& port_info = ports[name];
    if (!port_info.valid()) {
        return STD_ERR(INTERFACE, PARAM, 0);
    }

    memset(&details,0,sizeof(details));
    details.if_index = port_info.ifindex();
    details.q_type = HAL_INTF_INFO_FROM_IF;

    if (dn_hal_get_interface_info(&details)==STD_ERR_OK) {
        if (dn_hal_if_register(HAL_INTF_OP_DEREG,&details)!=STD_ERR_OK){
            EV_LOGGING(INTERFACE,ERR,"INT-DELETE", "Not deleted %s: - mapping error",
                       name);
            return STD_ERR(INTERFACE,FAIL,0);
        }
    }

    if (port_info.mapped()) {
        npu_id_t npu = port_info.npu();
        port_t port = port_info.port();
        _ports[npu][port] = _ports.dummy_port();
    }
    _ports[name].del();
    _ports.erase(name);

    return STD_ERR_OK;
}

t_std_error nas_int_port_init(void) {

    std_rw_lock_write_guard l(&ports_lock);
    size_t npus = 1; //!@TODO get the maximum ports

    _ports.resize(npus);

    for ( size_t npu_ix = 0; npu_ix < npus ; ++npu_ix ) {
        size_t port_mx = ndi_max_npu_port_get(npu_ix)*4;
        _ports[npu_ix].assign(port_mx, _ports.dummy_port());
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

void nas_nflog_dbg_counters ()
{
    printf("\rNAS NFLOG DEBUG COUNTERS\r\n");
    printf("\r========================\r\n");
    printf("\rTotal packets received                        : %d\r\n",
           nas_nflog_pkts_tx_to_ingress_pipeline);
    printf("\rTotal flood packets dropped                   : %d\r\n",
           nas_nflog_pkts_tx_to_ingress_pipeline_dropped);
}

void nas_nflog_dbg_reset_counters ()
{
    nas_nflog_pkts_tx_to_ingress_pipeline = 0;
    nas_nflog_pkts_tx_to_ingress_pipeline_dropped = 0;
}

static t_std_error update_if_reg_info(const char *name, npu_id_t npu, port_t port,
                                      bool connect)
{
    interface_ctrl_t info;

    memset(&info, 0, sizeof(info));
    strncpy(info.if_name, name, sizeof(info.if_name) - 1);
    info.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    if (dn_hal_get_interface_info(&info) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "INTF-UPDATE", "Could not find interface %s from port info db",
                   name);
        return STD_ERR(INTERFACE, PARAM, 0);
    }
    if (info.int_type != nas_int_type_PORT) {
        EV_LOGGING(INTERFACE, ERR, "INTF-UPDATE", "Registered info of interface %s is not port type",
                   name);
        return STD_ERR(INTERFACE, PARAM, 0);
    }
    if ((connect && info.port_mapped) ||
        (!connect && (info.npu_id != npu || info.port_id != port))) {
        EV_LOGGING(INTERFACE, ERR, "INTF-UPDATE", "Reg info mismatch between input and port info db");
        return STD_ERR(INTERFACE, PARAM, 0);
    }

    if (dn_hal_if_register(HAL_INTF_OP_DEREG, &info) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "INTF-UPDATE", "Failed to de-register interface %s",
                   name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    if (connect) {
        info.npu_id = npu;
        info.port_id = port;
        info.tap_id = (npu << 16) + port;
        info.port_mapped = true;
    } else {
        info.tap_id = 0;
        info.port_mapped = false;
    }

    if (dn_hal_if_register(HAL_INTF_OP_REG, &info) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "INTF-UPDATE", "Failed to re-register interface %s",
                   name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    return STD_ERR_OK;
}


t_std_error nas_int_update_npu_port(const char *name, npu_id_t npu, port_t port,
                                    bool connect)
{
    std_rw_lock_write_guard l(&ports_lock);

    if ((connect && (nas_int_port_mapped(name) ||
                     nas_int_port_used_int(nullptr, npu, port, true))) ||
        (!connect && (!nas_int_port_mapped(name) ||
                     !nas_int_port_used_int(nullptr, npu, port, true)))) {
        EV_LOGGING(INTERFACE, ERR, "INT-UPDATE", "Interface %s could not be connected or disconnected",
                   name);
        EV_LOGGING(INTERFACE, ERR, "INT-UPDATE", "npu %d port %d connect %s \
 logical_port mapped %s physical_port used %s",
                   npu, port, connect ? "TRUE" : "FALSE",
                   nas_int_port_mapped(name) ? "TRUE" : "FALSE",
                   nas_int_port_used_int(nullptr, npu, port, true) ? "TRUE" : "FALSE");
        return STD_ERR(INTERFACE, PARAM, 0);
    }

    if (connect) {
        _ports[name].init(npu, port);
        _ports[npu][port] = &_ports[name];
        ndi_intf_link_state_t link_state;
        if (ndi_port_link_state_get(npu, port, &link_state) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "INT-UPDATE", "Failed to get link state for port %d",
                       port);
            _ports[npu][port] = _ports.dummy_port();
            return STD_ERR(INTERFACE, FAIL, 0);
        }
        IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t state =
                        ndi_to_cps_oper_type(link_state.oper_status);
        _ports[npu][port]->set_link_state(state);
    } else {
        _ports[npu][port]->set_link_state(IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_DOWN);
        _ports[name].init();
        _ports[npu][port] = _ports.dummy_port();

        // Disable un-mapped NPU port to force its link down
        if (ndi_port_admin_state_set(npu, port, false) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR,
                       "INTF-UPDATE", "Failed to disable dis-associated interface %s",
                       name);
            return STD_ERR(INTERFACE, FAIL, 0);
        }
    }

    if (update_if_reg_info(name, npu, port, connect) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "INTF-UPDATE", "Failed to update reg info for interface %s",
                   name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    if (update_if_tracker(name, npu, port, connect) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "INTF-UPDATE", "Failed to update tracker for interface %s",
                   name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    EV_LOGGING(INTERFACE, NOTICE, "INTF-UPDATE", "Interface %s is %s NPU %d PORT %d",
               name, (connect ? "connected to" : "disconnected from"),
               npu, port);

    return STD_ERR_OK;
}
