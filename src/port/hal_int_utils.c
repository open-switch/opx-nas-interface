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
 * hal_int_utils.c
 */

#include "hal_if_mapping.h"
#include "event_log.h"
#include "nas_int_utils.h"
#include "std_utils.h"

#include <net/if.h>
#include <string.h>

int nas_int_name_to_if_index(hal_ifindex_t *if_index, const char *name) {

    interface_ctrl_t intf_ctrl;
    t_std_error rc = STD_ERR_OK;

    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    safestrncpy(intf_ctrl.if_name, name, strlen(name)+1);

    if((rc= dn_hal_get_interface_info(&intf_ctrl)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, CRIT, "NAS-INT",
                   "Interface %s returned error %d", intf_ctrl.if_name, rc);
        return STD_ERR(INTERFACE,FAIL, rc);
    }

    *if_index = intf_ctrl.if_index;
    return STD_ERR_OK;
}

t_std_error nas_int_get_npu_port(hal_ifindex_t port_index, ndi_port_t *ndi_port)
{
    interface_ctrl_t intf_ctrl;
    t_std_error rc = STD_ERR_OK;

    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
    intf_ctrl.if_index = port_index;

    if((rc= dn_hal_get_interface_info(&intf_ctrl)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, CRIT, "NAS-INT",
                   "Interface %d returned error %d", \
                    intf_ctrl.if_index, rc);

        return STD_ERR(INTERFACE,FAIL, rc);
    }

    ndi_port->npu_id = intf_ctrl.npu_id;
    ndi_port->npu_port = intf_ctrl.port_id;

    return STD_ERR_OK;
}
t_std_error nas_int_get_if_index_from_npu_port(hal_ifindex_t *port_index, ndi_port_t *ndi_port)
{
    interface_ctrl_t intf_ctrl;
    t_std_error rc = STD_ERR_OK;

    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_PORT;
    intf_ctrl.npu_id = ndi_port->npu_id;
    intf_ctrl.port_id = ndi_port->npu_port;

    if((rc= dn_hal_get_interface_info(&intf_ctrl)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, CRIT, "NAS-INT",
                   "npu:port = %d:%d returned error %d", \
                    ndi_port->npu_id, ndi_port->npu_port, rc);

        return STD_ERR(INTERFACE,FAIL, rc);
    }
    *port_index = intf_ctrl.if_index;

    return STD_ERR_OK;
}

t_std_error nas_int_get_if_index_to_name(hal_ifindex_t if_index, char * name, size_t len)
{
    interface_ctrl_t intf_ctrl;
    t_std_error rc = STD_ERR_OK;

    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
    intf_ctrl.if_index = if_index;

    if((rc= dn_hal_get_interface_info(&intf_ctrl)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, CRIT, "NAS-INT",
                   "Interface %d returned error %d", \
                    intf_ctrl.if_index, rc);

        return STD_ERR(INTERFACE,FAIL, rc);
    }
    safestrncpy(name, intf_ctrl.if_name, len);
    return STD_ERR_OK;
}


t_std_error nas_get_int_type(hal_ifindex_t index, nas_int_type_t *type)
{
    interface_ctrl_t intf_ctrl;
    t_std_error rc = STD_ERR_OK;

    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
    intf_ctrl.if_index = index;

    if((rc= dn_hal_get_interface_info(&intf_ctrl)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, CRIT, "NAS-INT",
                   "Interface %d returned error %d", \
                    intf_ctrl.if_index, rc);

        return STD_ERR(INTERFACE,FAIL, rc);
    }

    *type = intf_ctrl.int_type;
    return STD_ERR_OK;
}
