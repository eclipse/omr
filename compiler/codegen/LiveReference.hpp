/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef LIVEREFERENCE_INCL
#define LIVEREFERENCE_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "infra/List.hpp"

namespace TR { class Node; }

class TR_LiveReference
   {

private:

   TR::Node *       _referenceNode;
   List<TR::Node>   _parents;
   uint32_t         _numberOfParents;
   bool             _needSpillTemp;

public:

   TR_ALLOC(TR_Memory::LiveReference)

   TR_LiveReference(TR_Memory * m)
      : _referenceNode(NULL),
        _numberOfParents(0),
        _parents(m),
        _needSpillTemp(false)
      {}

   TR_LiveReference(TR::Node *rn, TR::Node *parent, TR_Memory * m)
      : _referenceNode(rn),
        _numberOfParents(0),
        _parents(m),
        _needSpillTemp(false)
      {
      addParentToList(parent);
      }

   List<TR::Node>& getParentList() { return _parents; }

   void addParentToList(TR::Node *parent)
      {
      ++_numberOfParents;
      _parents.add(parent);
      }

   uint32_t getNumberOfParents() { return _numberOfParents; }

   TR::Node *getReferenceNode()  { return _referenceNode; }

   bool needSpillTemp()          { return _needSpillTemp; }
   void setNeedSpillTemp(bool b) { _needSpillTemp = b; }

   };

#endif
