/*******************************************************************************
 * Copyright (c) 2000 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "codegen/AheadOfTimeCompile.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "runtime/TRRelocationRecord.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "ras/DebugCounter.hpp"
#include "runtime/CodeCacheConfig.hpp"
#include "runtime/CodeCacheManager.hpp"

TR::AheadOfTimeCompile*
OMR::X86::AheadOfTimeCompile::self()
   {
   return static_cast<TR::AheadOfTimeCompile *> (this);
   }

#ifdef OMR_RELOCATION_RUNTIME
void
OMR::X86::AheadOfTimeCompile::processRelocations()
   {
   TR::Compilation* comp = self()->comp();
   TR_FrontEnd *fe = comp->fe();
   TR::CodeGenerator* cg = comp->cg();
   // calculate the amount of memory needed to hold the relocation data

   for (auto aotIterator = cg->getExternalRelocationList().begin(); aotIterator != cg->getExternalRelocationList().end(); ++aotIterator)
      (*aotIterator)->addExternalRelocation(cg);

   TR::IteratedExternalRelocation *r;
   for (r = self()->getAOTRelocationTargets().getFirst();
        r != NULL;
        r = r->getNext())
      {
      self()->addToSizeOfAOTRelocations(r->getSizeOfRelocationData());
      }

   // now allocate the memory  size of all iterated relocations + the header (total length field)

   // Note that when using the SymbolValidationManager, the well-known classes
   // must be checked even if no explicit records were generated, since they
   // might be responsible for the lack of records.

   if (self()->getSizeOfAOTRelocations() != 0)
      {
      // It would be more straightforward to put the well-known classes offset
      // in the AOT method header, but that would use space for AOT bodies that
      // don't use the SVM. TODO: Move it once SVM takes over?
      size_t pointerSize  = OMR::AheadOfTimeCompile::SIZEPOINTER;
      uint8_t*  relocationDataCursor = self()->setRelocationData(fe->allocateRelocationData(self()->comp(), self()->getSizeOfAOTRelocations() + sizeof(uintptr_t)));
      TR::RelocationRecordBinaryTemplate* groups = reinterpret_cast<TR::RelocationRecordBinaryTemplate*> (relocationDataCursor);
      TR::RelocationRuntime *reloRuntime =self()->reloRuntime();
      TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
      OMR::RelocationRecordGroup reloGroup(groups);

      reloGroup.setSize(reloTarget,self()->getSizeOfAOTRelocations() + pointerSize);
      self()->addToSizeOfAOTRelocations(pointerSize);
      relocationDataCursor += pointerSize;

      TR::IteratedExternalRelocation *s;
      for (s = self()->getAOTRelocationTargets().getFirst();
           s != NULL;
           s = s->getNext())
         {
         s->setRelocationData(relocationDataCursor);
         s->initializeRelocation(cg);
         relocationDataCursor += s->getSizeOfRelocationData();
         }
      }
}

uint8_t*
OMR::X86::AheadOfTimeCompile::initializeAOTRelocationHeader(TR::IteratedExternalRelocation *relocation)
   {
   TR::Compilation* comp = self()->comp();
   TR::CodeGenerator* cg = comp->cg();
   TR::RelocationRuntime *reloRuntime =self()->reloRuntime();
   TR::RelocationTarget *reloTarget = reloRuntime->reloTarget();
   uint8_t* cursor;
   TR::RelocationRecordBinaryTemplate* binaryTemplate = reinterpret_cast<TR::RelocationRecordBinaryTemplate *>(relocation->getRelocationData());
   uint16_t sizeOfRelocationData = relocation->getSizeOfRelocationData();
   binaryTemplate->setSize(reloTarget,sizeOfRelocationData);
   uint8_t targetKind = relocation->getTargetKind();
   binaryTemplate->setType(reloTarget,targetKind);
   uint8_t  wideOffsets = relocation->needsWideOffsets() ? RELOCATION_TYPE_WIDE_OFFSET : 0;
   binaryTemplate->setFlags(reloTarget,wideOffsets);

   // This has to be created after the kind has been written into the header
   TR::RelocationRecord storage;
   TR::RelocationRecord *reloRecord = TR::RelocationRecord::create(&storage,
                                       reloRuntime, reloTarget,
                                       reinterpret_cast<TR::RelocationRecordBinaryTemplate *>
                                       (relocation->getRelocationData()));
   switch (targetKind)
      {
         default:
         // initializeCommonOMRAOTRelocationHeader is currently in the process
         // of becoming the canonical place to initialize the platform agnostic
         // relocation headers; new relocation records' header should be
         // initialized here.
         cursor = self()->initializeCommonRelocationHeader(relocation, reloRecord);
      }
      return cursor;
   }
#endif // OMR_RELOCATION_RUNTIME
