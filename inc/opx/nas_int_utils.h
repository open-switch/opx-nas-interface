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
 * nas_int_utils.h
 *
 */

#ifndef NAS_INT_UTILS_H_
#define NAS_INT_UTILS_H_

#include "nas_ndi_common.h"
#include "std_utils.h"

#ifdef __cplusplus
extern "C" {
#endif


int nas_int_name_to_if_index(hal_ifindex_t *if_index, const char *name);
t_std_error nas_int_get_npu_port(hal_ifindex_t port_index, ndi_port_t *ndi_port);

t_std_error nas_int_get_if_index_to_name(hal_ifindex_t if_index, char * name, size_t len);
t_std_error nas_get_int_type(hal_ifindex_t index, nas_int_type_t *type);
t_std_error nas_int_get_if_index_from_npu_port(hal_ifindex_t *port_index, ndi_port_t *ndi_port);
void nas_intf_to_npu_port_map_dump(std_parsed_string_t handle);
void nas_shell_command_init(void);

#ifdef __cplusplus
}
#endif

#endif /* NAS_INT_UTILS_H_ */
