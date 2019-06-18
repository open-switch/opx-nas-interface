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
 * filename: nas_int_list.c
 */

#include "nas_int_list.h"
#include "event_log.h"

nas_list_node_t *nas_get_first_link_node(std_dll_head *p_link_node_list)
{
    return (nas_list_node_t *)std_dll_getfirst(p_link_node_list);
}

nas_list_node_t *nas_get_next_link_node(std_dll_head *p_link_node_list, nas_list_node_t *p_link_node)
{
    return (nas_list_node_t *)std_dll_getnext(p_link_node_list, (std_dll *)p_link_node);
}

nas_list_node_t *nas_get_link_node(std_dll_head *p_link_node_list, hal_ifindex_t index)
{
    nas_list_node_t *p_link_node = (nas_list_node_t *)std_dll_getfirst(p_link_node_list);

    while(p_link_node != NULL) {
        if(p_link_node->ifindex == index)
        {
            return p_link_node;
        }

        p_link_node = (nas_list_node_t *)std_dll_getnext(p_link_node_list, (std_dll *)p_link_node);
    }

    return p_link_node;
}

void nas_insert_link_node(std_dll_head *p_link_node_list, nas_list_node_t *p_link_node)
{
    std_dll_insert(p_link_node_list, (std_dll *)p_link_node);
}

void nas_delete_link_node(std_dll_head *p_link_node_list, nas_list_node_t *p_link_node)
{
    std_dll_remove(p_link_node_list, (std_dll *)p_link_node);
    free(p_link_node);
}

void nas_delete_port_list(nas_list_t *p_link_node_list)
{
    nas_list_node_t *p_link_node = NULL, *temp_node = NULL;

    p_link_node = nas_get_first_link_node(&p_link_node_list->port_list);
    if(p_link_node != NULL) {
        EV_LOGGING(INTERFACE,DEBUG, "NAS-Link",
                    "Found  intf %d for deletion", p_link_node->ifindex);

        while(p_link_node != NULL) {
            temp_node = nas_get_next_link_node(&p_link_node_list->port_list, p_link_node);
            //delete the previous vlan node
            nas_delete_link_node(&p_link_node_list->port_list, p_link_node);
            p_link_node = temp_node;
            //decrement the count of ports
            p_link_node_list->port_count--;
        }
    }
}
