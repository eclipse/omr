/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "env/FrontEnd.hpp"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "env/FEBase_t.hpp"
#include "env/Processors.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOps.hpp"
#include "runtime/CodeMetaDataPOD.hpp"
#include "runtime/StackAtlasPOD.hpp"

//#include "util_api.h"

#define RANGE_NEEDS_FOUR_BYTE_OFFSET(r) (((r) >= (USHRT_MAX   )) ? 1 : 0)

namespace JitBuilder
{

FrontEnd *FrontEnd::_instance = 0;

FrontEnd::FrontEnd()
   : TR::FEBase<FrontEnd>()
   {
   TR_ASSERT(!_instance, "FrontEnd must be initialized only once");
   _instance = this;
   }

void
FrontEnd::reserveTrampolineIfNecessary(TR::Compilation *comp, TR::SymbolReference *symRef, bool inBinaryEncoding)
   {
   // Do we handle trampoline reservations? return here for now.
   return;
   }


TR_ResolvedMethod *
FrontEnd::createResolvedMethod(TR_Memory * trMemory, TR_OpaqueMethodBlock * aMethod,
                                  TR_ResolvedMethod * owningMethod, TR_OpaqueClassBlock *classForNewInstance)
   {
   return new (trMemory->trHeapMemory()) ResolvedMethod(aMethod);
   }

intptrj_t
FrontEnd::methodTrampolineLookup(TR::Compilation *comp, TR::SymbolReference *symRef, void *callSite)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

// -----------------------------------------------------------------------------

void
FrontEnd::encodeStackMap(
      TR_GCStackMap *map,
      uint8_t *location,
      bool encodeFourByteOffsets,
      uint32_t bytesPerStackMap,
      TR::Compilation *comp)
   {
   TR::CodeGenerator *cg = comp->cg();
   uint32_t lowCode = map->getLowestCodeOffset();

   // Encode lowest code offset of this map
   //
   if (encodeFourByteOffsets)
      {
      *(uint32_t *)location = lowCode;
      location += 4;
      }
   else
      {
      *(uint16_t *)location = lowCode;
      location += 2;
      }

   // Encode stack map
   //
   int32_t mapSize = map->getMapSizeInBytes();
   if (mapSize)
      {
      memcpy(location, map->getMapBits(), mapSize);
      }

   }


bool
FrontEnd::mapsAreIdentical(
      TR_GCStackMap *mapCursor,
      TR_GCStackMap *nextMapCursor,
      TR::GCStackAtlas *stackAtlas,
      TR::Compilation *comp)
   {
   if (nextMapCursor && nextMapCursor != stackAtlas->getParameterMap() &&
       mapCursor != stackAtlas->getParameterMap() &&
       mapCursor->getMapSizeInBytes() == nextMapCursor->getMapSizeInBytes() &&
       mapCursor->getRegisterMap() == nextMapCursor->getRegisterMap() &&
       !memcmp(mapCursor->getMapBits(), nextMapCursor->getMapBits(), mapCursor->getMapSizeInBytes()))
      {
      return true;
      }
   else
      {
      return false;
      }
   }


uint8_t *
FrontEnd::createStackAtlas(
      bool encodeFourByteOffsets,
      uint32_t numberOfSlotsMapped,
      uint32_t bytesPerStackMap,
      uint8_t *encodedAtlasBaseAddress,
      uint32_t atlasSizeInBytes,
      TR::Compilation *comp)
   {
   TR::CodeGenerator *cg = comp->cg();
   TR::GCStackAtlas *stackAtlas = cg->getStackAtlas();

   stackAtlas->setAtlasBits(encodedAtlasBaseAddress);

   // Calculate the size of each individual map in the atlas.  The fixed
   // portion of the map contains:
   //
   //    Low Code Offset (2 or 4)
   //    Stack map (depends on # of mapped parms/locals)
   //
   uint32_t sizeOfEncodedCodeOffsetInBytes = encodeFourByteOffsets ? 4 : 2;

   uint32_t sizeOfSingleEncodedMapInBytes = sizeOfEncodedCodeOffsetInBytes;
   sizeOfSingleEncodedMapInBytes += bytesPerStackMap;

   // Encode the atlas
   //
   OMR::StackAtlasPOD *pyAtlas = (OMR::StackAtlasPOD *)encodedAtlasBaseAddress;
   pyAtlas->numberOfMaps = stackAtlas->getNumberOfMaps();
   pyAtlas->bytesPerStackMap = bytesPerStackMap;

   // Offset to the MAPPED pyFrameObject parameter
   //
   pyAtlas->frameObjectParmOffset = 0;

   // Lowest stack offset where MAPPED locals begin.
   //
   pyAtlas->localBaseOffset = stackAtlas->getLocalBaseOffset();

   // Abort if we have overflowed the fields in pyAtlas.
   //
   if (bytesPerStackMap > USHRT_MAX ||
       stackAtlas->getNumberOfMaps() > USHRT_MAX ||
       stackAtlas->getNumberOfParmSlotsMapped() > USHRT_MAX ||
       stackAtlas->getParmBaseOffset()  < SHRT_MIN || stackAtlas->getParmBaseOffset()  > SHRT_MAX ||
       stackAtlas->getLocalBaseOffset() < SHRT_MIN || stackAtlas->getLocalBaseOffset() > SHRT_MAX)
      {
      comp->failCompilation<TR::CompilationException>("Overflowed the fields in pyAtlas");
      }

   // Maps are in reverse order in list from what we want in the atlas
   // so advance to the address where the last map should go and start
   // building the maps moving back toward the beginning of the atlas.
   //
   uint8_t *cursorInEncodedAtlas = encodedAtlasBaseAddress + atlasSizeInBytes;

   ListIterator<TR_GCStackMap> mapIterator(&stackAtlas->getStackMapList());
   TR_GCStackMap *mapCursor = mapIterator.getFirst();

   while (mapCursor != NULL)
      {
      // Move back from the end of the atlas till the current map can be fit in,
      // then pass the cursor to the routine that actually creates and fills in
      // the stack map
      //
      TR_GCStackMap *nextMapCursor = mapIterator.getNext();

      if (!mapsAreIdentical(mapCursor, nextMapCursor, stackAtlas, comp))
         {
         cursorInEncodedAtlas -= sizeOfSingleEncodedMapInBytes;
         encodeStackMap(mapCursor, cursorInEncodedAtlas, encodeFourByteOffsets, bytesPerStackMap, comp);
         }

      mapCursor = nextMapCursor;
      }

   return encodedAtlasBaseAddress;
   }


uint32_t
FrontEnd::calculateSizeOfStackAtlas(
      bool encodeFourByteOffsets,
      uint32_t numberOfSlotsMapped,
      uint32_t bytesPerStackMap,
      TR::Compilation *comp)
   {
   TR::CodeGenerator *cg = comp->cg();
   TR::GCStackAtlas * stackAtlas = cg->getStackAtlas();

   // Calculate the size of each individual map in the atlas.  The fixed
   // portion of the map contains:
   //
   //    Low Code Offset (2 or 4)
   //    Stack map (depends on # of mapped parms/locals)
   //
   uint32_t sizeOfEncodedCodeOffsetInBytes = encodeFourByteOffsets ? 4 : 2;
   uint32_t sizeOfSingleEncodedMapInBytes = sizeOfEncodedCodeOffsetInBytes;
   sizeOfSingleEncodedMapInBytes += bytesPerStackMap;

   // Calculate the atlas size
   //
   uint32_t atlasSize = sizeof(OMR::StackAtlasPOD);

   ListIterator<TR_GCStackMap> mapIterator(&stackAtlas->getStackMapList());
   TR_GCStackMap *mapCursor = mapIterator.getFirst();

   while (mapCursor != NULL)
      {
      TR_GCStackMap *nextMapCursor = mapIterator.getNext();

      if (!mapsAreIdentical(mapCursor, nextMapCursor, stackAtlas, comp))
         {
         atlasSize += sizeOfSingleEncodedMapInBytes;
         }

      mapCursor = nextMapCursor;
      }

   return atlasSize;
   }

} //namespace JitBuilder
