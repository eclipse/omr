/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef OMR_OBJECT_MODEL_INCL
#define OMR_OBJECT_MODEL_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_OBJECT_MODEL_CONNECTOR
#define OMR_OBJECT_MODEL_CONNECTOR
namespace OMR { class ObjectModel; }
namespace OMR { typedef OMR::ObjectModel ObjectModelConnector; }
#endif

#include <stdint.h>        // for int32_t, int64_t, uint32_t
#include "env/jittypes.h"  // for uintptrj_t, intptrj_t
#include "il/DataTypes.hpp"

class TR_OpaqueClassBlock;
namespace OMR { class ObjectModel; }
namespace TR { class Compilation; }
namespace TR { class Node; }

namespace OMR
{

class ObjectModel
   {

   public:

   ObjectModel();

   void initialize() { }

   bool mayRequireSpineChecks() { return false; }

   bool generateCompressedObjectHeaders() { return false; }

   // This query answers whether or not this VM is capable of generating arraylet
   // trees.  This does not imply that it will for this compilation unit: you must ask
   // Compilation::generateArraylets() to answer that.
   //
   bool canGenerateArraylets() { return false; }

   bool useHybridArraylets() { return false; }
   bool usesDiscontiguousArraylets() { return false; }

   int32_t maxContiguousArraySizeInBytes() { return 0; }

   uintptrj_t contiguousArrayHeaderSizeInBytes() { return 0; }

   uintptrj_t discontiguousArrayHeaderSizeInBytes() { return 0; }

   bool isDiscontiguousArray(int32_t sizeInBytes) { return false; }
   bool isDiscontiguousArray(int32_t sizeInElements, int32_t elementSize) { return false; }
   bool isDiscontiguousArray(TR::Compilation* comp, uintptrj_t objectPointer);
   intptrj_t getArrayLengthInElements(TR::Compilation* comp, uintptrj_t objectPointer);
   uintptrj_t getArrayLengthInBytes(TR::Compilation* comp, uintptrj_t objectPointer);
   uintptrj_t getArrayElementWidthInBytes(TR::DataType type);
   uintptrj_t getArrayElementWidthInBytes(TR::Compilation* comp, uintptrj_t objectPointer);
   uintptrj_t decompressReference(TR::Compilation* comp, uintptrj_t compressedReference);


   int32_t compressedReferenceShiftOffset();

   int32_t compressedReferenceShift();

   uintptrj_t offsetOfObjectVftField() { return 0; }

   // --------------------------------------------------------------------------
   // Object shape
   //
   int32_t sizeofReferenceField();
   intptrj_t sizeofReferenceAddress();
   uintptrj_t elementSizeOfBooleanArray();
   uint32_t getSizeOfArrayElement(TR::Node * node);
   int64_t maxArraySizeInElementsForAllocation(TR::Node *newArray, TR::Compilation *comp);
   int64_t maxArraySizeInElements(int32_t knownMinElementSize, TR::Compilation *comp);
   bool nativeAddressesCanChangeSize() { return false; }

   int32_t arraySpineShift(int32_t width) { return 0; }
   int32_t arrayletMask(int32_t width) { return 0; }
   int32_t arrayletLeafIndex(int32_t index, int32_t elementSize) { return 0; }
   int32_t objectAlignmentInBytes() { return 0; }
   uintptrj_t offsetOfContiguousArraySizeField() { return 0; }
   uintptrj_t offsetOfDiscontiguousArraySizeField() { return 0; }
   uintptrj_t objectHeaderSizeInBytes() { return 0; }
   uintptrj_t offsetOfIndexableSizeField() { return 0; }

   /**
   * @brief: Returns true if concurrent scavenging enabled in the VM's GC
   */
   bool shouldGenerateReadBarriersForFieldLoads() { return false; };

   /**
   * @brief: Returns true if option for software read barriers is enabled in the VM's GC
   */
   bool shouldReplaceGuardedLoadWithSoftwareReadBarrier() { return false; };

   };
}

#endif
