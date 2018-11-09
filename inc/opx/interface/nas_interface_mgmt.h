/*
 * Copyright (c) 2018 Dell EMC.
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
 * nas_int_mgmt.h
 */

#ifndef _NAS_INTERFACE_MGMT_H_
#define _NAS_INTERFACE_MGMT_H_

#include "nas_interface.h"
#include "cps_api_events.h"
#include "cps_api_object_attr.h"

class NAS_MGMT_INTERFACE : public NAS_INTERFACE {
    public:
        NAS_MGMT_INTERFACE(std::string ifname, hal_ifindex_t ifdx)
                           : NAS_INTERFACE(ifname, ifdx, nas_int_type_MGMT)
        {
            set_speed(BASE_IF_SPEED_AUTO);
            set_duplex(BASE_CMN_DUPLEX_TYPE_AUTO);
        }
};


#endif //NAS_INTERFACE_MGMT_H_
