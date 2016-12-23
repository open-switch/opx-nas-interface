/*
 * interface_utils.h
 *
 *  Created on: Feb 1, 2016
 *      Author: cwichmann
 */

#ifndef INTERFACE_UTILS_H_
#define INTERFACE_UTILS_H_


void interface_util_get_filter_details(cps_api_object_t obj, const char *&name,int &ifix);

void * interface_util_get_attr(cps_api_object_t obj, cps_api_attr_id_t id);


t_std_error interface_load_cps_object(cps_api_attr_id_t id, cps_api_object_list_t &lst, int retry=0);
t_std_error interface_load_cps_object(cps_api_object_t obj, cps_api_object_list_t &lst, int retry=0);

bool interface_util_obj_get_uint32(cps_api_object_t obj,cps_api_attr_id_t id, uint32_t &val);

#endif /* INTERFACE_UTILS_H_ */
