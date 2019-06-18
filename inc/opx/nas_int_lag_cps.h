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
 * nas_int_lag_cps.h
 *
 *  Created on: May 19, 2015
 */

#ifndef NAS_INT_LAG_CPS_H_
#define NAS_INT_LAG_CPS_H_

#include "std_error_codes.h"
#include "cps_api_operation.h"
#ifdef __cplusplus
extern "C" {
#endif


/**
 * Initialize the CPS interfaces for the Lag interface queries.
 * @param handle is the CPS handle that will be used to register the callback
 * @return STD_ERR_OK or an error if the initialization fails
 */
t_std_error nas_cps_lag_init(cps_api_operation_handle_t handle);


/**
 * Retrieve the resilient hash configuration (enabled, disabled) for LAG
 * @param void
 * @return boolean indication if LAG resilient has is enabled
 */
bool nas_lag_hash_value_get(void);


#ifdef __cplusplus
}
#endif

#endif /* NAS_INT_LAG_CPS_H_ */
