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
 * hal_int_utils.c
 */

#include "hal_if_mapping.h"
#include "event_log.h"
#include "nas_int_utils.h"
#include "std_utils.h"
#include "stdio.h"
#include "hal_shell.h"

#include <net/if.h>
#include <string.h>

bool nas_is_non_npu_phy_port(hal_ifindex_t if_index) {
     interface_ctrl_t _port;
    memset(&_port, 0, sizeof(_port));

    _port.if_index = if_index;
    _port.q_type = HAL_INTF_INFO_FROM_IF;

    if (dn_hal_get_interface_info(&_port) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR,"INTF-C","Failed to get if_info");
        return true;
    }
    if ((_port.int_type == nas_int_type_MGMT) ||
            ((_port.int_type == nas_int_type_PORT) &&
             (!_port.port_mapped))) {
        return true;
    }
    return false;
}
t_std_error nas_int_name_to_if_index(hal_ifindex_t *if_index, const char *name) {

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
        EV_LOGGING(INTERFACE, ERR, "NAS-INT",
                   "Interface %d returned error %d", \
                    intf_ctrl.if_index, rc);

        return STD_ERR(INTERFACE,FAIL, rc);
    }

    if (intf_ctrl.int_type != nas_int_type_PORT &&
        intf_ctrl.int_type != nas_int_type_CPU &&
        intf_ctrl.int_type != nas_int_type_FC) {
        EV_LOGGING(INTERFACE, ERR, "NAS-INT",
                   "Invalid interface type %d of ifindex %d",
                   intf_ctrl.int_type, intf_ctrl.if_index);
        return STD_ERR(INTERFACE, PARAM, 0);
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
        EV_LOGGING(INTERFACE, DEBUG, "NAS-INT",
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
        EV_LOGGING(INTERFACE, DEBUG, "NAS-INT",
                   "Interface %d returned error %d", \
                    intf_ctrl.if_index, rc);

        return STD_ERR(INTERFACE,FAIL, rc);
    }

    *type = intf_ctrl.int_type;
    return STD_ERR_OK;
}
t_std_error nas_get_int_name_type(const char *name, nas_int_type_t *type)
{
    interface_ctrl_t intf_ctrl;
    t_std_error rc = STD_ERR_OK;

    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    safestrncpy(intf_ctrl.if_name, name, strlen(name)+1);

    if((rc= dn_hal_get_interface_info(&intf_ctrl)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, DEBUG, "NAS-INT",
                   "Interface %d returned error %d", \
                    intf_ctrl.if_index, rc);

        return STD_ERR(INTERFACE,FAIL, rc);
    }

    *type = intf_ctrl.int_type;
    return STD_ERR_OK;
}

/*Initialize the debug command*/
void nas_shell_command_init(void) {
    hal_shell_cmd_add("nas-port-dump",nas_intf_to_npu_port_map_dump,
                                    "<port> example::nas-port-dump e101-005-0");
}

/*Dump the interface info*/
void nas_intf_to_npu_port_map_dump(std_parsed_string_t handle)
{
    if(std_parse_string_num_tokens(handle)==0) return;
    size_t ix = 0;
    const char *token = NULL;
    t_std_error rc = STD_ERR_OK;
    interface_ctrl_t intf_ctrl;

    token = std_parse_string_next(handle,&ix);
    if(NULL != token) {
        memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));
        intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF_NAME;
        safestrncpy(intf_ctrl.if_name, token, strlen(token)+1);

        if((rc= dn_hal_get_interface_info(&intf_ctrl)) != STD_ERR_OK) {
            printf("Interface %s returned error %d\r\n", token, rc);
        } else {
            printf("Interface if_index : %d\r\n",intf_ctrl.if_index);
            printf("Interface NPU_ID   : %d\r\n",intf_ctrl.npu_id);
            printf("Interface npu_port : 0x%x\r\n",intf_ctrl.port_id);
            printf("Interface tap_id   : %d\r\n",intf_ctrl.tap_id);
            printf("Interface lag_id   : 0x%lx\r\n",intf_ctrl.lag_id);
            nas_ndi_port_map_dump(intf_ctrl.npu_id,intf_ctrl.port_id);
        }
    } else {
        printf("Enter the interface name\r\n");
    }
}
