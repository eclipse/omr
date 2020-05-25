/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef OMR_Z_SNIPPET_INCL
#define OMR_Z_SNIPPET_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_SNIPPET_CONNECTOR
#define OMR_SNIPPET_CONNECTOR
namespace OMR { namespace Z { class Snippet; } }
namespace OMR { typedef OMR::Z::Snippet SnippetConnector; }
#else
#error OMR::Z::Snippet expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/codegen/OMRSnippet.hpp"

#include <stdint.h>
#include "codegen/InstOpCode.hpp"
#include "env/jittypes.h"
#include "infra/Flags.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }
namespace TR { class SymbolReference; }

namespace OMR
{

namespace Z
{

class OMR_EXTENSIBLE Snippet : public OMR::Snippet
   {
   /** offset from the codebase -- the 1st Snippet code location */
   int32_t    _codeBaseOffset;
   int8_t     _pad_bytes;
   flags16_t  _zflags;

   /** For trace files: Snippet Target Address (addr of helper method/trampoline). */
   intptr_t  _snippetDestAddr;

   public:

   Snippet(TR::CodeGenerator *cg, TR::Node * node, TR::LabelSymbol *lab, bool requiresGCMap);

   Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label);

   enum Kind
      {
      IsUnknown,
      IsCall,
         IsUnresolvedCall,
      IsVirtual,
         IsVirtualUnresolved,
         IsInterfaceCall,
      IsHelperCall,
      IsRecompilation,
      IsStackCheckFailure,
      IsUnresolvedData,
      IsConstantData,
         IsConstantInstruction,
         IsWritableData,
         IsEyeCatcherData,
         IsInterfaceCallData,
         IsForceRecompData,
         IsJNICallData,
      IsHeapAlloc,
      IsForceRecomp,
      IsMonitorEnter,
      IsMonitorExit,
      IsRestoreGPR7,
      numKinds
      };

   virtual Kind getKind() { return IsUnknown; }

   int32_t setCodeBaseOffset(int32_t offset) { return _codeBaseOffset=offset; }
   int32_t getCodeBaseOffset() { return _codeBaseOffset; }

   int8_t setPadBytes(int8_t numOfBytes) { return _pad_bytes=numOfBytes; }
   int8_t getPadBytes() { return _pad_bytes; }

   intptr_t setSnippetDestAddr(intptr_t addr) {return _snippetDestAddr = addr;}
   intptr_t getSnippetDestAddr()               {return _snippetDestAddr;}

   uint8_t *generatePICBinary(TR::CodeGenerator *, uint8_t *, TR::SymbolReference *);
   uint32_t getPICBinaryLength(TR::CodeGenerator *);

   /** Helper method to reload VM Thread into GPR13 */
   uint8_t *generateLoadVMThreadInstruction(TR::CodeGenerator *cg, uint8_t *cursor);
   uint32_t getLoadVMThreadInstructionLength(TR::CodeGenerator *cg);

   // helper methods to insert Runtime Instrumentation hooks
   uint8_t *generateRuntimeInstrumentationOnOffInstruction(TR::CodeGenerator *cg, uint8_t *cursor, TR::InstOpCode::Mnemonic op, bool isPrivateLinkage = false);
   uint32_t getRuntimeInstrumentationOnOffInstructionLength(TR::CodeGenerator *cg, bool isPrivateLinkage = false);

   //functions to access literal pool flag
   bool getNeedLitPoolBasePtr()      { return _zflags.testAny(NeedLitPoolBasePtr); }
   void setNeedLitPoolBasePtr()      { _zflags.set(NeedLitPoolBasePtr); }
   void resetNeedLitPoolBasePtr()    { _zflags.reset(NeedLitPoolBasePtr); }

   // For trace files: whether snippet used MCC trampoline to reach target
   void setUsedTrampoline(bool used) { return _zflags.set(UsedTrampoline, used); }
   bool usedTrampoline()             { return _zflags.testAny(UsedTrampoline); }

   // Whether snippet was used or not.
   bool getIsUsed()                  { return _zflags.testAny(IsUsed); }
   void setIsUsed(bool isUsed)       { return _zflags.set(IsUsed, isUsed); }

private:
   enum // _zflags
      {
      NeedLitPoolBasePtr            = 0x0001, ///< For determining the proper load instructions for accessing the literal pool
      UsedTrampoline                = 0x0002, ///< For trace files: Whether Snippet uses MCC trampoline to reach target.
      IsUsed                        = 0x0004, ///< Whether current snippet was used or not.
      DummyLastEnum
      };
   };

}

}

#endif
