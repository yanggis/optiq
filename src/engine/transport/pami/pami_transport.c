#include "pami_transport.h"

struct optiq_transport_interface optiq_pami_transport_implementation = {
    .init = optiq_pami_transport_init,
    .send = optiq_pami_transport_send       
};

void optiq_pami_transport_init(struct optiq_transport *self)
{
#ifdef __bgq__
    const char client_name[] = "OPTIQ";
    struct optiq_pami_transport *pami_transport;
    int configuration_count;
    pami_result_t result;
    pami_configuration_t *configurations;
    pami_configuration_t query_configurations[3];
    size_t contexts;

    configuration_count = 0;
    configurations = NULL;

    pami_transport = optiq_transport_get_concrete_transport(self);
    pami_transport->num_contexts = 1;

    /*
    * Create client
    */
    result = PAMI_Client_create(client_name, &pami_transport->client, configurations, configuration_count);

    CORE_DEBUGGER_ASSERT(result == PAMI_SUCCESS);

    if (result != PAMI_SUCCESS) {
        return;
    }

    /*
    * Create context
    */
    result = PAMI_Context_createv(pami_transport->client, configurations, configuration_count, &pami_transport->context, pami_transport->num_contexts);
    CORE_DEBUGGER_ASSERT(result == PAMI_SUCCESS);

    if (result != PAMI_SUCCESS) {
        return;
    }

    query_configurations[0].name = PAMI_CLIENT_NUM_TASKS;
    query_configurations[1].name = PAMI_CLIENT_TASK_ID;
    query_configurations[2].name = PAMI_CLIENT_NUM_CONTEXTS;

    result = PAMI_Client_query(pami_transport->client, query_configurations, 3);
    self->size = query_configurations[0].value.intval;
    self->rank = query_configurations[1].value.intval;
    contexts = query_configurations[2].value.intval;

    CORE_DEBUGGER_ASSERT(contexts > 1);

    pami_transport->endpoints = (pami_endpoint_t *)malloc(sizeof(pami_endpoint_t) * self->size);
    for (i = 0; i < self->size; i++) {
        PAMI_Endpoint_create(pami_transport->client, i, 0, &pami_transport->endpoints[i]);
    }

    /*
    * Register dispatch IDs
    */
    pami_dispatch_callback_function fn;
    pami_dispatch_hint_t options = {};
    fn.p2p = optiq_recv_message_fn;
    result = PAMI_Dispatch_set (pami_transport->context,
            RECV_MESSAGE_DISPATCH_ID,
            fn,
            (void *) pami_transport,
            options);

    CORE_DEBUGGER_ASSERT(result == PAMI_SUCCESS);
    if (result != PAMI_SUCCESS) {
        return;
    }
    
#endif
}

void optiq_pami_transport_send(struct optiq_transport *self, struct optiq_message &message)
{
    printf("Transport data of size %d to dest %d with flow_id = %d\n", message.length, message.dest, message.flow_id);
}

