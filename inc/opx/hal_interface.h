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
 * filename: hal_interface.h
 */

/*
 * hal_interface.h - general HAL interface code
 */

#ifndef HAL_INTERFACE_H_
#define HAL_INTERFACE_H_

#include "std_error_codes.h"

#ifdef __cplusplus
 extern "C" {
 #endif


/**
 * @brief Initialize the HAL interface management entity
 *
 * @param none
 */
t_std_error hal_interface_init(void);

#ifdef __cplusplus
 }
 #endif

/*!
 *  Init function for packet i/o thread for receiving and sending packets to the NPU
 *  \return  standard return code
 *
 */
t_std_error hal_packet_io_init(void);


#endif /* HAL_INTERFACE_H_ */
