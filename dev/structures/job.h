#ifndef OPTIQ_JOB_H
#define OPTIQ_JOB_H

#include <vector>

#include "path.h"

struct optiq_memregion;

struct job {
    char name[256];
    int job_id;
    int source_id;
    int dest_id;
    int source_rank;
    int dest_rank;
    int demand;

    std::vector<struct path *> paths;
    std::vector<struct path *> kpaths;
};

bool optiq_job_add_one_path_under_load (struct job &ajob, int maxload, int** &load);

void optiq_job_read_from_file (std::vector<struct job> &jobs, std::vector<struct path*> &paths, char *filepath);

void optiq_job_read_and_select (std::vector<struct job> &jobs, std::vector<struct path*> &paths, char *filepath, int maxload, int size, int num_ranks_per_node);

bool optiq_jobs_read_from_file (std::vector<struct job> &jobs, std::vector<struct path*> &paths, char *filepath);

void optiq_job_write_to_file (std::vector<struct job> &jobs, char *filepath);

void optiq_job_print(std::vector<struct job> &jobs, int rank);

void optiq_job_print_jobs (std::vector<struct job> &jobs);

void optiq_job_map_jobs_to_source_dests (std::vector<struct job> &jobs, std::vector<std::pair<int, std::vector<int> > > &source_dests);

void optiq_job_remove_paths_over_maxload (std::vector<struct job> &jobs, int maxload, int size, int num_ranks_per_node);

void optiq_job_write_jobs_model_format (char *filekpath, int maxload, int size, int num_ranks_per_node, std::vector<int> *neighbors, int capacity, int demand, char *modeldat, int max_num_paths);

void optiq_jobs_convert_ids_to_ranks (std::vector<struct job> &jobs, std::vector<struct path *> &path_ids, int num_ranks_per_node);

#endif
