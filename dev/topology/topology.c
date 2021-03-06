#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <math.h>
#include <time.h>

#include "util.h"
#include "topology.h"

struct optiq_topology *topo = NULL;

/*
 * Will init the topology with:
 *  1. num_dims
 *  2. size
 *  3. num_nodes
 *  4. num_edges
 *  5. node coordinate
 *  6. neighbors's node ids
 *  7. torus
 *  8. routing order
 *  9. coords of all nodes.
 * */
void optiq_topology_init ()
{
    if (topo != NULL && topo->initialized) {
	return;
    }

    topo = (struct optiq_topology *) calloc (1, sizeof (struct optiq_topology));

    int num_dims = 5;
    optiq_topology_get_size_bgq(topo->size);
    optiq_topology_init_with_params(num_dims, topo->size, topo);

    topo->num_ranks_per_node = 1;
    topo->initialized = true;
}

void optiq_topology_init_with_params(int num_dims, int *size, struct optiq_topology *topo)
{
    topo->num_dims = num_dims;
    int num_nodes = 1;

    for (int i = 0; i < num_dims; i++) {
        num_nodes *= size[i];
	topo->size[i] = size[i];
    }

    topo->num_nodes = num_nodes;

    optiq_topology_get_coord(topo->coord);
   
    topo->neighbors = optiq_topology_get_all_nodes_neighbors(num_dims, size);

    topo->num_edges = 0;
    for (int i = 0; i < num_nodes; i++) {
	topo->num_edges += topo->neighbors[i].size();
    }

    optiq_topology_get_bridge_bgq(topo);

    optiq_topology_get_torus(topo->torus);

    optiq_topology_compute_routing_order_bgq(topo->num_dims, topo->size, topo->order);

    topo->all_coords = optiq_topology_get_all_coords (topo->num_dims, topo->size);
}

struct optiq_topology* optiq_topology_get()
{
    return topo;
}

void optiq_topology_print_basic(struct optiq_topology *topo)
{
    printf("num dimension = %d\n", topo->num_dims);

    printf("size: ");
    for (int i = 0; i < topo->num_dims; i++) {
        printf("%d ", topo->size[i]);
    }
    printf("\n");

    printf("num nodes = %d\n", topo->num_nodes);

    printf("num ranks per node = %d\n", topo->num_ranks_per_node);

    printf("num physical links = %d\n", topo->num_edges);

    printf("torus: ");
    for (int i = 0; i < topo->num_dims; i++) {
        printf("%d ", topo->torus[i]);
    }
    printf("\n");

    printf("order: ");
    for (int i = 0; i < topo->num_dims; i++) {
        printf("%d ", topo->order[i]);
    }
    printf("\n\n");
}

void optiq_topology_print(struct optiq_topology *topo)
{
    optiq_topology_print_basic(topo);

    printf("node id: coord[A B C D E]\n");
    for (int i = 0; i < topo->num_nodes; i++) {
	printf("%d %d %d %d %d %d\n", i, topo->all_coords[i][0], topo->all_coords[i][1],topo->all_coords[i][2],topo->all_coords[i][3],topo->all_coords[i][4]);
    }

    printf("node id: neighbors list:\n");
    for (int i = 0; i < topo->num_nodes; i++) {
        printf("%d :", i);
	for (int j = 0; j < topo->neighbors[i].size(); j++) {
	    printf("%d ", topo->neighbors[i][j]);
	}
	printf("\n");
    }
}

void optiq_topology_print_graph(struct optiq_topology *topo, int cost)
{
    for (int i = 0; i < topo->num_nodes; i++)
    {
        for (int j = 0; j < topo->neighbors[i].size(); j++) {
            printf("%d %d %d\n", i, topo->neighbors[i][j], cost);
        }
    }
}

void optiq_topology_get_size_bgq(int *size)
{
#ifdef __bgq__
    Personality_t pers;
    Kernel_GetPersonality(&pers, sizeof(pers));

    size[0] = pers.Network_Config.Anodes;
    size[1] = pers.Network_Config.Bnodes;
    size[2] = pers.Network_Config.Cnodes;
    size[3] = pers.Network_Config.Dnodes;
    size[4] = pers.Network_Config.Enodes;
#endif
}

int optiq_topology_get_node_id(int world_rank)
{
    return world_rank/topo->num_ranks_per_node;
}

int optiq_topology_get_random_rank (int node_id)
{
    srand(time(NULL));
    int r = rand() % topo->num_ranks_per_node;

    return (node_id * topo->num_ranks_per_node + r);
}

int optiq_topology_compute_node_id(int num_dims, int *size, int *coord)
{
    int node_id = coord[num_dims-1];
    int  pre_size = 1;

    for (int i = num_dims-2; i >= 0; i--) {
        for (int j = i+1; j < num_dims; j++) {
            pre_size *= size[j];
        }

        node_id += coord[i]*pre_size;
        pre_size = 1;
    }
    return node_id;
}

int optiq_compute_neighbors(int num_dims, int *size, int *coord, int *neighbors)
{
    int num_neighbors = 0;
    int nid = 0;

    for (int i = num_dims - 1; i >= 0; i--) {
        if (coord[i] - 1 >= 0) {
            coord[i]--;
            nid = optiq_topology_compute_node_id(num_dims, size, coord);
            if (optiq_check_existing(num_neighbors, neighbors, nid) != 1) {
                neighbors[num_neighbors] = nid;
                num_neighbors++;
            }
            coord[i]++;
        }
        if (coord[i] + 1 < size[i]) {
            coord[i]++;
            nid = optiq_topology_compute_node_id(num_dims, size, coord);
            if (optiq_check_existing(num_neighbors, neighbors, nid) != 1) {
                neighbors[num_neighbors] = nid;
                num_neighbors++;
            }
            coord[i]--;
        }

        /*Torus neighbors*/
        for (int i = num_dims - 1; i >= 0; i--) {
            if (coord[i] == 0) {
                coord[i] = size[i]-1;
                nid = optiq_topology_compute_node_id(num_dims, size, coord);
                if (optiq_check_existing(num_neighbors, neighbors, nid) != 1) {
                    neighbors[num_neighbors] = nid;
                    num_neighbors++;
                }
                coord[i] = 0;
            }

            if (coord[i] == size[i]-1) {
                coord[i] = 0;
                nid = optiq_topology_compute_node_id(num_dims, size, coord);
                if (optiq_check_existing(num_neighbors, neighbors, nid) != 1) {
                    neighbors[num_neighbors] = nid;
                    num_neighbors++;
                }
                coord[i] = size[i]-1;
            }
        }
    }
    return num_neighbors;
}

int** optiq_topology_get_all_coords (int num_dims, int *size)
{
    int num_nodes = 1;
    for (int i = 0; i < num_dims; i++) {
        num_nodes *= size[i];
    }

    int **all_coords = (int **)malloc(sizeof(int *) * num_nodes);
    for (int i = 0; i < num_nodes; i++) {
        all_coords[i] = (int *)malloc(sizeof(int) * num_dims);
        for (int j = 0; j < num_dims; j++) {
            all_coords[i][j] = 0;
        }
    }

    int coord[5], nid = 0;
    for (int ad = 0; ad < size[0]; ad++) {
        coord[0] = ad;
        for (int bd = 0; bd < size[1]; bd++) {
            coord[1] = bd;
            for (int cd = 0; cd < size[2]; cd++) {
                coord[2] = cd;
                for (int dd = 0; dd < size[3]; dd++) {
                    coord[3] = dd;
                    for (int ed = 0; ed < size[4]; ed++) {
                        coord[4] = ed;
                        nid = optiq_topology_compute_node_id(num_dims, size, coord);
                        for (int i = 0; i < num_dims; i++) {
                            all_coords[nid][i] = coord[i];
                        }
                    }
                }
            }
        }
    }

    return all_coords;
}

std::vector<int> * optiq_topology_get_all_nodes_neighbors(int num_dims, int *size)
{
    int num_nodes = 1;
    for (int i = 0; i < num_dims; i++) {
        num_nodes *= size[i];
    }

    std::vector<int> *all_nodes_neighbors = (std::vector<int> *) calloc(1, sizeof(std::vector<int>) * num_nodes);

    int coord[5], nid = 0, neighbors[10], num_neighbors = 0;

    for (int ad = 0; ad < size[0]; ad++) {
        coord[0] = ad;
        for (int bd = 0; bd < size[1]; bd++) {
            coord[1] = bd;
            for (int cd = 0; cd < size[2]; cd++) {
                coord[2] = cd;
                for (int dd = 0; dd < size[3]; dd++) {
                    coord[3] = dd;
                    for (int ed = 0; ed < size[4]; ed++) {
                        coord[4] = ed;

                        nid = optiq_topology_compute_node_id(num_dims, size, coord);
                        num_neighbors = optiq_compute_neighbors(num_dims, size, coord, neighbors);

			all_nodes_neighbors[nid].clear();
                        for(int i = 0; i < num_neighbors; i++) {
                            all_nodes_neighbors[nid].push_back(neighbors[i]);
                        }
                    }
                }
            }
        }
    }

    return all_nodes_neighbors;
}

void optiq_topology_get_torus(int *torus)
{
#ifdef __bgq__
    Personality_t pers;
    Kernel_GetPersonality(&pers, sizeof(pers));

    uint64_t Nflags = pers.Network_Config.NetFlags;
    if (Nflags & ND_ENABLE_TORUS_DIM_A) torus[0] = 1; else torus[0] = 0;
    if (Nflags & ND_ENABLE_TORUS_DIM_B) torus[1] = 1; else torus[1] = 0;
    if (Nflags & ND_ENABLE_TORUS_DIM_C) torus[2] = 1; else torus[2] = 0;
    if (Nflags & ND_ENABLE_TORUS_DIM_D) torus[3] = 1; else torus[3] = 0;
    if (Nflags & ND_ENABLE_TORUS_DIM_E) torus[4] = 1; else torus[4] = 0;
#endif
}

void optiq_topology_move_along_one_dimension_bgq(int num_dims, int *size, int *source, int routing_dimension, int num_hops, int direction, int **path)
{
    int dimension_value = source[routing_dimension];
    for (int i = 0; i < num_hops; i++) {
        for (int d = 0; d < num_dims; d++) {
            if (d != routing_dimension) {
                path[i][d] = source[d];
            } else {
                dimension_value = (dimension_value + direction + size[d]) % size[d];
                path[i][d] = dimension_value;
            }
        }
    }
}

void optiq_topology_route_along_dimension (int *scoord, int routing_dimension, int destcoord, struct path &p)
{
    struct optiq_topology *topo = optiq_topology_get();

    int *torus = topo->torus;
    int *size = topo->size;
    int num_dims = topo->num_dims;

    int source[5];
    int intermediate_node[5];

    for (int i = 0; i < 5; i++) {
	source[i] = scoord[i];
        intermediate_node[i] = scoord[i];
    }

    /* From low to high e.g 0 -> 2 */
    if (scoord[routing_dimension] < destcoord)
    {
	int distance = destcoord - scoord[routing_dimension];

	/* Along the way e.g  0 -> 2 */
	if (torus[routing_dimension] == 0 || 
	    distance < size[routing_dimension] / 2 || 
	    ( distance == size[routing_dimension] / 2 && scoord[routing_dimension] % 2 == 0 ))
	{
	    for (int i = scoord[routing_dimension] + 1; i <= destcoord; i++)
	    {
		intermediate_node[routing_dimension] = i;

		struct arc a;

		a.u = optiq_topology_compute_node_id (num_dims, size, source);
		a.v = optiq_topology_compute_node_id (num_dims, size, intermediate_node);

		p.arcs.push_back(a);

		source[routing_dimension] = intermediate_node[routing_dimension];
	    }
	}

	/*Through the torus way 0 -> 3 by 1 hop*/
	else
	{
	    /* Route to 0*/
	    for (int i = scoord[routing_dimension] - 1; i >= 0; i--) 
	    {
		intermediate_node[routing_dimension] = i;

		struct arc a;

                a.u = optiq_topology_compute_node_id (num_dims, size, source);
                a.v = optiq_topology_compute_node_id (num_dims, size, intermediate_node);

                p.arcs.push_back(a);

                source[routing_dimension] = intermediate_node[routing_dimension];
	    }

	    /* Route from 0 to top*/
	    intermediate_node[routing_dimension] = size[routing_dimension] - 1;

	    struct arc a;

            a.u = optiq_topology_compute_node_id (num_dims, size, source);
            a.v = optiq_topology_compute_node_id (num_dims, size, intermediate_node);

            p.arcs.push_back(a);

	    source[routing_dimension] = intermediate_node[routing_dimension];

	    /* From top down to dest*/
	    for (int i = size[routing_dimension] - 2; i >= destcoord; i--)
	    {
		intermediate_node[routing_dimension] = i;
		struct arc a;
        
		a.u = optiq_topology_compute_node_id (num_dims, size, source);
		a.v = optiq_topology_compute_node_id (num_dims, size, intermediate_node);

		p.arcs.push_back(a);

		source[routing_dimension] = intermediate_node[routing_dimension];
	    }
	}
    }
    /* From high to low e.g 1 -> 0*/
    else
    {
	int distance = scoord[routing_dimension] - destcoord;

	/* Along the way e.g  2 -> 0 */
	if (torus[routing_dimension] == 0 || 
	    distance < size[routing_dimension] / 2 || 
	    ( distance == size[routing_dimension] / 2 && scoord[routing_dimension] % 2 == 0 ))
	{
	    for (int i = scoord[routing_dimension] - 1; i >= destcoord; i--)
	    {
		intermediate_node[routing_dimension] = i;

		struct arc a;

		a.u = optiq_topology_compute_node_id (num_dims, size, source);
		a.v = optiq_topology_compute_node_id (num_dims, size, intermediate_node);

		p.arcs.push_back(a);

		source[routing_dimension] = intermediate_node[routing_dimension];
	    }
	}

	/*Through the torus way 3 -> 1 by 2 hops*/
	else
	{
	    /* Route to top */
	    for (int i = scoord[routing_dimension] + 1; i <= size[routing_dimension] - 1; i++) 
	    {
		intermediate_node[routing_dimension] = i;

		struct arc a;

                a.u = optiq_topology_compute_node_id (num_dims, size, source);
                a.v = optiq_topology_compute_node_id (num_dims, size, intermediate_node);

                p.arcs.push_back(a);

                source[routing_dimension] = intermediate_node[routing_dimension];
	    }

	    /* Route from top to 0*/
	    intermediate_node[routing_dimension] = 0;

	    struct arc a;

            a.u = optiq_topology_compute_node_id (num_dims, size, source);
            a.v = optiq_topology_compute_node_id (num_dims, size, intermediate_node);

            p.arcs.push_back(a);

	    source[routing_dimension] = intermediate_node[routing_dimension];

	    /* From 0 up to dest*/
	    for (int i = 1; i <= destcoord; i++)
	    {
		intermediate_node[routing_dimension] = i;
		struct arc a;
        
		a.u = optiq_topology_compute_node_id (num_dims, size, source);
		a.v = optiq_topology_compute_node_id (num_dims, size, intermediate_node);

		p.arcs.push_back(a);

		source[routing_dimension] = intermediate_node[routing_dimension];
	    }
	}
    }
}

void optiq_topolog_reconstruct_path (int source, int dest, struct path &p)
{
    struct optiq_topology *topo = optiq_topology_get();

    int *size = topo->size;
    int *torus = topo->torus;
    int *order = topo->order;
    int num_dims = topo->num_dims;

    int *scoord = topo->all_coords[source];
    int *dcoord = topo->all_coords[dest];

    int routing_dimension = 0, abs_hops = 0;

    int intermediate_node[5];

    for (int i = 0; i < 5; i++) {
	intermediate_node[i] = scoord[i];
    }

    /*printf("from %d [%d %d %d %d %d] to %d [%d %d %d %d %d]\n", source, scoord[0], scoord[1], scoord[2], scoord[3], scoord[4], dest, dcoord[0], dcoord[1], dcoord[2], dcoord[3], dcoord[4]);*/

    for (int i = 0; i < num_dims; i++)
    {
	routing_dimension = order[i];

	abs_hops = abs (dcoord[routing_dimension] - scoord[routing_dimension]);

        if (abs_hops == 0) {
            continue;
        }

	optiq_topology_route_along_dimension(intermediate_node, routing_dimension, dcoord[routing_dimension], p);

	intermediate_node[routing_dimension] = dcoord[routing_dimension];
    }

    /*optiq_path_print_path_coords (&p, topo->all_coords);*/
}

void optiq_topology_reconstruct_path_bgq(int num_dims, int *size, int *torus, int *order, int *source, int *dest, int **path)
{
    int immediate_node[5];

    /*Add source node*/
    for (int i = 0; i < num_dims; i++) {
        path[0][i] = source[i];
        immediate_node[i] = source[i];
    }

    /*Add intermedidate nodes*/
    int num_nodes = 1, direction = 0;
    int routing_dimension, num_hops;

    for (int i = 0; i < num_dims; i++) 
    {
        routing_dimension = order[i];
        num_hops = abs(dest[routing_dimension]-source[routing_dimension]);

        if (num_hops == 0) {
            continue;
        }

        direction = (dest[routing_dimension] - source[routing_dimension])/num_hops;

        /*If there is torus link, the direction may change*/
        if (torus[routing_dimension] == 1) 
	{
            if (num_hops > size[routing_dimension]/2) {
                direction *= -1;
            }

	    /* if source coordinate (the torus coordinate in the dimension under consideration) is even, route in plus direction for equi-distant routes, for odd -- route in minus direction */
	    if (num_hops == size[routing_dimension]/2 && source[routing_dimension] % 2 == 1) {
		direction *= -1;
	    }
        }

	if (direction == -1) {
	    num_hops = size[routing_dimension] - dest[routing_dimension] + source[routing_dimension];
	}

        optiq_topology_move_along_one_dimension_bgq(num_dims, size, immediate_node, routing_dimension, num_hops, direction, &path[num_nodes]);

        immediate_node[routing_dimension] = dest[routing_dimension];
        num_nodes += num_hops;
    }
}

void optiq_topology_compute_routing_order_bgq(int num_dims, int *size, int *order)
{
    int num_nodes = 1, dims[5];
    for (int i = 0; i < num_dims; i++) {
        num_nodes *= size[i];
        dims[i] = size[i];
    }

    int longest_dimension, length;
    for (int i = 0; i < num_dims; i++) {
        longest_dimension = i;
        length = dims[i];
        for (int j = 0; j < num_dims; j++) {
            if(dims[j] > length) {
                longest_dimension = j;
                length = dims[j];
            }
        }

        if ((longest_dimension == 0) && (dims[0] == dims[1]) && (num_nodes == 128 || num_nodes == 256)) {
            longest_dimension = 1;
        }

        order[i] = longest_dimension;
        dims[longest_dimension] = -1;
    }
}

void optiq_topology_print_all_arcs(int num_dims, int *size, double cap)
{
    int num_neighbors = 0;
    int neighbors[10];
    int coord[5];
    int nid;

    for (int ad = 0; ad < size[0]; ad++) {
        coord[0] = ad;
        for (int bd = 0; bd < size[1]; bd++) {
            coord[1] = bd;
            for (int cd = 0; cd < size[2]; cd++) {
                coord[2] = cd;
                for (int dd = 0; dd < size[3]; dd++) {
                    coord[3] = dd;
                    for (int ed = 0; ed < size[4]; ed++) {
                        coord[4] = ed;
                        num_neighbors = 0;
                        nid = optiq_topology_compute_node_id(num_dims, coord, size);
                        num_neighbors = optiq_compute_neighbors(num_dims, coord, size, neighbors);
                        for (int i = 0; i < num_neighbors; i++) {
                            if (cap < 0.0) {
                                printf("%d %d\n", nid, neighbors[i]);
                            }
                            else {
                                printf("%d %d %8.0f\n", nid, neighbors[i], cap);
                            }
                        }
                    }
                }
            }
        }
    }
}

int optiq_topology_get_coord(int *coord)
{
#ifdef __bgq__
    Personality_t pers;
    Kernel_GetPersonality(&pers, sizeof(pers));

    coord[0] = pers.Network_Config.Acoord;
    coord[1] = pers.Network_Config.Bcoord;
    coord[2] = pers.Network_Config.Ccoord;
    coord[3] = pers.Network_Config.Dcoord;
    coord[4] = pers.Network_Config.Ecoord;
#endif
}

void optiq_topology_finalize()
{
    for (int i = 0; i < topo->num_nodes; i++) {
	free(topo->all_coords[i]);
    }

    free(topo->all_coords);
    free (topo->neighbors);

    topo->finalized = true;
    topo->initialized = false;

    free (topo);
}

int optiq_topology_get_hop_distance_2nodes(int node1, int node2)
{
    struct optiq_topology *topo = optiq_topology_get();

    int *coord1 = topo->all_coords[node1];
    int *coord2 = topo->all_coords[node2];

    return optiq_compute_num_hops_with_torus (topo->num_dims, coord1, coord2, topo->torus, topo->size);
}

int optiq_topology_get_hop_distance(int rank1, int rank2)
{
    struct optiq_topology *topo = optiq_topology_get();

    if (topo == NULL) {
        optiq_topology_init();
    }

    int node1 = rank1 / topo->num_ranks_per_node;
    int node2 = rank2 / topo->num_ranks_per_node;

    return optiq_topology_get_hop_distance_2nodes (node1, node2);
}

int optiq_topology_max_distance_2sets (std::vector<std::pair<int, std::vector<int> > > &source_dests)
{
    struct optiq_topology *topo = optiq_topology_get();

    int max_distance = 0;

    for (int i = 0; i < source_dests.size(); i++)
    {
	for (int j = 0; j < source_dests[i].second.size(); j++)
	{
	    int *coord1 = topo->all_coords[source_dests[i].first];
	    int *coord2 = topo->all_coords[source_dests[i].second[j]];

	    int distance = optiq_compute_num_hops(topo->num_dims, coord1, coord2);

	    if (max_distance < distance) {
		max_distance = distance;
	    }
	}
    }

    return max_distance;
}

int optiq_topology_max_distance_2sets_with_torus (std::vector<std::pair<int, std::vector<int> > > &source_dests)
{
   struct optiq_topology *topo = optiq_topology_get();

    int max_distance = 0;

    for (int i = 0; i < source_dests.size(); i++)
    {
        for (int j = 0; j < source_dests[i].second.size(); j++)
        {
            int *coord1 = topo->all_coords[source_dests[i].first];
            int *coord2 = topo->all_coords[source_dests[i].second[j]];

            int distance = optiq_compute_num_hops_with_torus (topo->num_dims, coord1, coord2, topo->torus, topo->size);

            if (max_distance < distance) {
                max_distance = distance;
            }
        }
    }

    return max_distance;
}

bool optiq_path_compare_2paths (struct path *p1, int s1, int e1, struct path *p2, int s2, int e2)
{
    if (e1 - s1 != e2 - s2) {
	return false;
    }

    for (int i = 0; i < e1 - s1; i++)
    {
	if (p1->arcs[s1 + i].u != p2->arcs[s2 + i].u || p1->arcs[s1 + i].v != p2->arcs[s2 + i].v) {
	    return false;
	}
    }

    return true;
}

void optiq_topology_reduce_intermediate_nodes (std::vector<struct path *> paths, std::vector<struct path *> &reduced_paths)
{
    struct optiq_topology *topo = optiq_topology_get();

    for (int i = 0; i < paths.size(); i++)
    {
	int j = 0;
	struct path *np = (struct path *) calloc (1, sizeof(struct path));

	while (j < paths[i]->arcs.size())
	{
	    int source = paths[i]->arcs[j].u;
	    int dest = 0;
	    bool issubpath = true;
	    int k = j + 1;

	    while (k < paths[i]->arcs.size() && issubpath) 
	    {
		dest = paths[i]->arcs[k].v;
		struct path p;
		p.arcs.clear();
		optiq_topolog_reconstruct_path (source, dest, p);

		issubpath = optiq_path_compare_2paths (paths[i], j, k, &p, 0, p.arcs.size() - 1);

		if (issubpath) {
		    k++;
		}
	    }

	    struct arc a;
	    a.u = source;
	    a.v = paths[i]->arcs[k-1].v;

	    np->arcs.push_back(a);

	    j = k;
	}

	reduced_paths.push_back(np);
    }
}

void optiq_topology_write_graph(struct optiq_topology *topo, int cost, char *filePath)
{
    std::ofstream myfile;
    myfile.open (filePath);
    myfile << topo->num_nodes << "\n\n";

    for (int i = 0; i < topo->num_nodes; i++)
    {
        for (int j = 0; j < topo->neighbors[i].size(); j++)
        {
            myfile << i << " " << topo->neighbors[i][j] << " " << cost << "\n";
        }
    }

    myfile.close();
}

void optiq_topology_get_bridge_bgq(struct optiq_topology *topo)
{
    Personality_t personality;
    Kernel_GetPersonality(&personality, sizeof(personality));

    topo->bridge[0] = personality.Network_Config.cnBridge_A;
    topo->bridge[1] = personality.Network_Config.cnBridge_B;
    topo->bridge[2] = personality.Network_Config.cnBridge_C;
    topo->bridge[3] = personality.Network_Config.cnBridge_D;
    topo->bridge[4] = personality.Network_Config.cnBridge_E;

    int size[5];
    optiq_topology_get_size_bgq(size);

    topo->bridge_id = topo->bridge[4] + topo->bridge[3]*size[4] + topo->bridge[2]*size[3]*size[4] + topo->bridge[1]*size[2]*size[3]*size[4] + topo->bridge[0]*size[1]*size[2]*size[3]*size[4];
}

