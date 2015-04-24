#include "optiq.h"
#include "mpi_benchmark.h"

int main(int argc, char **argv)
{
    optiq_init(argc, argv);

    struct optiq_pami_transport *pami_transport = optiq_pami_transport_get();
    int world_size = pami_transport->size;
    int world_rank = pami_transport->rank;

    int *sendcounts = (int *)calloc(1, sizeof(int) * world_size);
    int *sdispls = (int *)calloc(1, sizeof(int) * world_size);

    int *rdispls = (int *) calloc(1, sizeof(int) * world_size);
    int *recvcounts = (int *) calloc(1, sizeof(int) * world_size);

    int send_bytes = 1024 * 1024;
    char *sendbuf = (char *) malloc (send_bytes);
    int recv_bytes = 1024 * 1024;
    char *recvbuf = (char *) calloc (1, recv_bytes);

    int send_rank = 0;
    int recv_rank = 1;

    if (world_rank < world_size/2) {
	recv_rank = world_rank + world_size/2;
	sendcounts[recv_rank] = send_bytes;

	for (int i = 0; i < send_bytes; i++) {
	    sendbuf[i] = i%128;
	}
    }

    if (world_rank >= world_size/2) {
	send_rank = world_rank - world_size/2;
	recvcounts[send_rank] = recv_bytes;
    }

    if (world_rank == 0) {
	printf("Start to benchmark optiq_alltoallv\n");
    }

    optiq_benchmark_mpi_alltoallv(sendbuf, sendcounts, sdispls, recvbuf, recvcounts, rdispls);

    for (int i = 16 * 1024; i < 1024 * 1024; i*=2)
    {
	schedule->chunk_size = i;

	optiq_alltoallv(sendbuf, sendcounts, sdispls, recvbuf, recvcounts, rdispls);

	opi.iters = 1;
	optiq_opi_collect();
	if (world_rank == 0) {
	    optiq_opi_print();
	}
    }

    if (world_rank == 0) {
        printf("Finished benchmarking optiq_alltoallv\n");
    }

    optiq_finalize();

    return 0;
}
