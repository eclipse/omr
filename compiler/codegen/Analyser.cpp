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

#include "codegen/Analyser.hpp"

#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"

namespace TR { class SymbolReference; }

void TR_Analyser::setInputs(TR::Node     *firstChild,
                            TR::Register *firstRegister,
                            TR::Node     *secondChild,
                            TR::Register *secondRegister,
                            bool         nonClobberingDestination,
                            bool         dontClobberAnything,
                            TR::Compilation *comp,
                            bool         lockedIntoRegister1,
                            bool         lockedIntoRegister2)
   {
   _inputs = 0;

   if (firstRegister)
      {
      setReg1();
      }

   if (secondRegister)
      {
      setReg2();
      }

   // The VFT pointer can be low-tagged, so generally we can't operate on it
   // directly in memory.
   //
   TR::SymbolReference *vftPointerSymRef = TR::comp()->getSymRefTab()->element(TR::SymbolReferenceTable::vftSymbol);

   if (  firstChild->getOpCode().isMemoryReference()
      && firstChild->getSymbolReference() != vftPointerSymRef
      && firstChild->getReferenceCount() == 1
      && !lockedIntoRegister1)
      {
      setMem1();
      }

   if (  secondChild->getOpCode().isMemoryReference()
      && secondChild->getSymbolReference() != vftPointerSymRef
      && secondChild->getReferenceCount() == 1
      && !lockedIntoRegister2)
      {
      setMem2();
      }

   if (!dontClobberAnything)
      {

      if (!nonClobberingDestination)
         {
         if (firstChild == secondChild && firstChild->getReferenceCount() == 2)
            {
            setClob1();
            setClob2();
            }

         if (firstChild->getReferenceCount() == 1)
            {
            setClob1();
            }

         if (secondChild->getReferenceCount() == 1)
            {
            setClob2();
            }
         }
      else
         {
         // set both operands to be clobberable because know that instruction doesn't
         // actually clobber destination operand so want to make either one a possible
         // no-cost destination operand.
         setClob1();
         setClob2();
         }
      }
   }
