/***************************************************************************
* LEGALESE:   "Copyright (c) 2018, Dell Inc. All rights reserved."
*
* This source code is confidential, proprietary, and contains trade
* secrets that are the sole property of Dell Inc.
* Copy and/or distribution of this source code or disassembly or reverse
* engineering of the resultant object code are strictly forbidden without
* the written consent of Dell Inc.
*
**************************************************************************/

/**************************************************************************
* @file mgmt_intf.c
*
* @brief This file contains Management port object handling software.
**************************************************************************/


#include "std_utils.h"
#include "cps_api_key.h"
#include "cps_api_object_key.h"
#include "cps_api_object.h"
#include "cps_api_operation.h"
#include "cps_api_events.h"
#include "cps_api_interface_types.h"
#include "dell-base-if-mgmt.h"
#include "std_error_codes.h"
#include "event_log.h"
#include "interface/nas_interface_mgmt_cps.h"
#include "interface/nas_interface_cps.h"



#define MGMT_INTF_ERRNO_LOG(lvl)              EV_LOG_ERRNO(ev_log_t_MGMT_INTF, ev_log_s_ ## lvl, "mgmt-intf", errno)

/** log the errno to the error log */
#define MGMT_INTF_TRACE_LOG(lvl, format, ...) EV_LOG_TRACE(ev_log_t_MGMT_INTF, ev_log_s_ ## lvl, "mgmt-intf",       \
                                                "func: %s, line: %d " format, __func__, __LINE__, ##__VA_ARGS__)

/** log an error event for MGMTINTF*/
#define MGMT_INTF_ERR_LOG(lvl, format, ...)   EV_LOG_ERR(ev_log_t_MGMT_INTF, ev_log_s_ ## lvl, "mgmt-intf",         \
                                                "func: %s, line: %d " format, __func__, __LINE__, ##__VA_ARGS__)

/** log an info message for MGMTINTF*/
#define MGMT_INTF_INFO_LOG(lvl, format, ...)  EV_LOG_INFO(ev_log_t_MGMT_INTF, ev_log_s_ ## lvl, "mgmt-intf",        \
                                                format, ##__VA_ARGS__)

#define MGMT_INTF_ERRCODE(errtype, errcode)            STD_ERR_MK(e_std_err_MGMT_INTF, e_std_err_code_ ## errtype, errcode)

typedef char mgmt_intf_name[128];

extern cps_api_return_code_t if_mgmt_stats_get (void * context, cps_api_get_params_t * param,
        size_t ix);

/*
 * mgmt_intf_key_get function is to get the key attributes from management
 * object.
 */

void mgmt_intf_key_get (cps_api_object_t obj, cps_api_qualifier_t *qual,
                        cps_api_attr_id_t  sub_cat, bool *name_valid,
                        char *name, uint_t len)
{
    cps_api_key_t         *key = cps_api_object_key(obj);
    cps_api_object_attr_t a = NULL;


    *qual = cps_api_key_get_qual(key);

    if ((cps_api_key_get_len(key) <= CPS_OBJ_KEY_APP_INST_POS)
            || (name == NULL)) {
        /* No key attribute name */
        MGMT_INTF_INFO_LOG(WARNING, "Key not specified.");
        return;
    }

    *name_valid = false;


    if (sub_cat == BASE_IF_MGMT_IF_INTERFACES_INTERFACE) {

        a = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);

    } else if ((sub_cat == BASE_IF_MGMT_IF_INTERFACES_STATE_INTERFACE)
            || (sub_cat ==
                BASE_IF_MGMT_IF_INTERFACES_STATE_INTERFACE_STATISTICS)) {

        a = cps_api_get_key_data(obj, IF_INTERFACES_STATE_INTERFACE_NAME);
    }

    if (a != NULL) {

        safestrncpy(name, (const char *)cps_api_object_attr_data_bin(a), len);
        *name_valid = true;

    } else {
        MGMT_INTF_INFO_LOG(WARNING, "KEY (Interface name) not found \
                SubCat(%u).", sub_cat);
    }


    return;
}

//CPS set get methods for management interface object

/*
 * mgmt_intf_cps_get function is to handle CPS get requests.
 */

cps_api_return_code_t mgmt_intf_cps_get (cps_api_attr_id_t  sub_cat,
                                         cps_api_get_params_t *param,
                                         size_t key_ix)
{

    cps_api_qualifier_t     qualifier;
    bool                    name_valid;
    mgmt_intf_name          key_name;
    cps_api_object_t        req_obj;
    cps_api_return_code_t   ret = cps_api_ret_code_OK;

    req_obj = cps_api_object_list_get(param->filters, key_ix);

    mgmt_intf_key_get(req_obj, &qualifier, sub_cat, &name_valid,
                      key_name, sizeof(key_name));

    if (sub_cat == BASE_IF_MGMT_IF_INTERFACES_INTERFACE) {
        cps_api_key_from_attr_with_qual(cps_api_object_key(req_obj),
                DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, qualifier);
        ret = nas_interface_com_if_get_handler(NULL, param, key_ix);

    } else if (sub_cat == BASE_IF_MGMT_IF_INTERFACES_STATE_INTERFACE) {
        cps_api_key_from_attr_with_qual(cps_api_object_key(req_obj),
                DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_OBJ, qualifier);
        ret = nas_interface_com_if_state_get_handler(NULL, param, key_ix);
    } else if (sub_cat == BASE_IF_MGMT_IF_INTERFACES_STATE_INTERFACE_STATISTICS) {
        cps_api_key_from_attr_with_qual(cps_api_object_key(req_obj),
                DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_STATISTICS_OBJ, qualifier);
        ret = if_mgmt_stats_get(NULL, param, key_ix);
    }

    size_t mx = cps_api_object_list_size(param->list);
    size_t ix = 0;
    while (ix < mx) {
        cps_api_object_t object = cps_api_object_list_get(param->list,ix);
        cps_api_key_from_attr_with_qual(cps_api_object_key(object),
                sub_cat, qualifier);
        ix++;
    }

    return ret;
}

/*
 * mgmt_intf_cps_set function is to handle CPS set requests.
 */

cps_api_return_code_t mgmt_intf_cps_set (cps_api_attr_id_t  sub_cat,
                               cps_api_transaction_params_t * param, size_t ix)
{
    bool                    name_valid;
    cps_api_return_code_t   ret = cps_api_ret_code_OK;
    mgmt_intf_name          key_name;
    cps_api_qualifier_t     qualifier;
    cps_api_object_t        obj = cps_api_object_list_get(param->change_list,ix);


    mgmt_intf_key_get(obj, &qualifier, sub_cat, &name_valid,
            key_name, sizeof(key_name));

    if (sub_cat == BASE_IF_MGMT_IF_INTERFACES_INTERFACE) {
        cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, qualifier);
        ret = nas_interface_mgmt_if_set(NULL, param, ix);
    }

    size_t mx = cps_api_object_list_size(param->prev);
    size_t index = 0;
    while (index < mx) {
        cps_api_object_t object = cps_api_object_list_get(param->prev, index);
        cps_api_key_from_attr_with_qual(cps_api_object_key(object),
                sub_cat, qualifier);
        index++;
    }

    return ret;
}


static cps_api_event_service_handle_t handle = NULL;

/*
 * mgmt_intf_cps_ev_init function to register with CPS.
 */

static bool mgmt_intf_cps_ev_init(void)
{
    if (cps_api_event_service_init() != cps_api_ret_code_OK) {

        MGMT_INTF_ERR_LOG(CRITICAL, "cps_api_event_service_init: \
                failed to initialize.");
        return (false);
    }

    if (cps_api_event_client_connect(&handle) != cps_api_ret_code_OK) {
        MGMT_INTF_ERR_LOG(CRITICAL,
                "cps_api_event_client_connect() failed.");
        return (false);
    }
    return (true);
}

/*
 * mgmt_intf_cps_notify function is to publish an object.
 */

bool mgmt_intf_cps_notify(cps_api_object_t obj, cps_api_operation_types_t op)
{
    bool     ret = false;
    /* Keys for events always have OBSERVED qualifier */

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                BASE_IF_MGMT_IF_INTERFACES_STATE_INTERFACE, cps_api_qualifier_OBSERVED);
    cps_api_object_set_type_operation(cps_api_object_key(obj), op);
    MGMT_INTF_ERR_LOG(MAJOR, "mgmt_intf_cps_notify : op (%u)", op);
    if (handle != NULL) {
        if (cps_api_event_publish(handle, obj) == cps_api_ret_code_OK) {
    MGMT_INTF_ERR_LOG(MAJOR, "mgmt_intf_cps_notify publish success: op (%u)", op);

            ret = true;
        } else {
            MGMT_INTF_ERR_LOG(MAJOR, "cps_api_event_publish failed.");
            ret = false;
        }
    }

    cps_api_object_delete(obj);

    return (ret);
}

/*
 * mgmt_intf_config_read_function is to handle management interface
 * config object cps get.
 */

static cps_api_return_code_t mgmt_intf_config_read_function (void *context,
        cps_api_get_params_t *param,
        size_t key_ix)
{
    cps_api_object_t       obj = cps_api_object_list_get(param->filters,
                                                         key_ix);

    STD_ASSERT(obj != CPS_API_OBJECT_NULL);

    return mgmt_intf_cps_get(BASE_IF_MGMT_IF_INTERFACES_INTERFACE_OBJ,
                            param, key_ix);

}

/*
 * mgmt_intf_config_write_function is to handle management interface
 * config object cps set.
 */

static cps_api_return_code_t mgmt_intf_config_write_function (void *context,
        cps_api_transaction_params_t *param,
        size_t index_of_element_being_updated)
{
    cps_api_object_t          obj     = CPS_API_OBJECT_NULL;
    cps_api_operation_types_t op      = cps_api_oper_DELETE;
    cps_api_key_t             *the_key = NULL;

    obj = cps_api_object_list_get(param->change_list,
            index_of_element_being_updated);

    op = cps_api_object_type_operation(cps_api_object_key(obj));
    if (op != cps_api_oper_SET) {
        MGMT_INTF_INFO_LOG(WARNING, "Not supported operation");
        return cps_api_ret_code_ERR;
    }

    STD_ASSERT(obj != CPS_API_OBJECT_NULL);

    the_key  = cps_api_object_key(obj);

    if (cps_api_key_get_qual(the_key) != cps_api_qualifier_TARGET) {
        MGMT_INTF_INFO_LOG(WARNING, "Not supported qualifier \
                for set requests");
        return (cps_api_ret_code_ERR);
    }

    return mgmt_intf_cps_set(BASE_IF_MGMT_IF_INTERFACES_INTERFACE_OBJ,
                             param, index_of_element_being_updated);

}

/*
 * mgmt_intf_state_read_function is to handle management interface
 * state object cps get.
 */

static cps_api_return_code_t mgmt_intf_state_read_function (void *context,
        cps_api_get_params_t *param,
        size_t key_ix)
{
    cps_api_object_t       obj = cps_api_object_list_get(param->filters,
                                                         key_ix);

    STD_ASSERT(obj != CPS_API_OBJECT_NULL);

    return mgmt_intf_cps_get(BASE_IF_MGMT_IF_INTERFACES_STATE_INTERFACE_OBJ,
                            param, key_ix);
}

/*
 * mgmt_intf_stats_read_function is to handle management interface
 * statistics object cps get.
 */

static cps_api_return_code_t mgmt_intf_stats_read_function (void *context,
        cps_api_get_params_t *param,
        size_t key_ix)
{
    cps_api_object_t       obj = cps_api_object_list_get(param->filters,
                                                         key_ix);

    STD_ASSERT(obj != CPS_API_OBJECT_NULL);

    return mgmt_intf_cps_get(BASE_IF_MGMT_IF_INTERFACES_STATE_INTERFACE_STATISTICS_OBJ,
                            param, key_ix);
}

typedef cps_api_return_code_t (*read_function) (void * context,
        cps_api_get_params_t * param, size_t key_ix);

typedef cps_api_return_code_t (*write_function)(void * context,
        cps_api_transaction_params_t * param,
        size_t index_of_element_being_updated);

typedef cps_api_return_code_t (*rollback_function)(void * context,
        cps_api_transaction_params_t * param,
        size_t index_of_element_being_updated);

static t_std_error mgmt_intf_cps_api_register (
        cps_api_operation_handle_t reg_handle, cps_api_qualifier_t qualifier,
        cps_api_attr_id_t mgmt_obj_id, read_function read_callback,
        write_function write_callback, rollback_function rollback_callback)
{
    cps_api_registration_functions_t  api_reg;

    memset(&api_reg, 0, sizeof(api_reg));

    cps_api_key_from_attr_with_qual(&api_reg.key, mgmt_obj_id, qualifier);

    api_reg.handle              = reg_handle;
    api_reg._read_function      = read_callback;
    api_reg._write_function     = write_callback;
    api_reg._rollback_function  = rollback_callback;

    if (cps_api_register(&api_reg) != cps_api_ret_code_OK) {
        return MGMT_INTF_ERRCODE(FAIL, 0);
    }

    return STD_ERR_OK;
}


/*
 * mgmt_intf_cps_get_set_init is action handler registration function
 * for management interface handling objects.
 */

static t_std_error mgmt_intf_cps_get_set_init (void)
{
    cps_api_operation_handle_t       op_handle = NULL;

    if (cps_api_operation_subsystem_init(&op_handle, 1)
            != cps_api_ret_code_OK) {

        MGMT_INTF_ERR_LOG(MAJOR, "cps_api_operation_subsystem_init failed.");

        return MGMT_INTF_ERRCODE(FAIL, 0);
    }



    if (mgmt_intf_cps_api_register(op_handle, cps_api_qualifier_TARGET,
                BASE_IF_MGMT_IF_INTERFACES_INTERFACE_OBJ,
                mgmt_intf_config_read_function, mgmt_intf_config_write_function, NULL)
            != STD_ERR_OK) {
        MGMT_INTF_ERR_LOG(MAJOR, "CPS API registration failed for \
                BASE_IF_MGMT_IF_INTERFACES_INTERFACE_OBJ TARGET qualifier");
        return MGMT_INTF_ERRCODE(FAIL, 0);
    }


    if (mgmt_intf_cps_api_register(op_handle, cps_api_qualifier_OBSERVED,
                BASE_IF_MGMT_IF_INTERFACES_STATE_INTERFACE_OBJ,
                mgmt_intf_state_read_function, NULL, NULL)
            != STD_ERR_OK) {
        MGMT_INTF_ERR_LOG(MAJOR, "CPS API registration failed for \
                BASE_IF_MGMT_IF_INTERFACES_STATE_INTERFACE_OBJ OBSERVED qualifier");
        return MGMT_INTF_ERRCODE(FAIL, 0);
    }

    if (mgmt_intf_cps_api_register(op_handle, cps_api_qualifier_OBSERVED,
                BASE_IF_MGMT_IF_INTERFACES_STATE_INTERFACE_STATISTICS_OBJ,
                mgmt_intf_stats_read_function, NULL, NULL)
            != STD_ERR_OK) {
        MGMT_INTF_ERR_LOG(MAJOR, "CPS API registration failed for \
                BASE_IF_MGMT_IF_INTERFACES_STATE_INTERFACE_STATISTICS_OBJ \
                OBSERVED qualifier");
        return MGMT_INTF_ERRCODE(FAIL, 0);
    }


    return STD_ERR_OK;
}


/*
 * mgmt_intf_init is management interface handling initilization
 * function. Its responsible for initilizing the application.
 */

t_std_error mgmt_intf_init (void)
{
    t_std_error ret = STD_ERR_OK;


    if (mgmt_intf_cps_ev_init() == false)
        return MGMT_INTF_ERRCODE(FAIL, 0);

    if ((ret = mgmt_intf_cps_get_set_init()) != STD_ERR_OK)
        return ret;

    return ret;
}

