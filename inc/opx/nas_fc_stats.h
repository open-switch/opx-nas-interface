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
 * filename: nas_fc_stats.h
 */


#ifndef NAS_FC_STATS_H_
#define NAS_FC_STATS_H_

#include "std_error_codes.h"
#include "cps_api_operation.h"

#ifdef __cplusplus
extern "C" {
#endif

t_std_error nas_stats_fc_if_init(cps_api_operation_handle_t handle);
cps_api_return_code_t nas_if_fc_stats_clear (void * context,
                                            cps_api_transaction_params_t * param,
                                            size_t ix);

#ifdef __cplusplus
}
#endif

#endif /* NAS_FC_STATS_H_ */
