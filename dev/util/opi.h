#ifndef OPTIQ_PERFORMANCE_INDEX
#define OPTIQ_PERFORMANCE_INDEX

#include <sys/time.h>
#include <map>
#include <vector>

#include "path.h"

#define OPTIQ_EVENT_START 0

#define OPTIQ_EVENT_MEM_REQ 10
#define OPTIQ_EVENT_MEM_RES 11
#define OPTIQ_EVENT_RECV_MEM_REQ 12
#define OPTIQ_EVENT_RECV_MEM_RES 13

#define OPTIQ_EVENT_RPUT 20
#define OPTIQ_EVENT_RPUT_RDONE 21
#define OPTIQ_EVENT_RPUT_DONE_NOTIFY 22
#define OPTIQ_EVENT_RECV_RPUT_DONE 23

struct timestamp {
    int eventtype;
    int eventid;
    timeval tv;
};

struct optiq_stat {
    int max;
    int min;
    int med;
    double avg;
    double total;
};

struct optiq_performance_index {
    double transfer_time;
    double build_path_time;
    double context_advance_time;
    double notification_done_time;
    double sendimm_time;
    double matching_procesing_header_mr_response_time;
    double get_header_time;
    double post_rput_time;
    double check_complete_rput_time;
    double local_mem_req_time;
    double total_mem_req_time;

    int long recv_len;
    int iters;

    int test_id;
    char prefix[5];
    char name[256];
    int message_size;
    int chunk_size;
    struct optiq_stat load_link, load_path, hops, copies, numpaths, rputs;

    std::vector<struct path *> paths;
    std::vector<struct timestamp> timestamps;

    int numcopies;
    int *all_numcopies;
    std::map <int, int> link_loads;
    int *all_link_loads;
    int numrputs;
    int *all_numrputs;
    int *load_stat;
    std::vector<int> linkloads;
    std::map<int, int> hops_dist;
    std::map<int, int> copies_dist;

    std::map<int, int> path_hopbyte;
    std::map<int, int> path_copy;
};

struct optiq_debug_print {
    bool print_path_id;
    bool print_path_rank;
    bool print_rput_msg;
    bool print_debug_msg;
    bool print_timestamp;
    bool print_reduced_paths;
    bool print_local_jobs;
    bool print_sourcedests_id;
    bool print_sourcedests_rank;
    bool print_mem_reg_msg;
    bool print_mem_exchange_status;
    bool print_pami_transport_status;
    bool test_mpi_perf;
    bool print_elapsed_time;
    bool print_rput_rdone_notify_msg;
    bool print_recv_rput_done_msg;
    bool print_mem_adv_exchange_msg;
    bool print_notify_list;
    bool print_mem_avail;
    bool print_job;
    bool print_done_status;
    bool print_transport_perf;
    bool print_mpi_paths;

    bool collect_timestamp;
    bool collect_transport_perf;
};

extern "C" struct optiq_performance_index opi, max_opi;

extern "C" struct optiq_debug_print odp;

void optiq_opi_init();

struct optiq_performance_index * optiq_opi_get();

void optiq_opi_collect();

void optiq_opi_print();

void optiq_opi_print_path_hopbyte_copy_stat();

void optiq_opi_compute_stat();

void optiq_opi_clear();

void optiq_opi_timestamp_print(int rank);

#endif
