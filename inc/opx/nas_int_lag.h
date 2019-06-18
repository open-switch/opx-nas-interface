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
* filename: nas_int_lag.h
*/


#ifndef NAS_INT_LAG_H_
#define NAS_INT_LAG_H_

#include "ds_common_types.h"
#include "nas_ndi_common.h"
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INVALID_LAG_ID UINT_MAX

/**
 * @brief Handle the lag interface create request
 *
 * @param npu_id-  NPU Id on the system.
 *
 * @param ndi_lag_id - NAS/Application Lag ID
 *
 * @return STD_ERR_OK or STD_ERR
 */

t_std_error nas_lag_create(npu_id_t npu_id, ndi_obj_id_t *ndi_lag_id);

/**
 * @brief Handle the add ports to LAG request
 *
 * @param npu_id-  NPU Id on the system.
 *
 * @param ndi_lag_id - NAS/Application Lag ID
 *
 * @param p_ndi_port :- NDI port.
 *
 * @param ndi_lag_member_id :- NAS Lag member ID
 *
 * @return STD_ERR_OK or STD_ERR
 */
t_std_error nas_add_port_to_lag(npu_id_t npu_id, ndi_obj_id_t ndi_lag_id,
        ndi_port_t *p_ndi_port,ndi_obj_id_t *ndi_lag_member_id);

/**
 * @brief Handle the delete ports from LAG request
 *
 * @param npu_id-  NPU Id on the system.
 *
 * @param ndi_lag_member_id - NAS/Application Lag member ID
 *
 * @return STD_ERR_OK or STD_ERR
 */
t_std_error nas_del_port_from_lag(npu_id_t npu_id, ndi_obj_id_t ndi_lag_member_id);

/**
 * @brief Handle the lag interface deletion request
 *
 * @param npu_id-  NPU Id on the system.
 *
 * @param ndi_lag_id - NAS/Application Lag ID
 *
 * @return STD_ERR_OK or STD_ERR
 */

t_std_error nas_lag_delete(npu_id_t npu_id, ndi_obj_id_t ndi_lag_id);

/**
 * @brief Block a port on LAG in NPU
 *
 * @param npu_id-  NPU Id on the system.
 *
 * @param ndi_lag_member_id - NAS/Application Lag member ID
 *
 * @param egress_disable:- disable or enable traffic
 *
 * @return STD_ERR_OK or STD_ERR
 */
t_std_error nas_set_lag_member_attr(npu_id_t npu_id,ndi_obj_id_t ndi_lag_member_id,
                bool egress_disable);

/**
 * @brief Get port port block mode of LAG in NPU
 *
 * @param npu_id-  NPU Id on the system.
 *
 * @param ndi_lag_member_id - NAS/Application Lag member ID
 *
 * @param egress_disable:- pointer to store disable or enable traffic mode
 *
 * @return STD_ERR_OK or STD_ERR
 */
t_std_error nas_get_lag_member_attr(npu_id_t npu_id,ndi_obj_id_t ndi_lag_member_id,
                bool *egress_disable);
#ifdef __cplusplus
}
#endif

#endif /* NAS_INT_LAG_H_ */
