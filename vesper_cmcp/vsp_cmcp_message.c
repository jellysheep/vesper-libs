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

/** Message action: send or receive message. */
typedef enum {
    /** Message will be sent. */
    VSP_CMCP_MESSAGE_ACTION_SEND,
    /** Message was received. */
    VSP_CMCP_MESSAGE_ACTION_RECEIVE
} vsp_cmcp_message_action;

/** Message data sent over (network) connection. */
struct vsp_cmcp_message {
    /** Message type. */
    vsp_cmcp_message_type type;
    /** Message action. */
    vsp_cmcp_message_action action;
    /** Message topic or receiver ID. */
    uint16_t topic_id;
    /** Message sender ID. */
    uint16_t sender_id;
    /** Message command ID. */
    uint16_t command_id;
    /** Message parameters stored as data list items. */
    vsp_cmcp_datalist *cmcp_datalist;
};

vsp_cmcp_message *vsp_cmcp_message_create(vsp_cmcp_message_type message_type,
    uint16_t topic_id, uint16_t sender_id, uint16_t command_id,
    vsp_cmcp_datalist *cmcp_datalist)
{
    vsp_cmcp_message *cmcp_message;
    /* check parameters */
    VSP_ASSERT(message_type == VSP_CMCP_MESSAGE_TYPE_CONTROL
        || message_type == VSP_CMCP_MESSAGE_TYPE_DATA);
    /* allocate memory */
    VSP_ALLOC(cmcp_message, vsp_cmcp_message);
    /* initialize struct data */
    cmcp_message->type = message_type;
    cmcp_message->action = VSP_CMCP_MESSAGE_ACTION_SEND;
    cmcp_message->topic_id = topic_id;
    cmcp_message->sender_id = sender_id;
    cmcp_message->command_id = (command_id << 1) | message_type;
    cmcp_message->cmcp_datalist = cmcp_datalist;
    /* return struct pointer */
    return cmcp_message;
}

void vsp_cmcp_message_free(vsp_cmcp_message *cmcp_message)
{
    /* check parameter */
    VSP_CHECK(cmcp_message != NULL, return);

    if (cmcp_message->action == VSP_CMCP_MESSAGE_ACTION_RECEIVE) {
        vsp_cmcp_datalist_free(cmcp_message->cmcp_datalist);
    }

    /* free memory */
    VSP_FREE(cmcp_message);
}

vsp_cmcp_message *vsp_cmcp_message_create_parse(uint16_t data_length,
    void *data_pointer)
{
    /* using short int pointer for safe pointer arithmetic */
    uint16_t *current_data_pointer;
    vsp_cmcp_message *cmcp_message;

    /* check parameters */
    VSP_ASSERT(data_length >= VSP_CMCP_MESSAGE_HEADER_LENGTH);
    VSP_ASSERT(data_pointer != NULL);
    /* allocate memory */
    VSP_ALLOC(cmcp_message, vsp_cmcp_message);
    /* initialize struct data */
    cmcp_message->action = VSP_CMCP_MESSAGE_ACTION_RECEIVE;
    current_data_pointer = data_pointer;
    cmcp_message->topic_id = *current_data_pointer;
    ++current_data_pointer;
    cmcp_message->sender_id = *current_data_pointer;
    ++current_data_pointer;
    cmcp_message->command_id = *current_data_pointer;
    ++current_data_pointer;
    /* get message type */
    cmcp_message->type = cmcp_message->command_id & 1;
    /* parse data list values */
    cmcp_message->cmcp_datalist = vsp_cmcp_datalist_create_parse(
        data_length - VSP_CMCP_MESSAGE_HEADER_LENGTH, current_data_pointer);
    /* vsp_error_num() is set by vsp_cmcp_datalist_create_parse() */
    VSP_CHECK(cmcp_message->cmcp_datalist != NULL,
        VSP_FREE(cmcp_message); return NULL);

    /* return struct pointer */
    return cmcp_message;
}

int vsp_cmcp_message_get_data_length(vsp_cmcp_message *cmcp_message)
{
    int data_length;

    /* check parameter */
    VSP_ASSERT(cmcp_message != NULL);

    /* check message action */
    VSP_ASSERT(cmcp_message->action == VSP_CMCP_MESSAGE_ACTION_SEND);

    /* calculate data length */
    data_length = 0;

    /* empty data list value is allowed, then only message header is created */
    if (cmcp_message->cmcp_datalist != NULL) {
        /* add data list length */
        data_length =
            vsp_cmcp_datalist_get_data_length(cmcp_message->cmcp_datalist);
        VSP_ASSERT(data_length >= 0);
    }

    data_length += VSP_CMCP_MESSAGE_HEADER_LENGTH;

    return data_length;
}

void vsp_cmcp_message_get_data(vsp_cmcp_message *cmcp_message,
    void *data_pointer)
{
    int ret;
    /* using short int pointer for safe pointer arithmetic */
    uint16_t *current_data_pointer;

    /* check parameters */
    VSP_ASSERT(cmcp_message != NULL && data_pointer != NULL);

    /* check message action */
    VSP_ASSERT(cmcp_message->action == VSP_CMCP_MESSAGE_ACTION_SEND);

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
        VSP_ASSERT(ret == 0);
    }
}

vsp_cmcp_message_type vsp_cmcp_message_get_type(vsp_cmcp_message *cmcp_message)
{
    /* check parameter */
    VSP_ASSERT(cmcp_message != NULL);
    return cmcp_message->type;
}

uint16_t vsp_cmcp_message_get_topic_id(vsp_cmcp_message *cmcp_message)
{
    /* check parameter */
    VSP_ASSERT(cmcp_message != NULL);
    return cmcp_message->topic_id;
}

uint16_t vsp_cmcp_message_get_sender_id(vsp_cmcp_message *cmcp_message)
{
    /* check parameter */
    VSP_ASSERT(cmcp_message != NULL);
    return cmcp_message->sender_id;
}

uint16_t vsp_cmcp_message_get_command_id(vsp_cmcp_message *cmcp_message)
{
    /* check parameter */
    VSP_ASSERT(cmcp_message != NULL);
    return cmcp_message->command_id >> 1;
}

vsp_cmcp_datalist *vsp_cmcp_message_get_datalist(
    vsp_cmcp_message *cmcp_message) {
    /* check parameter */
    VSP_ASSERT(cmcp_message != NULL);

    /* check message action */
    VSP_ASSERT(cmcp_message->action == VSP_CMCP_MESSAGE_ACTION_RECEIVE);

    return cmcp_message->cmcp_datalist;
}
