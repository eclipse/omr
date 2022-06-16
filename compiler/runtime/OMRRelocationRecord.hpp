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

#ifndef OMR_RELOCATION_RECORD_INCL
#define OMR_RELOCATION_RECORD_INCL
/*
 * The following #define(s) and typedef(s) must appear before any #includes in this file
 */
#ifndef OMR_RELOCATION_RECORD_CONNECTOR
#define OMR_RELOCATION_RECORD_CONNECTOR
namespace OMR { class RelocationRecord; }
namespace OMR { typedef OMR::RelocationRecord RelocationRecordConnector; }
#else
   #error OMR::RelocationRecord expected to be a primary connector, but another connector is already defined
#endif


#ifndef OMR_RELOCATION_RECORD_BINARY_TEMPLATE_CONNECTOR
#define OMR_RELOCATION_RECORD_BINARY_TEMPLATE_CONNECTOR
namespace OMR { class RelocationRecordBinaryTemplate; }
namespace OMR { typedef OMR::RelocationRecordBinaryTemplate RelocationRecordBinaryTemplateConnector; }
#else
   #error OMR::RelocationRecordBinaryTemplate expected to be a primary connector, but another connector is already defined
#endif


#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/jittypes.h"
#include "infra/Link.hpp"
#include "infra/Flags.hpp"
#include "runtime/TRRelocationRuntime.hpp"
#include "runtime/Runtime.hpp"

namespace TR {
class RelocationRuntime;
class RelocationTarget;
class RelocationRecord;
class RelocationRecordBinaryTemplate;
/**
 * @brief RelocationRecords can specify a struct for caching all the related values
 * for accellerating relocation application process through avoiding multiple computations
 * of the same value when applying the relocation at different offsets.
 */
struct RelocationRecordMethodCallPrivateData
   {
   uintptr_t callTargetOffset;
   };
/**
 * @brief This union is supposed to be a union of all possible PrivateData structs
 * allowing RelocationRecords allocating enough space for all kinds of RelocationRecordPrivateData
 */
union RelocationRecordPrivateData
   {
   struct RelocationRecordMethodCallPrivateData;
   };

}

namespace OMR { typedef TR_ExternalRelocationTargetKind RelocationRecordType;}
namespace TR { typedef TR_ExternalRelocationTargetKind RelocationRecordType;}
namespace TR { class RelocationRecordBinaryTemplate;}
extern char* AOTcgDiagOn;

// TR::RelocationRecord is the base class for all relocation records.  It is used for all queries on relocation
// records as well as holding all the "wrapper" parts.  These classes are an interface to the *BinaryTemplate
// classes which are simply structs that can be used to directly access the binary representation of the relocation
// records stored in the cache (*BinaryTemplate structs are defined near the end of this file after the
// RelocationRecord* classes.  The RelocationRecord* classes permit virtual function calls access to the
// *BinaryTemplate classes and must access the binary structs via the _record field in the TR::RelocationRecord
// class.  Most consumers should directly manipulate the TR::RelocationRecord* classes since they offer
// the most flexibility.
// No class that derives from TR::RelocationRecord should define any state: all state variables should be declared
//  in TR::RelocationRecord or the constructor/decode() mechanisms will not work properly
// Relocation record classes for "real" relocation record types
// should be defined on the level of consuming project

namespace OMR
{

class OMR_EXTENSIBLE RelocationRecordBinaryTemplate
   {
public:
   RelocationRecordBinaryTemplate(){};
   void setType(TR::RelocationTarget*, uint8_t kind);
   void setSize(TR::RelocationTarget*, uint16_t size);
   void setFlags(TR::RelocationTarget*, uint8_t flags);
   uint8_t type(TR::RelocationTarget *reloTarget);
   uint16_t _size;
   uint8_t _type;
   uint8_t _flags;
protected:
   TR::RelocationRecordBinaryTemplate* self();
   };

enum RelocationRecordAction
   {
   ignore,
   apply,
   failCompilation
   };

class  OMR_EXTENSIBLE  RelocationRecord
   {
public:
   RelocationRecord() {}
   RelocationRecord(TR::RelocationRuntime *reloRuntime, TR::RelocationRecordBinaryTemplate *record)
      {
      _reloRuntime= reloRuntime;
      _record =record;
      }

   void * operator new (size_t s, RelocationRecord *p)   { return p; }
   char *name() { return "RelocationRecord"; }

   bool isValidationRecord() { return false; }

   /**
    * @brief Using a pre-specified location (storage), create an instance of relocationRecord,
    * as it is described by its serialized version RelocationRecordBinaryTemplate. The operation
    * varies between architecture, hence RelocationTarget is made available
    *
    * @param storage - location where to create relocation record
    * @param reloRuntime - class for manipulations on relocations
    * @param reloTarget  - class for performing operations on relocations on binary level
    * @param recordPointer - pointer to a binary template - sequence of bytes describing the relocation
    * @return TR::RelocationRecord* - pointer cast to the generic relocationRecord.
    */
   static TR::RelocationRecord *create(TR::RelocationRecord *storage, TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, TR::RelocationRecordBinaryTemplate *recordPointer);

   void clean(TR::RelocationTarget *reloTarget);
   int32_t bytesInHeader(TR::RelocationTarget *reloTarget);
   /**
    * @brief This function is used to cache the values that are retrieved at run-time, during the code loading
    * Sometimes, computing the values required for updating the code is non-trivial and might add extra cost.
    * To avoid repeated computation, this function is used
    *
    * @param reloRuntime - a reference to relocation runtime to query the runtime for the required values
    * @param reloTarget - a reference to the cross-platform API to store/load values at various machine dependent granularity
    */
   void preparePrivateData(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget);

   int32_t applyRelocationAtAllOffsets(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *relocationOrigin);

   int32_t applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocation) {return -1;}
   int32_t applyRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow) {return -1;}

   TR::RelocationRecordBinaryTemplate *nextBinaryRecord(TR::RelocationTarget *reloTarget);
   TR::RelocationRecordBinaryTemplate *binaryRecord();

   void setSize(TR::RelocationTarget *reloTarget, uint16_t size);
   uint16_t size(TR::RelocationTarget *reloTarget);

   void setType(TR::RelocationTarget *reloTarget, TR::RelocationRecordType type);
   RelocationRecordType type(TR::RelocationTarget *reloTarget);

   void setWideOffsets(TR::RelocationTarget *reloTarget);
   bool wideOffsets(TR::RelocationTarget *reloTarget);

   void setPcRelative(TR::RelocationTarget *reloTarget);
   bool pcRelative(TR::RelocationTarget *reloTarget);

   void setFlag(TR::RelocationTarget *reloTarget, uint8_t flag);
   uint8_t flags(TR::RelocationTarget *reloTarget);

   void setReloFlags(TR::RelocationTarget *reloTarget, uint8_t reloFlags);
   uint8_t reloFlags(TR::RelocationTarget *reloTarget);

   TR::RelocationRuntime *_reloRuntime;
   bool ignore(TR::RelocationRuntime *reloRuntime);
   RelocationRecordAction action(RelocationRuntime *reloRuntime);
   static uint32_t getSizeOfAOTRelocationHeader(TR_ExternalRelocationTargetKind k)
      {
      return _relocationRecordHeaderSizeTable[k];
      }

protected:
   TR::RelocationRecord* self();

   TR::RelocationRecordPrivateData *privateData()
      {
      return &_privateData;
      }

   TR::RelocationRecordBinaryTemplate *_record;

   TR::RelocationRecordPrivateData _privateData;

   static uint32_t _relocationRecordHeaderSizeTable[TR_NumExternalRelocationKinds];
   };


class RelocationRecordGroup
   {
public:
   RelocationRecordGroup(TR::RelocationRecordBinaryTemplate *groupData) : _dataBuffer(groupData) {};

   void setSize(TR::RelocationTarget *reloTarget, uintptr_t size);
   uintptr_t size(TR::RelocationTarget *reloTarget);

   TR::RelocationRecordBinaryTemplate *firstRecord(TR::RelocationTarget *reloTarget);
   TR::RelocationRecordBinaryTemplate *pastLastRecord(TR::RelocationTarget *reloTarget);

   int32_t applyRelocations(TR::RelocationRuntime *reloRuntime,
                              TR::RelocationTarget *reloTarget,
                              uint8_t *reloOrigin);
private:
   int32_t handleRelocation(TR::RelocationRuntime *reloRuntime, TR::RelocationTarget *reloTarget, TR::RelocationRecord *reloRecord, uint8_t *reloOrigin);

   TR::RelocationRecordBinaryTemplate *_dataBuffer;
   };

} //namespace OMR

#endif   // OMR_RELOCATION_RECORD_INCL
