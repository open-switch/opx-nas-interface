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
 * @file interface_plugins.h
 */

#ifndef INTERFACE_PLUGINS_H_
#define INTERFACE_PLUGINS_H_

#include "cps_api_operation.h"

#include "std_rw_lock.h"
#include "std_mutex_lock.h"

#include "std_assert.h"


#include <unordered_map>
#include <vector>
#include <utility>

class InterfacePluginExtn;

class InterfacePluginSequencer {
public:
    enum orders_t : int { FIRST=0,HIGH=1,MED=2,LOW=3,ORDER_MAX=4 };
    enum operation_t: int { GET=0, SET=1, OPERATION_MAX=2 };

    struct sequencer_request_t {
        cps_api_attr_id_t _id;
        size_t _ix;
        cps_api_transaction_params_t *_tran;
        cps_api_get_params_t *_get;
    };

    InterfacePluginSequencer(const std::vector<InterfacePluginExtn*> &l);

    t_std_error init();
    t_std_error sequence(operation_t oper, sequencer_request_t &req);

    bool reg(operation_t when, orders_t where_to_insert, InterfacePluginExtn *what_to_insert);

protected:
    std_rw_lock_t _lock;
    const std::vector<InterfacePluginExtn*> &_regs;
    std::vector<InterfacePluginExtn*> _op_map[OPERATION_MAX][ORDER_MAX];
};


class InterfacePluginExtn {
protected:
    std::unordered_map<cps_api_attr_id_t,std::unordered_map<int,cps_api_object_t>> _obj_map;
    std_mutex_type_t _lock;
public:
    InterfacePluginExtn();
    virtual t_std_error init(InterfacePluginSequencer *seq);
    virtual cps_api_return_code_t handle_set(InterfacePluginSequencer::sequencer_request_t &req);
    virtual cps_api_return_code_t handle_get(InterfacePluginSequencer::sequencer_request_t &req);

    virtual ~InterfacePluginExtn();
};


#endif /* INTERFACE_PLUGINS_H_ */
