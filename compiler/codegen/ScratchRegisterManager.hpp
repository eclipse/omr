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

#ifndef SCRATCHREGISTERMANAGER_INCL
#define SCRATCHREGISTERMANAGER_INCL

#include <stdint.h>
#include "codegen/RegisterConstants.hpp"
#include "env/TRMemory.hpp"
#include "infra/List.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }

enum TR_ManagedScratchRegisterStates
   {
   msrUnassigned = 0x00,    // no register associated
   msrAllocated  = 0x01,    // register has been allocated
   msrDonated    = 0x02     // register was donated to the list (not allocated)
   };

class TR_ManagedScratchRegister
   {
public:

   TR_ALLOC(TR_Memory::ScratchRegisterManager)

   TR::Register *_reg;
   int32_t      _state;

   TR_ManagedScratchRegister(TR::Register *preg, TR_ManagedScratchRegisterStates pstate)
      {
      _reg = preg;
      _state = pstate;
      }
   };


/**
 * The scratch register manager addresses a peculiarity of internal control flow
 * regions: all virtual registers used inside internal control flow must appear in
 * postconditions on the end-of-control-flow label in order for the local register
 * assigner to behave properly. Before the scratch register manager, If you had
 * complex logic in your internal control flow, requiring many short-lived
 * virtual registers, you had only two choices:
 * 
 * A. Add all these registers to the end-of-control-flow label. This causes all
 *    the registers to be live at the same time, increasing register pressure and
 *    causing unnecessary spills. It also stops working as soon as you need more
 *    virtuals than you have real registers in the processor.
 * 
 * B. Re-use virtuals. This is a very awkward ad-hoc approach that had a terrible
 *    effect on the tree evaluator code. Often, the logic for a particular evaluator
 *    (like checkcast or write barriers) is split among several functions, so to
 *    reuse these temporary virtuals, they must be passed from one function to
 *    another as parameters. Some functions ended up with three or four such
 *    parameters with increasingly obscure names, and it became hard to tell
 *    when register reuse decisions were for correctness or for efficiency.
 * 
 * Neither of these choices is very appealing. B has the potential for generating
 * good code, but the functions that implement B quickly become very hard to
 * maintain.
 * 
 * What the scratch register manager (SRM) does is to keep track of which virtuals
 * are available for reuse. The steps look like this:
 * 
 * 1. The code that creates an internal control flow region also creates an SRM The
 * 2. SRM is passed around to all the tree evaluator code implementing the internal
 *    control flow.
 * 3. Any code that wants a temporary register requests one from the SRM
 * 4. When the temporary register is no longer needed, it is returned to the SRM
 * 5. At the end, SRM is asked to add all its managed registers as
 *    postconditions to a given regdeps object.
 * 6. The end-of-control-flow label is created using those regdeps
 *
 * The effect is that you get the benefits of an optimally-implemented version of
 * B but with maintainability on par with ordinary virtual register allocation
 * outside an internal control flow region (where you'd just use allocateRegister
 * and stopUsingRegister).
 */
class TR_ScratchRegisterManager
   {

protected:

   TR::CodeGenerator *_cg;

   /// No more than '_capacity' scratch registers will be allowed to be created.
   ///
   int32_t _capacity;

   /// Number of scratch registers available so far.
   ///
   int32_t _cursor;

   List<TR_ManagedScratchRegister> _msrList;

public:

   TR_ALLOC(TR_Memory::ScratchRegisterManager)

   TR_ScratchRegisterManager(int32_t capacity, TR::CodeGenerator *cg);

   TR::Register *findOrCreateScratchRegister(TR_RegisterKinds rk = TR_GPR);
   bool donateScratchRegister(TR::Register *reg);
   bool reclaimScratchRegister(TR::Register *reg);

   int32_t getCapacity()          { return _capacity; }
   void    setCapacity(int32_t c) { _capacity = c; }

   int32_t numAvailableRegisters() { return _cursor; }

   void addScratchRegistersToDependencyList(TR::RegisterDependencyConditions *deps);
   void stopUsingRegisters();

   List<TR_ManagedScratchRegister>& getManagedScratchRegisterList() { return _msrList; }

   };

#endif
