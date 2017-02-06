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

/**
 * @file interface_utils.h
 */

#ifndef INTERFACE_UTILS_H_
#define INTERFACE_UTILS_H_


void interface_util_get_filter_details(cps_api_object_t obj, const char *&name,int &ifix);

void * interface_util_get_attr(cps_api_object_t obj, cps_api_attr_id_t id);


t_std_error interface_load_cps_object(cps_api_attr_id_t id, cps_api_object_list_t &lst, int retry=0);
t_std_error interface_load_cps_object(cps_api_object_t obj, cps_api_object_list_t &lst, int retry=0);

bool interface_util_obj_get_uint32(cps_api_object_t obj,cps_api_attr_id_t id, uint32_t &val);

#endif /* INTERFACE_UTILS_H_ */
