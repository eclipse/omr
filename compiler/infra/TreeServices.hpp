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

#ifndef TRS_INCL
#define TRS_INCL

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/NodeUtils.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"

class TR_AddressTree
   {
public:
   TR_ALLOC(TR_Memory::LoopTransformer)

   TR_AddressTree(TR_AllocationKind heapOrStack, TR::Compilation * c)
      : _offset(-1), _multiplier(1), _rootNode(NULL), _comp(c)
      {}

   bool process(TR::Node* aiaddNode, bool onlyConsiderConstAiaddSecondChild = false);

   TR::Compilation *          comp()                        { return _comp; }
   TR_Memory *               trMemory()                    { return comp()->trMemory(); }

   int64_t getOffset()
      {
      return _offset;
      }

   int32_t getMultiplier()
      {
      return _multiplier;
      }

   TR_ParentOfChildNode * getMultiplyNode()
      {
      return &_multiplyNode;
      }

   TR_ParentOfChildNode * getBaseVarNode()
      {
      return &_baseVarNode;
      }

   TR_ParentOfChildNode * getIndVarNode()
      {
      return &_indVarNode;
      }

   TR_ParentOfChildNode * getIndexBase()
      {
      return &_indexBaseNode;
      }

   TR::Node * getRootNode()
      {
      return _rootNode;
      }

   bool static isILLoad(TR::Node * node);

protected:
   int64_t _offset;
   int32_t _multiplier;
   TR::Node * _rootNode;
   TR::Compilation * _comp;

   TR_ParentOfChildNode _baseVarNode;
   TR_ParentOfChildNode _multiplyNode;
   TR_ParentOfChildNode _indVarNode;
   TR_ParentOfChildNode _indexBaseNode;

   virtual bool processBaseAndIndex(TR::Node* parent);
   virtual bool findComplexAddressGenerationTree(TR::Node *node, vcount_t visitCount, TR::Node *parent);


private:
   bool processMultiplyNode(TR::Node * multiplyNode);

   };

//////////////////////////////
//
// Pattern matching framework
//

class TR_Unification
   // A TR_Unification represents the act of "unifying" nodes within a pattern,
   // where "unifying" has the meaning from PROLOG: the binding of "variables"
   // within the pattern to nodes within the target tree.
   {
   public:

   // Max number of nodes that can be unified in a single pattern.
   // (For efficiency, should be chosen to make this struct a multiple of word size)
   //
   enum { UNIFICATION_LIMIT=7 };

   typedef uint8_t TR_Index;
   typedef uint8_t TR_Mark;

   protected:

   TR::Node **_unifiedNodes; // Array of pointers.  Entries not yet unified must be NULL

   // Undo stack
   //
   TR_Mark  _undoCount;                    // How many unifications so far
   TR_Index _undoStack[UNIFICATION_LIMIT]; // the first _undoCount entries in this are indices into _unifiedNodes that have been unified so far

   public:
   TR_Unification(TR::Node **unifiedNodes):_unifiedNodes(unifiedNodes),_undoCount(0){} // Caller is responsible for zeroing out unifiedNodes

   TR::Node *node(TR_Index index){ return _unifiedNodes[index]; }

   void add(TR_Index unifiedNodeIndex, TR::Node *node)
      {
      TR_ASSERT(_unifiedNodes[unifiedNodeIndex] == NULL, "Can't add a unification at index %d; node is already non-null", unifiedNodeIndex);
      _unifiedNodes[unifiedNodeIndex] = node;
      _undoStack[_undoCount++] = unifiedNodeIndex;
      }

   // Use these when attempting to match a sub-pattern when you would want to
   // recover and continue if the sub-pattern fails
   //
   TR_Mark mark(){ return _undoCount; }
   void    undoTo(TR_Mark mark)
      {
      for (; _undoCount > mark; _undoCount--)
         _unifiedNodes[_undoStack[_undoCount-1]] = NULL;
      }

   void dump(TR::Compilation *comp)
      {
      traceMsg(comp, "{");
      const char *sep = "";
      for (TR_Mark i = 0; i < _undoCount; i++)
         {
         traceMsg(comp, "%s%d:%s", sep, _undoStack[i], comp->getDebug()->getName(_unifiedNodes[_undoStack[i]]));
         sep = " ";
         }
      traceMsg(comp, "}");
      }
   };

class TR_Pattern
   {
   public: // TODO: should be protected.  Not sure why that doesn't work.
   TR_Pattern *_next; // Every pattern is assumed to be part of a conjunction because that's so common
   virtual const char *getName()=0;
   virtual bool thisMatches(TR::Node *node, TR_Unification &uni, TR::Compilation *comp)=0;
   virtual void tracePattern(TR::Node *node);

   public:
   TR_ALLOC(TR_MemoryBase::TR_Pattern)
   TR_Pattern(TR_Pattern *next):_next(next){}

   bool matches(TR::Node *node, TR_Unification &uni, TR::Compilation *comp);
   bool matches(TR::Node *node, TR::Node **unifiedNodes, TR::Compilation *comp)
      {
      TR_Unification uni(unifiedNodes);
      return matches(node, uni, comp);
      }
   };

class TR_AnythingPattern: public TR_Pattern
   {
   protected:
   virtual const char *getName(){ return "Anything"; }
   virtual bool thisMatches(TR::Node *node, TR_Unification &uni, TR::Compilation *comp){ return true; }

   public:
   TR_AnythingPattern(TR_Pattern *next=NULL):TR_Pattern(next){}
   };

class TR_OpCodePattern: public TR_Pattern
   {
   protected:
   virtual const char *getName(){ return "OpCode"; }
   TR::ILOpCodes _opCode;
   virtual bool thisMatches(TR::Node *node, TR_Unification &uni, TR::Compilation *comp){ return node->getOpCodeValue() == _opCode; }
   virtual void tracePattern(TR::Node *node);

   public:
   TR_OpCodePattern(TR::ILOpCodes opCode, TR_Pattern *next=NULL):TR_Pattern(next),_opCode(opCode){}
   };

class TR_UnifyPattern: public TR_Pattern
   {
   protected:
   virtual const char *getName(){ return "Unify"; }
   TR_Unification::TR_Index _index;
   virtual bool thisMatches(TR::Node *node, TR_Unification &uni, TR::Compilation *comp);

   public:
   TR_UnifyPattern(TR_Unification::TR_Index index, TR_Pattern *next=NULL):TR_Pattern(next),_index(index){}
   };

class TR_ChoicePattern: public TR_Pattern
   {
   protected:
   virtual const char *getName(){ return "Choice"; }
   TR_Pattern *_firstChoice, *_secondChoice;
   virtual bool thisMatches(TR::Node *node, TR_Unification &uni, TR::Compilation *comp){ return _firstChoice->matches(node, uni, comp) || _secondChoice->matches(node, uni, comp); }

   public:
   TR_ChoicePattern(TR_Pattern *firstChoice, TR_Pattern *secondChoice, TR_Pattern *next):TR_Pattern(next),_firstChoice(firstChoice),_secondChoice(secondChoice){}
   };

class TR_NoRegisterPattern: public TR_Pattern
   {
   protected:
   virtual const char *getName(){ return "NoRegister"; }
   virtual bool thisMatches(TR::Node *node, TR_Unification &uni, TR::Compilation *comp){ return node->getRegister() == NULL; }

   public:
   TR_NoRegisterPattern(TR_Pattern *next=NULL):TR_Pattern(next){}
   };

class TR_IConstPattern: public TR_OpCodePattern
   {
   protected:
   virtual const char *getName(){ return "IConst"; }
   int32_t _value;
   virtual bool thisMatches(TR::Node *node, TR_Unification &uni, TR::Compilation *comp){ return TR_OpCodePattern::thisMatches(node, uni, comp) && node->getInt() == _value; }

   public:
   TR_IConstPattern(int32_t value, TR_Pattern *next=NULL):TR_OpCodePattern(TR::iconst, next),_value(value){}
   };

class TR_LConstPattern: public TR_OpCodePattern
   {
   protected:
   virtual const char *getName(){ return "LConst"; }
   int64_t _value;
   virtual bool thisMatches(TR::Node *node, TR_Unification &uni, TR::Compilation *comp){ return TR_OpCodePattern::thisMatches(node, uni, comp) && node->getLongInt() == _value; }

   public:
   TR_LConstPattern(int64_t value, TR_Pattern *next=NULL):TR_OpCodePattern(TR::lconst, next),_value(value){}
   };

class TR_ChildPattern: public TR_Pattern
   {
   protected:
   virtual const char *getName(){ return "Child"; }
   TR_Pattern *_childPattern;
   int32_t _childIndex;
   virtual bool thisMatches(TR::Node *node, TR_Unification &uni, TR::Compilation *comp)
      {
      return node->getNumChildren() > _childIndex
         && _childPattern->matches(node->getChild(_childIndex), uni, comp);
      }

   public:
   TR_ChildPattern(int32_t childIndex, TR_Pattern *childPattern, TR_Pattern *next=NULL):TR_Pattern(next),_childIndex(childIndex),_childPattern(childPattern){}
   };

class TR_ChildrenPattern: public TR_Pattern
   {
   protected:
   virtual const char *getName(){ return "Children"; }
   TR_Pattern *_leftPattern, *_rightPattern;
   virtual bool thisMatches(TR::Node *node, TR_Unification &uni, TR::Compilation *comp)
      {
      return node->getNumChildren() >= 2
         && _leftPattern->matches(node->getFirstChild(), uni, comp)
         && _rightPattern->matches(node->getSecondChild(), uni, comp);
      }

   public:
   TR_ChildrenPattern(TR_Pattern *leftPattern, TR_Pattern *rightPattern, TR_Pattern *next=NULL):TR_Pattern(next),_leftPattern(leftPattern),_rightPattern(rightPattern){}
   };

class TR_CommutativePattern: public TR_ChildrenPattern
   {
   protected:
   virtual const char *getName(){ return "Commutative"; }
   virtual bool thisMatches(TR::Node *node, TR_Unification &uni, TR::Compilation *comp);

   public:
   TR_CommutativePattern(TR_Pattern *leftPattern, TR_Pattern *rightPattern, TR_Pattern *next=NULL):TR_ChildrenPattern(leftPattern, rightPattern, next){}
   };

class TR_PatternBuilder
   {
   // A handy facility for reducing the verbosity of pattern creation.
   // Many patterns can be created just by using the various overloaded operator() methods.
   //
   // NOTE: this uses trPersistentMemory for the pattern objects, so patterns
   // built this way should only be built once and stored in a static pointer
   // or equivalent (or else you will have a MEMORY LEAK).  However, this is
   // a good practise anyway, so it shouldn't be much of a hardship.

   protected:
   TR::Compilation *_comp;

   public:
   TR_PatternBuilder(TR::Compilation *comp):_comp(comp){}
   TR::Compilation *comp(){ return _comp; }

   // Basic pattern constructors, hiding the ugly placement new.
   //
   TR_AnythingPattern    *anything(TR_Pattern *next=NULL){ return new(comp()->trPersistentMemory()) TR_AnythingPattern(next); }
   TR_OpCodePattern      *opCode(TR::ILOpCodes op, TR_Pattern *next=NULL){ return new(comp()->trPersistentMemory()) TR_OpCodePattern(op, next); }
   TR_UnifyPattern       *unify(TR_Unification::TR_Index index, TR_Pattern *next=NULL){ return new(comp()->trPersistentMemory()) TR_UnifyPattern(index, next); }
   TR_ChoicePattern      *choice(TR_Pattern *firstChoice, TR_Pattern *secondChoice, TR_Pattern *next){ return new(comp()->trPersistentMemory()) TR_ChoicePattern(firstChoice, secondChoice, next); }
   TR_NoRegisterPattern  *noRegister(TR_Pattern *next=NULL){ return new(comp()->trPersistentMemory()) TR_NoRegisterPattern(next); }
   TR_IConstPattern      *iconst(int32_t value, TR_Pattern *next=NULL){ return new(comp()->trPersistentMemory()) TR_IConstPattern(value, next); }
   TR_LConstPattern      *lconst(int64_t value, TR_Pattern *next=NULL){ return new(comp()->trPersistentMemory()) TR_LConstPattern(value, next); }
   TR_ChildPattern       *child(int32_t childIndex, TR_Pattern *childPattern, TR_Pattern *next=NULL){ return new(comp()->trPersistentMemory()) TR_ChildPattern(childIndex, childPattern, next); }
   TR_ChildrenPattern    *children(TR_Pattern *leftPattern, TR_Pattern *rightPattern, TR_Pattern *next=NULL){ return new(comp()->trPersistentMemory()) TR_ChildrenPattern(leftPattern, rightPattern, next); }
   TR_CommutativePattern *commutative(TR_Pattern *leftPattern, TR_Pattern *rightPattern, TR_Pattern *next=NULL){ return new(comp()->trPersistentMemory()) TR_CommutativePattern(leftPattern, rightPattern, next); }

   // Patterns that also look for particular opcodes, in addition to their usual semantics
   TR_Pattern *unify(TR::ILOpCodes op, TR_Unification::TR_Index index, TR_Pattern *next=NULL){ return opCode(op, unify(index, next)); }
   TR_Pattern *noRegister(TR::ILOpCodes op, TR_Pattern *next=NULL){ return opCode(op, noRegister(next)); }
   TR_Pattern *child(TR::ILOpCodes op, int32_t childIndex, TR_Pattern *childPattern, TR_Pattern *next=NULL){ return opCode(op, child(childIndex, childPattern, next)); }
   TR_Pattern *children(TR::ILOpCodes op, TR_Pattern *leftPattern, TR_Pattern *rightPattern, TR_Pattern *next=NULL){ return opCode(op, children(leftPattern, rightPattern)); }
   TR_Pattern *commutative(TR::ILOpCodes op, TR_Pattern *leftPattern, TR_Pattern *rightPattern, TR_Pattern *next=NULL){ return opCode(op, commutative(leftPattern, rightPattern)); }

   TR_Pattern *commutativeOrNot(TR::ILOpCodes op, TR_Pattern *leftPattern, TR_Pattern *rightPattern, TR_Pattern *next=NULL)
      {
      // Create a TR_CommutativePattern or TR_ChildrenPattern based on whether the opcode is commutative.
      TR::ILOpCode opCode; opCode.setOpCodeValue(op);
      if (opCode.isCommutative())
         return commutative(op, leftPattern, rightPattern, next);
      else
         return children(op, leftPattern, rightPattern, next);
      }

   TR_Unification::TR_Index unificationIndex(void *structure, TR::Node *&element){ return (TR_Unification::TR_Index) ((&element) - (TR::Node**)structure); }

   //////////////////////////////
   //
   // Extremely terse shortand for common cases.
   //
   // Beware of adding too many of these.  The objective should be to make user
   // code as clear as possible, not as terse as possible.  Also, they should all
   // be one-liners; any logic should be in a function with a normal name (for
   // example, see commutativeOrNot).
   //
   TR_Pattern *operator() (TR::ILOpCodes op, TR_Pattern *next=NULL)
      { return opCode(op, next); }
   TR_Pattern *operator() (void *structure, TR::Node *&element)
      { return unify(unificationIndex(structure, element)); }
   TR_Pattern *operator() (TR::ILOpCodes op, void *structure, TR::Node *&element)
      { return unify(op, unificationIndex(structure, element)); }
   TR_Pattern *operator() (TR::ILOpCodes op, TR_Pattern *leftPattern, TR_Pattern *rightPattern)
      { return commutativeOrNot(op, leftPattern, rightPattern); }
   TR_Pattern *operator() (TR::ILOpCodes op, void *structure, TR::Node *&element, TR_Pattern *leftPattern, TR_Pattern *rightPattern)
      { return commutativeOrNot(op, leftPattern, rightPattern, unify(unificationIndex(structure, element))); }

   };

#endif
