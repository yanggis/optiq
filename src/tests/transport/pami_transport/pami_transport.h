#ifndef OPTIQ_PAMI_TRANSPORT_H
#define OPTIQ_PAMI_TRANSPORT_H

#include <vector>

#include <spi/include/kernel/location.h>
#include <spi/include/kernel/process.h>
#include <firmware/include/personality.h>
#include <pami.h>

#define MR_REQUEST 10
#define MR_RESPONSE 11
#define RPUT_DONE 12
#define RECV_MESSAGE 13
#define JOB_DONE 14

#define MAX_SHORT_MESSAGE_LENGTH 128

struct optiq_memregion {
    pami_memregion_t mr;
    int offset;
    int header_id;
};

struct optiq_message_header {
    int length;
    int source;
    int dest;
    int flow_id;
    struct optiq_memregion mem;
    int header_id;
};

struct optiq_pami_transport;

struct optiq_rput_cookie {
    struct optiq_pami_transport *pami_transport;
    struct optiq_message_header *message_header;
    int dest;
};

struct optiq_pami_extra {

    struct optiq_memregion *local_mr;
    struct optiq_memregion *near_mr;
    //struct optiq_memregion *far_mr;

    std::vector<struct optiq_rput_cookie *> rput_cookies;
    std::vector<struct optiq_rput_cookie *> complete_rputs;

    std::vector<struct optiq_message_header *> forward_headers;
    std::vector<struct optiq_message_header *> message_headers;
    std::vector<struct optiq_message_header *> local_headers;
    std::vector<struct optiq_message_header *> processing_headers;

    std::vector<struct optiq_memregion> mr_responses;

    int remaining_jobs;
    int *next_dest;

    int expecting_length;
    int sent_bytes;
    int *recv_bytes;
    int global_header_id;
};

struct optiq_pami_transport {
    int rank;
    int size;
    size_t num_contexts;
    pami_client_t client;
    pami_context_t context;
    pami_endpoint_t *endpoints;

    struct optiq_pami_extra extra;
};

void optiq_pami_init(struct optiq_pami_transport *self);

int optiq_pami_send_immediate(pami_context_t &context, int dispatch, void *header_base, int header_len, void *data_base, int data_len, pami_endpoint_t &endpoint);

void optiq_pami_rput_done_fn(pami_context_t context, void *cookie, pami_result_t result);

int optiq_pami_rput(pami_client_t client, pami_context_t context, pami_memregion_t *local_mr, size_t local_offset, size_t nbytes, pami_endpoint_t &endpoint, pami_memregion_t *remote_mr, size_t remote_offset, void *cookie);

void optiq_send_done_fn(pami_context_t context, void *cookie, pami_result_t result);

int optiq_pami_send(pami_context_t context, int dispatch, void *header_base, int header_len, void *data_base, int data_len, pami_endpoint_t endpoint, void *cookie);

void optiq_recv_message_fn (
        pami_context_t    context,      /**< IN: PAMI context */
        void            *cookie,       /**< IN: dispatch cookie */
        const void      *header,       /**< IN: header address */
        size_t            header_size,  /**< IN: header size */
        const void      *data,         /**< IN: address of PAMI pipe buffer */
        size_t            data_size,    /**< IN: size of PAMI pipe buffer */
        pami_endpoint_t   origin,
        pami_recv_t     *recv);        /**< OUT: receive message structure */

void optiq_recv_job_done_notification_fn (
        pami_context_t    context,      /**< IN: PAMI context */
        void            *cookie,       /**< IN: dispatch cookie */
        const void      *header,       /**< IN: header address */
        size_t            header_size,  /**< IN: header size */
        const void      *data,         /**< IN: address of PAMI pipe buffer */
        size_t            data_size,    /**< IN: size of PAMI pipe buffer */
        pami_endpoint_t   origin,
        pami_recv_t     *recv);        /**< OUT: receive message structure */

void optiq_recv_rput_done_notification_fn (
	pami_context_t    context,      /**< IN: PAMI context */
        void            *cookie,       /**< IN: dispatch cookie */
        const void      *header,       /**< IN: header address */
        size_t            header_size,  /**< IN: header size */
        const void      *data,         /**< IN: address of PAMI pipe buffer */
        size_t            data_size,    /**< IN: size of PAMI pipe buffer */
        pami_endpoint_t   origin,
        pami_recv_t     *recv);        /**< OUT: receive message structure */

void optiq_recv_mr_response_fn (
        pami_context_t    context,      /**< IN: PAMI context */
        void            *cookie,       /**< IN: dispatch cookie */
        const void      *header,       /**< IN: header address */
        size_t            header_size,  /**< IN: header size */
        const void      *data,         /**< IN: address of PAMI pipe buffer */
        size_t            data_size,    /**< IN: size of PAMI pipe buffer */
        pami_endpoint_t   origin,
        pami_recv_t     *recv);        /**< OUT: receive message structure */

void optiq_recv_mr_request_fn (
        pami_context_t    context,      /**< IN: PAMI context */
        void            *cookie,       /**< IN: dispatch cookie */
        const void      *header,       /**< IN: header address */
        size_t            header_size,  /**< IN: header size */
        const void      *data,         /**< IN: address of PAMI pipe buffer */
        size_t            data_size,    /**< IN: size of PAMI pipe buffer */
        pami_endpoint_t   origin,
        pami_recv_t     *recv);        /**< OUT: receive message structure */

#endif
