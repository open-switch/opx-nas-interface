/*
 * Copyright (c) 2016 Dell Inc.
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

/**
 * @file interface_plugin_linux.h
 */

#ifndef INTERFACE_PLUGIN_LINUX_H_
#define INTERFACE_PLUGIN_LINUX_H_


#include "plugins/interface_plugins.h"

class LinuxInterfacePluginExtn : public InterfacePluginExtn {
public:
    virtual t_std_error init(InterfacePluginSequencer *seq);
    virtual cps_api_return_code_t handle_set(InterfacePluginSequencer::sequencer_request_t &req);
    virtual cps_api_return_code_t handle_get(InterfacePluginSequencer::sequencer_request_t &req);


    virtual ~LinuxInterfacePluginExtn();
};


#endif /* INTERFACE_PLUGIN_LINUX_H_ */
