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

#include "std_socket_tools.h"
#include "std_file_utils.h"
#include "nas_packet_meta.h"
#include "ds_common_types.h"
#include <stdio.h>


int main ()
{
    int sock_fd = -1;
    t_std_error rc = std_socket_create (e_std_sock_INET4, e_std_sock_type_DGRAM,
                                        0, NULL, &sock_fd);
    if (rc != STD_ERR_OK) {
        printf ("Error creating socket");
    }

    printf ("Successfully created socket\r\n");
    uint8_t pkt [] = {0xbe, 0xef,0xf0, 0x0d};

    std_socket_address_t dest;

    static constexpr char SOCK_IP[] = "127.0.0.1";
    static constexpr int SOCK_PORT =   20055;
    std_sock_addr_from_ip_str (e_std_sock_INET4, SOCK_IP, SOCK_PORT, &dest);

    hal_ifindex_t rx_ifindex = 45;
    hal_ifindex_t tx_ifindex = 50;
    uint64_t sample_count = 100;

    static constexpr size_t BUF_SZ = 1024;
    uint8_t meta_buf [BUF_SZ];

    nas_pkt_meta_attr_it_t it;
    nas_pkt_meta_buf_init (meta_buf, sizeof(meta_buf), &it);
    nas_pkt_meta_add_u32 (&it, NAS_PKT_META_RX_PORT, rx_ifindex);
    nas_pkt_meta_add_u32 (&it, NAS_PKT_META_TX_PORT, tx_ifindex);
    nas_pkt_meta_add_u64 (&it, NAS_PKT_META_SAMPLE_COUNT,sample_count);
    nas_pkt_meta_add_u32 (&it, NAS_PKT_META_PKT_LEN, sizeof(pkt));
    size_t meta_len = sizeof(meta_buf) - it.len;
    struct iovec sock_data[] = { {(char*)meta_buf, meta_len},
                                {pkt, sizeof(pkt)} };

    std_socket_msg_t sock_msg = { &dest.address.inet4addr,
            sizeof (dest.address.inet4addr),
            sock_data, sizeof (sock_data)/sizeof (sock_data[0]),
            NULL, 0, 0};

    int n = std_socket_op (std_socket_transit_o_WRITE, sock_fd, &sock_msg,
            std_socket_transit_f_NONE, 0, &rc);
    if (n < 0) {
        printf ("[RX] SendMsg to UDP socket %d FAILED - Error %d \r\n",
                sock_fd, rc);
        return -1;
    }
    return 0;
}
