/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef COMPILATION_DATATYPES_INCL
#define COMPILATION_DATATYPES_INCL

#include <stdint.h>
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/Stack.hpp"
#include "optimizer/Optimizations.hpp"

class TR_BitVector;
class TR_OpaqueClassBlock;
class TR_ResolvedMethod;
namespace TR { class Compilation; }
namespace TR { class Node; }
namespace TR { class TreeTop; }

enum TR_Hotness
#if !defined(LINUXPPC) || defined(__LITTLE_ENDIAN__)
   : int8_t // use only 8 bits to save space; The ifdef is needed because xlC BE compiler does not accept the syntax
#endif
   {
   noOpt,
   deadCold             = noOpt,
   cold,
   warm,
   hot,
   lastOMRStrategy      = hot,  // currently used to test available omr optimizations
   veryHot,
   scorching,
   minHotness           = noOpt,
   maxHotness           = scorching,
   reducedWarm,                     ///< (Possibly temporary) OBSOLETE.  It's way down here
                                    ///< so that count/bcount strings are not effected.
   unknownHotness,
   numHotnessLevels
   }; // NOTE: if adding new hotnessLevels, corresponding names must be added to *pHotnessNames[numHotnessLevels] table in OMRCompilation.cpp



// If adding new entries in TR_CallingContext, also add corresponding names in callingContextNames[] in J9Compilation.cpp
enum TR_CallingContext {
   NO_CONTEXT=0,
   FBVA_INITIALIZE_CONTEXT=OMR::numOpts,
   FBVA_ANALYZE_CONTEXT,
   BBVA_INITIALIZE_CONTEXT,
   BBVA_ANALYZE_CONTEXT,
   GRA_ASSIGN_CONTEXT,
   PRE_ANALYZE_CONTEXT,
   AFTER_INSTRUCTION_SELECTION_CONTEXT,
   AFTER_REGISTER_ASSIGNMENT_CONTEXT,
   AFTER_POST_RA_SCHEDULING_CONTEXT,
   REAL_TIME_ADDITIONAL_CONTEXTS=AFTER_POST_RA_SCHEDULING_CONTEXT,
   BEFORE_PROCESS_STRUCTURE_CONTEXT,
   GRA_FIND_LOOPS_AND_CORRESPONDING_AUTOS_BLOCK_CONTEXT,
   GRA_AFTER_FIND_LOOP_AUTO_CONTEXT,
   ESC_CHECK_DEFSUSES_CONTEXT,
   LAST_CONTEXT
};



// function return type flags for word preceding the entry point in any
// linkage that needs to be callable from interpreted.  Also used in pic
// for use when dispatching methods that are interpreted in megamorphic send
// case
enum TR_ReturnInfo
   {
   TR_VoidReturn        = 0,
   TR_IntReturn         = 1,
   TR_LongReturn        = 2,
   TR_FloatReturn       = 3,
   TR_DoubleReturn      = 4,
   TR_ObjectReturn      = 5, // Currently 64-bit platforms only
   TR_FloatXMMReturn    = 6, // IA32 and AMD64 only
   TR_DoubleXMMReturn   = 7, // IA32 and AMD64 only
   TR_ConstructorReturn = 8  // Return from constructor
   };



typedef CS2::PhaseTimingSummary<TR::Allocator> PhaseTimingSummary;
typedef CS2::LexicalBlockTimer<TR::Allocator>  LexicalTimer;

// for TR_Debug usage
typedef CS2::HashTable<void*, intptr_t, TR::Allocator> ToNumberMap;
typedef CS2::HashTable<void*, char*, TR::Allocator>    ToStringMap;
typedef CS2::HashTable<void*, List<const char>*, TR::Allocator> ToCommentMap;



typedef struct TR_HWPInstructionInfo
   {
   enum type
      {
      callInstructions = 0,
      indirectCallInstructions,
      returnInstructions,
      valueProfileInstructions,
      };

   void                  *_instruction;
   void                  *_data;
   type                   _type;
   } TR_HWPInstructionInfo;



typedef struct TR_HWPBytecodePCToIAMap
   {
   void      *_bytecodePC;
   void      *_instructionAddr;
   } TR_HWPBytecodePCToIAMap;



class TR_DevirtualizedCallInfo
   {
   public:
   TR_DevirtualizedCallInfo(TR::Node *callNode, TR_OpaqueClassBlock *thisType) : _callNode(callNode), _thisType(thisType) {}

   TR::Node *             _callNode;
   TR_OpaqueClassBlock * _thisType;
   };



struct TR_PeekingArgInfo
   {
   TR_ResolvedMethod *_method;
   const char **_args;
   int32_t *_lengths;
   };



class TR_PrefetchInfo
   {
   public:
   TR_ALLOC(TR_Memory::IsolatedStoreElimination)

   TR_PrefetchInfo(TR::TreeTop *defTree, TR::TreeTop *useTree,
                   TR::Node *addrNode, TR::Node *useNode,
                   int32_t offset, bool increasing) :
      _defTree(defTree),
      _useTree(useTree),
      _addrNode(addrNode),
      _useNode(useNode),
      _offset(offset),
      _increasing(increasing)
      {
      }

   TR::TreeTop *_defTree;
   TR::TreeTop *_useTree;
   TR::Node *_addrNode;
   TR::Node *_useNode;
   int32_t _offset;
   bool _increasing;
   };



class TR_ClassExtendCheck : public TR_Link<TR_ClassExtendCheck>
   {
public:
   TR_ClassExtendCheck(TR_OpaqueClassBlock *c) : _clazz(c) { }
   TR_OpaqueClassBlock *_clazz;
   };



class TR_ClassLoadCheck : public TR_Link<TR_ClassLoadCheck>
   {
public:
   TR_ClassLoadCheck(char * n, int32_t l) : _name(n), _length(l) { }

   char *  _name;
   int32_t _length;

   struct Method : TR_Link<Method>
      {
      Method(char * name, int32_t nameLen, char * sig, int32_t sigLen)
         : _name(name), _sig(sig), _nameLen(nameLen), _sigLen(sigLen)
         { }
      char * _name, * _sig;
      int32_t _nameLen, _sigLen;
      };

   TR_LinkHead<Method> _methods;
   };



class BitVectorPool
   {
   TR_Stack<TR_BitVector*> _pool;
   TR::Compilation* _comp;

	public:
      BitVectorPool(TR::Compilation* c);
      TR_BitVector* get();
      void release (TR_BitVector* v);
   };

#endif
