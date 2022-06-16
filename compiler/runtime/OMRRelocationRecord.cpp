/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#include <stdint.h>
#include "omrcfg.h"
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Relocation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "infra/Assert.hpp"
#include "env/jittypes.h"
#include "env/VMAccessCriticalSection.hpp"
#include "infra/SimpleRegex.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/TRRelocationRecord.hpp"
#include "runtime/TRRelocationRuntime.hpp"
#include "runtime/TRRelocationTarget.hpp"

#if defined(__IBMCPP__) && !defined(AIXPPC) && !defined(LINUXPPC)
#define ASM_CALL __cdecl
#else
#define ASM_CALL
#endif
#if defined(TR_HOST_S390) || defined(TR_HOST_X86) // gotten from RuntimeAssumptions.cpp, should common these up
extern "C" void _patchVirtualGuard(uint8_t *locationAddr, uint8_t *destinationAddr, int32_t smpFlag);
#else
extern "C" void ASM_CALL _patchVirtualGuard(uint8_t*, uint8_t*, uint32_t);
#endif

uint8_t
OMR::RelocationRecordBinaryTemplate::type(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned8b(&_type);
   }

void
OMR::RelocationRecordBinaryTemplate::setSize(TR::RelocationTarget *reloTarget,uint16_t size)
   {
      return reloTarget->storeUnsigned16b(size, reinterpret_cast<uint8_t*>(&_size));
   }
void
OMR::RelocationRecordBinaryTemplate::setType(TR::RelocationTarget *reloTarget, uint8_t type)
   {
   return reloTarget->storeUnsigned8b(type,&_type);
   }

void
OMR::RelocationRecordBinaryTemplate::setFlags(TR::RelocationTarget *reloTarget,uint8_t flags)
   {
   return reloTarget->storeUnsigned8b(flags,&_flags);
   }


TR::RelocationRecord *
OMR::RelocationRecord::self()
   {
   return static_cast<TR::RelocationRecord *>(this);
   }

void
OMR::RelocationRecordGroup::setSize(TR::RelocationTarget *reloTarget,uintptr_t size)
   {
   reloTarget->storePointer((uint8_t *)size, (uint8_t *) _dataBuffer);
   }

uintptr_t
OMR::RelocationRecordGroup::size(TR::RelocationTarget *reloTarget)
   {
   return (uintptr_t)reloTarget->loadPointer((uint8_t *) _dataBuffer);
   }

TR::RelocationRecordBinaryTemplate *
OMR::RelocationRecordGroup::firstRecord(TR::RelocationTarget *reloTarget)
   {
   // first word of the group is a pointer size field for the entire group
   return (TR::RelocationRecordBinaryTemplate *) (((uintptr_t *)_dataBuffer)+1);
   }

TR::RelocationRecordBinaryTemplate *
OMR::RelocationRecordGroup::pastLastRecord(TR::RelocationTarget *reloTarget)
   {
   return (TR::RelocationRecordBinaryTemplate *) ((uint8_t *)_dataBuffer + size(reloTarget));
   }

int32_t
OMR::RelocationRecordGroup::applyRelocations(TR::RelocationRuntime *reloRuntime,
                                           TR::RelocationTarget *reloTarget,
                                           uint8_t *reloOrigin)
   {
   TR::RelocationRecordBinaryTemplate *recordPointer = firstRecord(reloTarget);
   TR::RelocationRecordBinaryTemplate *endOfRecords = pastLastRecord(reloTarget);

   while (recordPointer < endOfRecords)
      {
      TR::RelocationRecord storage;
      // Create a specific type of relocation record based on the information
      // in the binary record pointed to by `recordPointer`
      TR::RelocationRecord *reloRecord = TR::RelocationRecord::create(&storage, reloRuntime, reloTarget, recordPointer);
      int32_t rc = handleRelocation(reloRuntime, reloTarget, reloRecord, reloOrigin);
      if (rc != 0)
         return rc;

      recordPointer = reloRecord->nextBinaryRecord(reloTarget);
      }

   return 0;
   }

int32_t
OMR::RelocationRecordGroup::handleRelocation(TR::RelocationRuntime *reloRuntime,
                                           TR::RelocationTarget *reloTarget,
                                           TR::RelocationRecord *reloRecord,
                                           uint8_t *reloOrigin)
   {
   reloRecord->preparePrivateData(reloRuntime, reloTarget);
   return reloRecord->applyRelocationAtAllOffsets(reloRuntime, reloTarget, reloOrigin);
   }

#define FLAGS_RELOCATION_WIDE_OFFSETS   0x80
#define FLAGS_RELOCATION_EIP_OFFSET     0x40
#define FLAGS_RELOCATION_TYPE_MASK      (TR::ExternalRelocationTargetKindMask)
#define FLAGS_RELOCATION_FLAG_MASK      ((uint8_t) (FLAGS_RELOCATION_WIDE_OFFSETS | FLAGS_RELOCATION_EIP_OFFSET))

OMR::RelocationRecordAction
OMR::RelocationRecord::action(OMR::RelocationRuntime *reloRuntime)
   {
   return RelocationRecordAction::apply;
   }


TR::RelocationRecord *
OMR::RelocationRecord::create(TR::RelocationRecord *storage, TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, TR::RelocationRecordBinaryTemplate *record)
   {
   TR::RelocationRecord *reloRecord = NULL;
   // based on the type of the relocation record, create an object of a particular variety of OMR::RelocationRecord object
   uint8_t reloType = record->type(reloTarget);
   switch (reloType)
      {
      // an example switch-case; the implementations is runtime-specific
      case TR_ExternalRelocationTargetKind::TR_MethodCallAddress:
         TR_UNIMPLEMENTED();
         /** Example implementation would be to implement relocations here / overload this file following extensible classes technology.
          * For instance, if TR_MethodCallAddress is used in OtherProject, the line in OtherProjectRelocationRecord.cpp
          * (which includes TRRelocationRecord.hpp or, when adjusted - RelocationRecord.hpp) would be:
          * reloRecord =reinterpret_cast<TR::RelocationRecord*>( new (storage) OtherProject::RelocationRecordMethodCallAddress(reloRuntime,record));
          * The current assumption is that if a relocation is common enough, it could be added to OMR directly
          */
         break;
      default:
         // TODO: error condition
         TR_UNIMPLEMENTED();
         exit(0);
      }

   return reloRecord;
   }

void
OMR::RelocationRecord::clean(TR::RelocationTarget *reloTarget)
   {
   self()->setSize(reloTarget, 0);
   self()->setType(reloTarget,(OMR::RelocationRecordType)_record->_type);
   self()->setReloFlags(reloTarget, _record->_flags);
   }

int32_t
OMR::RelocationRecord::bytesInHeader(TR::RelocationTarget *reloTarget)
   {
   return OMR::RelocationRecord::getSizeOfAOTRelocationHeader(self()->type(reloTarget));
   }

TR::RelocationRecordBinaryTemplate *
OMR::RelocationRecordBinaryTemplate::self()
   {
   return static_cast<TR::RelocationRecordBinaryTemplate *>(this);
   }

TR::RelocationRecordBinaryTemplate *
OMR::RelocationRecord::nextBinaryRecord(TR::RelocationTarget *reloTarget)
   {
   return (TR::RelocationRecordBinaryTemplate*) (((uint8_t*)self()->_record) + self()->size(reloTarget));
   }

void
OMR::RelocationRecord::setSize(TR::RelocationTarget *reloTarget, uint16_t size)
   {
   reloTarget->storeUnsigned16b(size,(uint8_t *) &_record->_size);
   }

uint16_t
OMR::RelocationRecord::size(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &_record->_size);
   }


void
OMR::RelocationRecord::setType(TR::RelocationTarget *reloTarget, OMR::RelocationRecordType type)
   {
   reloTarget->storeUnsigned8b(type, (uint8_t *) &_record->_type);
   }

OMR::RelocationRecordType
OMR::RelocationRecord::type(TR::RelocationTarget *reloTarget)
   {
   return (OMR::RelocationRecordType)_record->type(reloTarget);
   }


void
OMR::RelocationRecord::setWideOffsets(TR::RelocationTarget *reloTarget)
   {
   self()->setFlag(reloTarget, FLAGS_RELOCATION_WIDE_OFFSETS);
   }

bool
OMR::RelocationRecord::wideOffsets(TR::RelocationTarget *reloTarget)
   {
   return (self()->flags(reloTarget) & FLAGS_RELOCATION_WIDE_OFFSETS) != 0;
   }

void
OMR::RelocationRecord::setPcRelative(TR::RelocationTarget *reloTarget)
   {
   self()->setFlag(reloTarget, FLAGS_RELOCATION_EIP_OFFSET);
   }

bool
OMR::RelocationRecord::pcRelative(TR::RelocationTarget *reloTarget)
   {
   return (self()->flags(reloTarget) & FLAGS_RELOCATION_EIP_OFFSET) != 0;
   }

void
OMR::RelocationRecord::setFlag(TR::RelocationTarget *reloTarget, uint8_t flag)
   {
   uint8_t flags = reloTarget->loadUnsigned8b((uint8_t *) &_record->_flags) | (flag & FLAGS_RELOCATION_FLAG_MASK);
   reloTarget->storeUnsigned8b(flags, (uint8_t *) &_record->_flags);
   }

uint8_t
OMR::RelocationRecord::flags(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned8b((uint8_t *) &_record->_flags) & FLAGS_RELOCATION_FLAG_MASK;
   }

void
OMR::RelocationRecord::setReloFlags(TR::RelocationTarget *reloTarget, uint8_t reloFlags)
   {
   TR_ASSERT((reloFlags & ~FLAGS_RELOCATION_FLAG_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
   uint8_t crossPlatFlags = self()->flags(reloTarget);
   uint8_t flags = crossPlatFlags | (reloFlags & ~FLAGS_RELOCATION_FLAG_MASK);
   reloTarget->storeUnsigned8b(flags, (uint8_t *) &_record->_flags);
   }

uint8_t
OMR::RelocationRecord::reloFlags(TR::RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned8b((uint8_t *) &_record->_flags) & ~FLAGS_RELOCATION_FLAG_MASK;
   }

void
OMR::RelocationRecord::preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget)
   {
      /**
       * Private data is used for caching computed data from what's stored in the record's binary representation
       * and the current runtime state, to prevent computing this value at every offset where this value should
       * be applied.
       *
       * This function can be used to query the value that should be inserted in the compiled code from the runtime
       * just once, instead of repeating the querying process every time the value is required.
       * The retrived value can be cached in _privateData field
       */
      TR_UNIMPLEMENTED();
   }

#undef FLAGS_RELOCATION_WIDE_OFFSETS
#undef FLAGS_RELOCATION_EIP_OFFSET
#undef FLAGS_RELOCATION_ORDERED_PAIR
#undef FLAGS_RELOCATION_TYPE_MASK
#undef FLAGS_RELOCATION_FLAG_MASK


bool
OMR::RelocationRecord::ignore(TR::RelocationRuntime *reloRuntime)
   {
   return false;
   }

int32_t
OMR::RelocationRecord::applyRelocationAtAllOffsets(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloOrigin)
   {
   if (self()->ignore(reloRuntime))
      {
      return 0;
      }

   if (self()->wideOffsets(reloTarget))
      {
      int32_t *offsetsBase = (int32_t *) (((uint8_t*)_record) + self()->bytesInHeader(reloTarget));
      int32_t *endOfOffsets = (int32_t *) self()->nextBinaryRecord(reloTarget);
      for (int32_t *offsetPtr = offsetsBase;offsetPtr < endOfOffsets; offsetPtr++)
         {
         int32_t offset = *offsetPtr;
         uint8_t *reloLocation = reloOrigin + offset;
         int32_t rc = self()->applyRelocation(reloRuntime, reloTarget, reloLocation);
         if (rc != 0)
            {
            return rc;
            }
         }
      }
   else
      {
      int16_t *offsetsBase = (int16_t *) (((uint8_t*)_record) + self()->bytesInHeader(reloTarget));
      int16_t *endOfOffsets = (int16_t *) self()->nextBinaryRecord(reloTarget);
      for (int16_t *offsetPtr = offsetsBase;offsetPtr < endOfOffsets; offsetPtr++)
         {
         int16_t offset = *offsetPtr;
         uint8_t *reloLocation = reloOrigin + offset;
         int32_t rc = self()->applyRelocation(reloRuntime, reloTarget, reloLocation);
         if (rc != 0)
            {
            return rc;
            }
         }
      }
      return 0;
   }


uint32_t OMR::RelocationRecord::_relocationRecordHeaderSizeTable[TR_NumExternalRelocationKinds] =
   {
   //This will be removed with further work on openJ9.
   (uint32_t) -1,
   };
