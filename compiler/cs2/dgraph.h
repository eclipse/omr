/*******************************************************************************
 * Copyright (c) 1996, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/***************************************************************************/
/*                                                                         */
/*  File name:  dgraph.h                                                   */
/*  Purpose:    Definition of the DirectedGraph template class.            */
/*                                                                         */
/***************************************************************************/

#ifndef CS2_DGRAPH_H
#define CS2_DGRAPH_H

// -----------------------------------------------------------------------------
// DirectedGraph
//
// A template class representing a simple directed graph.  The node types are
// assumed to be derived from 'DirectedGraphNode' and the edge types from
// 'DirectedGraphEdge'.
// -----------------------------------------------------------------------------

#include "cs2/tableof.h"
#include "cs2/listof.h"
#include "cs2/bitvectr.h"

namespace CS2 {

typedef uint32_t EdgeIndex;
typedef uint32_t NodeIndex;

class DirectedGraphNode {
  public:

  enum EdgeList {
    kSuccessorList = 1,
    kPredecessorList = 0
  };

  DirectedGraphNode() {
    fFirstEdge[0] = 0;
    fFirstEdge[1] = 0;
  }

  DirectedGraphNode (const DirectedGraphNode &inputNode) {
    *this = inputNode;
  }

  DirectedGraphNode &operator= (const DirectedGraphNode &inputNode) {
    fFirstEdge[0] = inputNode.fFirstEdge[0];
    fFirstEdge[1] = inputNode.fFirstEdge[1];
    return *this;
  }

  // Get/set the index of the first edge in the given chain
  EdgeIndex FirstEdge (bool direction) const {
    CS2Assert (direction == kSuccessorList || direction == kPredecessorList,
	    ("Invalid direction: %d", direction));
    return fFirstEdge[direction];
  }

  void SetFirstEdge (bool direction, EdgeIndex edgeIndex) {
    CS2Assert (direction == kSuccessorList || direction == kPredecessorList,
	    ("Invalid direction: %d", direction));
    fFirstEdge[direction] = edgeIndex;
  }

  // Get/set the index of the first successor edge
  EdgeIndex FirstSuccessor() const {
    return FirstEdge (kSuccessorList);
  }

  void SetFirstSuccessor (EdgeIndex edgeIndex) {
    SetFirstEdge (kSuccessorList, edgeIndex);
  }

  // Get/set the index of the first predecessor edge
  EdgeIndex FirstPredecessor() const {
    return FirstEdge (kPredecessorList);
  }

  void SetFirstPredecessor (EdgeIndex edgeIndex) {
    SetFirstEdge (kPredecessorList, edgeIndex);
  }

  // Dump routine
  template <class ostr>
    friend ostr &operator<< (ostr &out, const DirectedGraphNode &node) {
    out << "  FirstSucc: <" << node.FirstSuccessor() << ">"
	<< ", FirstPred: <" << node.FirstPredecessor() << ">"
	<< "\n";

    return out;
  }

  private:

  EdgeIndex fFirstEdge[2];
};

class DirectedGraphEdge {
  public:

  enum EdgeList {
    kSuccessorList = 1,
    kPredecessorList = 0
  };

  DirectedGraphEdge() {
    fNodes[0] = 0;
    fNodes[1] = 0;
    fEdges[0] = 0;
    fEdges[1] = 0;
  }

  DirectedGraphEdge (const DirectedGraphEdge &inputEdge) {
    *this = inputEdge;
  }

  DirectedGraphEdge &operator= (const DirectedGraphEdge &inputEdge) {
    fNodes[0] = inputEdge.fNodes[0];
    fNodes[1] = inputEdge.fNodes[1];
    fEdges[0] = inputEdge.fEdges[0];
    fEdges[1] = inputEdge.fEdges[1];
    return *this;
  }

  // Returns the next edge in the given chain.
  EdgeIndex NextEdge (bool direction) const {
    return fEdges[direction];
  }

  // Returns the next edge in the successor chain
  EdgeIndex NextSuccessor() const {
    return NextEdge(kSuccessorList);
  }

  // Returns the next edge in the predecessor chain
  EdgeIndex NextPredecessor() const {
    return NextEdge(kPredecessorList);
  }

  // Returns the index of the node at the source end of the edge
  NodeIndex FromNode (bool direction) const {
    return fNodes[direction ^ 1];
  }

  // Returns the index of the node at the target end of the edge
  NodeIndex ToNode (bool direction) const {
    return fNodes[direction];
  }

  // Returns the index of the successor node adjacent with the edge
  NodeIndex SuccessorNode() const {
    return ToNode(kSuccessorList);
  }

  // Returns the index of the predecessor node adjacent with the edge
  NodeIndex PredecessorNode() const {
    return ToNode(kPredecessorList);
  }

  // Sets the index of the node at the successor or predecessor end of the edge
  void SetNode (bool direction, NodeIndex inputNode) {
    fNodes[direction] = inputNode;
  }

  // Sets the successor node index
  void SetSuccessor (NodeIndex inputNode) {
    SetNode (kSuccessorList, inputNode);
  }

  // Sets the predecessor node index
  void SetPredecessor (NodeIndex inputNode) {
    SetNode (kPredecessorList, inputNode);
  }

  // Sets the next edge link in the given chain
  void SetNext (bool direction, EdgeIndex inputEdge) {
    fEdges[direction] = inputEdge;
  }

  // Sets the next successor edge link
  void SetNextSuccessor (EdgeIndex inputEdge) {
    SetNext (kSuccessorList, inputEdge);
  }

  // Sets the next predecessor edge link
  void SetNextPredecessor (EdgeIndex inputEdge) {
    SetNext (kPredecessorList, inputEdge);
  }

  // Dump routine
  template <class ostr>
    friend ostr &operator<< (ostr &out, const DirectedGraphEdge &edge) {
    out << "  Succ: (" << edge.SuccessorNode() << ")"
	<< ", Pred: (" << edge.PredecessorNode() << ")"
	<< ", NextSucc: <" << edge.NextSuccessor() << ">"
	<< ", NextPred: <" << edge.NextPredecessor() << ">"
	<< "\n";
    return out;
  }

  private:

  NodeIndex fNodes[2];      // FROM and TO node indices
  EdgeIndex fEdges[2];      // next PREDECESSOR and SUCCESSOR edge indices
};

#define CS2_DG_TEMPARGS <ANodeType, AEdgeType, Allocator>
#define CS2_DG_TEMP template<class ANodeType, class AEdgeType, class Allocator>
#define CS2_DG_DECL DirectedGraph <ANodeType, AEdgeType, Allocator>

 CS2_DG_TEMP class DirectedGraph : private Allocator {
  public:

  enum TraversalDirection {
    kForwardDirection = 1,
    kBackwardDirection = 0
  };

  enum TraversalType {
    kPostOrder = 1,
    kPreOrder = 0
  };

  DirectedGraph(NodeIndex ignore=0, EdgeIndex ignore2=0, const Allocator &a = Allocator());
  DirectedGraph(const Allocator &a);
  DirectedGraph (const CS2_DG_DECL &);

  const Allocator& allocator() const { return *this;}

  CS2_DG_DECL &operator= (const CS2_DG_DECL &);

  // Reset the graph (ie. remove all of the nodes and edges)
  void Reset();

  // Get a reference to a given node
  ANodeType &NodeAt (NodeIndex) const;

  // Existence of a given node
  bool    NodeExists (NodeIndex) const;

  // Get a reference to a given edge
  AEdgeType &EdgeAt (EdgeIndex) const;

  // Existence of a given edge
  bool    EdgeExists (EdgeIndex) const;

  //Returns the numbers of predecessors or successors of a given node
  uint32_t NumberOfPredecessors(NodeIndex) const;
  uint32_t NumberOfSuccessors(NodeIndex) const;

  // Returns the highest assigned node index
  NodeIndex HighestNodeIndex() const;
  NodeIndex LowestNodeIndex() const;

  // Returns the highest assigned edge index
  EdgeIndex HighestEdgeIndex() const;
  EdgeIndex LowestEdgeIndex() const;

  // Adds a node to the graph with no adjacent edges
  NodeIndex AddNode();

  // Adds a node to the graph using a given node index.  A false
  // return code is given if the index is already in use.
  bool AddNodeAtPosition(NodeIndex);

  // Removes a node from the graph.  If the node has adjacent edges, they
  // are also removed.
  void RemoveNode(NodeIndex);

  // Removes a set of nodes from the graph with all incident edges.
  void RemoveNodes(const ABitVector<Allocator>&);

  // Consistency checker
  void VerifyDGraph() const;

  // Adds an edge to the graph with no adjacent nodes.
  EdgeIndex AddEdge();

  // Adds an edge to the graph between the given nodes.
  EdgeIndex AddEdge (NodeIndex, NodeIndex,
                      bool direction = kForwardDirection);

  // Removes an edge from the graph.
  void RemoveEdge (EdgeIndex);

  // Get rid of link from a source node to an edge.
  void UndoLink (NodeIndex, EdgeIndex, bool);

  #define CS2_DFC_DECL CS2_DG_DECL::DepthFirstCursor

  class DepthFirstCursor {
    public:

    DepthFirstCursor (const CS2_DG_DECL &, NodeIndex, bool traversalType = kPostOrder,
                      bool direction = kForwardDirection);
    // ~DepthFirstCursor();
    DepthFirstCursor (const typename CS2_DFC_DECL &);
    typename CS2_DFC_DECL &operator= (const typename CS2_DFC_DECL &);

    // Set the cursor position
    NodeIndex SetToFirst();
    NodeIndex SetToLast();
    NodeIndex SetToNext();
    NodeIndex SetToPrevious();

    // Determine if the cursor points at a valid position
    bool Valid() const;

    operator NodeIndex() const;

    private:

    ListOf<NodeIndex, Allocator> fNodeList;
    int32_t fCurrentIndex;
  };

  #define CS2_BFC_DECL CS2_DG_DECL::BreadthFirstCursor

  class BreadthFirstCursor {
    public:

    BreadthFirstCursor (const CS2_DG_DECL &, NodeIndex, bool direction = kForwardDirection);
    // ~BreadthFirstCursor();
    BreadthFirstCursor (const typename CS2_BFC_DECL &);
    typename CS2_BFC_DECL &operator= (const typename CS2_BFC_DECL &);

    // Set the cursor position
    NodeIndex SetToFirst();
    NodeIndex SetToLast();
    NodeIndex SetToNext();
    NodeIndex SetToPrevious();

    // Determine if the cursor points at a valid position
    bool Valid() const;

    operator NodeIndex() const;

    private:

    ListOf<NodeIndex,Allocator> fNodeList;
    int32_t fCurrentIndex;
  };

  // Node and edge cursors
  #define CS2_DGNC_DECL CS2_DG_DECL::NodeCursor
  #define CS2_DGEC_DECL CS2_DG_DECL::EdgeCursor

  class NodeCursor : public TableOf<ANodeType,Allocator>::Cursor {
    public:

    NodeCursor (const CS2_DG_DECL &);
    // ~NodeCursor();
    NodeCursor (const NodeCursor &);

    private:

    NodeCursor &operator= (const NodeCursor &);
  };

  class EdgeCursor : public TableOf<AEdgeType,Allocator>::Cursor {
    public:

    EdgeCursor (const CS2_DG_DECL &);
    // ~EdgeCursor();
    EdgeCursor (const EdgeCursor &);

    private:

    EdgeCursor &operator= (const EdgeCursor &);
  };

  friend class NodeCursor;
  friend class EdgeCursor;

  // Return the memory usage in bytes  for this graph
  unsigned long MemoryUsage() const;

  template <class ostr>
  friend ostr &operator<< (ostr &out, const CS2_DG_DECL &graph) {
    /*
      out << "Node Table" << "\n";
      out << "----------" << "\n";
      out << graph.fNodeTable;

      out << "Edge Table" << "\n";
      out << "----------" << "\n";
      out << graph.fEdgeTable;
    */

    out << "Node Table" << "\n";
    out << "[" << "\n";

    int NumOfNodes = 0;

    typename CS2_DG_DECL::NodeCursor nodeIndex(graph);
    for (nodeIndex.SetToFirst(); nodeIndex.Valid(); nodeIndex.SetToNext()) {
      NumOfNodes ++;

      out << "(" << (int) nodeIndex << ")" << graph.NodeAt(nodeIndex) << "\n";

      int count;
      EdgeIndex edgeIndex;

      count=0;
      out << " Pred nodes: ";
      for (edgeIndex=graph.NodeAt(nodeIndex).FirstPredecessor(); edgeIndex; edgeIndex=graph.EdgeAt(edgeIndex).NextPredecessor()) {
	count++;
	out << "(" << graph.EdgeAt(edgeIndex).PredecessorNode() << ")<" << edgeIndex << "> ";
	if ((count%10 == 0) && graph.EdgeAt(edgeIndex).NextPredecessor())
	  out << "\n" << "             ";
      }
      out << "\n";

      count=0;
      out << " Succ nodes: ";
      for (edgeIndex=graph.NodeAt(nodeIndex).FirstSuccessor(); edgeIndex; edgeIndex=graph.EdgeAt(edgeIndex).NextSuccessor()) {
	count ++;
	out << "<" << edgeIndex << ">(" << graph.EdgeAt(edgeIndex).SuccessorNode() << ") ";
	if ((count%10 == 0) && graph.EdgeAt(edgeIndex).NextSuccessor())
	  out << "\n" << "             ";
      }
      out << "\n";

      out << "\n";
    }

    out << "]" << "\n";
    out << "Number of nodes: " << NumOfNodes << "\n";
    out << "\n";


    out << "Edge Table" << "\n";
    out << "[" << "\n";

    int NumOfEdges = 0;

    typename CS2_DG_DECL::EdgeCursor edgeIndex(graph);
    for (edgeIndex.SetToFirst(); edgeIndex.Valid(); edgeIndex.SetToNext()) {
      NumOfEdges ++;
      out << "<" << (int) edgeIndex << ">" << graph.EdgeAt(edgeIndex) << "\n";
    }

    out << "]" << "\n";
    out << "Number of edges: " << NumOfEdges << "\n";
    out << "\n";

    return out;
  }

  protected:
  TableOf<AEdgeType, Allocator> fEdgeTable;
  TableOf<ANodeType, Allocator> fNodeTable;

};

CS2_DG_TEMP inline
  CS2_DG_DECL::DirectedGraph (NodeIndex ignore, EdgeIndex ignore2, const Allocator &a) :
  Allocator(a), fEdgeTable(a), fNodeTable(a)
{ }

CS2_DG_TEMP inline
  CS2_DG_DECL::DirectedGraph (const Allocator &a) :
  Allocator(a), fEdgeTable(a), fNodeTable(a)
{ }


CS2_DG_TEMP inline CS2_DG_DECL &CS2_DG_DECL::operator= (const CS2_DG_DECL &inputGraph) {
  fNodeTable = inputGraph.fNodeTable;
  fEdgeTable = inputGraph.fEdgeTable;
  return *this;
}

CS2_DG_TEMP inline CS2_DG_DECL::DirectedGraph (const CS2_DG_DECL &inputGraph) {
  *this = inputGraph;
}

// DirectedGraph::Reset
//
// Reset the graph (ie. remove all of the nodes and edges)

CS2_DG_TEMP inline void CS2_DG_DECL::Reset() {
  fNodeTable.MakeEmpty();
  fEdgeTable.MakeEmpty();
}

CS2_DG_TEMP inline ANodeType &CS2_DG_DECL::NodeAt (NodeIndex nodeIndex) const {
  return fNodeTable.ElementAt(nodeIndex);
}

CS2_DG_TEMP inline bool CS2_DG_DECL::NodeExists (NodeIndex nodeIndex) const {
  return fNodeTable.Exists(nodeIndex);
}

CS2_DG_TEMP inline AEdgeType &CS2_DG_DECL::EdgeAt (EdgeIndex edgeIndex) const {
  return fEdgeTable.ElementAt(edgeIndex);
}

CS2_DG_TEMP inline bool CS2_DG_DECL::EdgeExists (EdgeIndex edgeIndex) const {
  return fEdgeTable.Exists(edgeIndex);
}

// DirectedGraph::HighestNodeIndex
//
// Return the highest allocated node index

CS2_DG_TEMP inline NodeIndex CS2_DG_DECL::HighestNodeIndex() const {
  return fNodeTable.HighestIndex();
}
CS2_DG_TEMP inline NodeIndex CS2_DG_DECL::LowestNodeIndex() const {
  return fNodeTable.LowestIndex();
}

// DirectedGraph::HighestEdgeIndex
//
// Return the highest allocated edge index

CS2_DG_TEMP inline EdgeIndex CS2_DG_DECL::HighestEdgeIndex() const {
  return fEdgeTable.HighestIndex();
}
CS2_DG_TEMP inline EdgeIndex CS2_DG_DECL::LowestEdgeIndex() const {
  return fEdgeTable.LowestIndex();
}

// DirectedGraph::AddNode
//
// Add an unattached node to the graph.

CS2_DG_TEMP inline NodeIndex CS2_DG_DECL::AddNode() {
  NodeIndex newNodeIndex;

  newNodeIndex = fNodeTable.AddEntry();
  NodeAt(newNodeIndex) = ANodeType();

  return newNodeIndex;
}

// DirectedGraph::AddNodeAtPosition
//
// Add a node to the graph at the given position.  Return false if the
// position is already occupied.

CS2_DG_TEMP inline bool
CS2_DG_DECL::AddNodeAtPosition (NodeIndex inputPosition) {
  if (fNodeTable.Exists(inputPosition)) return false;

  fNodeTable.AddEntryAtPosition(inputPosition);
  NodeAt(inputPosition) = ANodeType();

  return true;
}

// DirectedGraph::AddEdge
//
// Add an unattached edge to the graph.

CS2_DG_TEMP inline EdgeIndex CS2_DG_DECL::AddEdge() {
  EdgeIndex newEdgeIndex;
  newEdgeIndex = fEdgeTable.AddEntry();
  EdgeAt(newEdgeIndex) = AEdgeType();
  return newEdgeIndex;
}

CS2_DG_TEMP inline CS2_DGNC_DECL::NodeCursor (const CS2_DG_DECL &inputGraph) :
  TableOf<ANodeType,Allocator>::Cursor(inputGraph.fNodeTable) { }

CS2_DG_TEMP inline CS2_DGNC_DECL::NodeCursor (const typename CS2_DGNC_DECL &inputCursor) :
  TableOf<ANodeType,Allocator>::Cursor(inputCursor) { }

CS2_DG_TEMP inline CS2_DGEC_DECL::EdgeCursor (const CS2_DG_DECL &inputGraph) :
  TableOf<AEdgeType,Allocator>::Cursor(inputGraph.fEdgeTable) { }

CS2_DG_TEMP inline CS2_DGEC_DECL::EdgeCursor (const typename CS2_DGEC_DECL &inputCursor) :
  TableOf<AEdgeType,Allocator>::Cursor(inputCursor) { }

CS2_DG_TEMP inline CS2_DFC_DECL::DepthFirstCursor (const typename CS2_DFC_DECL &c) :
fNodeList(c.fNodeList),
fCurrentIndex(c.fCurrentIndex) { }

CS2_DG_TEMP inline typename CS2_DFC_DECL &CS2_DFC_DECL::operator= (const typename CS2_DFC_DECL &c) {
  fNodeList = c.fNodeList;
  fCurrentIndex = c.fCurrentIndex;
  return *this;
}

CS2_DG_TEMP inline NodeIndex CS2_DFC_DECL::SetToFirst() {
  fCurrentIndex = 0;
  return (Valid() ? fNodeList[fCurrentIndex] : 0);
}

CS2_DG_TEMP inline NodeIndex CS2_DFC_DECL::SetToLast() {
  fCurrentIndex = fNodeList.NumberOfElements() - 1;
  return (Valid() ? fNodeList[fCurrentIndex] : 0);
}

CS2_DG_TEMP inline NodeIndex CS2_DFC_DECL::SetToNext() {
  ++fCurrentIndex;
  return (Valid() ? fNodeList[fCurrentIndex] : 0);
}

CS2_DG_TEMP inline NodeIndex CS2_DFC_DECL::SetToPrevious() {
  --fCurrentIndex;
  return (Valid() ? fNodeList[fCurrentIndex] : 0);
}

CS2_DG_TEMP inline bool CS2_DFC_DECL::Valid() const {
  return (fCurrentIndex >= 0 && fCurrentIndex < fNodeList.NumberOfElements());
}

CS2_DG_TEMP inline CS2_DFC_DECL::operator NodeIndex() const {
  return fNodeList[fCurrentIndex];
}

CS2_DG_TEMP inline CS2_BFC_DECL::BreadthFirstCursor (const typename CS2_BFC_DECL &c) :
fNodeList(c.fNodeList),
fCurrentIndex(c.fCurrentIndex) { }

CS2_DG_TEMP inline typename CS2_BFC_DECL &CS2_BFC_DECL::operator= (const typename CS2_BFC_DECL &c) {
  fNodeList = c.fNodeList;
  fCurrentIndex = c.fCurrentIndex;
  return *this;
}

CS2_DG_TEMP inline NodeIndex CS2_BFC_DECL::SetToFirst() {
  fCurrentIndex = 0;
  return (Valid() ? fNodeList[fCurrentIndex] : 0);
}

CS2_DG_TEMP inline NodeIndex CS2_BFC_DECL::SetToLast() {
  fCurrentIndex = fNodeList.NumberOfElements() - 1;
  return (Valid() ? fNodeList[fCurrentIndex] : 0);
}

CS2_DG_TEMP inline NodeIndex CS2_BFC_DECL::SetToNext() {
  ++fCurrentIndex;
  return (Valid() ? fNodeList[fCurrentIndex] : 0);
}

CS2_DG_TEMP inline NodeIndex CS2_BFC_DECL::SetToPrevious() {
  --fCurrentIndex;
  return (Valid() ? fNodeList[fCurrentIndex] : 0);
}

CS2_DG_TEMP inline bool CS2_BFC_DECL::Valid() const {
  return (fCurrentIndex >= 0 && fCurrentIndex < fNodeList.NumberOfElements());
}

CS2_DG_TEMP inline CS2_BFC_DECL::operator NodeIndex() const {
  return fNodeList[fCurrentIndex];
}

// Consistency checker
CS2_DG_TEMP inline void CS2_DG_DECL::VerifyDGraph() const {

  typename CS2_DG_DECL::NodeCursor nodeIndex(*this);
  ABitVector<Allocator> seenEdges(*this);

  for (nodeIndex.SetToFirst(); nodeIndex.Valid(); nodeIndex.SetToNext()) {

    EdgeIndex edgeIndex, nextEdge;
    seenEdges.Clear();

    for (edgeIndex = NodeAt(nodeIndex).FirstSuccessor();
         edgeIndex != 0;
         edgeIndex = nextEdge) {

      CS2Assert(!seenEdges[edgeIndex], ("DGraph: Successors list has a cycle"));
      seenEdges[edgeIndex] = true;

      AEdgeType& currentEdge = EdgeAt(edgeIndex);
      nextEdge = currentEdge.NextSuccessor();
    }

    seenEdges.Clear();

    for (edgeIndex = NodeAt(nodeIndex).FirstPredecessor();
         edgeIndex != 0;
         edgeIndex = nextEdge) {

      CS2Assert(!seenEdges[edgeIndex], ("DGraph: Predecessors list has a cycle"));
      seenEdges[edgeIndex] = true;

      AEdgeType& currentEdge = EdgeAt(edgeIndex);
      nextEdge = currentEdge.NextPredecessor();
    }
  }
}

// Removing the set of Nodes from the graph with the incident edges
// in O(N) time, where N is the number of edges in the graph
CS2_DG_TEMP inline void CS2_DG_DECL::RemoveNodes(const ABitVector<Allocator>& victimNodes) {

  // traversing the Node Table
  typename CS2_DG_DECL::NodeCursor nodeIndex(*this);
  for (nodeIndex.SetToFirst(); nodeIndex.Valid(); nodeIndex.SetToNext()) {

    if (victimNodes.ValueAt(nodeIndex)) {
      //Removing the node from the Table
      fNodeTable.RemoveEntry(nodeIndex);
    } else {

      // The node is not being removed
      // Traversing the list of the successors and disconnecting the edges landing in the victim set
      EdgeIndex edgeIndex, nextEdge, prevEdge;
      ANodeType& currentNode = NodeAt(nodeIndex);
      prevEdge = 0;
      for (edgeIndex = NodeAt(nodeIndex).FirstSuccessor();
           edgeIndex != 0;
           edgeIndex = nextEdge) {

        AEdgeType& currentEdge = EdgeAt(edgeIndex);
        nextEdge = currentEdge.NextSuccessor();
        NodeIndex successorNode = currentEdge.SuccessorNode();
        // if the edge is landing in the set being removed
        // we need to only correct the links for the currently traversed
        // list of the successors since the associated list of predecessors
        // of the "to" node will be cleared
        if (victimNodes.ValueAt(successorNode)) {
          if (!prevEdge) {
            currentNode.SetFirstSuccessor (nextEdge);
          } else {
            EdgeAt(prevEdge).SetNextSuccessor (nextEdge);
          }
        } else {
          prevEdge = edgeIndex;
        }
      }

      // Traversing the list of the predecessors and disconnecting the edges starting in the victim set
      prevEdge = 0;
      for (edgeIndex = NodeAt(nodeIndex).FirstPredecessor();
           edgeIndex != 0;
           edgeIndex = nextEdge) {
        AEdgeType& currentEdge = EdgeAt(edgeIndex);
        nextEdge = currentEdge.NextPredecessor();
        NodeIndex predecessorNode = currentEdge.PredecessorNode();
        // if the edge is starting in the set being removed
        // we need to only correct the links for the currently traversed
        // list of the predecessors since the associated list of successors
        // of the "from" node will be cleared
        if (victimNodes.ValueAt(predecessorNode)) {
          if (!prevEdge) {
            currentNode.SetFirstPredecessor (nextEdge);
          } else {
            EdgeAt(prevEdge).SetNextPredecessor (nextEdge);
          }
        } else {
          prevEdge = edgeIndex;
        }
      }
    }
  }

  // traversing the Edge Table
  typename CS2_DG_DECL::EdgeCursor edgeIndex(*this);
  for (edgeIndex.SetToFirst(); edgeIndex.Valid(); edgeIndex.SetToNext()) {
    AEdgeType& currentEdge = EdgeAt(edgeIndex);
    NodeIndex predecessorNode = currentEdge.PredecessorNode();
    NodeIndex successorNode = currentEdge.SuccessorNode();
    // either  "from" or "to" nodes of the current edge
    // are in the set being removed, therefor the edge has to be removed
    // and no links correction is required - it was already accomplished
    // during the Node Table traversal
    if (victimNodes.ValueAt(predecessorNode) || victimNodes.ValueAt(successorNode)) {
      fEdgeTable.RemoveEntry(edgeIndex);
    }
  }
}

// DirectedGraph::RemoveNode
//
// Remove a node and all incident edges from the graph.

CS2_DG_TEMP inline void CS2_DG_DECL::RemoveNode (NodeIndex victimNode) {
  EdgeIndex edgeIndex, nextEdge;

  // Remove all the incident successor edges
  for (edgeIndex = NodeAt(victimNode).FirstSuccessor();
       edgeIndex != 0;
       edgeIndex = nextEdge) {
    nextEdge = EdgeAt(edgeIndex).NextSuccessor();
    RemoveEdge (edgeIndex);
  }
  CS2Assert (NodeAt(victimNode).FirstSuccessor() == 0,
          ("Failed to remove all successor edges"));

  // Remove all the incident predecessor edges
  for (edgeIndex = NodeAt(victimNode).FirstPredecessor();
       edgeIndex != 0;
       edgeIndex = nextEdge) {
    nextEdge = EdgeAt(edgeIndex).NextPredecessor();
    RemoveEdge (edgeIndex);
  }
  CS2Assert (NodeAt(victimNode).FirstPredecessor() == 0,
          ("Failed to remove all predecessor edges"));

  fNodeTable.RemoveEntry (victimNode);
}

// DirectedGraph::AddEdge (from, to, direction)
//
// Add an edge from a given node to another node in the given direction.

CS2_DG_TEMP inline EdgeIndex CS2_DG_DECL::AddEdge (NodeIndex fromNode, NodeIndex toNode,
                                     bool direction) {
  EdgeIndex newIndex;

  newIndex = AddEdge();
  AEdgeType &newEdge = fEdgeTable[newIndex];

  if (direction == kForwardDirection) {
    newEdge.SetPredecessor (fromNode);
    newEdge.SetSuccessor (toNode);

    // Add edge to chain of edges associated with "from" and "to"
    newEdge.SetNextSuccessor (NodeAt(fromNode).FirstSuccessor());
    NodeAt(fromNode).SetFirstSuccessor (newIndex);

    newEdge.SetNextPredecessor (NodeAt(toNode).FirstPredecessor());
    NodeAt(toNode).SetFirstPredecessor (newIndex);
  } else {
    newEdge.SetPredecessor (toNode);
    newEdge.SetSuccessor (fromNode);

    // Add edge to chain of edges associated with "from" and "to"
    newEdge.SetNextSuccessor (NodeAt(toNode).FirstSuccessor());
    NodeAt(toNode).SetFirstSuccessor (newIndex);

    newEdge.SetNextPredecessor (NodeAt(fromNode).FirstPredecessor());
    NodeAt(fromNode).SetFirstPredecessor (newIndex);
  }

  return newIndex;
}

// DirectedGraph::RemoveEdge
//
// Remove an edge from the graph.

CS2_DG_TEMP inline void CS2_DG_DECL::RemoveEdge (EdgeIndex edgeIndex) {
  AEdgeType  &thisEdge = EdgeAt(edgeIndex);
  NodeIndex  predNode, succNode;
  EdgeIndex  siblingEdge, nextEdge;

  // Remove this edge from the "from" and "to" chains
  succNode = thisEdge.SuccessorNode();
  if (NodeAt(succNode).FirstPredecessor() == edgeIndex)
    NodeAt(succNode).SetFirstPredecessor (thisEdge.NextPredecessor());
  else {
    for (siblingEdge = NodeAt(succNode).FirstPredecessor();
         siblingEdge != 0;
         siblingEdge = nextEdge) {
      nextEdge = EdgeAt(siblingEdge).NextPredecessor();
      if (nextEdge == edgeIndex) {
        EdgeAt(siblingEdge).SetNextPredecessor (thisEdge.NextPredecessor());
        break;
      }
    }

    CS2Assert (siblingEdge != 0, ("Cannot delete edge from chain"));
  }

  predNode = thisEdge.PredecessorNode();
  if (NodeAt(predNode).FirstSuccessor() == edgeIndex)
    NodeAt(predNode).SetFirstSuccessor (thisEdge.NextSuccessor());
  else {
    for (siblingEdge = NodeAt(predNode).FirstSuccessor();
         siblingEdge != 0;
         siblingEdge = nextEdge) {
      nextEdge = EdgeAt(siblingEdge).NextSuccessor();
      if (nextEdge == edgeIndex) {
        EdgeAt(siblingEdge).SetNextSuccessor (thisEdge.NextSuccessor());
        break;
      }
    }

    CS2Assert (siblingEdge != 0, ("Cannot delete edge from chain"));
  }

  fEdgeTable.RemoveEntry (edgeIndex);
}

// Get rid of link from a source node to an edge.
CS2_DG_TEMP void inline
CS2_DG_DECL::UndoLink (NodeIndex sourceNode, EdgeIndex edgeToBeRemoved,
		   bool direction) {
  EdgeIndex incidentEdge, nextEdge, lastEdge;

  lastEdge = 0;
  for (incidentEdge = NodeAt(sourceNode).FirstEdge(direction);
       incidentEdge != 0;
       incidentEdge = nextEdge) {
    nextEdge = EdgeAt(incidentEdge).NextEdge(direction);

    if (incidentEdge == edgeToBeRemoved) {
      if (lastEdge != 0)
        EdgeAt(lastEdge).SetNext (direction, nextEdge);
      else
        NodeAt(sourceNode).SetFirstEdge (direction, nextEdge);

      break;
    }

    lastEdge = incidentEdge;
  }
}

CS2_DG_TEMP uint32_t inline
CS2_DG_DECL::NumberOfPredecessors (NodeIndex node) const {
  uint32_t numOfPredecessors = 0;
  NodeIndex headIndex = node;
  for (EdgeIndex edgeIndex = NodeAt(headIndex).FirstPredecessor();
       edgeIndex != 0; edgeIndex = EdgeAt(edgeIndex).NextPredecessor()) {
    numOfPredecessors++;
  }
  return numOfPredecessors;
}

CS2_DG_TEMP uint32_t inline
CS2_DG_DECL::NumberOfSuccessors (NodeIndex node) const {
  uint32_t numOfSuccessors = 0;
  NodeIndex headIndex = node;
  for (EdgeIndex edgeIndex = NodeAt(headIndex).FirstSuccessor();
       edgeIndex != 0; edgeIndex = EdgeAt(edgeIndex).NextSuccessor()) {
    numOfSuccessors++;
  }
  return numOfSuccessors;
}

// DirectedGraph::MemoryUsage
//
// Return the memory usage in bytes  for this graph

CS2_DG_TEMP unsigned long CS2_DG_DECL::MemoryUsage() const {
  unsigned long sizeInBytes;

  sizeInBytes = fEdgeTable.MemoryUsage();
  sizeInBytes += fNodeTable.MemoryUsage();

  return sizeInBytes;
}

}
#undef CS2_DG_TEMPARGS
#undef CS2_DG_TEMP
#undef CS2_DG_DECL
#undef CS2_DFC_DECL
#undef CS2_BFC_DECL
#undef CS2_DGNC_DECL
#undef CS2_DGEC_DECL


#endif
