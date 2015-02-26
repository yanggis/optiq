#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <vector>
#include <mpi.h>
#include <pami.h>

#include "algorithm.h"
#include "schedule.h"

#define OPTIQ_MAX_NUM_PATHS (1024 * 1024)

struct optiq_schedule *schedule = NULL;

void optiq_schedule_init()
{
    struct optiq_pami_transport *pami_transport = optiq_pami_transport_get();

    if (pami_transport == NULL) {
	printf("Init pami transport first\n");
	return;
    }

    schedule = (struct optiq_schedule *) calloc (1, sizeof(struct optiq_schedule));

    schedule->world_size = pami_transport->size;
    schedule->world_rank = pami_transport->rank;

    schedule->next_dests = (int *) calloc (1, sizeof(int) * OPTIQ_MAX_NUM_PATHS);
    schedule->recv_bytes = (int *) calloc (1, sizeof(int) * schedule->world_size);

    schedule->pami_transport = pami_transport;
    pami_transport->sched = schedule;

    schedule->all_num_dests = (int *) malloc (sizeof(int) * pami_transport->size);
    schedule->active_immsends = pami_transport->size;
}

struct optiq_schedule *optiq_schedule_get()
{
    return schedule;
}

void optiq_schedule_finalize()
{
    struct optiq_schedule *schedule = optiq_schedule_get();

    free(schedule->next_dests);
    free(schedule->recv_bytes);
}

void build_next_dests(int world_rank, int *next_dests, std::vector<struct path *> &complete_paths)
{
    for (int i = 0; i < complete_paths.size(); i++)
    {
	for (int j = 0; j < complete_paths[i]->arcs.size(); j++)
	{
	    if (complete_paths[i]->arcs[j].u == world_rank)
	    {
		next_dests[i] = complete_paths[i]->arcs[j].v;
	    }
	}
    }
}

void optiq_schedule_split_jobs (struct optiq_pami_transport *pami_transport, std::vector<struct optiq_job> &jobs, int chunk_size)
{
    bool done = false;

    while(!done)
    {
	done = true;

	for (int i = 0; i < jobs.size(); i++)
	{
	    int nbytes = chunk_size;

	    if (jobs[i].buf_offset < jobs[i].buf_length) 
	    {
		if (nbytes > jobs[i].buf_length - jobs[i].buf_offset) {
		    nbytes = jobs[i].buf_length - jobs[i].buf_offset;
		}

		struct optiq_message_header *header = pami_transport->transport_info.message_headers.back();
		pami_transport->transport_info.message_headers.pop_back();

		header->length = nbytes;
		header->source = jobs[i].source_rank;
		header->dest = jobs[i].dest_rank;
		header->path_id = jobs[i].paths[0]->path_id;

		memcpy(&header->mem, &jobs[i].send_mr, sizeof(struct optiq_memregion));
		header->mem.offset = jobs[i].send_mr.offset + jobs[i].buf_offset;
		header->original_offset = jobs[i].buf_offset;
		jobs[i].buf_offset += nbytes;

		pami_transport->transport_info.send_headers.push_back(header);

		if (jobs[i].buf_offset < jobs[i].buf_length) {
		    done = false;
		}
	    }
	}
    }

    /*Clean the jobs*/
    for (int i = 0; i < jobs.size(); i++)
    {
	jobs[i].buf_offset = 0;
    }
}

void optiq_schedule_split_jobs_multipaths (struct optiq_pami_transport *pami_transport, std::vector<struct optiq_job> &jobs, int chunk_size)
{
    bool done = false;

    while(!done)
    {
	done = true;

	for (int i = 0; i < jobs.size(); i++)
	{
	    if (chunk_size == 0) {
		chunk_size = jobs[i].buf_length/2;
	    }
	    int nbytes = chunk_size;

	    if (jobs[i].buf_offset < jobs[i].buf_length) 
	    {
		if (nbytes > jobs[i].buf_length - jobs[i].buf_offset) {
		    nbytes = jobs[i].buf_length - jobs[i].buf_offset;
		}

		struct optiq_message_header *header = pami_transport->transport_info.message_headers.back();
		pami_transport->transport_info.message_headers.pop_back();

		header->length = nbytes;
		header->source = jobs[i].source_rank;
		header->dest = jobs[i].dest_rank;
		header->path_id = jobs[i].paths[jobs[i].last_path_index]->path_id;
		jobs[i].last_path_index = (jobs[i].last_path_index + 1) % jobs[i].paths.size();

		memcpy(&header->mem, &jobs[i].send_mr, sizeof(struct optiq_memregion));
		header->mem.offset = jobs[i].send_mr.offset + jobs[i].buf_offset;
		header->original_offset = jobs[i].buf_offset;
		jobs[i].buf_offset += nbytes;

		pami_transport->transport_info.send_headers.push_back(header);

		if (jobs[i].buf_offset < jobs[i].buf_length) {
		    done = false;
		}
	    }
	}
    }

    /*Clean the jobs*/
    for (int i = 0; i < jobs.size(); i++)
    {
	jobs[i].buf_offset = 0;
    }
}

void optiq_schedule_add_paths (struct optiq_schedule &schedule, std::vector<struct path *> &complete_paths)
{
    struct optiq_pami_transport *pami_transport = schedule.pami_transport;

    memset(schedule.next_dests, 0, sizeof(int) * OPTIQ_MAX_NUM_PATHS);

    build_next_dests(schedule.world_rank, schedule.next_dests, complete_paths);

    int world_rank = schedule.world_rank;

    bool isSource = false, isDest = false;

    schedule.local_jobs.clear();

    for (int i = 0; i < complete_paths.size(); i++) 
    {
	if (complete_paths[i]->arcs.back().v == world_rank) {
	    isDest = true;
	}

	if (complete_paths[i]->arcs.front().u == world_rank) {
	    isSource = true;

	    /*Check if the job is already existing*/
	    bool existed = false;
	    for (int j = 0; j < schedule.local_jobs.size(); j++)
	    {
		if (schedule.local_jobs[j].dest_rank == complete_paths[i]->arcs.back().v)
		{
		    schedule.local_jobs[j].paths.push_back(complete_paths[i]);
		    existed = true;
		    break;
		}
	    }

	    if (!existed) 
	    {
		struct optiq_job new_job;

		new_job.source_rank = world_rank;
		new_job.dest_rank = complete_paths[i]->arcs.back().v;
		new_job.paths.push_back(complete_paths[i]);
		new_job.buf_offset = 0;
		new_job.last_path_index = 0;

		schedule.local_jobs.push_back(new_job);
	    }
	}
    }

    schedule.isDest = isDest;
    schedule.isSource = isSource;
}

void optiq_schedule_print_jobs(struct optiq_schedule &schedule)
{
    std::vector<struct optiq_job> jobs = schedule.local_jobs;
    int world_rank = schedule.world_rank;

    printf("Rank %d has %ld jobs\n", world_rank, jobs.size());

    for (int i = 0; i < jobs.size(); i++)
    {
	printf("Rank %d job_id = %d source = %d dest = %d\n", world_rank, jobs[i].job_id, jobs[i].source_rank, jobs[i].dest_rank);
	for (int j = 0; j < jobs[i].paths.size(); j++)
	{
	    printf("Rank %d job_id = %d #paths = %ld path_id = %d flow = %d\n", world_rank, jobs[i].job_id, jobs[i].paths.size(), jobs[i].paths[j]->path_id, jobs[i].paths[j]->flow);
	}
    }
}


void optiq_schedule_mem_destroy(struct optiq_schedule &schedule, struct optiq_pami_transport *pami_transport)
{
    size_t bytes;

    pami_result_t result;

    result = PAMI_Memregion_destroy (pami_transport->context, &schedule.send_mr.mr);
    if (result != PAMI_SUCCESS)
    {
	printf("Destroy send_mr : No success\n");
    }

    result = PAMI_Memregion_destroy (pami_transport->context, &schedule.recv_mr.mr);
    if (result != PAMI_SUCCESS)
    {
	printf("Destroy recv_mr : No success\n");
    }
}

/* Register memory for sending and receiving and assign mem region for local_jobs*/
void optiq_schedule_mem_reg (struct optiq_schedule &schedule, struct optiq_comm_mem &comm_mem, struct optiq_pami_transport *pami_transport)
{
    schedule.rdispls = comm_mem.rdispls;

    size_t bytes;

    pami_result_t result;

    /* Register memory for sending data */
    if (comm_mem.send_len > 0) 
    {
	result = PAMI_Memregion_create (pami_transport->context, comm_mem.send_buf, comm_mem.send_len, &bytes, &schedule.send_mr.mr);
	schedule.send_mr.offset = 0;

	if (result != PAMI_SUCCESS)
	{
	    printf("No success\n");
	}
	else if (bytes < comm_mem.send_len)
	{
	    printf("Registered less\n");
	}
    }

    /* Assign mem region for local jobs */
    int dest_rank;
    for (int i = 0; i < schedule.local_jobs.size(); i++)
    {
	dest_rank = schedule.local_jobs[i].dest_rank;
	schedule.local_jobs[i].send_mr = schedule.send_mr;
	schedule.local_jobs[i].send_mr.offset = comm_mem.sdispls[dest_rank];
    }

    /* Create memory region for receiving data*/
    if (comm_mem.recv_len > 0)
    {
	result = PAMI_Memregion_create (pami_transport->context, comm_mem.recv_buf, comm_mem.recv_len, &bytes, &schedule.recv_mr.mr);
	schedule.recv_mr.offset = 0;

	if (result != PAMI_SUCCESS)
	{
	    printf("No success\n");
	}
	else if (bytes < comm_mem.recv_len)
	{
	    printf("Registered less\n");
	}
    }
}

void optiq_schedule_set(struct optiq_schedule &schedule, int num_jobs, int world_size)
{
    schedule.remaining_jobs = num_jobs;
    schedule.expecting_length = schedule.recv_len;
    schedule.sent_bytes = 0;
    memset (schedule.recv_bytes, 0, sizeof (int) * world_size);
}

void optiq_schedule_assign_job_demand(std::vector<struct optiq_job> &local_jobs, int nbytes)
{
    for (int i = 0; i < local_jobs.size(); i++) {
	local_jobs[i].buf_length = nbytes;
    }
}


int optiq_schedule_get_pair(int *sendcounts, std::vector<std::pair<int, std::vector<int> > > &source_dests)
{
    struct optiq_pami_transport *pami_transport = optiq_pami_transport_get();

    int world_size = pami_transport->size;
    int world_rank = pami_transport->rank;
    int num_dests = 0, num_sources = 0;

    for (int i = 0; i < world_size; i++) {
	if (sendcounts[i] != 0) {
	    num_dests++;
	}
    }

    int *dests = NULL;
    if (num_dests > 0) {
	dests = (int *) malloc (sizeof(int) * num_dests);
    }
    int j = 0;

    for (int i = 0; i < world_size; i++) {
	if (sendcounts[i] != 0) {
            dests[j] = i;
	    j++;
        }
    }

    int *all_num_dests = pami_transport->sched->all_num_dests;

    for (int i = 0; i < world_size; i++) {
	optiq_pami_send_immediate(pami_transport->context, BROADCAST_NUM_DESTS, 0, 0, &num_dests, sizeof(int), pami_transport->endpoints[i]);
    }

    while (pami_transport->sched->active_immsends > 0) {
	PAMI_Context_advance(pami_transport->context, 100);
    }

    /*for (int i = 0; i < world_size; i++) {
	printf("Rank %d num_dests = %d\n", i, all_num_dests[i]);
    }*/

    int **all_dests = (int **) malloc (sizeof(int *) * world_size);
    for (int i = 0; i < world_size; i++) {
	if (all_num_dests[i] > 0) {
	    all_dests[i] = (int *) malloc (sizeof (int) * all_num_dests[i]);
	    num_sources++;
	}
    }

    if (world_rank != 0) 
    {
	/* Send its dest to 0*/
	if (num_dests > 0) {
	    optiq_pami_transport_send(dests, num_dests * sizeof(int), 0);
	}

	/* Receive distribution from rank 0*/
	/*for (int i = 0; i < world_size; i++) {
	    if (all_num_dests[i] > 0) {
		optiq_pami_transport_recv(all_dests[i], all_num_dests[i] * sizeof(int), 0);
	    }
	}
	printf("Rank %d done receiving\n", world_rank);*/
    } 

    if (world_rank == 0) 
    {
	/* Copy its own dests */
	if (num_dests > 0) {
	    memcpy(all_dests[0], dests, sizeof(int) * num_dests);
	}

	/* Receive other dests */
	for (int i = 1; i < world_size; i++) {
	    if (all_num_dests[i] > 0) {
		optiq_pami_transport_recv(all_dests[i], all_num_dests[i] * sizeof(int), i);
	    }
	}

	/* Redistribute the dests list */
	/*for (int i = 0; i < world_size; i++) 
	{
	    if (all_num_dests[i] > 0) {
		for (int j = 1; j < world_size; j++) {
		    optiq_pami_transport_send(all_dests[i], all_num_dests[i] * sizeof(int), j);
		}
	    }
	}

	printf("Rank 0 done sending\n");*/
    }

    for (int i = 0; i < world_size; i++) 
    {
	if (all_num_dests[i] > 0) {
	    MPI_Bcast(all_dests[i], all_num_dests[i], MPI_INT, 0, MPI_COMM_WORLD);
	}
    }

    for (int i = 0; i < world_size; i++) 
    {
	std::vector<int> d;
	d.clear();

	for (int j = 0; j < all_num_dests[i]; j++) {
	    d.push_back(all_dests[i][j]);
	}

	if (d.size() > 0) {
	    std::pair<int, std::vector<int> > p = make_pair(i, d);
	    source_dests.push_back(p);
	}
    }

    return num_sources;
}


void optiq_mem_reg(void *buf, int *counts, int *displs, pami_memregion_t &mr)
{
    struct optiq_pami_transport *pami_transport = optiq_pami_transport_get();
    int world_size = pami_transport->size;

    int max_displ = 0, index = 0;
    for (int i = 0; i < world_size; i++) {
	if (max_displ < displs[i]) {
	    max_displ = displs[i];
	    index = i;
	}
    }

    int reg_size = max_displ + counts[index];
    size_t bytes = 0;

    pami_result_t result = PAMI_Memregion_create(pami_transport->context, buf, reg_size, &bytes, &mr);

    if (result != PAMI_SUCCESS) {
	printf("No success\n");
    }
    else if (bytes < reg_size) {
	printf("Registered less\n");
    }
}

void optiq_schedule_memory_register(void *sendbuf, int *sendcounts, int *sdispls, void *recvbuf, int *recvcounts, int *rdispls,  struct optiq_schedule *schedule)
{
    optiq_mem_reg(sendbuf, sendcounts, sdispls, schedule->send_mr.mr);
    schedule->send_mr.offset = 0;

    optiq_mem_reg(recvbuf, recvcounts, rdispls, schedule->recv_mr.mr);
    schedule->recv_mr.offset = 0;
}

/* Will do the follows:
 * 1. get the total amount of data send/recv
 * 2. create local job.
 * 3. register send/recv mem for the schedule
 * 4. collect all pairs of source-dests.
 *
 * */
void optiq_schedule_build (void *sendbuf, int *sendcounts, int *sdispls, void *recvbuf, int *recvcounts, int *rdispls)
{
    struct optiq_pami_transport *pami_transport = optiq_pami_transport_get();
    struct optiq_schedule *schedule = optiq_schedule_get();
    int world_rank = pami_transport->rank;

    /* Gather all pairs of source-dest and demand */
    std::vector<std::pair<int, std::vector<int> > > source_dests;
    int num_jobs = optiq_schedule_get_pair (sendcounts, source_dests);

    /* Search for paths */
    std::vector<struct path *> paths;
    optiq_algorithm_search_path (paths, source_dests, bfs);

    /*if (world_rank == 0) {
	optiq_path_print_paths(paths);
    }*/

    build_next_dests(world_rank, schedule->next_dests, paths);

    int recv_len = 0, send_len = 0;

    for (int i = 0; i < pami_transport->size; i++)
    {
	recv_len += recvcounts[i];
	send_len += sendcounts[i];
    }

    if (recv_len > 0) {
	schedule->isDest = true;
    }
    if (send_len > 0) {
	schedule->isSource = true;
    }

    /* Build a schedule to transfer data */
    schedule->rdispls = rdispls;
    schedule->recv_len = recv_len;

    /* Register memories */
    optiq_schedule_memory_register(sendbuf, sendcounts, sdispls, recvbuf, recvcounts, rdispls, schedule);

    /* Add local jobs */
    schedule->local_jobs.clear();
    for (int i = 0; i < paths.size(); i++)
    {
        if (paths[i]->arcs.front().u == world_rank) 
	{
            /*Check if the job is already existing*/
            bool existed = false;
            for (int j = 0; j < schedule->local_jobs.size(); j++)
            {
                if (schedule->local_jobs[j].dest_rank == paths[i]->arcs.back().v)
                {
                    schedule->local_jobs[j].paths.push_back (paths[i]);
                    existed = true;
                    break;
                }
            }

            if (!existed)
            {
                struct optiq_job new_job;

                new_job.source_rank = world_rank;
                new_job.dest_rank = paths[i]->arcs.back().v;
                new_job.paths.push_back(paths[i]);
                new_job.buf_offset = 0;
                new_job.last_path_index = 0;
		new_job.send_mr = schedule->send_mr;
		new_job.send_mr.offset = sdispls[new_job.dest_rank];
		new_job.buf_length = sendcounts[new_job.dest_rank];

                schedule->local_jobs.push_back(new_job);
            }
        }
    } 

    /* Split a message into chunk-size messages*/
    int chunk_size = 0;
    optiq_schedule_split_jobs_multipaths (pami_transport, schedule->local_jobs, chunk_size);

    /*Reset a few parameters*/
    optiq_schedule_set (*schedule, num_jobs, pami_transport->size);
}

/* Destroy the registered memory regions */
void optiq_schedule_destroy()
{
    struct optiq_pami_transport *pami_transport = optiq_pami_transport_get();
    struct optiq_schedule *schedule = optiq_schedule_get();
    
    optiq_schedule_mem_destroy(*schedule, pami_transport);
}
