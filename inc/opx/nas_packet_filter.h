/*
 * Copyright (c) 2019 Dell Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 * LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 * FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 * See the Apache Version 2.0 License for specific language governing
 * permissions and limitations under the License.
 */

/*
 * filename: nas_packet_filter.h
 *
 *  Created on: Nov 11, 2016
 */

#ifndef NAS_PACKET_FILTER_H_
#define NAS_PACKET_FILTER_H_

#include "dell-base-packet.h"
#include "ds_common_types.h"
#include "std_type_defs.h"
#include "std_error_codes.h"
#include "nas_ndi_common.h"
#include "std_socket_tools.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef union _pf_match_value_t{
    hal_mac_addr_t  mac;
    hal_ip_addr_t   ip;
    ndi_port_t      ndi_port;
    ndi_port_list_t ndi_portlist;
    uint64_t        u64;
    uint32_t        u32;
} pf_match_value_t;

typedef struct _pf_match_t {
    BASE_PACKET_PACKET_MATCH_TYPE_t m_type;
    pf_match_value_t m_val;
}pf_match_t;

typedef union _pf_act_value_t{
    hal_ip_addr_t        ip;
    ndi_port_t           ndi_port;
    uint64_t             u64;
    uint32_t             u32;
    std_socket_address_t sock_addr;
} pf_act_value_t;

typedef struct _pf_action_t {
    BASE_PACKET_PACKET_ACTION_TYPE_t m_type;
    pf_act_value_t m_val;
}pf_action_t;

/**
 * @brief : API to initialize control packet filtering in packet-io
 * @param : none
 * @return : none
 */
void nas_pf_initialize ();

/**
 * @brief : API to check if ingress filtering is enabled in the system
 * @param : none
 * @return true if enabled, false if disabled
 */
bool nas_pf_ingr_enabled ();

/**
 * @brief : API to check if egress filtering is enabled in the system
 * @param : none
 * @return true if enabled, false if disabled
 */
bool nas_pf_egr_enabled ();

/**
 * @brief : API to process in-bound packets via packet-filter engine
 * @param pkt : pointer to packet buffer
 * @param pkt_len : packet buffer length
 * @param p_attr : packet attribute pointer
 * @return true to stop further packet processing, false to continue
 */
bool nas_pf_in_pkt_hndlr(uint8_t *pkt, uint32_t pkt_len, ndi_packet_attr_t *p_attr);

/**
 * @brief : API to process out-bound packets via packet-filter engine
 * @param pkt : pointer to packet buffer
 * @param pkt_len : packet buffer length
 * @param p_attr : packet attribute pointer
 * @return true to stop further packet processing, false to continue
 */
bool nas_pf_out_pkt_hndlr(uint8_t *pkt, uint32_t pkt_len, ndi_packet_attr_t *p_attr);

/**
 * @brief : API to create/delete packet filter rules
 * @param obj : the result object
 * @return STD_ERR_OK if successful otherwise an error code
 */
t_std_error nas_pf_cps_write(cps_api_object_t obj);

/**
 * @brief : Query packet filters based on "id" specified in filt
 * @param filt : the filter object
 * @param obj : the result object
 * @return STD_ERR_OK if successful otherwise an error code
 */
t_std_error nas_pf_cps_read(cps_api_object_t filt, cps_api_object_t obj);

#ifdef __cplusplus
}
#endif

#endif /* NAS_PACKET_FILTER_H_ */
