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
 * filename: nas_packet_meta.c
 */

#include "nas_packet_meta.h"

static const uint32_t META_SIGNATURE = 0xdeadbeef;

/* Internal packet meta-data header */
typedef struct _nas_pkt_meta_hdr_int_s {
    uint32_t signature;
    uint32_t total_len; /* total length of meta-data header
                           and all meta attributes put together */
    uint8_t attrs[];
} nas_pkt_meta_hdr_int_t;

uint_t nas_pkt_meta_attr_data_uint (nas_pkt_meta_attr_ptr_t attr)
{
    size_t len = nas_pkt_meta_attr_len(attr);
    switch (len) {
        case sizeof(uint64_t): return std_tlv_data_u64(attr); // The truncated use of u64
        case sizeof(uint32_t): return std_tlv_data_u32 (attr);
        case sizeof(uint16_t): return std_tlv_data_u16 (attr);
        case sizeof(uint8_t): return *(uint8_t*)std_tlv_data (attr);
        default: return 0;
    }
}

uint64_t nas_pkt_meta_attr_data_uint64 (nas_pkt_meta_attr_ptr_t attr)
{
    return std_tlv_data_u64(attr);
}

bool nas_pkt_meta_it_begin (uint8_t* pkt, nas_pkt_meta_attr_it_t* it)
{
    nas_pkt_meta_hdr_int_t*  meta = (nas_pkt_meta_hdr_int_t *) pkt;
    if (meta->signature != META_SIGNATURE) {
        it->attr = NULL; it->len = 0;
        return false;
    }
    it->attr = meta->attrs;
    it->start = pkt;
    it->len = (le32toh (meta->total_len)) - sizeof (nas_pkt_meta_hdr_int_t);
    return true;
}

uint8_t* nas_pkt_data_offset (uint8_t* pkt)
{
    nas_pkt_meta_hdr_int_t*  meta = (nas_pkt_meta_hdr_int_t *) pkt;
    if (meta->signature != META_SIGNATURE) return pkt;
    else return (pkt + le32toh (meta->total_len));
}

bool nas_pkt_meta_buf_init (uint8_t* buf, size_t buf_len, nas_pkt_meta_attr_it_t* it)
{
    if (buf_len < sizeof (nas_pkt_meta_hdr_int_t)) {
        it->attr = NULL; it->len = 0;
        return false;
    }
    nas_pkt_meta_hdr_int_t* meta = (nas_pkt_meta_hdr_int_t*) buf;
    meta->total_len = sizeof(nas_pkt_meta_hdr_int_t);
    meta->signature = META_SIGNATURE;

    it->start = buf;
    it->len = buf_len - sizeof(nas_pkt_meta_hdr_int_t);
    it->attr = meta->attrs;
    return true;
}

bool nas_pkt_meta_add_u32 (nas_pkt_meta_attr_it_t *it, nas_pkt_meta_attr_type_t type, uint32_t val)
{
    size_t prev_len = it->len;
    it->attr = std_tlv_add_u32(it->attr, &it->len, type, val);
    if (it->attr != NULL) {
        nas_pkt_meta_hdr_int_t* meta = (nas_pkt_meta_hdr_int_t*) it->start;
        meta->total_len += (prev_len - it->len);
        return true;
    }
    return false;
}

bool nas_pkt_meta_add_u64 (nas_pkt_meta_attr_it_t *it, nas_pkt_meta_attr_type_t type, uint64_t val)
{
    size_t prev_len = it->len;
    it->attr = std_tlv_add_u64(it->attr, &it->len, type, val);
    if (it->attr != NULL) {
        nas_pkt_meta_hdr_int_t* meta = (nas_pkt_meta_hdr_int_t*) it->start;
        meta->total_len += (prev_len - it->len);
        return true;
    }
    return false;
}
