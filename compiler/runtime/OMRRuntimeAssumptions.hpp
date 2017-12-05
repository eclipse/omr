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
namespace TR { class PatchNOPedGuardSite; }
namespace TR { class PatchMultipleNOPedGuardSites; }
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

   /*
    * These functions are used to determine whether the runtime assumption falls within
    * a given range. Both bounds are inclusive.
    */
   virtual uint8_t *getFirstAssumingPC() = 0;
   virtual uint8_t *getLastAssumingPC() = 0;
   bool assumptionInRange(uintptrj_t start, uintptrj_t end) { return ((uintptrj_t)getFirstAssumingPC()) <= end && start <= ((uintptrj_t)getLastAssumingPC()); }

   virtual uintptrj_t getKey() { return _key; }

   virtual uintptrj_t hashCode() { return TR_RuntimeAssumptionTable::hashCode(getKey()); }
   virtual bool matches(uintptrj_t key) { return _key == key; }
   virtual bool matches(char *sig, uint32_t sigLen) { return false; }

   virtual TR::PatchNOPedGuardSite   *asPNGSite() { return 0; }
   virtual TR::PatchMultipleNOPedGuardSites   *asPMNGSite() { return 0; }
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

}  // namespace OMR

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

   virtual uint8_t *getFirstAssumingPC() { return NULL; }
   virtual uint8_t *getLastAssumingPC() { return NULL; }
   virtual void     dumpInfo() {};
   }; // TR::SentinelRuntimeAssumption

class PatchNOPedGuardSite : public OMR::LocationRedirectRuntimeAssumption
   {
   protected:
   PatchNOPedGuardSite(TR_PersistentMemory *pm, uintptrj_t key, TR_RuntimeAssumptionKind kind,
                       uint8_t *location, uint8_t *destination)
      : OMR::LocationRedirectRuntimeAssumption(pm, key), _location(location), _destination(destination) {}

   public:
   static  void compensate(bool isSMP, uint8_t *loc, uint8_t *dest);

   /**
    * This method is invoked to perform atomic patching at a given location to
    * unconditionally jump to a given destination when the runtime assumption
    * is violated. Location and destination being used here have been passed in
    * during the class constructor.
    */
   virtual void compensate(TR_FrontEnd *vm, bool isSMP, void *) { compensate(isSMP, _location, _destination); }

   virtual bool equals(OMR::RuntimeAssumption &other)
         {
         PatchNOPedGuardSite *site = other.asPNGSite();
         return site != 0 && _location == site->getLocation();
         }

   virtual PatchNOPedGuardSite *asPNGSite() { return this; }
   virtual uint8_t *getFirstAssumingPC() { return _location; }
   virtual uint8_t *getLastAssumingPC() { return _location; }
   uint8_t* getLocation() { return _location; }
   uint8_t* getDestination() { return _destination; }

   virtual void dumpInfo();

   private:
   uint8_t *_location;
   uint8_t *_destination;
   }; // TR::PatchNOPedGuardSite

class PatchSites
   {
   private:
   size_t    _refCount;
   size_t    _size;
   size_t    _maxSize;

   /**
    * List of location and destination pairs, eg. {loc0, dest0, loc1, dest1, ...}
    * This improves locality and reduces memory overhead
    */
   uint8_t **_patchPoints;

   // Cache lower and upper bounds
   uint8_t *_firstLocation;
   uint8_t *_lastLocation;

   bool internalContainsLocation(uint8_t *location);

   public:
   TR_PERSISTENT_ALLOC_THROW(TR_Memory::PatchSites);
   PatchSites(TR_PersistentMemory *pm, size_t maxSize);

   size_t   getSize() { return _size; }
   uint8_t *getLocation(size_t index);
   uint8_t *getDestination(size_t index);
   uint8_t *getFirstLocation() { return _firstLocation; }
   uint8_t *getLastLocation() { return _lastLocation; }

   void add(uint8_t *location, uint8_t *destination);

   bool equals(PatchSites *other);
   bool containsLocation(uint8_t *location);   

   void addReference();
   static void reclaim(PatchSites *sites);
   };

class PatchMultipleNOPedGuardSites : public OMR::LocationRedirectRuntimeAssumption
   {
   protected:
   PatchMultipleNOPedGuardSites(TR_PersistentMemory *pm, uintptrj_t key, TR_RuntimeAssumptionKind kind, PatchSites *sites)
      : OMR::LocationRedirectRuntimeAssumption(pm, key), _patchSites(sites) {}
   public:

   virtual void compensate(TR_FrontEnd *vm, bool isSMP, void *)
      {
      for (size_t i = 0; i < _patchSites->getSize(); ++i)
         TR::PatchNOPedGuardSite::compensate(isSMP, _patchSites->getLocation(i), _patchSites->getDestination(i));
      }

   virtual bool equals(OMR::RuntimeAssumption &other)
      {
      PatchMultipleNOPedGuardSites *site = other.asPMNGSite(); 
      return site != 0 && _patchSites->equals(site->getPatchSites());
      }

   virtual void reclaim()
      {
      PatchSites::reclaim(_patchSites);
      }

   virtual PatchMultipleNOPedGuardSites *asPMNGSite() { return this; }

   virtual uint8_t *getFirstAssumingPC() { return _patchSites->getFirstLocation(); }
   virtual uint8_t *getLastAssumingPC() { return _patchSites->getLastLocation(); }

   virtual PatchSites* getPatchSites() { return _patchSites; }
   virtual void dumpInfo();

   private:
   PatchSites *_patchSites;
   }; // TR::PatchMultipleNOPedGuardSites

}  // namespace TR

#endif
