/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef NODEPOOL_INCL
#define NODEPOOL_INCL

#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "il/NodeUtils.hpp"

namespace TR { class SymbolReference; }
namespace TR { class Compilation; }
template <class T> class TR_Array;

namespace TR {

class NodePool
   {
   public:

   TR_ALLOC(TR_Memory::Compilation)
   NodePool(TR::Compilation * comp);

   TR::Node * allocate();
   bool      deallocate(TR::Node * node);
   bool      removeDeadNodes();
   void      enableNodeGC()  { _disableGC = false; }
   void      disableNodeGC() { _disableGC = true; }
   ncount_t  getLastGlobalIndex()     { return _globalIndex; }
   ncount_t  getMaxIndex()           { return _globalIndex; }
   TR::Compilation * comp() { return _comp; }

   void cleanUp();

   private:
   TR::Compilation *     _comp;
   bool                  _disableGC;
   ncount_t              _globalIndex;

   TR::Region            _nodeRegion;
   };

}

#endif
