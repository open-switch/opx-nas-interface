/*
 * interface_plugin_front_panel_port.h
 *
 *  Created on: Feb 2, 2016
 *      Author: cwichmann
 */

#ifndef INTERFACE_PLUGIN_FRONT_PANEL_PORT_H_
#define INTERFACE_PLUGIN_FRONT_PANEL_PORT_H_

#include "plugins/interface_plugins.h"

class FrontPanelPortDetails : public InterfacePluginExtn {
public:
    virtual t_std_error init(InterfacePluginSequencer *seq);
    virtual cps_api_return_code_t handle_set(InterfacePluginSequencer::sequencer_request_t &req);
    virtual cps_api_return_code_t handle_get(InterfacePluginSequencer::sequencer_request_t &req);

    virtual ~FrontPanelPortDetails();
};



#endif /* INTERFACE_PLUGIN_FRONT_PANEL_PORT_H_ */
