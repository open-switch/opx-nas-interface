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
 * filename: nas_int_filter_class.h
 *
 *  Created on: Nov 15, 2016
 */

#ifndef NAS_INT_FILTER_CLASS_H_
#define NAS_INT_FILTER_CLASS_H_

#include "nas_packet_filter.h"
#include "nas_base_utils.h"
#include "nas_base_obj.h"

#include <iostream>
#include <unordered_map>
#include <functional>
#include <mutex>

//Key hash based on enum type
template<typename T>
class pf_key_hash {

public:
    std::size_t operator() (T const& src) const noexcept;
};

template<>
std::size_t pf_key_hash<BASE_PACKET_PACKET_MATCH_TYPE_t>::operator()
             (BASE_PACKET_PACKET_MATCH_TYPE_t const& idx) const  noexcept{
    return std::hash<int>() (idx);
}

template<>
std::size_t pf_key_hash<BASE_PACKET_PACKET_ACTION_TYPE_t>::operator()
             (BASE_PACKET_PACKET_ACTION_TYPE_t const& idx) const  noexcept{
    return std::hash<int>() (idx);
}

//Base object for managing the list of entries based on enum types
template<typename T, typename E>
class v_base_map {

public:
    v_base_map() { };

    v_base_map(const T& t, const E& e) {
        m_list.insert(std::make_pair(t, e));
    }

    v_base_map(const v_base_map<T, E>& rhs) = default;

    v_base_map<T,E>& operator=(const v_base_map<T,E>& rhs) = default;

    v_base_map(v_base_map<T, E>&& rhs) noexcept {
        m_list = std::move(rhs.m_list);
    }

    v_base_map<T,E>& operator=(v_base_map<T,E>&& rhs) noexcept {
        if(this == &rhs) return *this;

        m_list.clear();
        m_list = std::move(rhs.m_list);
        return *this;
    }

    virtual bool add_entry(T& t, E& e) {
        return m_list.insert(std::make_pair(t, e)).second;
    }

    virtual int del_entry(T& t) {
        return m_list.erase(t);
    }

    virtual void get_entry(T& t) const {
        m_list.at(t);
    }

    virtual void get_entries(std::function< void (E&)> fn) const {
        for (auto ix:m_list) {
            fn(ix.second);
        }
    }

    virtual ~v_base_map() {};

private:

    /*
     * Base map for [key] - [key, value] pair. Stored as unordered map for quick
     * insertion/deletion and traversal during match/action lookup
     */

    std::unordered_map<T, E,  pf_key_hash<T>> m_list;
};

using pf_match_template = v_base_map<BASE_PACKET_PACKET_MATCH_TYPE_t, pf_match_t>;
using pf_action_template = v_base_map<BASE_PACKET_PACKET_ACTION_TYPE_t, pf_action_t>;
using pf_pkt_attr = ndi_packet_attr_t;

//Packet_filter match object - May contain a list of match params
class pf_match final : public pf_match_template {

public:
    pf_match(): pf_match_template() {
        pf_m_init_fptr();
    };

    pf_match(const BASE_PACKET_PACKET_MATCH_TYPE_t& t, const pf_match_t& e):
        pf_match_template(t, e) {
        pf_m_init_fptr();
    };

    virtual bool add_entry(BASE_PACKET_PACKET_MATCH_TYPE_t& t, pf_match_t& e) override;

    virtual int del_entry(BASE_PACKET_PACKET_MATCH_TYPE_t& t) override {
        auto d_cnt = pf_match_template::del_entry(t);
        if(d_cnt) --m_count;
        return d_cnt;
    }

    virtual void get_entries(std::function< void (pf_match_t&)> fn) const override {
        pf_match_template::get_entries(fn);
    }

    //Initialize function pointers - Refer private functions.
    void pf_m_init_fptr();

    //Invoke functions based on match type
    inline bool pf_m_inv_fptr(BASE_PACKET_PACKET_MATCH_TYPE_t ix,
                       uint8_t *pkt, uint32_t len, pf_pkt_attr *p_attr, pf_match_t& m_attr) const {
        return (this->*fptr[ix])(pkt, len, p_attr, m_attr);
    }

private:
    //Count of matching entries
    int m_count=0;

    //Function pointer to each match-type
    bool (pf_match::*fptr[BASE_PACKET_PACKET_MATCH_TYPE_MAX+1])
                     (uint8_t *, uint32_t, pf_pkt_attr *, pf_match_t&) const;

    //Matching evaluation functions
    bool pf_m_usr_trap_id(uint8_t *, uint32_t, pf_pkt_attr *, pf_match_t&) const;
    bool pf_m_dest_mac(uint8_t *, uint32_t, pf_pkt_attr *, pf_match_t&) const;
    bool pf_m_pseudo_fn(uint8_t *, uint32_t, pf_pkt_attr *, pf_match_t&) const;
};

//Packet_filter action object - May contain a list of actions
class pf_action final : public pf_action_template {

public:
    pf_action(): pf_action_template() {
        pf_a_init_fptr();
    };

    pf_action(const BASE_PACKET_PACKET_ACTION_TYPE_t& t, const pf_action_t& e):
        pf_action_template(t, e) {
        pf_a_init_fptr();
    };

    virtual bool add_entry(BASE_PACKET_PACKET_ACTION_TYPE_t& t, pf_action_t& e) override;

    virtual int del_entry(BASE_PACKET_PACKET_ACTION_TYPE_t& t) override {
        auto d_cnt = pf_action_template::del_entry(t);
        if(d_cnt) --m_count;
        return d_cnt;
    }

    virtual void get_entries(std::function< void (pf_action_t&)> fn) const override {
        pf_action_template::get_entries(fn);
    }

    //Initialize function pointers - Refer private functions.
    void pf_a_init_fptr();

    //Trigger action for each one in action-list for the matching packet/attribute
    inline bool trigger_action(uint8_t *pkt, uint32_t len, pf_pkt_attr *p_attr,
                               pf_action_t& a_tv ) const {
        return pf_a_inv_fptr(a_tv.m_type, pkt, len, p_attr, a_tv);
    }

    //Invoke functions based on action types
    inline bool pf_a_inv_fptr(BASE_PACKET_PACKET_ACTION_TYPE_t ix,
                       uint8_t *pkt, uint32_t len, pf_pkt_attr *p_attr, pf_action_t& a_tv) const {
        return (this->*fptr[ix])(pkt, len, p_attr, a_tv);
    }

private:
    //Count of action entries
    int m_count=0;

    //Function pointer to each action
    bool (pf_action::*fptr[BASE_PACKET_PACKET_ACTION_TYPE_MAX+1])
                          (uint8_t *, uint32_t, pf_pkt_attr *, pf_action_t&) const;

    //Specific packet action APIs
    bool pf_a_redirect_sock(uint8_t *, uint32_t, pf_pkt_attr *, pf_action_t&) const;
    bool pf_a_redirect_if(uint8_t *, uint32_t, pf_pkt_attr *, pf_action_t&) const;
    bool pf_a_pseudo_fn(uint8_t *, uint32_t, pf_pkt_attr *, pf_action_t&) const;
};

using pf_direction = BASE_PACKET_PACKET_DIRECTION_TYPE_t;

//Packet_filter rule/entry
class pf_rule final {

public:
    pf_rule(int idx = 0):id_(idx) { };

    pf_rule(int idx, pf_match_t& pfm, pf_action_t& pfa):
        id_(idx), match_lst_(pfm.m_type, pfm), action_lst_(pfa.m_type, pfa) {
    }

    inline void pf_r_set_stop (bool st) { stop = st; }

    inline bool pf_r_get_stop () const { return stop; }

    inline void pf_r_set_dir (pf_direction dir) {
        direction = dir;
    }

    inline pf_direction pf_r_get_dir () const { return direction; }

    //Add single match to the list within the rule
    bool pf_r_add_match_param(pf_match_t& flt);

    void pf_r_get_match_params(std::function<void (pf_match_t&)> fn) const {
        match_lst_.get_entries(fn);
    }

    //Add single action to the list within the rule
    bool pf_r_add_action_param(pf_action_t& flt);

    void pf_r_get_action_params(std::function<void (pf_action_t&)> fn) const {
        action_lst_.get_entries(fn);
    }

    nas_obj_id_t pf_r_get_id() const { return id_; }

    //Unique id for the rule - Generated by the pf-table-id algorithm.
    inline void pf_r_set_id(nas_obj_id_t id) { id_ = id; }

    inline const pf_match& pf_r_get_match_list() { return match_lst_; }

    inline const pf_action& pf_r_get_action_list() { return action_lst_; }

private:
    //Unique id for this rule
    nas_obj_id_t id_;

    //Direction of Rule - Incoming or Outgoing packets
    pf_direction direction = BASE_PACKET_PACKET_DIRECTION_TYPE_DIR_IN;

    //List of match params
    pf_match match_lst_;

    //List of action params
    pf_action action_lst_;

    //Stop processing further rules if set to true
    bool stop = true;
};

//Packet Filter Main Table
class pf_table {

public:
    pf_table() { };

    //Id generator
    nas_obj_id_t pf_t_alloc_tid () {return pf_tid.alloc_id ();}
    void pf_t_reserve_tid (nas_obj_id_t tid) { pf_tid.reserve_id (tid); }
    void pf_t_release_tid (nas_obj_id_t tid) noexcept { pf_tid.release_id (tid); }

    //Add/Del rules
    nas_obj_id_t pf_t_create_rule(pf_direction, pf_match_t&, pf_action_t&, bool stop);
    nas_obj_id_t pf_t_add_rule(pf_direction dir, pf_rule& rule);
    bool pf_t_del_rule(nas_obj_id_t);
    const pf_rule* pf_t_get_rule(nas_obj_id_t) noexcept;

    /*
     * Non-zero return-value means control packet goes through the filter engine.
     * These two are used as the primary validation APIs for diverting packet flow
     */
    uint32_t pf_t_get_ingr_count() { return ingress_rules; }
    uint32_t pf_t_get_egr_count() { return egress_rules; }

    //Packet handler APIs
    bool pf_t_in_pkt_hndlr(uint8_t *, uint32_t, pf_pkt_attr*);
    bool pf_t_out_pkt_hndlr(uint8_t *, uint32_t, pf_pkt_attr*);

    friend bool pf_gen_erase_id(std::vector<pf_rule>&, nas_obj_id_t& );
    friend const pf_rule* pf_gen_get_id(const std::vector<pf_rule>&, nas_obj_id_t& );

private:
    const size_t pf_max_id = 256;

    //Table safe-access - Lock all operations
    std::mutex pf_mtx;

    //Rules count
    unsigned int ingress_rules = 0;
    unsigned int egress_rules = 0;

    //Id generator
    nas::id_generator_t pf_tid {pf_max_id};

    //Primary tables for ingress/egress rules
    std::vector<pf_rule> pf_ingress_table;
    std::vector<pf_rule> pf_egress_table;
};

#endif /* NAS_INT_FILTER_CLASS_H_ */
