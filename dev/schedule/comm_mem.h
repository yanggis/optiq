#ifndef OPTIQ_COMM_MEM
#define OPTIQ_COMM_MEM

#include <utility>
#include <vector>

struct optiq_comm_mem {
    char *send_buf;
    int send_len;
    int *sendcounts;
    int *sdispls;

    char *recv_buf;
    int recv_len;
    int *recvcounts;
    int *rdispls;
};

int optiq_comm_mem_allocate (std::vector<std::pair<int, std::vector<int> > > &source_dests, int count, struct optiq_comm_mem &comm_mem, int rank, int world_size);

void optiq_comm_mem_delete(struct optiq_comm_mem &comm_mem);

#endif