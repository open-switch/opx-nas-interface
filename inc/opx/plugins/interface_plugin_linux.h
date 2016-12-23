/*
 * interface_plugin_linux.h
 *
 *  Created on: Feb 2, 2016
 *      Author: cwichmann
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
