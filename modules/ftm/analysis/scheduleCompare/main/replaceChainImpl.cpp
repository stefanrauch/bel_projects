#include "replaceChainImpl.h"
#include "configuration.h"

int replaceChain(ScheduleGraph& graph1, configuration& config) {
  ReplaceChain object = ReplaceChain(graph1, config);
  int result = object.replaceChainLoop();
  object.outputGraph();
  return result;
}

bool ReplaceChain::findStartOfChain() {
  // result = true means: found start of a chain and this->startOfChain
  // is the VertexNum of the start.
  bool result = false;
  BOOST_FOREACH (VertexDescriptor descriptor, vertices(*g)) {
    VertexNum v = boost::get(id, descriptor);
    //~ std::cout << "descriptor: " << descriptor << ", id: " << v << std::endl;
    result = getStartOfChain(v, v);
    if (result) {
      break;
    }
  }
  return result;
}

bool ReplaceChain::getStartOfChain(VertexNum v, VertexNum first) {
  // checks that v is start of a chain.
  // If yes, return true and set this->startOfChain.
  // If no but v is in a chain, call this method recursively with p(v).
  // If no but v is not in chain, return false.
  bool result = false;
  if ((boost::in_degree(v, *g) == 0 && boost::out_degree(v, *g) == 0) || (boost::in_degree(v, *g) > 1 && boost::out_degree(v, *g) > 1)){
    // v is not in a chain, proceed with next vertex in loop (return false).
    if (c->superverbose) {
      std::cout << "1 v: " << v << std::endl;
    }
  } else if (boost::in_degree(v, *g) == 1 && boost::out_degree(v, *g) == 0) {
    // check p(v). If p(v) is in the chain, proceed this test with p(v).
    // Otherwise next vertex in loop (return false).
    if (c->superverbose) {
      std::cout << "2 v: " << v << std::endl;
    }
    VertexNum p = predecessor(v);
    if (boost::in_degree(p, *g) <= 1 && boost::out_degree(p, *g) <= 1) {
      result = getStartOfChain(p, first);
    } else {
      result = false;
    }
  } else if (boost::in_degree(v, *g) == 0 && boost::out_degree(v, *g) == 1) {
    // v is start of chain.
    if (c->superverbose) {
      std::cout << "3 v: " << v << std::endl;
    }
    result = true;
    startOfChain = boost::get(id, v);
  } else if (boost::in_degree(v, *g) == 1 && boost::out_degree(v, *g) == 1) {
    // check p(v). If p(v) is in this chain, proceed this test with p(v).
    // if p(v) == first, we are in a cycle. Use v as startOfChain.
    // Otherwise v is start of chain.
    if (c->superverbose) {
      std::cout << "4 v: " << v << std::endl;
    }
    VertexNum p = predecessor(v);
    if (predecessor(p) == v) {
      // we have a two-vertex cycle, not a start of a chain to replace.
      result = false;
    } else if (p != first) {
      result = true;
      startOfChain = first;
    } else if (boost::in_degree(p, *g) <= 1 && boost::out_degree(p, *g) <= 1) {
      result = getStartOfChain(p, first);
    } else {
      result = true;
      startOfChain = v;
    }
  }
  if (c->verbose) {
    std::cout << "getStartOfChain: " << result << ", startOfChain:" << startOfChain << ", v: " << v << ", first: " << first << std::endl;
  }
  return result;
}

VertexNum ReplaceChain::predecessor(VertexNum v) {
  VertexNum predecessor = ULONG_MAX;
  if (boost::in_degree(v, *g) == 1) {
    ScheduleGraph::in_edge_iterator in_begin, in_end;
    boost::tie(in_begin, in_end) = boost::in_edges(v, *g);
    VertexDescriptor source1 = source(*in_begin, *g);
    predecessor = boost::get(id, source1);
  }
  return predecessor;
}

VertexNum ReplaceChain::successor(VertexNum v) {
  VertexNum successor = ULONG_MAX;
  if (boost::out_degree(v, *g) == 1) {
    ScheduleGraph::out_edge_iterator out_begin, out_end;
    boost::tie(out_begin, out_end) = boost::out_edges(v, *g);
    VertexDescriptor target1 = target(*out_begin, *g);
    successor = boost::get(id, target1);
  }
  //~ std::cout << "successor v:" << v << ", s:" << successor << ", out degree: " << boost::out_degree(v, *g) << ", size: " << chain.size() << std::endl;
  return successor;
}

void ReplaceChain::getBeforeEdge(VertexNum v) {
  ScheduleGraph::in_edge_iterator inBegin, inEnd;
  boost::tie(inBegin, inEnd) = boost::in_edges(v, *g);
  //~ std::cout << "getBeforeEdge edge: " << *inBegin << ", " << *inEnd << std::endl;
  beforeEdgeOld = *inBegin;
}

void ReplaceChain::getAfterEdge(VertexNum v) {
  ScheduleGraph::out_edge_iterator outBegin, outEnd;
  boost::tie(outBegin, outEnd) = boost::out_edges(v, *g);
  //~ std::cout << "getAfterEdge edge: " << *outBegin << ", " << *outEnd << std::endl;
  afterEdgeOld = *outBegin;
}

bool ReplaceChain::checkToReplace(VertexNum v) {
  // this method walks through the chain starting with startOfChain.
  bool result = true;
  if (chain.size() == 0) {
    // v may be the first vertex of a chain, check successor.
    VertexNum s = successor(v);
    if (c->superverbose) {
      std::cout << "1 checkToReplace v:" << v << ", s:" << s << ", size: " << chain.size() << std::endl;
    }
    if (s != ULONG_MAX && boost::in_degree(s, *g) == 1 && boost::out_degree(s, *g) <= 1) {
      // v is the first vertex of the chain.
      newName = (*g)[v].name;
      newLabel = (*g)[v].label;
      chain.insert(v);
      if (c->superverbose) {
        printChain("2 checkToReplace chain " + std::to_string(chain.size()) + ":");
      }
      result = checkToReplace(s);
    } else {
      result = false;
    }
  } else {
    // v may be the second or further vertex of the chain. Check this vertex.
    // test that v is not already in the chain. This may happen with cycles.
    if (chain.count(v) == 0 && boost::in_degree(v, *g) == 1 && boost::out_degree(v, *g) <= 1) {
      if (c->superverbose) {
        printChain("3 checkToReplace chain " + std::to_string(chain.size()) + ":");
      }
      VertexNum s = successor(v);
      if (c->superverbose) {
        std::cout << "4 checkToReplace v:" << v << ", s:" << s << ", size: " << chain.size() << std::endl;
      }
      // if the successor is startOfChain, return. Otherwise, insert vertex into chain.
      if (s == startOfChain) {
        result = false;
      } else {
        newName = newName + "\n" + (*g)[v].name;
        if ((*g)[v].label.size() > 0) {
          newLabel = newLabel + "\n" + (*g)[v].label;
        }
        chain.insert(v);
        if (s != ULONG_MAX) {
          result = checkToReplace(s);
        } else {
          result = true;
        }
      }
    } else {
      result = false;
    }
  }
  return result;
}

void ReplaceChain::createVertexAndEdges(VertexNum v) {
  chainStatus("createVertexAndEdges", std::cout);
  if (v == startOfChain) {
    // create a new vertex n and copy properties from v.
    newVertexNum = createVertexProperties(v);
    // if there is a predecessor, create an edge to it.
    VertexNum p = predecessor(v);
    if (c->superverbose) {
      std::cout << "0 createVertexAndEdges v:" << v << ", p " << p << std::endl;
    }
    if (p != ULONG_MAX) {
      beforeEdge = createEdgeProperties(p, v, newVertexNum, true);
    }
  }
  // if there is a successor of v and v is the last vertex in the chain, create an edge to it.
  VertexNum s = successor(v);
  if (c->superverbose) {
    std::cout << "1 createVertexAndEdges v:" << v << ", s " << s << std::endl;
  }
  if (s != ULONG_MAX && chain.count(s) == 0) {
    afterEdge = createEdgeProperties(newVertexNum, v, s, false);
    (*g)[newVertexNum].name = (*g)[newVertexNum].name + "\n...\n" + (*g)[v].name;
    (*g)[newVertexNum].label = (*g)[newVertexNum].label + "\n...\n" + ((*g)[v].label.size() > 0 ? (*g)[v].label : (*g)[v].name);
  }
}

VertexNum ReplaceChain::createVertexProperties(VertexNum v) {
  if (c->superverbose) {
    std::cout << "createVertexProperties v:" << v << std::endl;
  }
  ScheduleVertex newVertex = ScheduleVertex();
  // Concate names from vertices in chain and use this as name for new vertex.
  newVertex.name = (*g)[v].name;
  newVertex.label = (*g)[v].label;
  newVertex.pos = (*g)[v].pos;
  newVertex.height = (*g)[v].height;
  newVertex.width = (*g)[v].width;
  newVertex._draw_ = (*g)[v]._draw_;
  newVertex._hdraw_ = (*g)[v]._hdraw_;
  newVertex._ldraw_ = (*g)[v]._ldraw_;
  newVertex.style = (*g)[v].style;
  newVertex.penwidth = (*g)[v].penwidth;
  newVertex.shape = (*g)[v].shape;
  newVertex.fillcolor = (*g)[v].fillcolor;
  newVertex.color = (*g)[v].color;
  newVertex.pattern = (*g)[v].pattern;
  // if there is no label, copy the name.
  if (newVertex.label.size() == 0) {
    newVertex.label = newVertex.name;
  }
  // Add the new vertex to the graph.
  VertexNum newVertexNum1 = add_vertex(newVertex, *g);
  return newVertexNum1;
}

EdgeDescriptor* ReplaceChain::createEdgeProperties(VertexNum v1, VertexNum v2, VertexNum v3, bool flag) {
  if (c->superverbose) {
    std::cout << "createEdgeProperties v1:" << v1 << ", v2:" << v2 << ", v3:" << v3 << ", flag: " << (flag == true) << ", " << flag << ", " << newEdge.first << std::endl;
  }
  EdgeDescriptor* edge;
  if (flag) {
    // flag = true: v3 is new. Create (v1, v3), copy properties from (v1, v2).
    getBeforeEdge(v2);
    edge = &beforeEdgeOld;
  } else {
    // flag = false: v1 is new. Create (v1, v3), copy properties from (v2, v3).
    getAfterEdge(v2);
    edge = &afterEdgeOld;
  }
  newEdge = boost::add_edge(v1, v3, *g);
  chainStatus("createEdgeProperties", std::cout);
  (*g)[newEdge.first].pos = (*g)[*edge].pos;
  (*g)[newEdge.first]._draw_ = (*g)[*edge]._draw_;
  (*g)[newEdge.first]._hdraw_ = (*g)[*edge]._hdraw_;
  (*g)[newEdge.first].type = (*g)[*edge].type;
  (*g)[newEdge.first].color = (*g)[*edge].color;
  return &(newEdge.first);
}

bool ReplaceChain::insertEdges() {
  //~ bool result = false;
  for (auto v: chain) {
    createVertexAndEdges(v);
  }
  if (chain.size() < 4) {
    //~ chainStatus("insertEdges", std::cout);
    (*g)[newVertexNum].name = newName;
    //~ chainStatus("insertEdges", std::cout);
    (*g)[newVertexNum].label = newLabel;
    //~ chainStatus("insertEdges", std::cout);
  }
  for (auto reverseIterator = chain.rbegin(); reverseIterator != chain.rend(); reverseIterator++) {
    boost::clear_vertex(*reverseIterator, *g);
    boost::remove_vertex(*reverseIterator, *g);
  }
  // prepare for the next chain.
  startOfChain = ULONG_MAX;
  chain.clear();
  counterReplacedChains++;
  return true;
}

bool ReplaceChain::replaceSingleChain() {
  bool result = false;
  if (findStartOfChain()) {
    if (c->verbose) {
      std::cout << "replaceSingleChain startOfChain: " << startOfChain << ", " << (*g)[startOfChain].name << std::endl;
    }
    checkToReplace(startOfChain);
    if (chain.size() > 0) {
      result = insertEdges();
    } else {
      result = false;
    }
  }
  return result;
}

int ReplaceChain::replaceChainLoop() {
  bool result = true;
  for (int i = 0; i < c->chainCount && result; i++) {
    if (c->verbose) {
      std::cout << "replaceChainLoop counterReplacedChains: " << counterReplacedChains << ", startOfChain: " << startOfChain << ", result:" << result << std::endl;
    }
    result = replaceSingleChain();
  }
  return result;
}

void ReplaceChain::outputGraph() {
  if (!c->silent) {
    std::cout << "Output to file: '" << c->outputFile << "', counter: " << counterReplacedChains << std::endl;
  }
  saveSchedule(c->outputFile, *g, *c);
}

void ReplaceChain::printChain(std::string title) {
  if (c->superverbose) {
    std::cout << title;
    for (auto e : chain) {
      std::cout << std::setw(6) << e;
    }
    std::cout << "." << std::endl;
  }
}

void ReplaceChain::chainStatus(std::string title, std::ostream& out) {
  if (c->superverbose) {
    out << title << std::endl;
    out << "startOfChain " << startOfChain
        << ", newVertexNum " << newVertexNum
        << ", beforeEdge " << *beforeEdge
        << ", beforeEdgeOld " << beforeEdgeOld
        << ", afterEdge " << *afterEdge
        << ", afterEdgeOld " << afterEdgeOld
        << ", newEdge " << newEdge.first << " " << newEdge.second << std::endl;
    out << "newName: '" << newName << "'" << std::endl;
    out << "newLabel: '" << newLabel << "'" << std::endl;
  }
}
