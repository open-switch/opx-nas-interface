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
 * filename: nas_packet_meta.h
 */

/**
 * nas_packet_meta.h - Host Packet Meta-data attributes
 */

#ifndef _NAS_PACKET_META_H_
#define _NAS_PACKET_META_H_

#include "std_tlv.h"
#include "std_type_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Packet Meta-data attribute pointer type
 * The NAS Packet interface encodes meta-data in the form of list of TLV attributes.
 */
typedef void* nas_pkt_meta_attr_ptr_t;

/**
 * Packet Meta-data attribute types
 */
typedef enum {
    NAS_PKT_META_RX_PORT, /* value type - uint32_t */

    NAS_PKT_META_TX_PORT, /* value type - uint32_t */

    NAS_PKT_META_SAMPLE_COUNT, /* value type - uint64_t */

    NAS_PKT_META_PKT_LEN, /* value type - uint32_t */

    NAS_PKT_META_TRAP_ID, /* value type - uint64_t */
} nas_pkt_meta_attr_type_t;

/**
 * Packet meta-data attribute iterator.
 */
typedef struct _nas_pkt_meta_attr_it_s {
    nas_pkt_meta_attr_ptr_t attr;
    size_t   len;
    uint8_t* start;
} nas_pkt_meta_attr_it_t;

/**
 * Get an iterator to the first meta-data attribute in the packet buffer.
 * This can be passed to nas_pkt_meta_attr_it_next to walk through
 * the list of meta-data attributes.  For example...
 *
 * @verbatim
 *   nas_pkt_meta_attr_it_t it;
 *   for (nas_pkt_meta_attr_it_begin (pkt, &it);
 *        nas_pkt_meta_attr_it_valid(&it); nas_pkt_meta_attr_it_next (&it)) {
 *
 *       if (nas_pkt_meta_attr_type (it.attr) == DN_PKT_META_RX_PORT) {
 *           hal_ifindex_t ifidx = nas_pkt_meta_attr_data_uint (it->attr);
 *           break;
 *       }
 *   }
 * @endverbatim
 *
 * The iterator should always be checked for validity before being used.
 *
 * @param[in]  buf received buffer containing the meta-data and actual packet
 * @param[out] it  iterator to the first attribute in the meta-data header
 *                 or invalid iterator if there are no attributes
 * @return false if there are no attributes
 */
bool nas_pkt_meta_it_begin (uint8_t* buf, nas_pkt_meta_attr_it_t* it);

/**
 * Get the next attribute in the packet meta-data
 * @param[in/out] it  pass in iterator to the current attribute
 *                    fill iterator to the next attribute
 * @return false if there is no next
 */
static inline bool nas_pkt_meta_it_next (nas_pkt_meta_attr_it_t* it) {
    it->attr = std_tlv_next (it->attr, &it->len);
    return it->attr != NULL && it->len!=0;
}

/**
 * Check to see if the current iterator is valid
 * @param  it  the iterator that contains the attribute to check
 * @return true if the attribute contained by the current iterator is valid
 */
static inline bool nas_pkt_meta_it_valid (nas_pkt_meta_attr_it_t* it) {
    return std_tlv_valid(it->attr,it->len);
}

/**
 * Get offset to the start of actual packet data
 * @param   buf  received buffer containing the meta-data and actual packet
 * @return  pointer to the start of actual packet data
 */
uint8_t* nas_pkt_data_offset (uint8_t* buf);

/**
 * Extract packet meta-data attribute type for current attribute
 * @param  attr  pointer to the meta-data attribute
 * @return attribute's type
 */
static inline nas_pkt_meta_attr_type_t nas_pkt_meta_attr_type (nas_pkt_meta_attr_ptr_t attr) {
    return (nas_pkt_meta_attr_type_t)std_tlv_tag (attr);
}

/**
 * Extract length of the data contained within the current meta-data attribute
 * @param  attr  pointer to the meta-data attribute
 * @return length of attribute's data
 */
static inline size_t nas_pkt_meta_attr_len(nas_pkt_meta_attr_ptr_t attr) {
    return (size_t)std_tlv_len(attr);
}

/**
 * Extract data from current packet meta-data attribute as an integer.
 * @param  attr  pointer to the meta-data attribute
 * @return attribute's data
 */
uint_t nas_pkt_meta_attr_data_uint (nas_pkt_meta_attr_ptr_t attr);
uint64_t nas_pkt_meta_attr_data_uint64 (nas_pkt_meta_attr_ptr_t attr);

/**
 * Get pointer to data in current packet meta-data attribute.
 * @param  attr  pointer to the meta-data attribute
 * @return pointer to attribute's data
 */
static inline void* nas_pkt_meta_attr_data_bin (nas_pkt_meta_attr_ptr_t attr) {
    return std_tlv_data(attr);
}

/**
 * Initialize a buffer with the packet metadata header.
 * Used for generating the metadata header for a packet.
 *
 * @param[in]     buf  pointer to the buffer that needs to be intialized
 * @param[in] buf_len  total available bytes in the buffer
 * @param[out]    pos  buffer position where the first meta data attribute
 *                     can be added
 * @return     true if successful init
 */
bool nas_pkt_meta_buf_init (uint8_t* buf, size_t buf_len, nas_pkt_meta_attr_it_t* pos);

/**
 * Add attribute containing 4 byte value at the specified position.
 *
 * @param[in/out] pos  indicates buffer pos at which to add current attribute
 *                     this will be moved forward after adding attribute
 * @param[in]    type  attribute type
 * @param[in]     val  attribute data
 * @return     true if attribute successfully added
 */
bool nas_pkt_meta_add_u32 (nas_pkt_meta_attr_it_t *pos,
                           nas_pkt_meta_attr_type_t type, uint32_t val);

/**
 * Add attribute containing 8 byte value at the specified position.
 *
 * @param[in/out] pos  indicates buffer pos at which to add current attribute
 *                     this will be moved forward after adding attribute
 * @param[in]    type  attribute type
 * @param[in]     val  attribute data
 * @return     true if attribute successfully added
 */
bool nas_pkt_meta_add_u64 (nas_pkt_meta_attr_it_t *pos,
                           nas_pkt_meta_attr_type_t type, uint64_t val);

#ifdef __cplusplus
}
#endif

#endif /* _NAS_PACKET_META_H_ */
