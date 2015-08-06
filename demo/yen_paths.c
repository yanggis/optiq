/*
 * This is an example of code to generate k shortest paths using Yen's algorithm. There is one output file for each pair of communication with maximum k paths. After paths generated, one can use collect.sh to gather paths of pairs of the same job/test id into one single file for heu1/heu2/opt_model
 * */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <vector>

#include <mpi.h>

#include "yen.h"
#include "topology.h"
#include "job.h"
#include "util.h"
#include "patterns.h"
#include "yen_gen_basic.h"

int main(int argc, char **argv)
{
    MPI_Init (&argc, &argv);
    int rank;
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);

    struct optiq_topology *topo = (struct optiq_topology *) malloc (sizeof (struct optiq_topology));
    int num_dims = 5;
    int psize[5];

    psize[0] = atoi (argv[1]);
    psize[1] = atoi (argv[2]);
    psize[2] = atoi (argv[3]);
    psize[3] = atoi (argv[4]);
    psize[4] = atoi (argv[5]);

    int numpaths = atoi (argv[6]);

    int demand = atoi (argv[7]) * 1024;

    mintestid = atoi (argv[8]);
    maxtestid = atoi (argv[9]);

    optiq_topology_init_with_params(num_dims, psize, topo);
    topo->num_ranks_per_node = atoi (argv[10]);

    char graphFilePath[] = "graph";

    if (rank == 0)
    {
	optiq_topology_write_graph (topo, 1, graphFilePath);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    gen_91_cases (topo, demand, graphFilePath, numpaths);
}