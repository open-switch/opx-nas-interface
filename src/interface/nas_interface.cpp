
/*
 * Copyright (c) 2018 Dell Inc.
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
 * filename: .cpp
 */

#include "interface/nas_interface.h"

cps_api_return_code_t NAS_INTERFACE::nas_interface_fill_com_info(cps_api_object_t obj)
{
    // TODO add common attributes like if index, mtu phy address
    return cps_api_ret_code_OK;
}


cps_api_return_code_t NAS_INTERFACE::nas_interface_fill_info(cps_api_object_t obj){
    /*
     *  Class structure needs to be updated. Need only one method to fill interface info
     *  need to instantiate the interface object for lag and physical port
     */

    return cps_api_ret_code_OK;
}
