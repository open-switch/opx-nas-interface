/*
 * interface_object_cache.h
 *
 *  Created on: Jan 20, 2016
 *      Author: cwichmann
 */

#ifndef INTERFACE_OBJECT_CACHE_H_
#define INTERFACE_OBJECT_CACHE_H_

#include "std_error_codes.h"
#include "cps_api_object.h"

typedef enum {
    if_obj_cache_T_PHY=0,
    if_obj_cache_T_VLAN=1,
    if_obj_cache_T_LAG=2,
    if_obj_cache_T_OS=3,
    if_obj_cache_T_MAX=3,//maximum attribute ID
} if_obj_cache_types_t;

t_std_error if_obj_cache_get(if_obj_cache_types_t type,int ifindex, cps_api_object_t obj, bool merge);
t_std_error if_obj_cache_set(if_obj_cache_types_t type,int ifindex, cps_api_object_t obj);
t_std_error if_obj_cache_delete(if_obj_cache_types_t type,int ifindex, cps_api_object_t obj);

t_std_error if_obj_cache_walk(if_obj_cache_types_t type,int ifindex, cps_api_object_t obj);

t_std_error if_obj_cache_init();


#endif /* INTERFACE_OBJECT_CACHE_H_ */
