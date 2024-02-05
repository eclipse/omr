/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef J9_PROJECT_SPECIFIC
#error Runtime assumptions require refactoring to be used outside of OpenJ9
#endif

#ifndef OMR_RUNTIME_ASSUMPTIONS_INCL
#define OMR_RUNTIME_ASSUMPTIONS_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/RuntimeAssumptionTable.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "infra/Link.hpp"


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

enum AssumptionSameJittedBodyFlags
   {
   MARK_FOR_DELETE=1
   };

class RuntimeAssumption
   {
   friend class ::TR_RuntimeAssumptionTable;

   protected:
   RuntimeAssumption(TR_PersistentMemory *, uintptr_t key)
      : _next(NULL), _key(key), _nextAssumptionForSameJittedBody(NULL) {}

   void addToRAT(TR_PersistentMemory * persistentMemory, TR_RuntimeAssumptionKind kind,
                 TR_FrontEnd *fe, RuntimeAssumption **sentinel);

   /**
    * Directly return the next runtime assumption regardless of whether it is marked for detach or not
    *
    * Note this API is only intended for use by utility methods of the RuntimeAssumptionTable
    * and must be run under the assumptionTableMutex
    */
   RuntimeAssumption *getNextEvenIfDead() { return _next; }

   /**
    * Update the next pointer to point to the specified RuntimeAssumption
    *
    * Note this API is only intended for use by utility methods of teh RuntimeAssumptionTable
    * and must be run under the assumptionTableMutex
    */
   void setNext(RuntimeAssumption *next){ _next = next; }

   /**
    * Directly return the next runtime assumption in the same jitted body
    *
    * Note this API is only intended for use by utility methods of the RuntimeAssumptionTable
    * and must be run under the assumptionTableMutex
    */
   RuntimeAssumption * getNextAssumptionForSameJittedBodyEvenIfDead() const
      { 
      return (RuntimeAssumption *)( ((uintptr_t)_nextAssumptionForSameJittedBody) & ~MARK_FOR_DELETE );
      }
   
   /**
    * Update the next pointer for the same jitted body to the specified RuntimeAssumption
    *
    * This update preserves the MARK_FOR_DELETE bit.
    * Note this API only intended for use by utility methods of the RuntimeAssumptionTable
    * and must be run under the assumptionTableMutex
    */ 
   void setNextAssumptionForSameJittedBody(RuntimeAssumption *link)
      {
      _nextAssumptionForSameJittedBody = (RuntimeAssumption*)(
         (uintptr_t)link | ( ((uintptr_t)_nextAssumptionForSameJittedBody) & MARK_FOR_DELETE )
      );
      }

   /**
    *
    * Note this API only intended for use by utility methods of the RuntimeAssumptionTable
    * and must be run under the assumptionTableMutex
    */
   bool enqueueInListOfAssumptionsForJittedBody(RuntimeAssumption **sentinel);
   /**
    *
    * Note this API only intended for use by utility methods of the RuntimeAssumptionTable
    * and must be run under the assumptionTableMutex
    */
   void dequeueFromListOfAssumptionsForJittedBody();

   /**
    * This utility destroys the data in the RuntimeAssumption to facilitate debugging
    */
   void paint()
      {
      _key = 0xDEADF00D;
      _nextAssumptionForSameJittedBody = 0;
      setNext(NULL);
      }

   /**
    * Mark this assumption for future removal from the RuntimeAssumptionTable
    *
    * Once marked the assumption will not be locatable in the RAT and the entry
    * will be reclaimed lazily at a future GC start.
    */
   void markForDetach()
      {
      _nextAssumptionForSameJittedBody = (RuntimeAssumption *)(
         ((uintptr_t)_nextAssumptionForSameJittedBody) | MARK_FOR_DELETE
      );
      }

   /**
    * Returns true when the assumption has previously been marked using the markForDetach routine
    */
   bool isMarkedForDetach() const 
      {
      return ( ((uintptr_t)(_nextAssumptionForSameJittedBody) & MARK_FOR_DELETE ) == MARK_FOR_DELETE); 
      }

   /** \brief
    *     Reclaims any persistent memory allocated by the runtime assumption.
    *
    *  \note
    *     This function should be called when we attempt to reclaim the runtime assumption right before deallocation.
    *     Calling this function too early can result in potentially accessing already deallocated memory, as the
    *     runtime assumption may still be examined/processed even after calling `compensate` and all the way up until
    *     the runtime assumption is deallocated.
    */
   virtual void reclaim() {}

   public:
   TR_PERSISTENT_ALLOC_THROW(TR_Memory::Assumption);

   RuntimeAssumption *getNext()
      {
      RuntimeAssumption *toReturn = _next;
      while (toReturn && toReturn->isMarkedForDetach())
         toReturn = toReturn->_next;
      return toReturn;
      }
   RuntimeAssumption * getNextAssumptionForSameJittedBody() const
      {
      RuntimeAssumption *toReturn = (RuntimeAssumption *)(((uintptr_t)_nextAssumptionForSameJittedBody)&~MARK_FOR_DELETE);
      while (toReturn && toReturn->isMarkedForDetach() && toReturn != this)
         toReturn = (RuntimeAssumption*)(((uintptr_t)toReturn->_nextAssumptionForSameJittedBody)&~MARK_FOR_DELETE);
      // handle the case where you have gone full circle
      return toReturn != this || !toReturn->isMarkedForDetach() ? toReturn : const_cast<RuntimeAssumption*>(this);
      }

   /**
    * @brief Returns the sentinel for the circular linked list of assumptions associated with a method body.
    *
    * @return Returns the sentinel RuntimeAssumption object.
    */
   RuntimeAssumption * getSentinel()
      {
      RuntimeAssumption *toReturn = (RuntimeAssumption *)(((uintptr_t)_nextAssumptionForSameJittedBody)&~MARK_FOR_DELETE);
      TR_ASSERT_FATAL(toReturn, "toReturn can't be NULL, this=%p\n", this);
      while (toReturn->getAssumptionKind() != RuntimeAssumptionSentinel)
         {
         TR_ASSERT_FATAL(toReturn != this, "Looped through assumptions for same jitted body without finding sentinel! toReturn=%p\n", toReturn);
         RuntimeAssumption *next = (RuntimeAssumption*)(((uintptr_t)toReturn->_nextAssumptionForSameJittedBody)&~MARK_FOR_DELETE);
         TR_ASSERT_FATAL(next, "next can't be NULL, toReturn=%p\n", toReturn);
         toReturn = next;
         }
      return toReturn;
      }

   virtual void     compensate(TR_FrontEnd *vm, bool isSMP, void *data) = 0;
   virtual bool     equals(RuntimeAssumption &other) = 0;

   /**
    * @brief Used to serialize an assumption to a buffer
    *
    * @param cursor Pointer into the buffer where the assumption to be is serialized into
    * @param owningMetadata pointer to a metadata structure associated with the compiled body
    *                       the current assumption is associated with.
    */
   virtual void serialize(uint8_t *cursor, uint8_t *owningMetadata) { TR_ASSERT_FATAL(false, "Should not be called\n"); }

   /**
    * @brief Provides the size of the serialized assumption
    *
    * @return Returns the size of the serialized assumption
    */
   virtual uint32_t size() { TR_ASSERT_FATAL(false, "Should not be called\n"); return 0; }

   /*
    * These functions are used to determine whether the runtime assumption falls within
    * a given range. Both bounds are inclusive.
    */
   virtual uint8_t *getFirstAssumingPC() = 0;
   virtual uint8_t *getLastAssumingPC() = 0;
   bool assumptionInRange(uintptr_t start, uintptr_t end) { return ((uintptr_t)getFirstAssumingPC()) <= end && start <= ((uintptr_t)getLastAssumingPC()); }

   virtual uintptr_t getKey() { return _key; }

   virtual uintptr_t hashCode() { return TR_RuntimeAssumptionTable::hashCode(getKey()); }
   virtual bool matches(uintptr_t key) { return _key == key; }
   virtual bool matches(char *sig, uint32_t sigLen) { return false; }

   virtual TR::PatchNOPedGuardSite   *asPNGSite() { return 0; }
   virtual TR::PatchMultipleNOPedGuardSites   *asPMNGSite() { return 0; }
   virtual TR_PreXRecompile         *asPXRecompile() { return 0; }
   virtual TR_UnloadedClassPicSite  *asUCPSite() { return 0; }
   virtual TR_RedefinedClassPicSite *asRCPSite() { return 0; }
   virtual TR_PatchJNICallSite      *asPJNICSite() { return 0; }
   void dumpInfo(const char *subclassName);
   virtual void dumpInfo() = 0;
   virtual TR_RuntimeAssumptionKind getAssumptionKind() = 0;
   virtual RuntimeAssumptionCategory getAssumptionCategory() = 0;


   bool isAssumingMethod(void *metaData, bool reclaimPrePrologueAssumptions = false);
   bool isAssumingRange(uintptr_t rangeStartPC, uintptr_t rangeEndPC, uintptr_t rangeColdStartPC, uintptr_t rangeColdEndPC, uintptr_t rangeStartMD, uintptr_t rangeEndMD);

   private:
   RuntimeAssumption *_next;
   RuntimeAssumption *_nextAssumptionForSameJittedBody; // links assumptions that pertain to the same method body
                                                        // These should form a circular linked list with a sentinel
                                                        // Lower bit is used to mark assumptions that will be lazily removed
   protected:
   uintptr_t _key; // key for searching in the hashtable
   };

/**
 * An abstract class representing a category of runtime assumptions that will 
 * patch code at a particular location to unconditionally jump to a destination.
 */
class LocationRedirectRuntimeAssumption : public RuntimeAssumption
   {
   protected:
   LocationRedirectRuntimeAssumption(TR_PersistentMemory *pm, uintptr_t key)
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
   ValueModifyRuntimeAssumption(TR_PersistentMemory *pm, uintptr_t key)
      : RuntimeAssumption(pm, key) {}

   virtual RuntimeAssumptionCategory getAssumptionCategory() { return ValueModification; }
   };

}  // namespace OMR

namespace TR
{

class SentinelRuntimeAssumption : public OMR::RuntimeAssumption
   {
   public:
   SentinelRuntimeAssumption() :  _owningMetaData(NULL), RuntimeAssumption(NULL, 0)
      {
      setNextAssumptionForSameJittedBody(this); // pointing to itself means that the list is empty
      }
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionSentinel; }
   virtual OMR::RuntimeAssumptionCategory getAssumptionCategory() { return OMR::SentinelCategory; }
   virtual void     compensate(TR_FrontEnd *vm, bool isSMP, void *data) {}
   virtual bool     equals(RuntimeAssumption &other) { return false; }

   virtual uint8_t *getFirstAssumingPC() { return NULL; }
   virtual uint8_t *getLastAssumingPC() { return NULL; }
   virtual void     dumpInfo() {}

   void *getOwningMetadata()                     { return _owningMetaData; }
   void  setOwningMetadata(void *owningMetadata) { _owningMetaData = owningMetadata; }

   private:

   /* A pointer to the owning metadata.  This allows one to dangle a chain of runtime assumptions
    * associated with a specific compiled body off of a metadata structure that describes said
    * body when reifying the assumptions is necessary.
    */
   void * _owningMetaData;
   }; // TR::SentinelRuntimeAssumption

class PatchNOPedGuardSite : public OMR::LocationRedirectRuntimeAssumption
   {
   protected:
   PatchNOPedGuardSite(TR_PersistentMemory *pm, uintptr_t key, TR_RuntimeAssumptionKind kind,
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
   PatchMultipleNOPedGuardSites(TR_PersistentMemory *pm, uintptr_t key, TR_RuntimeAssumptionKind kind, PatchSites *sites)
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
