#include <stdlib.h>
#include <stdio.h>
#include <queue>
#include <limits.h>
#include <string.h>

#include <util.h>
#include <flow.h>

using namespace std;

int get_next_dest(struct optiq_flow &flow, int current_ep)
{
    for (int i = 0; i < flow.num_arcs; i++) {
        if (flow.arcs[i].ep1 == current_ep) {
            return flow.arcs[i].ep2;
        }
    }
    return -1;
}

void get_flows(int **rGraph, int num_vertices, struct optiq_job *job, int *flow_id)
{
    int source = job->source;
    int dest = job->dest;

    bool *visited = (bool *)malloc(sizeof(bool) * num_vertices);
    int *parent = (int *) malloc(sizeof(int) * num_vertices);
    int u, v, throughput, num_arcs = 0;
    queue<struct optiq_flow *> flows;
    while (bfs(num_vertices, visited, rGraph, source, dest, parent)) {
        /*Get the number of arcs and throughput of the flow*/
        throughput = INT_MAX;
        num_arcs = 0;
        for (v = dest; v != source; v = parent[v]) {
            u = parent[v];
            throughput = min(throughput, rGraph[u][v]);
            num_arcs++;
        }

        /*Create new flow*/
        struct optiq_flow *flow = (struct optiq_flow *)malloc(sizeof(struct optiq_flow));
        flow->throughput = throughput;
        flow->num_arcs = num_arcs;
        flow->arcs = (struct optiq_arc *)malloc(sizeof(struct optiq_arc) * num_arcs);
        flow->id = *flow_id;
        (*flow_id)++;

        /* Adding arcs for each flow and subtract used throughput */
        int ai = 0;
        for (v = dest; v != source; v = parent[v]) {
            u = parent[v];
            flow->arcs[ai].ep1 = u;
            flow->arcs[ai].ep2 = v;
            rGraph[u][v] -= throughput;
            ai++;
        }

        /*Add the flow to the job*/
        flows.push(flow);
        job->num_flows++;
    }

    job->flows = (struct optiq_flow**)malloc(sizeof(struct optiq_flow *) * job->num_flows);
    for (int i = 0; i < job->num_flows; i++) {
	job->flows[i] = flows.front();
	flows.pop();
    }
}

void read_flow_from_file(char *file_path, struct optiq_job **jobs, int num_jobs)
{
    FILE *file = fopen(file_path, "r");

    char buf[256];

    int num_vertices = 256;
    int **rGraph = (int **)malloc(sizeof(int *)*num_vertices);
    for (int i = 0; i < num_vertices; i++) {
        rGraph[i] = (int *)malloc(sizeof(int) * num_vertices);
        for (int j = 0; j < num_vertices; j++) {
            rGraph[i][j] = 0;
        }
    }

    int source = 0, dest = 171, ep1, ep2, bw;
    int flow_id = 0;
    int job_id = 0;
    *jobs = (struct optiq_job *)malloc(sizeof(struct optiq_job) * num_jobs);

    while (fgets(buf, 256, file)!=NULL) {
        trim(buf);
        /*printf("source = %d, dest = %d\n", source, dest);*/

        while(strlen(buf) > 0) {
            sscanf(buf, "%d %d %d", &ep1, &ep2, &bw);
            /*printf("%d %d %d\n", ep1, ep2, bw);*/
            rGraph[ep1][ep2] = bw;
            fgets(buf, 256, file);
            trim(buf);
        }

        /*Now we have the matrix, check what connects to what*/
        (*jobs)[job_id].id = job_id;
        (*jobs)[job_id].source = source;
        (*jobs)[job_id].dest = dest;
        (*jobs)[job_id].num_flows = 0;

        get_flows(rGraph, num_vertices, &(*jobs)[job_id], &flow_id);
        job_id++;

        for (int i = 0; i < num_vertices; i++) {
            for (int j = 0; j < num_vertices; j++) {
                rGraph[i][j] = 0;
            }
        }

        source++;
        dest++;
    }

    fclose(file);
    for (int i = 0; i < num_vertices; i++) {
        free(rGraph[i]);
    }
    free(rGraph);
}
