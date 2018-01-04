/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#ifndef ILVALIDATION_UTILS_HPP
#define ILVALIDATION_UTILS_HPP

#include "compile/Compilation.hpp"
#include "infra/Assert.hpp"
#include "il/Node.hpp"
#include "infra/SideTable.hpp"

namespace TR { class Compilation; }

namespace TR {

struct NodeState
  {
  TR::Node    *_node;
  ncount_t     _futureReferenceCount;

  NodeState(TR::Node *node):_node(node),_futureReferenceCount(node->getReferenceCount()){}
  };

/**
 * This is like a NodeChecklist, but more compact.  Rather than track
 * node global indexes, which can be sparse, this tracks local
 * indexes, which are relatively dense.  Furthermore, the _basis field
 * allows us not to waste space on nodes we saw in prior blocks.
 */
class LiveNodeWindow
  {

  NodeSideTable<NodeState> &_sideTable;
  int32_t                   _basis;
  TR_BitVector              _liveOffsets; // sideTable index = basis + offset

  public:

  LiveNodeWindow(NodeSideTable<NodeState> &sideTable, TR_Memory *memory);

  bool contains(TR::Node *node)
     {
     int32_t index = _sideTable.indexOf(node);
     if (index < _basis)
	return false;
     else
	return _liveOffsets.isSet(index - _basis);
     }

  void add(TR::Node *node)
     {
     int32_t index = _sideTable.indexOf(node);
     TR_ASSERT(index >= _basis, "Cannot mark node n%dn before basis %d live",
               node->getGlobalIndex(), _basis);
     _liveOffsets.set(index - _basis);
     }

  void remove(TR::Node *node)
     {
     int32_t index = _sideTable.indexOf(node);
     if (index >= _basis)
	_liveOffsets.reset(index - _basis);
     }

  bool isEmpty()
     {
     return _liveOffsets.isEmpty();
     }

  TR::Node *anyLiveNode()
     {
     Iterator iter(*this);
     return iter.currentNode();
     }

  void startNewWindow()
     {
     TR_ASSERT(_liveOffsets.isEmpty(),
               "Cannot close LiveNodeWindow when there are still live nodes");
     _basis = _sideTable.size();
     }
  public:

  class Iterator
     {
     LiveNodeWindow      &_window;
     TR_BitVectorIterator _bvi;
     int32_t              _currentOffset; // -1 means iteration is past end

     bool isPastEnd(){ return _currentOffset < 0; }

     public:

     Iterator(LiveNodeWindow &window):
        _window(window),
        _bvi(window._liveOffsets),
        _currentOffset(0xdead /* anything non-negative */)
	{
	stepForward();
	}

     TR::Node *currentNode()
	{
	if (isPastEnd())
	   return NULL;
	else
	   return _window._sideTable.getAt(_window._basis + _currentOffset)._node;
	}

     void stepForward()
	{
	TR_ASSERT(!isPastEnd(),
                  "Can't stepForward a LiveNodeWindow::Iterator that's already past end");
	if (_bvi.hasMoreElements())
	   _currentOffset = _bvi.getNextElement();
	else
	   _currentOffset = -1;
	}

     public: // operators

     void operator ++() { stepForward(); }
     };

  };


bool isILValidationLoggingEnabled(TR::Compilation *comp);

void checkILCondition(TR::Node *node, bool condition, TR::Compilation *comp,
                      const char *formatStr, ...);

void printILDiagnostic(TR::Compilation *comp, const char *formatStr, ...);
void vprintILDiagnostic(TR::Compilation *comp, const char *formatStr, va_list ap);

}
#endif // ILVALIDATION_UTIL_HPP
