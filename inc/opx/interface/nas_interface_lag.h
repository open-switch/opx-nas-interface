

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
 * filename: nas_interface_lag.h
 */

#ifndef _NAS_INTERFACE_LAG_H
#define _NAS_INTERFACE_LAG_H

#include "nas_interface.h"
#include "ds_common_types.h"

class NAS_LAG_INTERFACE : public NAS_INTERFACE {

    public:
        NAS_LAG_INTERFACE(std::string if_name,
                        hal_ifindex_t if_index,
                        nas_int_type_t if_type) : NAS_INTERFACE(if_name,
                                                                  if_index,
                                                                  if_type) {
                                                                  }

/* Attributes and APIs to be added in the next phase of development */
}; 

#endif /* _NAS_INTERFACE_LAG_H */
