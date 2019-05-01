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

#ifndef OMR_CODE_METADATA_MANAGER_INCL
#define OMR_CODE_METADATA_MANAGER_INCL

/*
 * The following #define(s) and typedef(s) must appear before any #includes in this file
 */
#ifndef OMR_CODE_METADATA_MANAGER_CONNECTOR
#define OMR_CODE_METADATA_MANAGER_CONNECTOR
namespace OMR { class CodeMetaDataManager; }
namespace OMR { typedef OMR::CodeMetaDataManager CodeMetaDataManagerConnector; }
#endif

#ifndef OMR_METADATA_HASHTABLE_CONNECTOR
#define OMR_METADATA_HASHTABLE_CONNECTOR
namespace OMR { class MetaDataHashTable; }
namespace OMR { typedef OMR::MetaDataHashTable MetaDataHashTableConnector; }
#endif

#include <stdint.h>
#include "env/TRMemory.hpp"
#include "infra/Annotations.hpp"
#include "j9nongenerated.h"

namespace TR { class CodeCache; }
namespace TR { class CodeMetaDataManager; }
namespace TR { class MetaDataHashTable; }
namespace TR { struct MethodMetaDataPOD; }

namespace OMR
{
/**
 * Manages metadata about code produced by the compiler.
 *
 * This class is built around a singleton, that can be initialized and verified
 * by calling initializeCodeMetaDataManager.
 *
 * The class assumes that each code cache is registered with the
 * CodeMetaDataManager usign addCodeCache.
 *
 * The CodeMetaDataManager only manages pointers; It takes no ownership of the
 * POD pointers provided to it.
 */
class OMR_EXTENSIBLE CodeMetaDataManager
   {

   public:

   TR_PERSISTENT_ALLOC(TR_Memory::CodeMetaData);

   inline TR::CodeMetaDataManager *self();

   static TR::CodeMetaDataManager *codeMetaDataManager() { return _codeMetaDataManager; }

   bool initializeCodeMetaDataManager();

   /**
    * @brief For a given method's MethodMetaDataPOD, finds the appropriate
    * hashtable in the AVL tree and inserts the data pointer.

    * Note, insertMetaData does not check to verify that an metadata's given range
    * is not already occupied by an existing metadata.  This is because metadata  
    * ranges represent the virtual memory actually occupied by compiled code.
    * Thus, if two metadata ranges overlap, then two pieces of compiled code
    * co-exist on top of each other, which is inherently incorrect.  If the
    * possiblity occurs that two metadata may legitimately have range values that
    * overlap, checks will need to be added.

    * @param metaData The MetaDataPOD to insert into the metadata.
    * @return Returns true if successful, and false otherwise.
   */
   bool insertMetaData(TR::MethodMetaDataPOD *metaData);

   /**
    * @brief Determines if the metadata manager contain a reference to a particular
    * MethodMetaDataPOD.

    * Note: Just because the metadata manager return an metadata for a particular
    * startPC does not mean that it is the same metadata being searched for.
    * The method could have been recompiled, and then subsequently unloaded and
    * another method compiled to the same location before the recompilation-based
    * reclamation could occur.  This will result in the metadata manager containing a
    * different MethodMetaDataPOD for a given startPC.

    * @param  metadata The metadata for which to search for.
    * @return Returns true if the same metadata is found for the metadata 's startPC
    *         and false otherwise.
   */
   bool containsMetaData(const TR::MethodMetaDataPOD *metaData);

   /**
    * @brief Remove a given metadata from the metadata manager if it is found therein.
    *
    * @param  compiledMethod The MethodMetaDataPOD representing the method to
    *         remove from the JIT metadata .
    * @return Returns true if the metadata is found within, and successfully
    *         removed from, the JIT metadata and false otherwise.
   */
   bool removeMetaData(const TR::MethodMetaDataPOD *metaData);


   /**
    * @brief Attempts to find a registered metadata for a given metadata's startPC.
    * 
    * Note: findMetaDataForPC does not locally acquire the JIT metadata monitor.
    * Users must first manually acquire the monitor or go through another function
    * that does.
    *
    * @param pc The PC for which we require the JIT metadata .
    * @return If an metadata for a given startPC is successfully found, returns
    * that metadata , returns NULL otherwise.
    */
   const TR::MethodMetaDataPOD *findMetaDataForPC(uintptr_t pc);


   /**
    * @brief Register code cache with metadata manager. 
    *
    * Whenever a new codecache is allocated, register it with this metadata
    * manager so that subsequent insertion and query is aware of the contents.
    */
   TR::MetaDataHashTable *addCodeCache(TR::CodeCache *codeCache);


   protected:

   /**
    * @brief Initializes the translation metadata manager's members.
    *
    * The non-cache members are pointers to types instantiated externally.  The
    * constructor simply copies the pointers into the objects private members.
    */
   CodeMetaDataManager();


   /**
    * @brief Inserts an metadata into the metadata manager causing it to represent a
    * given memory range.
    *
    * Note this method expects to be called via another method in the metadata  
    * manager and thus does not acquire the metadata manager's monitor.
    *
    * @param metadata The MethodMetaDataPOD which will represent the given memory
    * range.
    * @param startPC The beginning of the memory range to represent.
    * @param endPC The end of the memory range to represent.
    * @return Returns true if the metadata was successfully inserted as
    * representing the given range, false otherwise.
    */
   bool insertRange(TR::MethodMetaDataPOD *metaData, uintptr_t startPC, uintptr_t endPC);

   /**
    * @brief Removes an metadata from the metadata manager  causing it to no longer
    * represent a given memory range.
    *
    * Note this method expects to be called via another method in the metadata  
    * manager and thus does not acquire the metadata manager's monitor.
    *
    * @param metadata The MethodMetaDataPOD to remove from representing the given
    * memory range.
    * @param startPC The beginning of the memory range the metadata represents.
    * @param endPC The end of the memory range the metadata represents.
    * @return Returns true if the metadata was successfully removed from
    * representing the given range, false otherwise.
    */
   bool removeRange(const TR::MethodMetaDataPOD *metaData, uintptr_t startPC, uintptr_t endPC);


   /**
    * @brief Determines if the current metadataManager query is using the same
    * metadata as the previous query, and if not, searches for and retrieves the
    * new metadata's code cache's hash table.
    *
    * Note this method expects to be called via another method in the metadata  
    * manager and thus does not acquire the metadata manager's monitor.
    *
    * @param currentPC The PC we are currently inquiring about.
    */
   void updateCache(uintptr_t currentPC);

   TR::MethodMetaDataPOD *findMetaDataInHash(
      TR::MetaDataHashTable *table,
      uintptr_t searchValue);

   uintptr_t insertMetaDataRangeInHash(
      TR::MetaDataHashTable *table,
      TR::MethodMetaDataPOD *dataToInsert,
      uintptr_t startPC,
      uintptr_t endPC);

   TR::MethodMetaDataPOD **insertMetaDataArrayInHash(
      TR::MetaDataHashTable *table,
      TR::MethodMetaDataPOD **array,
      TR::MethodMetaDataPOD *dataToInsert,
      uintptr_t startPC);

   TR::MethodMetaDataPOD **allocateMethodStoreInHash(TR::MetaDataHashTable *table);

   uintptr_t removeMetaDataRangeFromHash(
      TR::MetaDataHashTable *table,
      const TR::MethodMetaDataPOD *dataToRemove,
      uintptr_t startPC,
      uintptr_t endPC);

   TR::MethodMetaDataPOD **removeMetaDataArrayFromHash(
      TR::MethodMetaDataPOD **array,
      const TR::MethodMetaDataPOD *dataToRemove);

   TR::MetaDataHashTable *allocateCodeMetaDataHash(
      uintptr_t start,
      uintptr_t end);

   J9AVLTree *allocateMetaDataAVL();

   // Singleton: Protected to allow manipulation of singleton pointer 
   // in test cases. 
   static TR::CodeMetaDataManager *_codeMetaDataManager;

   J9AVLTree *_metaDataAVL;

   private:

   mutable uintptr_t _cachedPC;
   mutable TR::MetaDataHashTable *_cachedHashTable;
   mutable TR::MethodMetaDataPOD *_retrievedMetaDataCache;


   };


struct OMR_EXTENSIBLE MetaDataHashTable
   {
   J9AVLTreeNode parentAVLTreeNode;
   uintptr_t *buckets;
   uintptr_t start;
   uintptr_t end;
   uintptr_t flags;
   uintptr_t *methodStoreStart;
   uintptr_t *methodStoreEnd;
   uintptr_t *currentAllocate;
   };


extern "C"
{
intptr_t avl_jit_metadata_insertionCompare(J9AVLTree *tree, TR::MetaDataHashTable *insertNode, TR::MetaDataHashTable *walkNode);

intptr_t avl_jit_metadata_searchCompare(J9AVLTree *tree, uintptr_t searchValue, TR::MetaDataHashTable *walkNode);
}


}

#endif
