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
#include <iostream>
#include <gtest/gtest.h>

#define SOCK_PORT 20055

TEST(meta_test, rx_pkt)
{
    std_socket_address_t bind;
    t_std_error rc = std_sock_addr_from_ip_str (e_std_sock_INET4, "127.0.0.1",
                                                SOCK_PORT, &bind);
    int fd;
    rc = std_socket_create (e_std_sock_INET4, e_std_sock_type_DGRAM, 0, &bind, &fd);

    ASSERT_TRUE(STD_ERR_OK == rc);

    constexpr size_t MAXLEN = 256;
    uint8_t buf[MAXLEN];
    struct iovec sock_data[] = {buf, MAXLEN};
    std_socket_msg_t msg = { NULL, 0, sock_data,
                             sizeof(sock_data)/sizeof(struct iovec), NULL, 0,
                             0};

    auto n = std_socket_op (std_socket_transit_o_READ, fd, &msg,
                            std_socket_transit_f_NONE, 0, &rc);

    ASSERT_TRUE(STD_ERR_OK == rc);
    if (n < 0) printf ("FAILED\r\n");
    else {
        std::cout << "Received bytes: " << n << std::endl;

        nas_pkt_meta_attr_it_t it;
        bool found=false;

        for (nas_pkt_meta_it_begin (buf, &it); nas_pkt_meta_it_valid (&it);
             nas_pkt_meta_it_next (&it)) {
            std_tlv_tag_t t = nas_pkt_meta_attr_type (it.attr);
            found=true;
            std::cout << "Found Packet Meta attr " << t << std::endl;

            if (t == NAS_PKT_META_RX_PORT) {
                hal_ifindex_t ifidx = nas_pkt_meta_attr_data_uint (it.attr);
                std::cout << "Received pkt on IfIndex " << ifidx << std::endl;
                EXPECT_TRUE (ifidx == 45);
            }

            if (t == NAS_PKT_META_PKT_LEN) {
                uint32_t len = nas_pkt_meta_attr_data_uint (it.attr);
                std::cout << "Received pkt Len " << len << std::endl;
                EXPECT_TRUE (len == 4);
            }

            if (t == NAS_PKT_META_TX_PORT) {
                hal_ifindex_t ifidx = nas_pkt_meta_attr_data_uint (it.attr);
                std::cout << "Egress Interface for sampled packet " << ifidx << std::endl;
                EXPECT_TRUE (ifidx == 50);
            }

            if (t == NAS_PKT_META_SAMPLE_COUNT) {
                uint64_t count = nas_pkt_meta_attr_data_uint (it.attr);
                std::cout << "Sample count " << count << std::endl;
                EXPECT_TRUE (count == 100);
            }
        }
        EXPECT_TRUE (found);
        // Offset to the actual packet data
        uint8_t* pkt = nas_pkt_data_offset (buf);
        uint32_t test = 0xbeeff00d;
        test = ntohl (test);
        uint32_t data = *((uint32_t*)pkt);

        EXPECT_TRUE (test == data);
    }

    rc = std_close (fd);
    ASSERT_TRUE(STD_ERR_OK == rc);
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

