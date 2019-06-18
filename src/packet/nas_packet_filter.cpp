/*
 * Copyright (c) 2019 Dell Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 * LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 * FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 * See the Apache Version 2.0 License for specific language governing
 * permissions and limitations under the License.
 */
/*
 * filename: nas_packet_filter.cpp
 *
 *  Created on: Nov 13, 2016
 */

#include "event_log.h"
#include "event_log_types.h"
#include "nas_int_filter_class.h"
#include "cps_api_operation.h"
#include "nas_packet_meta.h"
#include "hal_if_mapping.h"
#include "nas_int_port.h"
#include "nas_int_utils.h"

#include <iostream>
#include <memory>
#include <functional>
#include <exception>

//Global instance of packet-filter table
std::unique_ptr<pf_table> pf_table_inst(nullptr);

static int pf_sock_fd = -1;

//Filter Table class definitions
nas_obj_id_t pf_table::pf_t_create_rule(pf_direction dir, pf_match_t& mat, pf_action_t& act,
                                        bool stop) {

    nas_obj_id_t id = pf_t_alloc_tid();
    EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","New id generated %lu", id);

    pf_rule rule (id, mat, act);
    rule.pf_r_set_stop(stop);

    return pf_t_add_rule(dir, rule);
}

nas_obj_id_t pf_table::pf_t_add_rule(pf_direction dir, pf_rule& rule) {

    if(!rule.pf_r_get_id()) {
        nas_obj_id_t id = pf_t_alloc_tid();
        rule.pf_r_set_id(id);
    }

    std::lock_guard<std::mutex> pf_tlock {pf_mtx};

    if( dir == BASE_PACKET_PACKET_DIRECTION_TYPE_DIR_IN) {
        ++ingress_rules;
        EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","Ingress rule %lu added - Total count %d",
                                   rule.pf_r_get_id(),ingress_rules);
        pf_ingress_table.emplace_back(rule);
    } else {
        ++egress_rules;
        EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","Egress rule %lu added - Total count %d",
                                   rule.pf_r_get_id(), egress_rules);
        pf_egress_table.emplace_back(rule);
    }

    return rule.pf_r_get_id();
}

const pf_rule* pf_gen_get_id(const std::vector<pf_rule>& vec, nas_obj_id_t& id) {
    for (auto itr = vec.begin(); itr != vec.end(); ++itr) {
        if((*itr).pf_r_get_id() == id) {
            return &(*itr);
        }
    }
    return nullptr;
}

bool pf_gen_erase_id(std::vector<pf_rule>& vec, nas_obj_id_t& id) {
    for (auto itr = vec.begin(); itr != vec.end(); ++itr) {
        if((*itr).pf_r_get_id() == id) {
            vec.erase(itr);
            return true;
        }
    }
    return false;
}

const pf_rule* pf_table::pf_t_get_rule(nas_obj_id_t id) noexcept {

    const pf_rule* p_pfr = nullptr;

    std::lock_guard<std::mutex> pf_tlock {pf_mtx};

    if((p_pfr = pf_gen_get_id(pf_ingress_table, id)) == nullptr)
        p_pfr = pf_gen_get_id(pf_egress_table, id);

    return p_pfr;
}

bool pf_table::pf_t_del_rule(nas_obj_id_t id) {

    bool del=false;

    std::lock_guard<std::mutex> pf_tlock {pf_mtx};

    if((del = pf_gen_erase_id(pf_ingress_table, id))) {
        --ingress_rules;
        EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","Ingress rule %lu deleted, rem %d", id, ingress_rules);
    } else if((del = pf_gen_erase_id(pf_egress_table, id))) {
        --egress_rules;
        EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","Egress rule %lu deleted, rem %d", id, egress_rules);
    }

    if(!del) EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","Rule not found id %lu", id);
    else pf_t_release_tid(id);

    return del;
}

/*
 * Return true if you want to terminate handling this packet and stop processing further actions
 */
bool pf_table::pf_t_in_pkt_hndlr(uint8_t *pkt, uint32_t pkt_len, pf_pkt_attr *p_attr) {

    EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","In_Pkt handler - len %d, port %d, trap id %lld",
		pkt_len, p_attr->rx_port, p_attr->trap_id);

    std::lock_guard<std::mutex> pf_tlock {pf_mtx};

    for (auto pfr : pf_ingress_table) {
        EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","Scanning rule id %lu", pfr.pf_r_get_id());
        bool match = true, action = true;

        //Consider using std::invoke
        pfr.pf_r_get_match_params([&](pf_match_t& m_tv) {
            const pf_match ref = pfr.pf_r_get_match_list();
            match &= ref.pf_m_inv_fptr(m_tv.m_type, pkt, pkt_len, p_attr, m_tv);
        });

        if(match) {
            pfr.pf_r_get_action_params([&](pf_action_t& a_tv) {
                const pf_action ref = pfr.pf_r_get_action_list();
                action &= ref.trigger_action(pkt, pkt_len, p_attr, a_tv);
            });
            if(pfr.pf_r_get_stop()) return true;
        }
    }

    return false;
}

bool pf_table::pf_t_out_pkt_hndlr(uint8_t *pkt, uint32_t pkt_len, pf_pkt_attr *p_attr) {

    EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","Out_Pkt handler - len %d, port %d",
                                pkt_len, p_attr->tx_port);

    std::lock_guard<std::mutex> pf_tlock {pf_mtx};

    for (auto pfr : pf_egress_table) {
        EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","Scanning rule id %lu", pfr.pf_r_get_id());
        bool match = true, action = true;

        //Consider using std::invoke
        pfr.pf_r_get_match_params([&](pf_match_t& m_tv) {
            const pf_match ref = pfr.pf_r_get_match_list();
            match &= ref.pf_m_inv_fptr(m_tv.m_type, pkt, pkt_len, p_attr, m_tv);
        });
        if(match) {
            pfr.pf_r_get_action_params([&](pf_action_t& a_tv) {
                const pf_action ref = pfr.pf_r_get_action_list();
                action &= ref.trigger_action(pkt, pkt_len, p_attr, a_tv);
            });
            if(pfr.pf_r_get_stop()) return true;
        }
    }

    return false;
}

/*
 * Sub Class Definitions
 */

void pf_match::pf_m_init_fptr() {
    for(auto ix = 0; ix < BASE_PACKET_PACKET_MATCH_TYPE_MAX; ++ix) {
        fptr[ix] = &pf_match::pf_m_pseudo_fn;
    }
    //@TODO - Current support for User trap Id, Dest Mac. Extend in future
    fptr[BASE_PACKET_PACKET_MATCH_TYPE_HOSTIF_USER_TRAP_ID] = &pf_match::pf_m_usr_trap_id;
    fptr[BASE_PACKET_PACKET_MATCH_TYPE_DST_MAC] = &pf_match::pf_m_dest_mac;
}

bool pf_match::pf_m_usr_trap_id(uint8_t *pkt, uint32_t len, pf_pkt_attr *p_attr,
                                pf_match_t& m_tv) const {
    EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","PKT TrapID: %lld, FILTER Type %u TRAPID: %llu",
		p_attr->trap_id, m_tv.m_type, m_tv.m_val.u64);

    if(m_tv.m_val.u64 == p_attr->trap_id) return true;
    return false;
}

bool pf_match::pf_m_dest_mac(uint8_t *pkt, uint32_t len, pf_pkt_attr *p_attr,
                                pf_match_t& m_tv) const {
    if(!memcmp(pkt, &m_tv.m_val.mac, HAL_MAC_ADDR_LEN)) return true;
    return false;
}

bool pf_match::pf_m_pseudo_fn(uint8_t *pkt, uint32_t len, pf_pkt_attr *p_attr,
                              pf_match_t& m_tv) const {
    EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","Executing pseudo-match for type %d val %lld",
                m_tv.m_type, m_tv.m_val.u64);
    return false;
}

bool pf_match::add_entry(BASE_PACKET_PACKET_MATCH_TYPE_t& t, pf_match_t& e) {
    if(t > BASE_PACKET_PACKET_MATCH_TYPE_MAX )
        throw std::invalid_argument("Invalid value for type " + t);
    else if(!pf_match_template::add_entry(t, e))
        throw std::runtime_error("Entry already exists");

    ++m_count;
    return true;
}

void pf_action::pf_a_init_fptr() {
    for(auto ix = 0; ix < BASE_PACKET_PACKET_ACTION_TYPE_MAX; ++ix) {
        fptr[ix] = &pf_action::pf_a_pseudo_fn;
    }

    fptr[BASE_PACKET_PACKET_ACTION_TYPE_REDIRECT_SOCK] = &pf_action::pf_a_redirect_sock;
    fptr[BASE_PACKET_PACKET_ACTION_TYPE_COPY_TO_SOCK] = &pf_action::pf_a_redirect_sock;
    fptr[BASE_PACKET_PACKET_ACTION_TYPE_REDIRECT_IF] = &pf_action::pf_a_redirect_if;
}

bool pf_action::add_entry(BASE_PACKET_PACKET_ACTION_TYPE_t& t, pf_action_t& e) {
    if(t > BASE_PACKET_PACKET_ACTION_TYPE_MAX )
        throw std::invalid_argument("Invalid value for type " + t);
    else if(!pf_action_template::add_entry(t, e))
        throw std::runtime_error("Entry already exists");

    ++m_count;
    return true;
}

bool pf_action::pf_a_redirect_sock(uint8_t *pkt, uint32_t pkt_len, pf_pkt_attr *p_attr,
                                   pf_action_t& a_tv) const {

    hal_ifindex_t rx_ifindex=0;

    if (!nas_int_port_ifindex (p_attr->npu_id, p_attr->rx_port, &rx_ifindex)) {
        EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","Matching index not found for %d:%d",
                    p_attr->npu_id, p_attr->rx_port);
    }

    dn_ipv4_addr_t ip;
    ip.s_addr = htonl(a_tv.m_val.sock_addr.address.inet4addr.sin_addr.s_addr);
    int port = htons(a_tv.m_val.sock_addr.address.inet4addr.sin_port);

    EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","Pkt len %d, rx_port %d, if_index %d, "
               "sockaddr 0x%x, udp-port %d", pkt_len, p_attr->rx_port, rx_ifindex, ip.s_addr, port);

    const uint32_t META_BUF_SIZE=1024;
    uint8_t meta_buf [META_BUF_SIZE];

    nas_pkt_meta_attr_it_t it;
    nas_pkt_meta_buf_init (meta_buf, sizeof(meta_buf), &it);
    nas_pkt_meta_add_u32 (&it, NAS_PKT_META_RX_PORT, rx_ifindex);
    nas_pkt_meta_add_u32 (&it, NAS_PKT_META_PKT_LEN, pkt_len);
    nas_pkt_meta_add_u64 (&it, NAS_PKT_META_TRAP_ID, p_attr->trap_id);

    size_t meta_len = sizeof(meta_buf) - it.len;
    struct iovec sock_data[] = {{(char*)meta_buf, meta_len}, {pkt, pkt_len} };

    std_socket_msg_t sock_msg = { &a_tv.m_val.sock_addr.address.inet4addr,
            sizeof (a_tv.m_val.sock_addr.address.inet4addr),
            sock_data, sizeof (sock_data)/sizeof (sock_data[0]), NULL, 0, 0};

    t_std_error rc;
    int n = std_socket_op (std_socket_transit_o_WRITE, pf_sock_fd, &sock_msg,
                           std_socket_transit_f_NONE, 0, &rc);
    if (n < 0) {
        EV_LOGGING (NAS_PKT_FILTER, ERR,"PKT-FIL","Failed to fwd to UDP socket %d - Error code (%d)\r\n",
                                   pf_sock_fd, STD_ERR_EXT_PRIV(rc));
        return false;
    }

    return true;
}

bool pf_action::pf_a_redirect_if(uint8_t *pkt, uint32_t pkt_len, pf_pkt_attr *p_attr,
                                   pf_action_t& a_tv) const {
    ndi_port_t ndi_port;

    EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","Pkt len %d, rx_port %d, if_index %d",
                                          pkt_len, p_attr->rx_port, a_tv.m_val.u32);

    if(nas_int_get_npu_port(a_tv.m_val.u32, &ndi_port) != STD_ERR_OK) {
        EV_LOGGING (NAS_PKT_FILTER, ERR,"PKT-FIL","Cannot find npu/port for ifindx %d", a_tv.m_val.u32);
        return false;
    }

    p_attr->npu_id = ndi_port.npu_id;
    p_attr->rx_port = ndi_port.npu_port;
    p_attr->tx_port = ndi_port.npu_port;

    return true;
}

bool pf_action::pf_a_pseudo_fn(uint8_t *pkt, uint32_t pkt_len, pf_pkt_attr *p_attr,
                               pf_action_t& a_tv) const {
    EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","Executing pseudo-action for type %d", a_tv.m_type);
    return false;
}

bool pf_rule::pf_r_add_match_param(pf_match_t& flt) {
    try {
        match_lst_.add_entry(flt.m_type, flt);
    } catch (std::exception& e) {
        EV_LOGGING (NAS_PKT_FILTER, ERR,"PKT-FIL","Adding match entry failed %s", e.what());
        return false;
    }
    return true;
}

bool pf_rule::pf_r_add_action_param(pf_action_t& flt) {
    try {
        action_lst_.add_entry(flt.m_type, flt);
    } catch (std::exception& e) {
        EV_LOGGING (NAS_PKT_FILTER, ERR,"PKT-FIL","Adding action entry failed %s", e.what());
        return false;
    }
    return true;
}

/*
 * External APIs
 */

void nas_pf_initialize ()
{
    EV_LOGGING (NAS_PKT_FILTER, DEBUG,"PKT-FIL","Initializing Packet Filtering!");
    try {
       pf_table_inst.reset(new (pf_table));
       t_std_error rc = std_socket_create (e_std_sock_INET4, e_std_sock_type_DGRAM,
                                        0, NULL, &pf_sock_fd);
       if (rc != STD_ERR_OK) {
           throw std::runtime_error("Socket create failed");
       }
    } catch (std::exception& e) {
        pf_table_inst.reset();
        EV_LOGGING (NAS_PKT_FILTER, ERR,"PKT-FIL","Initialization failed %s", e.what());
    }
}

bool nas_pf_ingr_enabled () {
    return (pf_table_inst && (pf_table_inst->pf_t_get_ingr_count() > 0));
}

bool nas_pf_egr_enabled () {
    return (pf_table_inst && (pf_table_inst->pf_t_get_egr_count() > 0));
}

bool nas_pf_in_pkt_hndlr(uint8_t *pkt, uint32_t pkt_len, ndi_packet_attr_t *p_attr) {
    return pf_table_inst->pf_t_in_pkt_hndlr(pkt, pkt_len, p_attr);
}

bool nas_pf_out_pkt_hndlr(uint8_t *pkt, uint32_t pkt_len, ndi_packet_attr_t *p_attr) {
    return pf_table_inst->pf_t_out_pkt_hndlr(pkt, pkt_len, p_attr);
}

/*
 * CPS OBJ Handlers - API Definitions
 */

static void nas_pf_match_list (const cps_api_object_t obj, const cps_api_object_it_t& it,
                               std::function<void (pf_match_t&)> fn)
{
    BASE_PACKET_PACKET_MATCH_TYPE_t  _type;
    uint64_t                         _value;
    cps_api_object_it_t              _list = it;
    t_std_error                      std_err = STD_ERR (INTERFACE, FAIL, 0);
    size_t                           attr_len;

    for (cps_api_object_it_inside (&_list); cps_api_object_it_valid (&_list);
         cps_api_object_it_next (&_list)) {

        cps_api_attr_id_t l_idx = cps_api_object_attr_id (_list.attr);

        cps_api_object_it_t _attr = _list;
        cps_api_object_it_inside (&_attr);
        if (!cps_api_object_it_valid (&_attr)) {
            throw nas::base_exception {std_err, __FUNCTION__, "Missing param for match in obj"};
        }

        auto attr_m_type = cps_api_object_it_find (&_attr, BASE_PACKET_RULE_MATCH_TYPE);
        if (attr_m_type == NULL) {
            throw nas::base_exception {std_err, __FUNCTION__, "Missing MATCH_TYPE attribute"};
        }

        _type = (BASE_PACKET_PACKET_MATCH_TYPE_t) cps_api_object_attr_data_u32 (attr_m_type);
        pf_match_t m_tv;
        m_tv.m_type = _type;
        cps_api_object_attr_t attr_match_val;

        /*
         * @TODO - to be extended to support rest of the match type
         */
        EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL", "list indx %lu type %d", l_idx, _type);

        switch (_type) {
        case BASE_PACKET_PACKET_MATCH_TYPE_DST_MAC:
            attr_match_val = cps_api_object_it_find (&_attr, BASE_PACKET_RULE_MATCH_MAC_ADDR);
            if (attr_match_val == NULL) {
                throw nas::base_exception {std_err, __FUNCTION__, "Missing MATCH_VALUE attribute"};
            }
            attr_len = cps_api_object_attr_len(attr_match_val);
            if (attr_len < sizeof(hal_mac_addr_t)) {
                throw nas::base_exception {std_err, __FUNCTION__, "Invalid attribute len"};
            }
            memcpy(m_tv.m_val.mac, cps_api_object_attr_data_bin(attr_match_val),
                   sizeof(hal_mac_addr_t));

            EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL", "Match_type %d", _type);
            break;

        case BASE_PACKET_PACKET_MATCH_TYPE_ETHER_TYPE:
            attr_match_val = cps_api_object_it_find (&_attr, BASE_PACKET_RULE_MATCH_DATA);
            if (attr_match_val == NULL) {
                throw nas::base_exception {std_err, __FUNCTION__, "Missing MATCH_VALUE attribute"};
            }
            _value = cps_api_object_attr_data_u32 (attr_match_val);
            m_tv.m_val.u64 = _value;
            EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL", "Match_type %d value %lu", _type, _value);
            break;

        case BASE_PACKET_PACKET_MATCH_TYPE_HOSTIF_TRAP_ID:
        case BASE_PACKET_PACKET_MATCH_TYPE_HOSTIF_USER_TRAP_ID:
            attr_match_val = cps_api_object_it_find (&_attr, BASE_PACKET_RULE_MATCH_DATA);
            if (attr_match_val == NULL) {
                throw nas::base_exception {std_err, __FUNCTION__, "Missing MATCH_VALUE attribute"};
            }
            _value = cps_api_object_attr_data_u64 (attr_match_val);
            m_tv.m_val.u64 = _value;
            EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL", "Match_type %d value %llu", _type, _value);
            break;

        default:
            throw nas::base_exception {std_err, __FUNCTION__, "Match attribute not supported"};
        }

        //Invoke the callback function
        fn(m_tv);
    }
}

static inline void nas_pf_util_fill_act_val (pf_action_t& at, dn_ipv4_addr_t& ip, int port)
{
    std_socket_address_t sock_dst;
    sock_dst.address.inet4addr.sin_family = AF_INET;
    sock_dst.address.inet4addr.sin_addr = ip;
    sock_dst.addr_type = e_std_socket_a_t_INET;
    sock_dst.type = e_std_sock_INET4;
    sock_dst.address.inet4addr.sin_port = htons(port);

    at.m_val.sock_addr = sock_dst;
}

static void nas_pf_action_list (const cps_api_object_t obj, const cps_api_object_it_t& it,
                                std::function<void (pf_action_t&)> fn)
{
    BASE_PACKET_PACKET_ACTION_TYPE_t _type;
    uint64_t                         _value = 0;
    cps_api_object_it_t              _list = it;
    t_std_error                      std_err = STD_ERR (INTERFACE, FAIL, 0);

    for (cps_api_object_it_inside (&_list); cps_api_object_it_valid (&_list);
         cps_api_object_it_next (&_list)) {

        cps_api_attr_id_t l_idx = cps_api_object_attr_id (_list.attr);

        cps_api_object_it_t _attr = _list;
        cps_api_object_it_inside (&_attr);
        if (!cps_api_object_it_valid (&_attr)) {
            throw nas::base_exception {std_err, __FUNCTION__, "Missing param for actions in obj"};
        }

        auto attr_m_type = cps_api_object_it_find (&_attr, BASE_PACKET_RULE_ACTION_TYPE);
        if (attr_m_type == NULL) {
            throw nas::base_exception {std_err, __FUNCTION__, "Missing ACTION_TYPE attribute"};
        }

        _type = (BASE_PACKET_PACKET_ACTION_TYPE_t) cps_api_object_attr_data_u32 (attr_m_type);
        dn_ipv4_addr_t ip;
        pf_action_t a_tv;
        a_tv.m_type = _type;
        cps_api_object_attr_t attr_val;
        cps_api_object_attr_t socket_attr;
        cps_api_object_it_t socket_iter;

        /*
         * Handle all the action type here
         */
        EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL", "list index %lu, type %d", l_idx, _type);

        switch (_type) {
        case BASE_PACKET_PACKET_ACTION_TYPE_REDIRECT_SOCK:
        case BASE_PACKET_PACKET_ACTION_TYPE_COPY_TO_SOCK:

            socket_attr = cps_api_object_it_find (&_attr, BASE_PACKET_RULE_ACTION_SOCKET_ADDRESS);

            if (socket_attr == NULL)
                throw nas::base_exception {std_err, __FUNCTION__, "Missing (socket) attribute"};

            cps_api_object_it_from_attr(socket_attr, &socket_iter);
            cps_api_object_it_inside (&socket_iter);
            attr_val = cps_api_object_it_find (&socket_iter, BASE_PACKET_RULE_ACTION_SOCKET_ADDRESS_IP);

            if (attr_val == NULL)
                throw nas::base_exception {std_err, __FUNCTION__, "Missing (IP) attribute"};

            if (cps_api_object_attr_len (attr_val) < sizeof (ip))
                throw nas::base_exception {std_err, __FUNCTION__, "Invalid IP address len"};

            ip = *(dn_ipv4_addr_t*)cps_api_object_attr_data_bin(attr_val);

            attr_val = cps_api_object_it_find (&socket_iter, BASE_PACKET_RULE_ACTION_SOCKET_ADDRESS_UDP_PORT);
            if (attr_val == NULL)
                throw nas::base_exception {std_err, __FUNCTION__, "Missing (Port) attribute"};

            _value = cps_api_object_attr_data_u16 (attr_val);

            nas_pf_util_fill_act_val(a_tv, ip, (int)_value);

            EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL", "Port %lu, ip 0x%x", _value, htonl(ip.s_addr));
            break;

        case BASE_PACKET_PACKET_ACTION_TYPE_REDIRECT_IF:
            attr_val = cps_api_object_it_find (&_attr, BASE_PACKET_RULE_ACTION_REDIRECT_IF);
            if (attr_val == NULL)
                throw nas::base_exception {std_err, __FUNCTION__, "Missing ACTION_VALUE(IF) attribute"};

            _value = cps_api_object_attr_data_u32 (attr_val);
            a_tv.m_val.u32 = _value;

            EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL", "Action IF - ifidx %lu", _value);
            break;

        default:
            throw nas::base_exception {std_err, __FUNCTION__, "Action attribute not supported"};
        }

        //Invoke the callback function
        fn(a_tv);
    }
}

static t_std_error nas_pf_cps_create(cps_api_object_t obj)
{
    cps_api_object_it_t  it;
    pf_direction dir = BASE_PACKET_PACKET_DIRECTION_TYPE_DIR_IN;
    pf_rule pfr;
    bool stop = false;
    bool invalid = false;
    uint32_t id = 0;

    for (cps_api_object_it_begin (obj, &it);
         cps_api_object_it_valid (&it); cps_api_object_it_next (&it)) {

        cps_api_attr_id_t attr_id = cps_api_object_attr_id (it.attr);

        switch (attr_id) {
            case BASE_PACKET_RULE_ID:
                id = cps_api_object_attr_data_u32(it.attr);
                EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL",
                        "Ignoring %d during create. New id will be generated", id);
                break;

            case BASE_PACKET_RULE_DIRECTION:
                dir = (pf_direction) cps_api_object_attr_data_u32(it.attr);
                EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL", "CPS Dir %d", dir);
                pfr.pf_r_set_dir(dir);
                break;

            case BASE_PACKET_RULE_MATCH:
                try {
                    nas_pf_match_list (obj, it, [&] (pf_match_t& ft) {
                        try {
                            pfr.pf_r_add_match_param(ft);
                        } catch (std::exception& e) {
                            invalid = true;
                        }
                    });
                } catch (nas::base_exception& b) {
                    EV_LOGGING (NAS_PKT_FILTER, ERR, "PKT-FIL","%s: %s",
                                b.err_fn.c_str(), b.err_msg.c_str());
                    invalid = true;
                }
                break;

            case BASE_PACKET_RULE_ACTION:
                try {
                    nas_pf_action_list (obj, it, [&] (pf_action_t& ft) {
                        try {
                            pfr.pf_r_add_action_param(ft);
                        } catch (std::exception& e) {
                            invalid = true;
                        }
                    });
                } catch (nas::base_exception& b) {
                    EV_LOGGING (NAS_PKT_FILTER, ERR, "PKT-FIL","%s: %s",
                                b.err_fn.c_str(), b.err_msg.c_str());
                    invalid = true;
                }
                break;

            case BASE_PACKET_RULE_STOP:
                stop = cps_api_object_attr_data_uint (it.attr);
                EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL", "CPS Stop %d", stop);
                pfr.pf_r_set_stop(stop);
                break;

            default:
                EV_LOGGING (NAS_PKT_FILTER, ERR, "PKT-FIL","Unknown attribute %lu", attr_id);
                break;
        }
    }

    if(!invalid) {
        nas_obj_id_t g_id = pf_table_inst->pf_t_add_rule(dir, pfr);
        cps_api_object_attr_delete (obj, BASE_PACKET_RULE_ID);
        cps_api_object_attr_add_u32 (obj, BASE_PACKET_RULE_ID, g_id);
        return STD_ERR_OK;
    }

    return (STD_ERR(INTERFACE, FAIL, 0));
}

static t_std_error nas_pf_cps_delete(cps_api_object_t obj)
{
    cps_api_object_attr_t id_attr = cps_api_object_attr_get(obj, BASE_PACKET_RULE_ID);
    if(id_attr == CPS_API_ATTR_NULL) {
        EV_LOGGING (NAS_PKT_FILTER, ERR, "PKT-FIL","Id missing");
        return (STD_ERR(INTERFACE, FAIL, 0));
    }

    auto id = cps_api_object_attr_data_u32(id_attr);
    if(!pf_table_inst->pf_t_del_rule(id)) {
        EV_LOGGING (NAS_PKT_FILTER, ERR, "PKT-FIL","Couldn't delete rule %d", id);
        return (STD_ERR(INTERFACE, FAIL, 0));
    }

    return STD_ERR_OK;
}

t_std_error nas_pf_cps_write(cps_api_object_t obj)
{
    t_std_error rc = STD_ERR_OK;
    cps_api_operation_types_t op = cps_api_object_type_operation (cps_api_object_key (obj));

    EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL", "CPS PF Set! - Operation %d", op);

    if(!pf_table_inst) {
        EV_LOGGING (NAS_PKT_FILTER, ERR, "PKT-FIL","Table instance not created");
        return (STD_ERR(INTERFACE, FAIL, 0));
    }

    switch (op) {
    case cps_api_oper_CREATE:
        rc = nas_pf_cps_create(obj);
        break;
    case cps_api_oper_SET:
        //@TODO - TBD
        EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL", "Not supported!");
        break;
    case cps_api_oper_DELETE:
        rc = nas_pf_cps_delete(obj);
        break;
    default:
        EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL", "Unknown operation!");
        return (STD_ERR(INTERFACE, FAIL, 0));
    }

    return rc;
}

static bool nas_pf_emb_match_params(cps_api_object_t obj, pf_match_t& m_tv, uint64_t l_idx) {

    cps_api_attr_id_t ids[3] = {BASE_PACKET_RULE_MATCH, l_idx, BASE_PACKET_RULE_MATCH_TYPE};
    const int ids_len = sizeof(ids)/sizeof(ids[0]);
    cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U32,&(m_tv.m_type),
                         sizeof(m_tv.m_type));

    switch (m_tv.m_type) {
    case BASE_PACKET_PACKET_MATCH_TYPE_ETHER_TYPE:
    case BASE_PACKET_PACKET_MATCH_TYPE_HOSTIF_TRAP_ID:
    case BASE_PACKET_PACKET_MATCH_TYPE_HOSTIF_USER_TRAP_ID:

        ids[2]=BASE_PACKET_RULE_MATCH_DATA;
        cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U64,&(m_tv.m_val.u64),
                             sizeof(m_tv.m_val.u64));
        break;

    default:
        EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL", "Type not handled for get");
        return false;
    }

    return true;
}

static bool nas_pf_emb_action_params(cps_api_object_t obj, pf_action_t& a_tv, uint64_t l_idx) {

    cps_api_attr_id_t ids[3] = {BASE_PACKET_RULE_ACTION, l_idx, BASE_PACKET_RULE_ACTION_TYPE};
    const int ids_len = sizeof(ids)/sizeof(ids[0]);
    cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U32,&(a_tv.m_type),
                         sizeof(a_tv.m_type));

    dn_ipv4_addr_t ip;
    uint16_t port;

    switch (a_tv.m_type) {
    case BASE_PACKET_PACKET_ACTION_TYPE_REDIRECT_SOCK:
    case BASE_PACKET_PACKET_ACTION_TYPE_COPY_TO_SOCK:

        ids[2]=BASE_PACKET_RULE_ACTION_SOCKET_ADDRESS_IP;
        ip = a_tv.m_val.sock_addr.address.inet4addr.sin_addr;
        port = ntohs(a_tv.m_val.sock_addr.address.inet4addr.sin_port);
        cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_BIN,&(ip),sizeof(ip));

        ids[2]=BASE_PACKET_RULE_ACTION_SOCKET_ADDRESS_UDP_PORT;
        cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U16,&(port),sizeof(port));
        break;

    case BASE_PACKET_PACKET_ACTION_TYPE_REDIRECT_IF:
        ids[2]=BASE_PACKET_RULE_ACTION_REDIRECT_IF;
        cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U32,&(a_tv.m_val.u32),
                             sizeof(a_tv.m_val.u32));
        break;

    default:
        EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL", "Type not handled for get");
        return false;
    }

    return true;
}

t_std_error nas_pf_cps_read(cps_api_object_t filt, cps_api_object_t obj)
{
    EV_LOGGING (NAS_PKT_FILTER, DEBUG, "PKT-FIL", "CPS PF Get!");

    if(!pf_table_inst) {
        EV_LOGGING (NAS_PKT_FILTER, ERR, "PKT-FIL","Table instance not created");
        return (STD_ERR(INTERFACE, FAIL, 0));
    }

    cps_api_object_attr_t attr = cps_api_object_attr_get(filt, BASE_PACKET_RULE_ID);
    if(attr == CPS_API_ATTR_NULL) {
        EV_LOGGING (NAS_PKT_FILTER, ERR, "PKT-FIL","Id missing.. Specify the id for get!");
        return (STD_ERR(INTERFACE, FAIL, 0));
    }

    auto id = cps_api_object_attr_data_u32(attr);

    const pf_rule* p_pfr;
    if((p_pfr = pf_table_inst->pf_t_get_rule(id)) == nullptr) {
        EV_LOGGING (NAS_PKT_FILTER, ERR, "PKT-FIL","Rule %d doesn't exist!", id);
        return (STD_ERR(INTERFACE, FAIL, 0));
    }

    if(!cps_api_object_attr_add_u32 (obj, BASE_PACKET_RULE_ID, id)) {
        return (STD_ERR(INTERFACE, FAIL, 0));
    }

    if(!cps_api_object_attr_add_u32 (obj, BASE_PACKET_RULE_STOP, p_pfr->pf_r_get_stop())) {
        return (STD_ERR(INTERFACE, FAIL, 0));
    }

    if(!cps_api_object_attr_add_u32 (obj, BASE_PACKET_RULE_DIRECTION, p_pfr->pf_r_get_dir())) {
        return (STD_ERR(INTERFACE, FAIL, 0));
    }

    auto idx = 0;
    p_pfr->pf_r_get_match_params([&obj, &idx](pf_match_t& m_tv) {
        nas_pf_emb_match_params(obj, m_tv, idx++);
    });

    idx = 0;
    p_pfr->pf_r_get_action_params([&obj, &idx](pf_action_t& a_tv) {
        nas_pf_emb_action_params(obj, a_tv, idx++);
    });

    return STD_ERR_OK;
}
