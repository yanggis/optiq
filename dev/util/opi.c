#include "opi.h"
#include <mpi.h>
#include <stdlib.h>
#include <limits.h>
#include <algorithm>
#include <vector>

struct optiq_performance_index opi, max_opi;
struct optiq_debug_print odp;

void optiq_opi_init()
{
    int size, rank;
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &size);

    if (rank == 0)
    {
        max_opi.all_numcopies = (int *) calloc (1, sizeof(int) * size);
        max_opi.all_numrputs = (int *) calloc (1, sizeof(int) * size);

        max_opi.all_link_loads = (int *) calloc (1, sizeof(int) * size * 9);
    }

    opi.load_stat = NULL;

    optiq_opi_clear();
}

struct optiq_performance_index * optiq_opi_get()
{
    return &opi;
}

void optiq_opi_collect()
{
    MPI_Reduce (&opi.transfer_time, &max_opi.transfer_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Reduce (&opi.notification_done_time, &max_opi.notification_done_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Reduce (&opi.sendimm_time, &max_opi.sendimm_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Reduce (&opi.local_mem_req_time, &max_opi.local_mem_req_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Reduce (&opi.total_mem_req_time, &max_opi.total_mem_req_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Reduce (&opi.recv_len, &max_opi.recv_len, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Reduce (&opi.matching_procesing_header_mr_response_time, &max_opi.matching_procesing_header_mr_response_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Reduce (&opi.context_advance_time, &max_opi.context_advance_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Reduce (&opi.post_rput_time, &max_opi.post_rput_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Reduce (&opi.check_complete_rput_time, &max_opi.check_complete_rput_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Reduce (&opi.get_header_time, &max_opi.get_header_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    int size, rank;
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &size);

    int link_loads[9] = {}, in = 0;
    std::map<int, int>::iterator it;

    for (it  = opi.link_loads.begin(); it != opi.link_loads.end(); it++)
    {
	link_loads[in] = it->second;
    }

    if (rank == 0)
    {
	memset (max_opi.all_numcopies, 0, sizeof(int) * size);
	memset (max_opi.all_numrputs, 0, sizeof(int) * size);
	memset (max_opi.all_link_loads, 0, sizeof(int) * size * 9);
    }

    MPI_Gather (&opi.numcopies, 1, MPI_INT, max_opi.all_numcopies, 1, MPI_INT, 0, MPI_COMM_WORLD);

    MPI_Gather (&opi.numrputs, 1, MPI_INT, max_opi.all_numrputs, 1, MPI_INT, 0, MPI_COMM_WORLD);

    MPI_Gather (link_loads, 9, MPI_INT, max_opi.all_link_loads, 9, MPI_INT, 0, MPI_COMM_WORLD);
}

void optiq_opi_print_perf()
{
    int size, rank;
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &size);

    if (rank == 0)
    {
	int mincopies = 0, maxcopies = 0, medcopies = 0, total_numcopies;
	int minrputs = 0, maxrputs = 0, medrputs = 0, total_rputs = 0;
	int minlinkloads = 0, maxlinkloads = 0, medlinkloads = 0, total_linkloads = 0;
	double avgcopies = 0, avglinkloads = 0, avgrputs = 0;

	std::vector<int> copies (max_opi.all_numcopies, max_opi.all_numcopies + size);
	std::sort (copies.begin(), copies.end());

	std::vector<int> rputs (max_opi.all_numrputs, max_opi.all_numrputs + size);
	std::sort (rputs.begin(), rputs.end());

	std::vector<int> linkloads (max_opi.all_link_loads, max_opi.all_link_loads + size * 9);
	std::sort (linkloads.begin(), linkloads.end());

	int ncpy = 0, nrput = 0, nlinks = 0;
	maxcopies = copies[size - 1];
	maxrputs = rputs[size - 1];
	maxlinkloads = linkloads[size - 1];

	for (int i = size; i >= 0; i--)
	{
	    if (copies[i] != 0)
	    {
		total_numcopies += copies[i];
		mincopies = copies[i];
		ncpy++;
	    }

	    if (rputs[i] != 0)
	    {
		total_rputs += rputs[i];
		minrputs = rputs[i];
		nrput++;
	    }
	}

	for (int i = size * 9; i >= 0; i--)
	{
	    if (linkloads[i] != 0)
	    {
		total_linkloads += linkloads[i];
		minlinkloads = linkloads[i];
		nlinks++;
	    }
	}

	medcopies = copies[ncpy/2];
	avgcopies = (double) total_numcopies / ncpy;

	medrputs = rputs[nrput/2];
	avgrputs = (double) total_rputs / nrput;

	medlinkloads = linkloads[nlinks/2];
	avglinkloads = (double) total_linkloads / nlinks;

	printf(" %d %d %8.2f %d  %d %d %8.2f %d  %d %d %8.2f %d \n", maxcopies, mincopies, avgcopies, medcopies, maxrputs, minrputs, avgrputs, medrputs, maxlinkloads, minlinkloads, avglinkloads, medlinkloads);
	
	/* Print path load stat */
	optiq_path_print_load_stat(opi.load_stat);

	/* Print actual link load - data size pass through a link*/
	for (int i = linkloads.size() - 1; i >= 0 && linkloads[i] > 0; i--)
	{
	    printf("link load = %d\n", linkloads[i]);
	}

	printf("\n");
    }
}

void optiq_opi_print()
{
    double max_time =  max_opi.transfer_time / opi.iters;
    double bw = (double) max_opi.recv_len / max_time / 1024 / 1024 * 1e6;

    if (max_opi.recv_len < 1024) {
	printf(" %ld %8.4f %8.4f ", max_opi.recv_len, max_time, bw);
    }
    else if (max_opi.recv_len < 1024 * 1024) {
	printf(" %ld %8.4f %8.4f ", max_opi.recv_len/1024, max_time, bw);
    }
    else {
	printf(" %ld %8.4f %8.4f ", max_opi.recv_len/1024/1024, max_time, bw);
    }

    if (odp.print_elapsed_time)
    {
	printf("context_advance_time time is %8.4f\n", max_opi.context_advance_time);
	printf("matching_procesing_header_mr_response_time time is %8.4f\n", max_opi.matching_procesing_header_mr_response_time);
	printf("get_header_time time is %8.4f\n", max_opi.get_header_time);
	printf("post_rput_time time is %8.4f\n", max_opi.post_rput_time);
	printf("check_complete_rput_time time is %8.4f\n", max_opi.check_complete_rput_time);
	printf("notification done time is %8.4f\n", max_opi.notification_done_time);
	printf("send_immediate time is %8.4f\n", max_opi.sendimm_time);
	printf("local mem req time is %8.4f\n", max_opi.local_mem_req_time);
	printf("total mem req time is %8.4f\n", max_opi.total_mem_req_time);
    }
}

void optiq_opi_clear()
{
    for (int i = 0; i < opi.paths.size(); i++)
    {
	if (opi.paths[i] != NULL) {
	    free(opi.paths[i]);
	}
    }
    opi.paths.clear();

    opi.recv_len = 0;
    opi.sendimm_time  = 0;
    opi.notification_done_time = 0;
    opi.transfer_time = 0;
    opi.build_path_time = 0;
    opi.context_advance_time = 0;
    opi.matching_procesing_header_mr_response_time = 0;
    opi.get_header_time = 0;
    opi.post_rput_time = 0;
    opi.check_complete_rput_time = 0;
    opi.timestamps.clear();
    opi.total_mem_req_time = 0;
    opi.local_mem_req_time = 0;

    odp.print_path_id = false;
    odp.print_path_rank = false;
    odp.print_rput_msg = false;
    odp.print_debug_msg = false;
    odp.print_timestamp = false;
    odp.print_reduced_paths = false;
    odp.print_local_jobs = false;
    odp.print_sourcedests_id = false;
    odp.print_sourcedests_rank = false;
    odp.print_mem_reg_msg = false;
    odp.print_mem_exchange_status = false;
    odp.print_pami_transport_status = false;
    odp.print_rput_rdone_notify_msg = false;
    odp.print_recv_rput_done_msg = false;
    odp.print_mem_adv_exchange_msg = false;
    odp.print_mem_avail = false;
    odp.test_mpi_perf = true;
    odp.print_notify_list = false;
    odp.print_elapsed_time = false;
    odp.print_job = false;
    odp.print_done_status = false;
    odp.print_transport_perf = false;
    odp.collect_transport_perf = false;

    odp.collect_timestamp = false;

    opi.numcopies = 0;
    opi.numrputs = 0;
    opi.link_loads.clear();

    if (opi.load_stat != NULL) 
    {
	free(opi.load_stat);
	opi.load_stat = NULL;
    }
}

void optiq_opi_timestamp_print(int rank)
{
    if (opi.timestamps.size() == 0) {
	return;
    }

    timeval t0 = opi.timestamps[0].tv;
    timeval t1;
    double t = 0;
    int eventid;
    int eventtype;

    for (int i = 1; i < opi.timestamps.size(); i++)
    {
	t1 = opi.timestamps[i].tv;
	eventid = opi.timestamps[i].eventid;
	eventtype = opi.timestamps[i].eventtype;

	t = (t1.tv_sec - t0.tv_sec) * 1e6 + (t1.tv_usec - t0.tv_usec);
	printf("rank = %d %d %d %8.4f\n", rank, eventtype, eventid, t);
    }
}
