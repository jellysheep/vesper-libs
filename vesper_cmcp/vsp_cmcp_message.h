/**
 * \file
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#if !defined VSP_CMCP_MESSAGE_H_INCLUDED
#define VSP_CMCP_MESSAGE_H_INCLUDED

#include "vsp_cmcp_datalist.h"

#include <vesper_util/vsp_api.h>
#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif /* defined __cplusplus */

/** Size of message headers in bytes:
 * 2 bytes topic ID, 2 bytes sender ID, 2 bytes command ID. */
#define VSP_CMCP_MESSAGE_HEADER_LENGTH 6

/** Message types. */
typedef enum {
    /** Control message. */
    VSP_CMCP_MESSAGE_TYPE_CONTROL,
    /** Data message. */
    VSP_CMCP_MESSAGE_TYPE_DATA
} vsp_cmcp_message_type;

/** Message data sent over (network) connection. */
struct vsp_cmcp_message;

/** Define type vsp_cmcp_message to avoid 'struct' keyword. */
typedef struct vsp_cmcp_message vsp_cmcp_message;

/**
 * Create new vsp_cmcp_message object to send message data.
 * Returned pointer should be freed with vsp_cmcp_message_free().
 * The specified command_id has to be lower than 2^15, i.e. MSB cleared.
 * The specified vsp_cmcp_datalist object may be NULL, in this case only the
 * message header will be written by vsp_cmcp_message_get_data().
 * The vsp_cmcp_message will not free the vsp_cmcp_datalist object and it has
 * to be accessible until vsp_cmcp_message_free() is called.
 * Returns NULL and sets vsp_error_num() if failed.
 */
vsp_cmcp_message *vsp_cmcp_message_create(vsp_cmcp_message_type message_type,
    uint16_t topic_id, uint16_t sender_id, uint16_t command_id,
    vsp_cmcp_datalist *cmcp_datalist);

/**
 * Free vsp_cmcp_message object.
 * Object should be created with vsp_cmcp_message_create() or
 * vsp_cmcp_message_create_parse().
 * Frees internal vsp_cmcp_datalist object when created with
 * vsp_cmcp_message_create_parse().
 */
void vsp_cmcp_message_free(vsp_cmcp_message *cmcp_message);

/**
 * Create new vsp_cmcp_message object from received binary data.
 * The data will be copied partially and pointers to it are stored, so the data
 * has to be accessible until vsp_cmcp_message_free() is called.
 * This function creates an internal object of type vsp_cmcp_datalist.
 * Returned pointer should be freed with vsp_cmcp_message_free().
 * Returns NULL and sets vsp_error_num() if failed.
 */
vsp_cmcp_message *vsp_cmcp_message_create_parse(uint16_t data_length,
    void *data_pointer);

/**
 * Calculate necessary length of a binary data array storing the message data.
 * This function should only be called for messages created with
 * vsp_cmcp_message_create().
 */
int vsp_cmcp_message_get_data_length(vsp_cmcp_message *cmcp_message);

/**
 * Copy message data to specified binary data array.
 * The specified array has to be at least as long as the number of bytes
 * vsp_cmcp_message_get_data_length() returns.
 * This function should only be called for messages created with
 * vsp_cmcp_message_create().
 */
void vsp_cmcp_message_get_data(vsp_cmcp_message *cmcp_message,
    void *data_pointer);

/**
 * Get the message type.
 * cmcp_message must not be NULL. Aborts if failed.
 */
vsp_cmcp_message_type vsp_cmcp_message_get_type(vsp_cmcp_message *cmcp_message);

/**
 * Get the message topic ID.
 * cmcp_message must not be NULL. Aborts if failed.
 */
uint16_t vsp_cmcp_message_get_topic_id(vsp_cmcp_message *cmcp_message);

/**
 * Get the message sender ID.
 * cmcp_message must not be NULL. Aborts if failed.
 */
uint16_t vsp_cmcp_message_get_sender_id(vsp_cmcp_message *cmcp_message);

/**
 * Get the message command ID.
 * cmcp_message must not be NULL. Aborts if failed.
 * The returned command_id is lower than 2^15, i.e. MSB cleared.
 */
uint16_t vsp_cmcp_message_get_command_id(vsp_cmcp_message *cmcp_message);

/**
 * Get data list parsed by this message object.
 * This function should only be called for messages created with
 * vsp_cmcp_message_create_parse().
 */
vsp_cmcp_datalist *vsp_cmcp_message_get_datalist(
    vsp_cmcp_message *cmcp_message);

#if defined __cplusplus
}
#endif /* defined __cplusplus */

#endif /* !defined VSP_CMCP_MESSAGE_H_INCLUDED */
