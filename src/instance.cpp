#include "../include/instance.h"
#include <chrono>

Instance::Instance(Graph g, std::vector<Train> trains, uint32_t maxTimeSteps,
                   context *c)
    : graph(g), trains(trains), maxTimeSteps(maxTimeSteps), c(c), solver(*c),
      occupiedVars(trains.size(), std::vector<expr_vector>(maxTimeSteps, *c)),
      borderVars(*c), trainPaths(trains.size()) {
  // init boundary variables
  for (int i = 0; i < g.vertexCount; i++) {
    std::stringstream bVarName;
    bVarName << "border_" << i;
    borderVars.push_back(c->bool_const(bVarName.str().c_str()));
  }

  EdgeSet reachable;
  // init occupied variables and train paths
  for (size_t train = 0; train < trains.size(); train++) {
    trainPaths[train] = graph.paths(trains[train].start.stopEdge,
                                    trains[train].stops.back().stopEdge);
    for (size_t time = trains[train].start.arrivalTime; time < maxTimeSteps;
         time++) {
      occupiedVars[train][time] = expr_vector(*c);
      for (size_t edge = 0; edge < graph.edges.size(); edge++) {
        std::stringstream oVarName;
        oVarName << "occupies_" << train << "_" << time << "_" << edge;
        occupiedVars[train][time].push_back(
            c->bool_const(oVarName.str().c_str()));
      }
    }
  }
  computeConstraints();
}

Instance::Instance(const Instance &otherInstance)
    : graph(otherInstance.graph), trains(otherInstance.trains),
      maxTimeSteps(otherInstance.maxTimeSteps), c(otherInstance.c), solver(*c),
      occupiedVars(otherInstance.occupiedVars),
      borderVars(otherInstance.borderVars),
      trainPaths(otherInstance.trainPaths),
      constraints(otherInstance.constraints) {}

void Instance::printVSS() {
  model m = solver.get_model();
  for (auto iter : graph.ttds) {
    for (vssType vss : getVSS(iter.first)) {
      for (int node : vss) {
        std::cout << node << " ";
      }
      std::cout << std::endl;
    }
  }
}

void Instance::computeConstraints() {
  // these constraints always have to hold no matter the task
  for (Train &train : trains) {

    computeReachableConstraint(train);

    computeCollisionConstraint(train);
    for (Edge *edge : graph.edges) {
      computeVssConstraint(train, edge);
    }

    computeLengthConstraint(train);
  }
}

void Instance::computeLengthConstraint(const Train &train) {
  for (size_t time = train.start.arrivalTime; time < maxTimeSteps; time++) {
    expr_vector lengthHot(*c);
    for (pathType path : trainPaths[train.id]) {
      for (size_t startIndex = 0; startIndex < path.size(); startIndex++) {
        size_t lengthIndex = startIndex;
        int length = train.length;
        do {
          length -= path[lengthIndex]->length;
        } while (length > 0 && ++lengthIndex < path.size() - 1);

        expr_vector pathVars(*c);
        for (const Edge *edge : graph.edges) {
          bool inPath = false;
          for (size_t pathIndex = startIndex; pathIndex <= lengthIndex;
               pathIndex++) {
            if (edge == path[pathIndex]) {
              pathVars.push_back(occupiedVars[train.id][time][edge->id]);
              inPath = true;
              break;
            }
          }
          if (!inPath) {
            pathVars.push_back(!occupiedVars[train.id][time][edge->id]);
          }
        }
        lengthHot.push_back(mk_and(pathVars));
      }
    }
    expr_vector leftNetwork(*c);
    for (const Edge *edge : graph.edges)
      leftNetwork.push_back(!occupiedVars[train.id][time][edge->id]);

    lengthHot.push_back(mk_and(leftNetwork));
    constraints.push_back(mk_or(lengthHot));
  }
}

void Instance::computeReachableConstraint(const Train &train) {
  std::vector<EdgeSet> reachableFrom(graph.edges.size());
  for (pathType path : trainPaths[train.id]) {
    for (size_t startIndex = 0; startIndex < path.size() - 1; startIndex++) {
      size_t endIndex = graph.reachableInPath(path, startIndex, train.speed);
      for (size_t pathIndex = startIndex; pathIndex <= endIndex; pathIndex++) {
        reachableFrom[path[startIndex]->id].insert(path[pathIndex]);
      }
    }
  }
  for (size_t startEdge = 0; startEdge < reachableFrom.size(); startEdge++) {
    if (graph.edges[startEdge] == train.stops.back().stopEdge)
      continue;
    for (size_t time = train.start.arrivalTime; time < maxTimeSteps - 1;
         time++) {
      expr_vector pathVars(*c);
      for (const Edge *reachable : reachableFrom[startEdge]) {
        pathVars.push_back(occupiedVars[train.id][time + 1][reachable->id]);
      }
      constraints.push_back(
          implies(occupiedVars[train.id][time][graph.edges[startEdge]->id],
                  mk_or(pathVars)));
    }
  }
}

void Instance::computeCollisionConstraint(const Train &train) {
  for (pathType path : trainPaths[train.id]) {
    expr_vector otherTrainPosVars(*c);
    for (size_t startIndex = 0; startIndex < path.size(); startIndex++) {
      size_t endIndex = graph.reachableInPath(path, startIndex, train.speed);
      for (size_t pathIndex = startIndex; pathIndex <= endIndex; pathIndex++) {

        for (size_t time = train.start.arrivalTime; time < maxTimeSteps - 1;
             time++) {
          expr_vector otherPathVars(*c);

          for (const Train &otherTrain : trains) {
            if (train == otherTrain || otherTrain.start.arrivalTime > time)
              continue;
            for (size_t betweenIndex = startIndex; betweenIndex <= pathIndex;
                 betweenIndex++) {

              otherPathVars.push_back(
                  !occupiedVars[otherTrain.id][time][path[betweenIndex]->id]);
              otherPathVars.push_back(!occupiedVars[otherTrain.id][time + 1]
                                                   [path[betweenIndex]->id]);
            }
          }

          expr startVar = occupiedVars[train.id][time][path[startIndex]->id];
          expr endVar = occupiedVars[train.id][time + 1][path[pathIndex]->id];
          constraints.push_back(
              implies(startVar && endVar, mk_and(otherPathVars)));
        }
      }
    }
  }
}

void Instance::computeVssConstraint(const Train &train, const Edge *pos) {
  for (const Train &otherTrain : trains) {
    if (train == otherTrain)
      continue;

    std::vector<const Edge *> ttd = graph.getTTD(pos->ttd);
    size_t posIndex = 0;
    for (size_t i = 0; i < ttd.size(); i++) {
      if (ttd[i] == pos) {
        posIndex = i;
        break;
      }
    }

    int minRelevantTime =
        std::max(train.start.arrivalTime, otherTrain.start.arrivalTime);
    size_t currIndex = posIndex;
    const Edge *currPos = pos;
    expr_vector potentialBorderVars(*c);

    while (currIndex > 0) {
      const Edge *newPos = ttd[--currIndex];
      int leftNode = currPos->sharedVertex(newPos);
      currPos = newPos;
      potentialBorderVars.push_back(borderVars[leftNode]);
      for (size_t time = minRelevantTime; time < maxTimeSteps; time++) {
        expr trainOnPos = occupiedVars[train.id][time][pos->id];
        expr otherTrainOnOtherPos =
            occupiedVars[otherTrain.id][time][ttd[currIndex]->id];
        constraints.push_back(implies(trainOnPos && otherTrainOnOtherPos,
                                      mk_or(potentialBorderVars)));
      }
    }
    potentialBorderVars = expr_vector(*c);
    currIndex = posIndex;
    currPos = pos;
    while (currIndex < ttd.size() - 1) {
      const Edge *newPos = ttd[++currIndex];
      int rightNode = currPos->sharedVertex(newPos);
      currPos = newPos;
      potentialBorderVars.push_back(borderVars[rightNode]);
      for (size_t time = minRelevantTime; time < maxTimeSteps; time++) {
        expr trainOnPos = occupiedVars[train.id][time][pos->id];
        expr otherTrainOnOtherPos =
            occupiedVars[otherTrain.id][time][ttd[currIndex]->id];
        constraints.push_back(implies(trainOnPos && otherTrainOnOtherPos,
                                      mk_or(potentialBorderVars)));
      }
    }
    for (size_t time = minRelevantTime; time < maxTimeSteps; time++) {
      expr trainOnPos = occupiedVars[train.id][time][pos->id];
      expr otherTrainOnPos = occupiedVars[otherTrain.id][time][pos->id];
      constraints.push_back(!(trainOnPos && otherTrainOnPos));
    }
  }
}

vssListType Instance::getVSS(int ttd) {
  model m = solver.get_model();
  vssListType vss;
  vssType currVss;
  std::vector<const Edge *> ttdEdges = graph.getTTD(ttd);
  const Edge *currEdge = ttdEdges[0];
  int currNode =
      graph.isBoundary(currEdge->from) ? currEdge->from : currEdge->to;
  currVss.push_back(currNode);
  for (size_t i = 1; i < ttdEdges.size(); i++) {
    if (m.eval(borderVars[(currNode = currEdge->getOtherVertex(currNode))],
               false)
            .bool_value() == Z3_TRUE) {
      currVss.push_back(currNode);
      vss.push_back(currVss);
      currVss = vssType();
    }
    currVss.push_back(currNode);
    currEdge = ttdEdges[i];
  }
  currVss.push_back(currEdge->getOtherVertex(currNode));
  vss.push_back(currVss);
  return vss;
}

bool Instance::solve() {
  enforceConstraints();
  return solver.check() == sat;
}

void Instance::enforceConstraints() {
  for (expr constraint : constraints)
    solver.add(constraint);

  enforceStopConstraint();
  enforceBoundaryConstraints();
  minimizeVss();
}

void Instance::enforceBoundaryConstraints() {
  for (size_t vertex = 0; vertex < borderVars.size(); vertex++) {
    if (graph.isBoundary(vertex)) {
      solver.add(borderVars[vertex]);
    }
  }
}

void Instance::enforceStopConstraint() {
  for (const Train &train : trains) {
    solver.add(occupiedVars[train.id][train.start.arrivalTime]
                           [train.start.stopEdge->id]);
    for (Stop stop : train.stops) {
      expr_vector stopVars(*c);
      for (size_t time = train.start.arrivalTime; time < maxTimeSteps; time++) {
        stopVars.push_back(occupiedVars[train.id][time][stop.stopEdge->id]);
      }
      solver.add(mk_or(stopVars));
    }
  }
}

void Instance::printTrainRoute() {
  model m = solver.get_model();
  for (Train train : trains) {
    bool reachedGoal = false;
    for (size_t time = train.start.arrivalTime; time < maxTimeSteps; time++) {
      std::cout << time << ": "; 
      if(reachedGoal) continue;
      for (Edge *edge : graph.edges) {
        reachedGoal = edge == train.stops[-1].stopEdge;
        if (m.eval(occupiedVars[train.id][time][edge->id], false)
                .bool_value() == Z3_TRUE) {
          std::cout << "(" << edge->to << " " << edge->from << ") ";
        }
      }
      std::cout << std::endl;
    }
    std::cout << std::endl;
  }
}

void FixedVssInstance::enforceBoundaryConstraints() {
  for (size_t vertex = 0; vertex < borderVars.size(); vertex++) {

    if (graph.isBoundary(vertex)) {
      solver.add(borderVars[vertex]);
    } else {
      solver.add(!borderVars[vertex]);
    }
  }
}

void FixedScheduleInstance::enforceStopConstraint() {
  Instance::enforceStopConstraint();
}

void OptimizeInstance::enforceConstraints() {
  params order(*c);
  order.set("priority", "lex");
  solver.set(order);

  minimizeTrainSchedule(); // prioritize throughput
  Instance::enforceConstraints();
}

void Instance::minimizeVss() { solver.minimize(sumOfBoolVec(borderVars)); }

void OptimizeInstance::minimizeTrainSchedule() {
  expr_vector done(*c);
  for (size_t time = 0; time < maxTimeSteps; time++) {
    expr_vector trainDone(*c);
    for (const Train &train : trains) {
      if (time < train.start.arrivalTime)
        continue;
      expr_vector notOnTrack(*c);
      for (size_t edgeIndex = 0; edgeIndex < graph.edges.size(); edgeIndex++) {
        notOnTrack.push_back(!occupiedVars[train.id][time][edgeIndex]);
      }
      trainDone.push_back(mk_and(notOnTrack));
    }
    if (trainDone.size() > 0)
      done.push_back(sumOfBoolVec(trainDone));
  }
  solver.maximize(sum(done));
}

void OptimizeInstance::enforceStopConstraint() {
  for (const Train &train : trains) {
    solver.add(occupiedVars[train.id][train.start.arrivalTime]
                           [train.start.stopEdge->id]);
    for (Stop stop : train.stops) {
      expr_vector stopVars(*c);
      for (size_t time = train.start.arrivalTime; time < maxTimeSteps; time++) {
        stopVars.push_back(occupiedVars[train.id][time][stop.stopEdge->id]);
      }
      solver.add(mk_or(stopVars));
    }
  }
}

expr Instance::sumOfBoolVec(const expr_vector &bools) {
  expr_vector ints(*c);
  for (expr b : bools)
    ints.push_back(ite(b, c->int_val(1), c->int_val(0)));
  return sum(ints);
}

FixedScheduleInstance::FixedScheduleInstance(const Instance &otherInstance)
    : Instance(otherInstance) {}

OptimizeInstance::OptimizeInstance(const Instance &otherInstance)
    : Instance(otherInstance) {}

FixedVssInstance::FixedVssInstance(const Instance &otherInstance)
    : Instance(otherInstance) {}
