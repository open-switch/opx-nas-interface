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
 * filename: hal_interface_defaults.h
 */

/*
 * hal_interface_defaults.h
 * Internal header file
 */

#ifndef HAL_INTERFACE_DEFAULTS_H_
#define HAL_INTERFACE_DEFAULTS_H_


#include "cps_api_interface_types.h"
#include "std_error_codes.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The default settings for an interface
 */
#define DEFAULT_INTF_ADMINSTATE DB_ADMIN_STATE_DN
#define DEFAULT_INTF_MTU 1500
#define DEFAULT_VRF (0)

typedef enum nas_port_mode_t {
    NAS_PORT_UNTAGGED = 1,
    NAS_PORT_TAGGED,
    NAS_PORT_HYBRID,
    NAS_PORT_NONE
} nas_port_mode_t;

#ifdef __cplusplus
}
#endif

#endif /* HAL_INTERFACE_DEFAULTS_H_ */
