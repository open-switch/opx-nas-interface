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
 * nas_int_breakout.cpp
 *
 *  Created on: Jul 30, 2015
 */

#include "dell-base-if-phy.h"
#include "dell-base-if.h"

#include "nas_ndi_port.h"
#include "cps_class_map.h"
#include "cps_api_operation.h"
#include "cps_api_object_key.h"

#include "event_log.h"

#include <vector>

static cps_api_return_code_t if_breakout(void * context, cps_api_transaction_params_t * param,size_t ix) {
    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    if (obj==NULL) return cps_api_ret_code_ERR;

    cps_api_object_t prev = cps_api_object_list_create_obj_and_append(param->prev);
    if (prev==nullptr) return cps_api_ret_code_ERR;

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if (op!=cps_api_oper_ACTION) {
         EV_LOGGING(INTERFACE,ERR,"NAS-PHY-BREAKOUT-MODE","Invalid operation");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_t _npu = cps_api_object_attr_get(obj,BASE_IF_PHY_SET_BREAKOUT_MODE_INPUT_NPU_ID);
    cps_api_object_attr_t _port = cps_api_object_attr_get(obj,BASE_IF_PHY_SET_BREAKOUT_MODE_INPUT_PORT_ID);
    cps_api_object_attr_t _mode = cps_api_object_attr_get(obj,BASE_IF_PHY_SET_BREAKOUT_MODE_INPUT_BREAKOUT_MODE);
    cps_api_object_attr_t _ports = cps_api_object_attr_get(obj,BASE_IF_PHY_SET_BREAKOUT_MODE_INPUT_EFFECTED_PORT);

    if (_npu==nullptr || _port==nullptr || _mode==nullptr || _ports==nullptr) {
        EV_LOGGING(INTERFACE,ERR,"NAS-PHY-BREAKOUT-MODE","Missing required parameters..");
        return cps_api_ret_code_ERR;
    }
    cps_api_object_it_t it;
    cps_api_object_it_begin(obj,&it);

    std::vector<port_t> ports;
    while (cps_api_object_it_valid(&it)) {
        if (cps_api_object_attr_id(it.attr)==BASE_IF_PHY_SET_BREAKOUT_MODE_INPUT_EFFECTED_PORT) {
            ports.push_back(cps_api_object_attr_data_u32(it.attr));
        }
        cps_api_object_it_next(&it);
    }

    if (ndi_port_breakout_mode_set(cps_api_object_attr_data_u32(_npu),
            cps_api_object_attr_data_u32(_port),
            (BASE_IF_PHY_BREAKOUT_MODE_t)cps_api_object_attr_data_u32(_mode), &ports[0],ports.size())!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-PHY-BREAKOUT-MODE","Failed to set hwmode for %d:%d",cps_api_object_attr_data_u32(_npu),
                cps_api_object_attr_data_u32(_port));
        return cps_api_ret_code_ERR;
    }
    return cps_api_ret_code_OK;
}

extern "C" t_std_error nas_int_breakout_init(cps_api_operation_handle_t handle)  {
    char buff[CPS_API_KEY_STR_MAX];
    cps_api_registration_functions_t f;
    memset(&f,0,sizeof(f));
    if (!cps_api_key_from_attr_with_qual(&f.key,BASE_IF_PHY_SET_BREAKOUT_MODE_OBJ,cps_api_qualifier_TARGET)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Could not translate %d to key %s",
            (int)(DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ),cps_api_key_print(&f.key,buff,sizeof(buff)-1));
        return STD_ERR(INTERFACE,FAIL,0);
    }

    EV_LOGGING(INTERFACE,INFO,"NAS-IF-REG","Registering for %s",cps_api_key_print(&f.key,buff,sizeof(buff)-1));

    f.handle = handle;
    f._write_function = if_breakout;

    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;
}
