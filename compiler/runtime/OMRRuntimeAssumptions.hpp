/******************************************************************************* 
 *
 * (c) Copyright IBM Corp. 2000, 2017
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/



#ifndef OMR_RUNTIME_ASSUMPTIONS_INCL
#define OMR_RUNTIME_ASSUMPTIONS_INCL

#include <stddef.h>                        // for NULL
#include <stdint.h>                        // for int32_t, uint8_t, etc
#include "env/RuntimeAssumptionTable.hpp"  // for TR_RuntimeAssumptionKind, etc
#include "env/TRMemory.hpp"                // for TR_Memory, etc
#include "env/jittypes.h"                  // for uintptrj_t
#include "infra/Link.hpp"                  // for TR_LinkHead0, TR_Link0


class TR_FrontEnd;
class TR_PatchJNICallSite;
class TR_PatchNOPedGuardSite;
class TR_PreXRecompile;
class TR_RedefinedClassPicSite;
class TR_UnloadedClassPicSite;


namespace OMR
{

enum RuntimeAssumptionCategory
   {
   LocationRedirection = 0,
   ValueModification,
   SentinelCategory,
   LastRuntimeAssumptionCategory = SentinelCategory
   };

class RuntimeAssumption : public TR_Link0<RuntimeAssumption>
   {
   friend class ::TR_DebugExt;

   protected:
   RuntimeAssumption(TR_PersistentMemory *, uintptrj_t key)
      : _key(key), _nextAssumptionForSameJittedBody(NULL) {}

   void addToRAT(TR_PersistentMemory * persistentMemory, TR_RuntimeAssumptionKind kind,
                 TR_FrontEnd *fe, RuntimeAssumption **sentinel);
   public:
   TR_PERSISTENT_ALLOC_THROW(TR_Memory::Assumption);
   virtual void     reclaim() {}
   virtual void     compensate(TR_FrontEnd *vm, bool isSMP, void *data) = 0;
   virtual bool     equals(RuntimeAssumption &other) = 0;
   virtual uint8_t *getAssumingPC() = 0;
   virtual uintptrj_t getKey() { return _key; }

   virtual uintptrj_t hashCode() { return TR_RuntimeAssumptionTable::hashCode(getKey()); }
   virtual bool matches(uintptrj_t key) { return _key == key; }
   virtual bool matches(char *sig, uint32_t sigLen) { return false; }

   virtual TR_PatchNOPedGuardSite   *asPNGSite() { return 0; }
   virtual TR_PreXRecompile         *asPXRecompile() { return 0; }
   virtual TR_UnloadedClassPicSite  *asUCPSite() { return 0; }
   virtual TR_RedefinedClassPicSite *asRCPSite() { return 0; }
   virtual TR_PatchJNICallSite      *asPJNICSite() { return 0; }
   void dumpInfo(char *subclassName);
   virtual void dumpInfo() = 0;
   virtual TR_RuntimeAssumptionKind getAssumptionKind() = 0;
   virtual RuntimeAssumptionCategory getAssumptionCategory() = 0;


   bool isAssumingMethod(void *metaData, bool reclaimPrePrologueAssumptions = false);
   bool isAssumingRange(uintptrj_t rangeStartPC, uintptrj_t rangeEndPC, uintptrj_t rangeColdStartPC, uintptrj_t rangeColdEndPC, uintptrj_t rangeStartMD, uintptrj_t rangeEndMD);
   RuntimeAssumption * getNextAssumptionForSameJittedBody() const { return _nextAssumptionForSameJittedBody; }
   void setNextAssumptionForSameJittedBody(RuntimeAssumption *link) { _nextAssumptionForSameJittedBody = link; }

   bool enqueueInListOfAssumptionsForJittedBody(RuntimeAssumption **sentinel); // must be executed under assumptionTableMutex
   void dequeueFromListOfAssumptionsForJittedBody();
   void paint()
      {
      _key = 0xDEADF00D;
      _nextAssumptionForSameJittedBody = 0;
      setNext(NULL);
      }

   protected:
   RuntimeAssumption *_nextAssumptionForSameJittedBody; // links assumptions that pertain to the same method body
                                                           // These should form a circular linked list with a sentinel
   uintptrj_t _key; // key for searching in the hashtable
   };

/**
 * An abstract class representing a category of runtime assumptions that will 
 * patch code at a particular location to unconditionally jump to a destination.
 */
class LocationRedirectRuntimeAssumption : public RuntimeAssumption
   {
   protected:
   LocationRedirectRuntimeAssumption(TR_PersistentMemory *pm, uintptrj_t key)
      : RuntimeAssumption(pm, key) {}

   virtual RuntimeAssumptionCategory getAssumptionCategory() { return LocationRedirection; }
   };

/**
 * An abstract class representing a category of runtime assumptions that will 
 * patch code at a particular location to change its value to another value.
 */
class ValueModifyRuntimeAssumption : public RuntimeAssumption
   {
   protected:
   ValueModifyRuntimeAssumption(TR_PersistentMemory *pm, uintptrj_t key)
      : RuntimeAssumption(pm, key) {}

   virtual RuntimeAssumptionCategory getAssumptionCategory() { return ValueModification; }
   };

}

namespace TR
{

class SentinelRuntimeAssumption : public OMR::RuntimeAssumption
   {
   public:
   SentinelRuntimeAssumption() :  RuntimeAssumption(NULL, 0)
      {
      _nextAssumptionForSameJittedBody = this; // pointing to itself means that the list is empty
      }
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionSentinel; }
   virtual OMR::RuntimeAssumptionCategory getAssumptionCategory() { return OMR::SentinelCategory; }
   virtual void     compensate(TR_FrontEnd *vm, bool isSMP, void *data) {}
   virtual bool     equals(RuntimeAssumption &other) { return false; }

   virtual uint8_t *getAssumingPC() { return NULL; }
   virtual void     dumpInfo() {};
   }; // TR::SentinelRuntimeAssumption

}

#endif
