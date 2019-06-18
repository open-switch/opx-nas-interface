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
 * filename: nas_int_lag.c
 */


#include "nas_ndi_lag.h"
#include "nas_int_lag.h"
#include "event_log.h"
#include "std_error_codes.h"
#include <inttypes.h>

t_std_error nas_lag_create(npu_id_t npu_id,ndi_obj_id_t *ndi_lag_id) {
    EV_LOGGING(INTERFACE, INFO, "NAS-LAG","Creating ndi_lag_id in npu %d",
               npu_id);
    return ndi_create_lag(npu_id, ndi_lag_id);
}

t_std_error nas_add_port_to_lag(npu_id_t npu_id, ndi_obj_id_t ndi_lag_id,
        ndi_port_t *p_ndi_port,ndi_obj_id_t *ndi_lag_member_id) {
    ndi_port_list_t ndi_port_list;

    ndi_port_list.port_count = 1;
    ndi_port_list.port_list =p_ndi_port;

    EV_LOGGING(INTERFACE, INFO, "NAS-LAG","Adding NAS port <%d %d>  %"PRIx64" ",
               p_ndi_port->npu_id, p_ndi_port->npu_port, ndi_lag_id);

    return ndi_add_ports_to_lag(npu_id, ndi_lag_id,  &ndi_port_list,ndi_lag_member_id);
}

t_std_error nas_del_port_from_lag(npu_id_t npu_id,ndi_obj_id_t ndi_lag_member_id) {

    EV_LOGGING(INTERFACE, INFO, "NAS-LAG", "Deleting NAS Lag member ID  %"PRIx64" ",
               ndi_lag_member_id);

    return ndi_del_ports_from_lag(npu_id,ndi_lag_member_id);
}

t_std_error nas_lag_delete(npu_id_t npu_id, ndi_obj_id_t ndi_lag_id) {

    EV_LOGGING(INTERFACE, INFO, "NAS-LAG", "Deleting ndi_lag_id  %"PRIx64" in npu %d",
               ndi_lag_id, npu_id);

    return ndi_delete_lag(npu_id, ndi_lag_id);
}

t_std_error nas_set_lag_member_attr(npu_id_t npu_id,ndi_obj_id_t ndi_lag_member_id,
        bool egress_disable) {

    EV_LOGGING(INTERFACE, INFO, "NAS-LAG",
               "Block/Unblock NAS port <%d >  %"PRIx64" egress_disable %d ",
               npu_id, ndi_lag_member_id, egress_disable);

    return ndi_set_lag_member_attr(npu_id,ndi_lag_member_id,egress_disable);
}

t_std_error nas_get_lag_member_attr(npu_id_t npu_id,ndi_obj_id_t ndi_lag_member_id,
        bool *egress_disable) {

    EV_LOGGING(INTERFACE, INFO, "NAS-LAG", "Block/Unblock NAS port <%d >  %"PRIx64,
               npu_id,ndi_lag_member_id);

    return ndi_get_lag_member_attr(npu_id,ndi_lag_member_id,egress_disable);
}
