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

#include "codegen/Linkage.hpp"            // for Linkage
#include "codegen/RegisterConstants.hpp"  // for TR_RegisterKinds, etc
#include "il/ILOps.hpp"                   // for ILOpCode
#include "il/Node.hpp"                    // for Node
#include "il/symbol/ParameterSymbol.hpp"  // for ParameterSymbol

TR::Linkage *
OMR::Linkage::self()
   {
   return static_cast<TR::Linkage*>(this);
   }

bool
OMR::Linkage::hasToBeOnStack(TR::ParameterSymbol *parm)
   {
   // The "this" pointer of synchronized and JVMPI methods must be on the stack.
   // TODO: The PPC implementation is superior, but it uses
   // interfaces currently only supported by TR::PPCPrivateLinkage.  The first
   // order of business: currently, every platform's Linkage can access the
   // Compilation and/or CodeGenerator, but the common one can't.  This
   // prevents us from checking that the current method is synchronized and so
   // forth, and makes the implementation below more conservative than it could
   // be.  Once we move that facility to TR::Linkage, and this function could be
   // more selective.
   //
   return(parm->getAllocatedIndex()>=0       &&
          ( (  parm->getLinkageRegisterIndex()==0 &&
               parm->isCollectedReference()
            ) ||
            parm->isParmHasToBeOnStack()
          )
         );
   }

TR_RegisterKinds
OMR::Linkage::argumentRegisterKind(TR::Node *argumentNode)
   {
   if (argumentNode->getOpCode().isFloatingPoint())
      return TR_FPR;
   else if (argumentNode->getOpCode().isVector())
      return TR_VRF;
   else
      return TR_GPR;
   }
