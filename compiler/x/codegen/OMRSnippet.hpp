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

#ifndef OMR_X86_SNIPPET_INCL
#define OMR_X86_SNIPPET_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_SNIPPET_CONNECTOR
#define OMR_SNIPPET_CONNECTOR
namespace OMR { namespace X86 { class Snippet; } }
namespace OMR { typedef OMR::X86::Snippet SnippetConnector; }
#else
#error OMR::X86::Snippet expected to be a primary connector, but an OMR connector is already defined
#endif


#include "compiler/codegen/OMRSnippet.hpp"
#include "env/CompilerEnv.hpp"

namespace TR { class X86GuardedDevirtualSnippet; }
namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }

namespace OMR
{

namespace X86
{

class OMR_EXTENSIBLE Snippet : public OMR::Snippet
   {
   public:

   Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label, bool isGCSafePoint);

   Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label);

   enum Kind
      {
      IsUnknown,
      IsCall,
         IsUnresolvedCall,
      IsIPicData,
      IsVPicData,
      IsUnresolvedVTableSlot,
      IsVirtualPIC,
      IsCheckFailure,
         IsCheckFailureWithResolve,
         IsBoundCheckWithSpineCheck,
      IsSpineCheck,
      IsConstantData,
      IsData,
      IsRecompilation,
      IsRestart,
         IsDivideCheck,
         IsForceRecompilation,
         IsGuardedDevirtual,
         IsHelperCall,
         IsFPConversion,
            IsFPConvertToInt,
            IsFPConvertToLong,
      IsUnresolvedDataIA32,
      IsUnresolvedDataAMD64,
      numKinds
      };

   virtual Kind getKind() { return IsUnknown; }
   };

}

}


inline const char *commentString() { return TR::Compiler->target.isLinux() ? "#" : ";"; }
inline const char *hexPrefixString() { return TR::Compiler->target.isLinux() ? "0x" : "0"; }
inline const char *hexSuffixString() { return TR::Compiler->target.isLinux() ? "" : "h"; }
inline const char *dbString() { return TR::Compiler->target.isLinux() ? ".byte" : "db"; }
inline const char *dwString() { return TR::Compiler->target.isLinux() ? ".short" : "dw"; }
inline const char *ddString() { return TR::Compiler->target.isLinux() ? ".int" : "dd"; }
inline const char *dqString() { return TR::Compiler->target.isLinux() ? ".quad" : "dq"; }

#endif
