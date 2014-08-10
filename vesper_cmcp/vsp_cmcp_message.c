/**
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#include "vsp_cmcp_message.h"

#include <vesper_util/vsp_error.h>
#include <vesper_util/vsp_util.h>

/** Size of message headers in bytes:
 * 2 bytes topic ID, 2 bytes sender ID, 2 bytes command ID. */
#define VSP_CMCP_MESSAGE_HEADER_LENGTH 6

/** Message type: send or receive message. */
typedef enum {
    /** Message will be sent. */
    VSP_CMCP_MESSAGE_TYPE_SEND,
    /** Message was received. */
    VSP_CMCP_MESSAGE_TYPE_RECEIVE
} vsp_cmcp_message_type;

/** Message data sent over (network) connection. */
struct vsp_cmcp_message {
    /** Message type. */
    vsp_cmcp_message_type type;
    /** Message topic or receiver ID. */
    uint16_t topic_id;
    /** Message sender ID. */
    uint16_t sender_id;
    /** Message command ID. */
    uint16_t command_id;
    /** Message parameters stored as data list items. */
    vsp_cmcp_datalist *cmcp_datalist;
};

vsp_cmcp_message *vsp_cmcp_message_create(uint16_t topic_id,
    uint16_t sender_id, uint16_t command_id, vsp_cmcp_datalist *cmcp_datalist)
{
    vsp_cmcp_message *cmcp_message;
    /* allocate memory */
    VSP_ALLOC(cmcp_message, vsp_cmcp_message, return NULL);
    /* initialize struct data */
    cmcp_message->type = VSP_CMCP_MESSAGE_TYPE_SEND;
    cmcp_message->topic_id = topic_id;
    cmcp_message->sender_id = sender_id;
    cmcp_message->command_id = command_id;
    cmcp_message->cmcp_datalist = cmcp_datalist;
    /* return struct pointer */
    return cmcp_message;
}

int vsp_cmcp_message_free(vsp_cmcp_message *cmcp_message)
{
    /* check parameter */
    VSP_ASSERT(cmcp_message != NULL, vsp_error_set_num(EINVAL); return -1);

    if (cmcp_message->type == VSP_CMCP_MESSAGE_TYPE_RECEIVE) {
        vsp_cmcp_datalist_free(cmcp_message->cmcp_datalist);
    }

    /* free memory */
    VSP_FREE(cmcp_message);
    return 0;
}

vsp_cmcp_message *vsp_cmcp_message_create_parse(uint16_t data_length,
    void *data_pointer)
{
    /* using short int pointer for safe pointer arithmetic */
    uint16_t *current_data_pointer;
    vsp_cmcp_message *cmcp_message;

    /* check parameters */
    VSP_ASSERT(data_length >= VSP_CMCP_MESSAGE_HEADER_LENGTH,
        vsp_error_set_num(EINVAL); return NULL);
    VSP_ASSERT(data_pointer != NULL, vsp_error_set_num(EINVAL); return NULL);
    /* allocate memory */
    VSP_ALLOC(cmcp_message, vsp_cmcp_message, return NULL);
    /* initialize struct data */
    cmcp_message->type = VSP_CMCP_MESSAGE_TYPE_RECEIVE;
    current_data_pointer = data_pointer;
    cmcp_message->topic_id = *current_data_pointer;
    ++current_data_pointer;
    cmcp_message->sender_id = *current_data_pointer;
    ++current_data_pointer;
    cmcp_message->command_id = *current_data_pointer;
    ++current_data_pointer;
    /* parse data list values */
    cmcp_message->cmcp_datalist = vsp_cmcp_datalist_create_parse(
        data_length - VSP_CMCP_MESSAGE_HEADER_LENGTH, current_data_pointer);
    /* in case of failure vsp_error_num() is already set */
    VSP_ASSERT(cmcp_message->cmcp_datalist != NULL, return NULL);

    /* return struct pointer */
    return cmcp_message;
}

int vsp_cmcp_message_get_data_length(vsp_cmcp_message *cmcp_message)
{
    int data_length;

    /* check parameter */
    VSP_ASSERT(cmcp_message != NULL, vsp_error_set_num(EINVAL); return -1);

    /* check message type */
    VSP_ASSERT(cmcp_message->type == VSP_CMCP_MESSAGE_TYPE_SEND,
        vsp_error_set_num(EINVAL); return -1);

    /* calculate data length */
    data_length = 0;

    /* empty data list value is allowed, then only message header is created */
    if (cmcp_message->cmcp_datalist != NULL) {
        /* add data list length */
        data_length =
            vsp_cmcp_datalist_get_data_length(cmcp_message->cmcp_datalist);
        /* in case of failure vsp_error_num() is already set */
        VSP_ASSERT(data_length >= 0, return -1);
    }

    data_length += VSP_CMCP_MESSAGE_HEADER_LENGTH;

    return data_length;
}

int vsp_cmcp_message_get_data(vsp_cmcp_message *cmcp_message,
    void *data_pointer)
{
    int ret;
    /* using short int pointer for safe pointer arithmetic */
    uint16_t *current_data_pointer;

    /* check parameter */
    VSP_ASSERT(cmcp_message != NULL, vsp_error_set_num(EINVAL); return -1);

    /* check message type */
    VSP_ASSERT(cmcp_message->type == VSP_CMCP_MESSAGE_TYPE_SEND,
        vsp_error_set_num(EINVAL); return -1);

    /* store message header */
    current_data_pointer = data_pointer;
    *current_data_pointer = cmcp_message->topic_id;
    ++current_data_pointer;
    *current_data_pointer = cmcp_message->sender_id;
    ++current_data_pointer;
    *current_data_pointer = cmcp_message->command_id;
    ++current_data_pointer;

    /* empty data list value is allowed, then only message header is created */
    if (cmcp_message->cmcp_datalist != NULL) {
        /* store data list values */
        ret = vsp_cmcp_datalist_get_data(cmcp_message->cmcp_datalist,
            current_data_pointer);
        /* in case of failure vsp_error_num() is already set */
        VSP_ASSERT(ret == 0, return -1);
    }

    /* success */
    return 0;
}

int vsp_cmcp_message_get_id(vsp_cmcp_message *cmcp_message,
    vsp_cmcp_message_id_type id_type, uint16_t *id_pointer)
{
    /* check parameters */
    VSP_ASSERT(cmcp_message != NULL, vsp_error_set_num(EINVAL); return -1);
    VSP_ASSERT(id_pointer != NULL, vsp_error_set_num(EINVAL); return -1);

    switch (id_type) {
        case VSP_CMCP_MESSAGE_TOPIC_ID:
            *id_pointer = cmcp_message->topic_id;
            return 0;
        case VSP_CMCP_MESSAGE_SENDER_ID:
            *id_pointer = cmcp_message->sender_id;
            return 0;
        case VSP_CMCP_MESSAGE_COMMAND_ID:
            *id_pointer = cmcp_message->command_id;
            return 0;
        default:
            return -1;
    }
}

vsp_cmcp_datalist *vsp_cmcp_message_get_datalist(
    vsp_cmcp_message *cmcp_message) {
    /* check parameter */
    VSP_ASSERT(cmcp_message != NULL, vsp_error_set_num(EINVAL); return NULL);

    /* check message type */
    VSP_ASSERT(cmcp_message->type == VSP_CMCP_MESSAGE_TYPE_RECEIVE,
        vsp_error_set_num(EINVAL); return NULL);

    return cmcp_message->cmcp_datalist;
}
