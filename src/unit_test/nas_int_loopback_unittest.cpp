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


#include "cps_api_object.h"
#include "cps_api_operation.h"
#include "cps_api_object_category.h"
#include "cps_api_events.h"
#include "cps_class_map.h"
#include "cps_api_object_key.h"
#include "hal_if_mapping.h"
#include "dell-base-common.h"
#include "dell-base-if-phy.h"

#include <gtest/gtest.h>
#include <iostream>

using namespace std;
static void test_port_lpbk_mode_set(npu_id_t npu, npu_port_t port, BASE_CMN_LOOPBACK_TYPE_t lpbk_mode)
{

    cps_api_object_t obj = cps_api_object_create();

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    BASE_IF_PHY_PHYSICAL_OBJ,
                                    cps_api_qualifier_TARGET);

    cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_NPU_ID,npu);
    cps_api_object_attr_add_u32(obj,BASE_IF_PHY_PHYSICAL_PORT_ID,port);


    cps_api_object_attr_add_u32(obj, BASE_IF_PHY_PHYSICAL_LOOPBACK, (uint32_t)lpbk_mode);

    cps_api_transaction_params_t tr;
    ASSERT_TRUE(cps_api_transaction_init(&tr)==cps_api_ret_code_OK);
    cps_api_set(&tr,obj);
    ASSERT_TRUE(cps_api_commit(&tr)==cps_api_ret_code_OK);

    cps_api_transaction_close(&tr);
}
TEST(lpbk_test, set_loopback_mode)
{
    test_port_lpbk_mode_set(0,25,BASE_CMN_LOOPBACK_TYPE_PHY);
    test_port_lpbk_mode_set(0,29,BASE_CMN_LOOPBACK_TYPE_MAC);
}
TEST(lpbk_test, reset_loopback_mode)
{
    test_port_lpbk_mode_set(0,25,BASE_CMN_LOOPBACK_TYPE_NONE);
    test_port_lpbk_mode_set(0,29,BASE_CMN_LOOPBACK_TYPE_NONE);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
