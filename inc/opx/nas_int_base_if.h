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
 * filename: nas_int_base_if.h
 *
 *  Created on: May 15, 2017
 */

#ifndef NAS_INT_BASE_IF_H_
#define NAS_INT_BASE_IF_H_

#include "nas_int_com_utils.h"
#include "std_rw_lock.h"
#include "dell-base-interface-common.h"

#include <stdbool.h>
#include <unordered_map>
#include <memory>
#include <list>
#include <utility>

using master_fn_type = std::function< void (if_master_info_t)>;

class nas_intf_obj {

    hal_ifindex_t  m_if_idx;
    std::string    m_if_name;
    int tagged_cnt = 0;
    int untagged_cnt = 0;
    std::list<if_master_info_t> m_list;

public:
    nas_intf_obj(hal_ifindex_t if_idx, const char* if_name):
        m_if_idx(if_idx), m_if_name(if_name) { };

    bool nas_intf_obj_master_add(if_master_info_t m_info);

    bool nas_intf_obj_master_delete(if_master_info_t m_info);

    void nas_intf_obj_for_each_master(master_fn_type fn) {
        for (auto ix: m_list) fn(ix);
    }

    //Return a copy of master_list
    std::list<if_master_info_t> nas_intf_obj_master_list(void) const {
        return m_list;
    }
    // Return tagged / untagged count
    std::pair<int, int> nas_intf_obj_untag_tag_cnt(void);

    bool nas_intf_obj_is_mlist_empty(void) const {
        return m_list.empty();
    }
};

using _if_unique_obj = std::unique_ptr<nas_intf_obj>;
using _if_iter_type = std::unordered_map<hal_ifindex_t, _if_unique_obj>::iterator;

class nas_intf_container {

    //Lock for read-write access
    std_rw_lock_t rw_lock;

    std::unordered_map<hal_ifindex_t, _if_unique_obj> if_objects;

public:

    nas_intf_container() {
        std_rw_lock_create_default(&rw_lock);
    }

    auto nas_intf_add_object(hal_ifindex_t ifx) -> _if_iter_type;
    void nas_intf_del_object(hal_ifindex_t ifx);

    bool nas_intf_add_master(hal_ifindex_t ifx, if_master_info_t m_info);
    bool nas_intf_add_master(hal_ifindex_t ifx, if_master_info_t m_info, BASE_IF_MODE_t *new_mode, bool *mode_change);
    bool nas_intf_del_master(hal_ifindex_t ifx, if_master_info_t m_info);
    bool nas_intf_del_master(hal_ifindex_t ifx, if_master_info_t m_info, BASE_IF_MODE_t *new_mode, bool *mode_change);
    bool nas_intf_update_master(hal_ifindex_t ifx, if_master_info_t m_info, bool add, BASE_IF_MODE_t *new_mode,
                                                                            bool *mode_change);
    void nas_intf_master_callbk(hal_ifindex_t ifx, master_fn_type fn);
    std::list<if_master_info_t> nas_intf_get_master_list(hal_ifindex_t ifx);
    std::pair<int,int> nas_intf_get_untag_tag_cnt(hal_ifindex_t ifx);
    BASE_IF_MODE_t nas_intf_get_mode(hal_ifindex_t ifx);

    //Debug routine
    void nas_intf_dump_container(hal_ifindex_t ifx=0) noexcept;
};

#endif /* NAS_INT_BASE_IF_H_ */
