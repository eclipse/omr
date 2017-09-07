/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef OMR_Power_SNIPPET_INCL
#define OMR_Power_SNIPPET_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_SNIPPET_CONNECTOR
#define OMR_SNIPPET_CONNECTOR
namespace OMR { namespace Power { class Snippet; } }
namespace OMR { typedef OMR::Power::Snippet SnippetConnector; }
#else
#error OMR::Power::Snippet expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/codegen/OMRSnippet.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }

namespace OMR
{

namespace Power
{

class OMR_EXTENSIBLE Snippet : public OMR::Snippet
   {
   public:

   Snippet(TR::CodeGenerator *cg, TR::Node * node, TR::LabelSymbol * label, bool isGCSafePoint);

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
         IsMonitorEnter,
         IsMonitorExit,
         IsReadMonitor,
         IsLockReservationEnter,
         IsLockReservationExit,
         IsArrayCopyCall,
      IsHeapAlloc,
      IsAllocPrefetch,
      IsNonZeroAllocPrefetch,
      IsRecompilation,
      IsForceRecompilation,
      IsStackCheckFailure,
      IsUnresolvedData,
      IsInterfaceCastSnippet,
      IsTLB1,
         IsTLB1LoadArgs,
      numKinds
      };

   virtual Kind getKind() { return IsUnknown; }
   };

}

}

#endif
