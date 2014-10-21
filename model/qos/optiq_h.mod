set Nodes;
set Arcs within Nodes cross Nodes;
set Jobs;
set Classes;

param Delay {Arcs} default 0;
param Capacity {Arcs} >= 0 default Infinity;
param Source {Jobs};
param Destination {Jobs} default 0;
param Demand {Jobs} default 0;
param Class {Jobs};
param Weight {Classes} default 0;

var Flow {Jobs,Classes,Arcs} >= 0;
var Z >= 0;

var total_flow{(i,j) in Arcs} = sum {job in Jobs, c in Classes} Flow[job,c,i,j];
var total_flow_by_class{c in Classes, {i,j in Arcs}} = sum {job in Jobs} Flow[job,c,i,j];

maximize obj: Z;

subject to

zero_flow {job in Jobs, c in Classes, i in Nodes}:
sum{(i,j) in Arcs} Flow[job,c,i,j] - sum{(j,i) in Arcs} Flow[job,c,j,i] =
if (i == Source[job]) then Demand[job]*Z else if (i == Destination[job]) then -Demand[job]*Z else 0;



capacity {(i,j) in Arcs}:
total_flow[i,j] <= Capacity[i,j];

flow_by_class{c in Classes, (i,j) in Arcs}:
total_flow_by_class[c,i,j] <= Weight[c] * Capacity[i,j];