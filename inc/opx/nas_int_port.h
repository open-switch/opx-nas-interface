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
 * nas_int_port.h
 *
 *  Created on: Jun 9, 2015
 */

#ifndef NAS_INTERFACE_INC_NAS_INT_PORT_H_
#define NAS_INTERFACE_INC_NAS_INT_PORT_H_

#include "nas_ndi_port.h"
#include "hal_if_mapping.h"

static inline IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t ndi_to_cps_oper_type(ndi_port_oper_status_t ndi) {
    //ignore the other test and fail cases since if reacted on - could effect hardware
    return ndi == ndi_port_OPER_DOWN ?
            IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_DOWN : IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_UP;
}

#ifdef __cplusplus
extern "C" {
#endif
t_std_error nas_int_port_create_mapped(npu_id_t npu, port_t port, const char *name, nas_int_type_t type);
t_std_error nas_int_port_create_unmapped(const char *name, nas_int_type_t type);
t_std_error nas_int_port_delete(const char *name);

void nas_int_port_link_change(npu_id_t npu, port_t port,
                            IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t state);

bool nas_int_port_id_used(npu_id_t npu, port_t port);

bool nas_int_port_name_used(const char *name);

t_std_error nas_int_port_init(void);

bool nas_int_port_ifindex (npu_id_t npu, port_t port, hal_ifindex_t *ifindex);

t_std_error nas_int_update_npu_port(const char *name, npu_id_t npu, port_t port,
                                    bool connect);

t_std_error update_if_tracker(const char *name, npu_id_t npu, port_t port, bool connect);

#ifdef __cplusplus
}
#endif
#endif /* NAS_INTERFACE_INC_NAS_INT_PORT_H_ */
