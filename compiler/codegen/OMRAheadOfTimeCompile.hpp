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

#ifndef OMR_AHEADOFTIMECOMPILE_INCL
#define OMR_AHEADOFTIMECOMPILE_INCL

#ifndef OMR_AHEADOFTIMECOMPILE_CONNECTOR
#define OMR_AHEADOFTIMECOMPILE_CONNECTOR
namespace OMR { class AheadOfTimeCompile; }
namespace OMR { typedef OMR::AheadOfTimeCompile AheadOfTimeCompileConnector; }
#endif // OMR_AHEADOFTIMECOMPILE_CONNECTOR

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "infra/Link.hpp"
#include "infra/Annotations.hpp"
#include "runtime/Runtime.hpp"

class TR_Debug;
namespace TR { class ExternalRelocation; }
namespace TR { class IteratedExternalRelocation; }
namespace TR { class AheadOfTimeCompile; }

namespace OMR
{

class OMR_EXTENSIBLE AheadOfTimeCompile
   {
   public:

   TR_ALLOC(TR_Memory::AheadOfTimeCompile)

   AheadOfTimeCompile(uint32_t *headerSizeMap, TR::Compilation * c)
      : _comp(c), _sizeOfAOTRelocations(0),
        _relocationData(NULL),
        _aotRelocationKindToHeaderSizeMap(headerSizeMap)
   {}
   
   TR::AheadOfTimeCompile * self();

   TR::Compilation * comp()     { return _comp; }
   TR_Debug *   getDebug();
   TR_Memory *      trMemory();

   TR_LinkHead<TR::IteratedExternalRelocation>& getAOTRelocationTargets() {return _aotRelocationTargets;}

   uint32_t getSizeOfAOTRelocations()             {return _sizeOfAOTRelocations;}
   uint32_t setSizeOfAOTRelocations(uint32_t s)   {return (_sizeOfAOTRelocations = s);}
   uint32_t addToSizeOfAOTRelocations(uint32_t n) {return (_sizeOfAOTRelocations += n);}

   uint8_t *getRelocationData()           {return _relocationData;}
   uint8_t *setRelocationData(uint8_t *p) {return (_relocationData = p);}

   uint32_t getSizeOfAOTRelocationHeader(TR_ExternalRelocationTargetKind k)
      {
      return _aotRelocationKindToHeaderSizeMap[k];
      }

   uint32_t *setAOTRelocationKindToHeaderSizeMap(uint32_t *p)
      {
      return (_aotRelocationKindToHeaderSizeMap = p);
      }

   virtual void     processRelocations() = 0;
   virtual uint8_t *initializeAOTRelocationHeader(TR::IteratedExternalRelocation *relocation) = 0;

   // virtual void dumpRelocationData() = 0;
   void dumpRelocationData() {}

   void traceRelocationOffsets(uint8_t *&cursor, int32_t offsetSize, const uint8_t *endOfCurrentRecord, bool isOrderedPair);

   /**
    * Do project-specific processing of an AOT relocation just before combining
    * it into an IteratedExternalRelocation.
    */
   static void interceptAOTRelocation(TR::ExternalRelocation *relocation) { }

   private:
   TR::Compilation *                           _comp;
   TR_LinkHead<TR::IteratedExternalRelocation> _aotRelocationTargets;
   uint32_t                                   _sizeOfAOTRelocations;
   uint8_t                                   *_relocationData;
   uint32_t                                  *_aotRelocationKindToHeaderSizeMap;
   };

}

#endif
