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

#ifndef NODEUTILS_INCL
#define NODEUTILS_INCL

#include <stddef.h>
#include <stdint.h>
#include "codegen/RegisterConstants.hpp"
#include "il/DataTypes.hpp"
#include "il/NodeExtension.hpp"
#include "infra/Link.hpp"

/*
 * Definitions of assorted classes used in Node and Node-associated tasks
 */

namespace TR { class SymbolReference; }
namespace TR { class Node; }
template <class T> class TR_Array;

#define NUM_DEFAULT_RELOCATIONS 1

// fence relocation types
#define TR_AbsoluteAddress         0x00
#define TR_EntryRelative16Bit      0x01
#define TR_EntryRelative32Bit      0x02
#define TR_ExternalAbsoluteAddress 0x04

/* Because this structure is used both inside and outside the child union, it
 * is defined here, but not in the ChildUnionMembers namespace
 */
struct GlobalRegisterInfo
   {
   // Used by the OMR::?RegLoad and OMR::?RegStore opcodes
   //
   void setFirst(TR::Node * ptr, TR::Node * owner)
      {
      freeNodeExtension(owner);
      _firstChild = ptr;
      }

   void setLow(TR_GlobalRegisterNumber low,  TR::Node * owner)
      {
      freeNodeExtension(owner);
      _lowRegisterNumber = low;
      }

   void setHigh(TR_GlobalRegisterNumber high, TR::Node * owner)
      {
      freeNodeExtension(owner);
      _highRegisterNumber = high;

      }

   void freeNodeExtension(TR::Node * owner) ;

   TR::Node * getFirst() { return _firstChild;};
   TR_GlobalRegisterNumber getLow() { return _lowRegisterNumber;}
   TR_GlobalRegisterNumber getHigh() { return _highRegisterNumber;}

   //  private:
   TR::Node * _firstChild; // dummy to overlay first child
   TR_GlobalRegisterNumber _lowRegisterNumber;
   TR_GlobalRegisterNumber _highRegisterNumber;
   };

/*
 * These definitions are used only as part of UnionedWithChildren
 * grouped into a namespace
 */
namespace ChildUnionMembers
{

struct CaseInfo
   {
   // Used by the TR::Case
   //
   void setFirst(TR::Node * ptr, TR::Node * owner)
      {
      freeNodeExtension(owner);
      _firstChild = ptr;
      }

   CASECONST_TYPE setConst(CASECONST_TYPE caseConst, TR::Node * owner)
      {
      freeNodeExtension(owner);
      _caseConstant = caseConst;
      return _caseConstant;
      }

   void freeNodeExtension(TR::Node * owner);

   TR::Node * getFirst()
      {
      return _firstChild;
      }

   CASECONST_TYPE getConst()
      {
      return _caseConstant;
      }

private:

   // This is very unsafe and gross.
   // dummy to overlay first child which may be GlRegDep
   TR::Node * _firstChild;
   CASECONST_TYPE _caseConstant;
   };

struct MonitorInfo
   {
   // Used by TR::monent and TR::monexit
   //
   void setFirst(TR::Node * ptr, TR::Node * owner)
      {
      freeNodeExtension(owner);
      _firstChild = ptr;
      }

   void * setInfo(void * info,  TR::Node * owner)
      {
      freeNodeExtension(owner); _info = info;
      return _info;
      }

   void freeNodeExtension(TR::Node * owner);

   TR::Node * getFirst()
      {
      return _firstChild;
      }

   void * getInfo()
      {
      return _info;
      }

private:
   // This is very unsafe and gross.
   TR::Node * _firstChild; // dummy to overlay first child
   void    * _info;
   };

struct RelocationInfo
   {
   // number of relocations
   uint16_t _numRelocations;

   // relocation type to be performed
   uint16_t _relocationType;

   // Used to hold the address where the
   // binary code address must be filled in for a boundary opcode
   void    *_relocationDestination[NUM_DEFAULT_RELOCATIONS];
   };

}

/**
 * Map one node to another.
 */
class TR_NodeMappings
   {
public:
   TR_ALLOC(TR_Memory::NodeMappings)

   TR_NodeMappings() { }

   struct Mapper : TR_Link<Mapper>
      {
      Mapper(TR::Node * f, TR::Node * t) : _from(f), _to(t) { }
      TR::Node * _from, * _to;
      };

   TR::Node * getTo(TR::Node * from)
      {
      for (Mapper * m = _mappings.getFirst(); m; m = m->getNext())
         if (m->_from == from)
            return m->_to;
      return 0;
      }

   void clear() { _mappings.setFirst(0); }

   void add(TR::Node * from, TR::Node * to, TR_Memory * m)
      {
      _mappings.add(new (m->trStackMemory()) Mapper(from, to));
      }

private:
   TR_LinkHead<Mapper> _mappings;
   };

/**
 * TR_ParentOfChildNode is a simple helper class that lets you keep
 * track of a particular node that you want to update with a new
 * child later on in processing.
 */
class TR_ParentOfChildNode
   {
public:
   TR_ALLOC(TR_Memory::Node)

   TR_ParentOfChildNode(TR::Node * parent, int32_t childNumber)
      : _parent(parent), _childNumber(childNumber)
      {
      }

   TR_ParentOfChildNode()
      : _parent(NULL), _childNumber(0)
      {
      }

   TR::Node * getParent();
   void setParent(TR::Node * parent);

   int32_t getChildNumber();

   void setParentAndChildNumber(TR::Node * parent, int32_t childNumber);
   void setChild(TR::Node * child);

   TR::Node * getChild();
   bool isNull();

private:
   TR::Node * _parent;
   int32_t _childNumber;

   };

#endif
