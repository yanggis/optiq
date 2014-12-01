#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "pami_transport.h"

void optiq_pami_transport_init(struct optiq_pami_transport *pami_transport)
{
#ifdef __bgq__
    const char client_name[] = "OPTIQ";
    pami_result_t result;
    pami_configuration_t query_configurations[3];
    size_t contexts;

    int configuration_count = 0;
    pami_configuration_t *configurations = NULL;
    pami_transport->num_contexts = 1;

    /*
     * Create client
     */
    result = PAMI_Client_create(client_name, &pami_transport->client, configurations, configuration_count);

    assert(result == PAMI_SUCCESS);

    if (result != PAMI_SUCCESS) {
	return;
    }

    /*
     * Create context
     */
    result = PAMI_Context_createv(pami_transport->client, configurations, configuration_count, &pami_transport->context, pami_transport->num_contexts);
    assert(result == PAMI_SUCCESS);

    if (result != PAMI_SUCCESS) {
	return;
    }

    query_configurations[0].name = PAMI_CLIENT_NUM_TASKS;
    query_configurations[1].name = PAMI_CLIENT_TASK_ID;
    query_configurations[2].name = PAMI_CLIENT_NUM_CONTEXTS;

    result = PAMI_Client_query(pami_transport->client, query_configurations, 3);
    pami_transport->size = query_configurations[0].value.intval;
    pami_transport->rank = query_configurations[1].value.intval;
    contexts = query_configurations[2].value.intval;
    pami_transport->node_id = pami_transport->rank;

    assert(contexts >= 1);

    /*Create endpoint for communication*/
    pami_transport->endpoints = (pami_endpoint_t *)core_memory_alloc(sizeof(pami_endpoint_t) * pami_transport->size, "endpoints", "pami_init");
    for (int i = 0; i < pami_transport->size; i++) {
	PAMI_Endpoint_create(pami_transport->client, i, 0, &pami_transport->endpoints[i]);
    }

    /*
     * Register dispatch IDs
     */
    pami_dispatch_callback_function fn;
    pami_dispatch_hint_t options = {};

    /*Message has come notification*/
    fn.p2p = optiq_recv_message_fn;
    result = PAMI_Dispatch_set (pami_transport->context,
	    RECV_MESSAGE_DISPATCH_ID,
	    fn,
	    (void *) pami_transport,
	    options);

    assert(result == PAMI_SUCCESS);
    if (result != PAMI_SUCCESS) {
	return;
    }

    /*Job done notification*/
    fn.p2p = optiq_recv_job_done_notification_fn;
    result = PAMI_Dispatch_set (pami_transport->context,
	    JOB_DONE_NOTIFICATION_DISPATCH_ID,
	    fn,
	    (void *) pami_transport,
	    options);

    assert(result == PAMI_SUCCESS);
    if (result != PAMI_SUCCESS) {
	return;
    }

    /*Other initialization*/
    /*Init a number of messages with buffer for receiving incomming messages*/
    struct optiq_message **recv_messages = get_messages_with_buffer(NUM_RECV_MESSAGES, RECV_MESSAGE_SIZE);
    for (int i = 0; i < NUM_RECV_MESSAGES; i++) {
        pami_transport->avail_recv_messages.push_back(recv_messages[i]);
    }

    /*Init a number of messages without buffer for sending messages*/
    struct optiq_message **send_messages = get_messages(NUM_SEND_MESSAGES);
    for (int i = 0; i < NUM_SEND_MESSAGES; i++) {
        pami_transport->avail_send_messages.push_back(send_messages[i]);
    }


    /*Prepare cookies for sending*/
    for (int i = 0; i < NUM_SEND_COOKIES; i++) {
	struct optiq_send_cookie *send_cookie = (struct optiq_send_cookie *)core_memory_alloc(sizeof(struct optiq_send_cookie), "send_cookies", "pami_init");
	send_cookie->pami_transport = pami_transport;
	pami_transport->avail_send_cookies.push_back(send_cookie);
    }

    /*Prepare cookies for receiving*/
    for (int i = 0; i < NUM_RECV_COOKIES; i++) {
	struct optiq_recv_cookie *recv_cookie = (struct optiq_recv_cookie *)core_memory_alloc(sizeof(struct optiq_recv_cookie), "recv_cookies", "pami_init");
	recv_cookie->pami_transport = pami_transport;
	pami_transport->avail_recv_cookies.push_back(recv_cookie);
    }
#endif
}

int optiq_pami_transport_send(struct optiq_pami_transport *self, struct optiq_message *message)
{
    bool use_window = false;
    int ret = 0;

    if (use_window) {
	/*Depend of the message size and source, do the spit up*/
	if (message->header.original_source == self->rank) {
	    int window_size = calculate_winsize(message->length);
	    int offset = 0;
	    while (offset < message->length) {
		struct optiq_message *instant = optiq_pami_transport_get_send_message(self);
		instant->header = message->header;
		instant->header.original_offset += offset;
		instant->next_dest = message->next_dest;
		instant->source = message->source;
		instant->buffer = &message->buffer[offset];
		instant->length = (offset + window_size <= message->length ? window_size : message->length - offset);
		offset += instant->length;

		ret = optiq_pami_transport_actual_send(self, instant);
	    }

	    optiq_pami_transport_return_send_message(self, message);
	} else {
	    ret = optiq_pami_transport_actual_send(self, message);
	}
    } else {
	ret = optiq_pami_transport_actual_send(self, message);
    }
    return ret;
}

int optiq_pami_transport_actual_send(struct optiq_pami_transport *pami_transport, struct optiq_message *message)
{
#ifdef __bgq__
    pami_result_t result;

    struct optiq_send_cookie *send_cookie = optiq_pami_transport_get_send_cookie(pami_transport);

    send_cookie->message = message;

    if (message->length <= MAX_SHORT_MESSAGE_LENGTH) {
	pami_send_immediate_t parameter;
	parameter.dispatch = RECV_MESSAGE_DISPATCH_ID;
	parameter.header.iov_base = (void *)&message->header;
	parameter.header.iov_len = sizeof(struct optiq_message_header);
	parameter.data.iov_base = (void *)message->buffer;
	parameter.data.iov_len = message->length;
	parameter.dest = pami_transport->endpoints[message->next_dest];

	result = PAMI_Send_immediate (pami_transport->context, &parameter);
	assert(result == PAMI_SUCCESS);
	if (result != PAMI_SUCCESS) {
	    return 1;
	}

	/*Add the cookie to the vector of in-use send cookies*/
	pami_transport->in_use_send_cookies.push_back(send_cookie);
    } else {
	pami_send_t param_send;
	param_send.send.dest = message->next_dest;
	param_send.send.dispatch = RECV_MESSAGE_DISPATCH_ID;
	param_send.send.header.iov_base = (void *)&message->header;
	param_send.send.header.iov_len = sizeof(struct optiq_message_header);
	param_send.send.data.iov_base = (void *)message->buffer;
	param_send.send.data.iov_len = message->length;
	param_send.events.cookie = (void *)send_cookie;
	param_send.events.local_fn = optiq_send_done_fn;
	param_send.events.remote_fn = NULL;

	result = PAMI_Send(pami_transport->context, &param_send);
	assert(result == PAMI_SUCCESS);
	if (result != PAMI_SUCCESS) {
	    return 1;
	}
    }

#ifdef DEBUG
    printf("Rank %d is sending data of size %d to Rank %d with flow_id = %d, original_offset = %d\n", pami_transport->rank, message->length, message->next_dest, message->header.flow_id, message->header.original_offset);
#endif

#endif
    return 0;
}

/* Return 1 if entire message is ready, 0 if not*/
int optiq_pami_transport_recv(struct optiq_pami_transport *pami_transport, struct optiq_message *message)
{
#ifdef __bgq__
    PAMI_Context_advance (pami_transport->context, 100);

    for (int i = 0; i < pami_transport->local_messages.size(); i++) {
	struct optiq_message *instant = pami_transport->local_messages.back();

	memcpy((void *)&message->buffer[instant->header.original_offset], (const void*)instant->buffer, instant->length);
	/*printf("Done copy data\n");*/
	message->recv_length += instant->length;

	pami_transport->local_messages.pop_back();
	(pami_transport->avail_recv_messages).push_back(instant);

	if (message->recv_length == instant->header.original_length) {
	    /*printf("Rank %d received entire message of the job, notify the involved tasks\n", pami_transport->rank);*/
	    optiq_notify_job_done(pami_transport, &pami_transport->involved_task_ids);
	    return 1;
	}
    }

#endif
    return 0;
}

bool optiq_pami_transport_test(struct optiq_pami_transport *pami_transport, struct optiq_message *message)
{
    bool isDone = true;
    pami_result_t result;
#ifdef __bgq__
    PAMI_Context_advance (pami_transport->context, 100);
    optiq_send_cookie *send_cookie;

    /*Return cookie, messag back to available queues. Adding message sent to flow's*/
    while (pami_transport->in_use_send_cookies.size() > 0) {
	send_cookie = pami_transport->in_use_send_cookies.back();
	message->sent_bytes += send_cookie->message->length;

	pami_transport->in_use_send_cookies.pop_back();
	pami_transport->avail_send_messages.push_back(send_cookie->message);
	pami_transport->avail_send_cookies.push_back(send_cookie);
    }

    /*Checking if every flow is done*/
    if (message->length != message->sent_bytes) {
	isDone = false;
    }

#endif
    return isDone;
}

/*
 * Process the incomming message. Either forward it by putting it in a virtual lane or keep it
 * */
int optiq_pami_transport_process_incomming_message(struct optiq_pami_transport *pami_transport)
{

    if (pami_transport->in_use_recv_cookies.size() > 0) {
	struct optiq_recv_cookie *recv_cookie = pami_transport->in_use_recv_cookies.back();
	struct optiq_message *message = recv_cookie->message;
	pami_transport->local_messages.push_back(message);

	/*Move the recv_cookie to available vector*/
	pami_transport->in_use_recv_cookies.pop_back();
	pami_transport->avail_recv_cookies.push_back(recv_cookie);
    }

    return 0;
}

#ifdef __bgq__
void optiq_recv_done_fn(pami_context_t context, void *cookie, pami_result_t result)
{
    struct optiq_recv_cookie *recv_cookie = (struct optiq_recv_cookie *)cookie;
    recv_cookie->pami_transport->in_use_recv_cookies.push_back(recv_cookie);

    optiq_pami_transport_process_incomming_message(recv_cookie->pami_transport);
}

void optiq_send_done_fn(pami_context_t context, void *cookie, pami_result_t result)
{
    /*Add send cookie into in_use_cookie*/
    struct optiq_send_cookie *send_cookie = (struct optiq_send_cookie *)cookie;
    /*printf("Rank %d done sending data to Rank %d\n", send_cookie->pami_transport->rank, send_cookie->message->next_dest);*/
    send_cookie->pami_transport->in_use_send_cookies.push_back(send_cookie);
    /*printf("Done put data into in_use_send_cookies\n");*/
}

void optiq_recv_message_fn(pami_context_t context, void *cookie, const void *header, size_t header_size,
	const void *data, size_t data_size, pami_endpoint_t origin, pami_recv_t *recv)
{
    struct optiq_pami_transport *pami_transport = (struct optiq_pami_transport *) cookie;
    struct optiq_message_header *message_header = (struct optiq_message_header *) header;

#ifdef DEBUG
    printf("Rank %d is receiving from Rank %d with %d bytes\n", pami_transport->rank, origin, data_size);
#endif
    struct optiq_message *message;

    /*If incomming message is larger than the default size, need to allocate new memory*/
    if (data_size > RECV_MESSAGE_SIZE) {
	message = get_message_with_buffer(data_size);
    } else {
	/*If there is still available message to use*/
	if ((pami_transport->avail_recv_messages).size() > 0) {
	    message = (pami_transport->avail_recv_messages).back();
	    (pami_transport->avail_recv_messages).pop_back();
	} else {
	    message = get_message_with_buffer(RECV_MESSAGE_SIZE);
	}
    }

    memcpy(&message->header, header, sizeof(struct optiq_message_header));
    message->length = data_size;
    message->source = origin;
    message->current_offset = 0;

    struct optiq_recv_cookie *recv_cookie;

    if (pami_transport->avail_recv_cookies.size() > 0) {
	recv_cookie = pami_transport->avail_recv_cookies.back();
	pami_transport->avail_recv_cookies.pop_back();
    } else {
	recv_cookie = (struct optiq_recv_cookie *) core_memory_alloc(sizeof(struct optiq_recv_cookie), "recv_cookie", "recv_message_fn");
	recv_cookie->pami_transport = pami_transport;
    }

    recv_cookie->message = message;

    if (data != NULL) {
	memcpy(message->buffer, data, data_size);
	pami_transport->in_use_recv_cookies.push_back(recv_cookie);
	optiq_pami_transport_process_incomming_message(pami_transport);
    } else {
	recv->local_fn = optiq_recv_done_fn;
	recv->cookie = (void *)recv_cookie;
	recv->type = PAMI_TYPE_BYTE;
	recv->addr = (void *)message->buffer;
	recv->offset = 0;
	recv->data_fn = PAMI_DATA_COPY;
	recv->data_cookie = NULL;
    }
    return;
}
#endif

int optiq_notify_job_done(struct optiq_pami_transport *pami_transport, vector<int> *dests)
{
#ifdef __bgq__
    pami_result_t result;
    int job_id = 0;

    for (int i = 0; i < dests->size(); i++) {
	pami_send_immediate_t parameter;
	parameter.dispatch = JOB_DONE_NOTIFICATION_DISPATCH_ID;
	parameter.header.iov_base = &job_id;
	parameter.header.iov_len = sizeof(int);
	parameter.data.iov_base = NULL;
	parameter.data.iov_len = 0;
	parameter.dest = pami_transport->endpoints[(*dests)[i]];

	result = PAMI_Send_immediate (pami_transport->context, &parameter);
	assert(result == PAMI_SUCCESS);
	if (result != PAMI_SUCCESS) {
	    return 1;
	}
    }
#endif
    return 0;
}

#ifdef __bgq__
void optiq_recv_job_done_notification_fn(pami_context_t context, void *cookie, const void *header, size_t header_size,
	const void *data, size_t data_size, pami_endpoint_t origin, pami_recv_t *recv)
{
    struct optiq_pami_transport *pami_transport = (struct optiq_pami_transport *) cookie;

    int *job_id = (int *) header;

    for (int i = 0; i < pami_transport->involved_job_ids.size(); i++) {
	pami_transport->involved_job_ids.erase(pami_transport->involved_job_ids.begin() + i);
    }
}
#endif

int calculate_winsize(int message_size)
{
    if (message_size < 1024)
	return message_size;
    else if(1024 <= message_size && message_size < 2048)
	return 1024;
    if (2048 <= message_size && message_size < 4096)
	return 2048;
    if (4096 <= message_size && message_size < 8192)
	return 4096;
    if (8192 <= message_size && message_size < 16384)
	return 4096;
    if (16384 <= message_size && message_size < 32768)
	return 4096;
    if (32768 <= message_size && message_size < 65536)
	return 8192;
    if (65536 <= message_size && message_size < 131072)
	return 16384;
    if (131072 <= message_size && message_size < 262144)
	return 16384;
    if (262144 <= message_size && message_size < 524288)
	return 16384;
    if (524288 <= message_size && message_size < 1048576)
	return 32768;
    if (1048576 <= message_size && message_size < 2097152)
	return 65536;
    if (2097152 <= message_size && message_size < 4194304)
	return 65536;
    if (4194304 <= message_size && message_size < 8388608)
	return 65536;
    if (8388608 <= message_size && message_size < 16777216)
	return 131072;
    if (16777216 <= message_size && message_size < 33554432)
	return 131072;
    if (33554432 <= message_size && message_size < 67108864)
	return 262144;
    if (67108864 <= message_size && message_size < 134217728)
	return 262144;
    if (134217728 <= message_size)
	return 524288;

    return message_size;
}

struct optiq_send_cookie* optiq_pami_transport_get_send_cookie(struct optiq_pami_transport *self)
{
    struct optiq_send_cookie *send_cookie;

    if (self->avail_send_cookies.size() > 0) {
        send_cookie = self->avail_send_cookies.back();
        self->avail_send_cookies.pop_back();
    } else {
        send_cookie = (struct optiq_send_cookie *)core_memory_alloc(sizeof(struct optiq_send_cookie), "send_cookies", "pami_transport_send");
        send_cookie->pami_transport = self;
    }

    return send_cookie;
}

struct optiq_message* optiq_pami_transport_get_send_message(struct optiq_pami_transport *self)
{
    return get_send_message(&self->avail_send_messages);
}

void optiq_pami_transport_return_send_message(struct optiq_pami_transport *self, struct optiq_message *message)
{
    self->avail_send_messages.push_back(message);
}
