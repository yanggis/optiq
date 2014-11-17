#include <stdlib.h>

#include "memory.h"
#include "transport.h"

void optiq_transport_init(struct optiq_transport *self, enum optiq_transport_type type)
{
    if (type == PAMI) {
	self->transport_implementation = &optiq_pami_transport_implementation;
    } else if (type == NONBLK_MPI) {
	self->transport_implementation = &optiq_nonblk_mpi_transport_implementation;
    } else if (type == GNI) {
	self->transport_implementation = &optiq_gni_transport_implementation;
    } else {
	/*self->transport_implementation = &optiq_tcp_ip_transport_implementation;*/
    }

    /*Init a number of messages with buffer for receiving incomming messages*/
    struct optiq_message *recv_messages = (struct optiq_message *)core_memory_alloc(sizeof(struct optiq_message) * NUM_RECV_MESSAGES, "recv_messsage", "transport_init");

    for (int i = 0; i < NUM_RECV_MESSAGES; i++) {
        recv_messages[i].buffer = (char *)core_memory_alloc(RECV_MESSAGE_SIZE, "recv_message buffer", "transport_init");
        self->avail_recv_messages.push_back(recv_messages + i);
    }

    /*Init a number of messages without buffer for sending messages*/
    struct optiq_message *send_messages = (struct optiq_message *)core_memory_alloc(sizeof(struct optiq_message) * NUM_SEND_MESSAGES, "send_messages", "transport_init");

    for (int i = 0; i < NUM_SEND_MESSAGES; i++) {
        self->avail_send_messages.push_back(send_messages + i);
    }

    self->type = type;
    self->concrete_transport = NULL;
    self->transport_implementation->init(self);
}

int  optiq_transport_send(struct optiq_transport *self, struct optiq_message *message)
{
    return self->transport_implementation->send(self, message);
}

int optiq_transport_recv(struct optiq_transport *self, struct optiq_message *message)
{
    return self->transport_implementation->recv(self, message);
}

bool optiq_transport_test(struct optiq_transport *self, struct optiq_job *job)
{
    return self->transport_implementation->test(self, job);
}

int optiq_transport_destroy(struct optiq_transport *self)
{
    return self->transport_implementation->destroy(self);
}

void* optiq_transport_get_concrete_transport(struct optiq_transport *self)
{
    if (self->concrete_transport == NULL) {
	if (self->type == PAMI) {
	    self->concrete_transport = (struct optiq_pami_transport *)core_memory_alloc(sizeof(struct optiq_pami_transport), "pami concrete transport", "get_concrete_transport");
	} else if (self->type == GNI) {
	    self->concrete_transport = (struct optiq_gni_transport *)core_memory_alloc(sizeof(struct optiq_gni_transport), "gni concrete transport", "get_concrete_transport");
	} else if (self->type == NONBLK_MPI) {
	    self->concrete_transport = (struct optiq_nonblk_mpi_transport *)core_memory_alloc(sizeof(struct optiq_nonblk_mpi_transport), "nonblk concrete transport", "get_concrete_transport");
	} else {

	}
    }

    return self->concrete_transport;
}
