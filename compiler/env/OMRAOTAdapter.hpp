/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#ifndef OMR_AOTADAPTER_INCL
#define OMR_AOTADAPTER_INCL
#include "env/TRMemory.hpp"
#include "env/TypedAllocator.hpp"
#include "infra/Annotations.hpp"
#include "compiler/env/jittypes.h"
#include <map>
#ifndef OMR_AOTADAPTER_CONNECTOR
#define OMR_AOTADAPTER_CONNECTOR
namespace OMR { class AOTAdapter; }
namespace OMR { typedef OMR::AOTAdapter AOTAdapterConnector; }
#endif

namespace TR 
   {
   class AOTAdapter;
   class AOTRelocationRuntime;
   class AOTStorageInterface;
   class CodeCache;
   class CodeCacheManager;
   class RelocationRuntime;
   class CompilerEnv;
   class AOTMethodHeader;
   }

namespace OMR
{

/**
 * @brief OMRAOTAdapter, an extensible class that is designed to dispatch 
 * the interaction between compiler components to perform persisting of 
 * relocatable code. The benefit of having this class is in decoupling AOT-related
 * classes and removing indirect responsibility by introducing new separation of
 * responsibilities: the RelocationRuntime class is only concerned with managing 
 * the generation and application of relocations, without knowing about persisting
 * methods implemented in the current runtime environment. Similarly, Shared Cache
 * or other options for persisting code could implement a simplistic interface
 * (that, obviously can be extended) that would have two functions: receive a message
 * that contains a serialized version of the AOTMethod header, as well as send one.
 * The AOTAdapter will register the header and make it available to RelocationRuntime
 * and hence, the compiler, as well as to runtime environment, since AOTAdapter is
 * planned to be alive longer than a compilation thread.
 */

class OMR_EXTENSIBLE AOTAdapter
   {
public:
   TR_ALLOC(TR_Memory::AOTAdapter)
   AOTAdapter();
   AOTAdapter(const AOTAdapter &src);

   /**
    * @brief Get the Relocation Runtime object
    * 
    * @return TR::RelocationRuntime* 
    */
   TR::RelocationRuntime* getRelocationRuntime();

   /**
    * @brief Accessor to the storage object
    * 
    * @return TR::AOTStorageInterface* 
    */
   TR::AOTStorageInterface* getAOTStorageInterface();

   void initializeAOTClasses(TR::CodeCacheManager* CodeCacheManager);
   /**
    * @brief method that inserts a string and an AOTMethodHeader key-value 
    * pair into the symbol table (currently realized as a map)
    * @param methodName 
    * @param hdr 
    */
   void registerAOTMethodHeader(const char *methodName, TR::AOTMethodHeader* hdr);

   /**
    * @brief a method that is checking whether there is a registered AOTMethodHeader 
    * with matching name. If such method is registered, it calls the 
    * storeAOTMethodAndDataInTheCache and passes the recently-retrieved AOTMethodHeader as an argument.
    * 
    * @param methodName 
    */
   void storeHeaderForCompiledMethod(const char *methodName);

   /**
    * @brief Register the method (or a data item )loaded to a runtime environment from another source—for example, 
    * from an external ELF file using dlopen() routine. This method would register it with a 
    * AOTRelocationRuntime, one that assumes there is a need to keep method addresses for relocation purposes.
    * 
    * @param itemName 
    * @param itemAddress 
    */
   void storeExternalItem(const char *itemName, void* itemAddress);

   /**
    * @brief The createAndRegisterAOTMethodHeader: a function that is supposed to be called from 
    * the CompileMethod.cpp, after successful JIT compilation of a method. This method would 
    * create a methodHeader and register it under an external name, as specified in the 
    * methodBuilder object
    * 
    * @param methodName 
    * @param codeStart 
    * @param codeSize 
    * @param dataStart 
    * @param dataSize 
    * @return TR::AOTMethodHeader* 
    */
   TR::AOTMethodHeader* createAndRegisterAOTMethodHeader(const char *methodName, uint8_t* codeStart, uint32_t codeSize,
                    uint8_t* dataStart, uint32_t dataSize);

   /**
    * @brief A function that would extract a method code from a registered AOTMethodHeader, 
    * request CodeCacheManager for a chunk of executable code and return a pointer to it. 
    * 
    * @param methodName - a string uniquely identifying a method. Maintaining the integrity 
    * and unique identification of the method by said string is on runtime constructor
    * responsibility. The string should later be replaced by appropriate calls to 
    * ResolvedMethod API
    * @return void* 
    */
   void *getMethodCode(const char *methodName);

   /**
    * @brief The relocateRegisteredMethod is a method accepting the name of the method that 
    * is desired to be relocated. The method retrieves the registered AOTMethodHeader by calling 
    * getRegisteredAOTMethodHeader and sending the result to the RelocationRuntime through 
    * prepareRelocateAOTCodeAndData that accepts AOTMethodHeader. 
    * 
    * @param methodName - a string uniquely identifying a method. Maintaining the integrity 
    * and unique identification of the method by said string is on runtime constructor
    * responsibility. The string should later be replaced by appropriate calls to 
    * ResolvedMethod API
    */
   void relocateRegisteredMethod(const char *methodName);

protected:
   /**
    * @brief The self() method, required by the extensible classes hierarchy
    * 
    * @return TR::AOTAdapter* 
    */
   TR::AOTAdapter* self();

   /**
    * @brief std::map requires an implementation of comparators
    * for types that are not parts of the standard c++ library
    */
   typedef bool (*StrComparator)(const char *, const char*);
   /**
    * @brief The map of consisting of AOTMethod headers, keyed by string variables. 
    * This map effectively represents a 'symbol table' for making the runtime environment
    * aware and able to access persisted information.  A subject to be replaced with
    * SymbolValidationManager
    */
   typedef std::map<const char *, TR::AOTMethodHeader*,StrComparator> NameToHeaderMap;
   NameToHeaderMap         _methodNameToHeaderMap;
   
private:
   /**
    * @brief The storeAOTMethodAndDataInTheCache accepts a method name and performs a 
    * lookup of an AOTMethodHeader in the map. Later, the AOTMethodHeader is serialized 
    * and is sent to the cache. 
    * 
    * @param methodName 
    */
   void storeAOTMethodAndDataInTheCache(const char *methodName);

   /**
    * @brief The loadAOTMethodAndDataFromTheCache performs a call to a Storage Object for
    * loading an entry keyed by method name. If there is a cache entry retrieved from the cache,
    * the AOTmethodHeader is deserialized. Later, it is registered in the AOTMethodHeader map, 
    * as well as with AOTRelocationRuntime
    * 
    * @param methodName a string uniquely identifying a method. Maintaining the integrity 
    * and unique identification of the method by said string is on runtime constructor
    * responsibility. The string should later be replaced by appropriate calls to 
    * ResolvedMethod API
    * @return TR::AOTMethodHeader* 
    */
   TR::AOTMethodHeader* loadAOTMethodAndDataFromTheCache(const char *methodName);

   /**
    * @brief The getRegisteredAotMethodHeader is a method that performs a lookup by name for an 
    * AOTMethodHeader in the map of registered AOTMethodHeaders. If the method cannot be located, 
    * the getRegisteredAotMethodHeader issues a call to loadAOTMethodAndDataFromTheCache.
    * 
    * @param methodName a string uniquely identifying a method. Maintaining the integrity 
    * and unique identification of the method by said string is on runtime constructor
    * responsibility. The string should later be replaced by appropriate calls to 
    * ResolvedMethod API
    * @return TR::AOTMethodHeader* 
    */
   TR::AOTMethodHeader* getRegisteredAOTMethodHeader(const char * methodName);

   /**
    * @brief A pointer to the object that is used as a storage
    * 
    */
   TR::AOTStorageInterface* _storage;

   /**
    * @brief Pointer to relocationRuntime that applies relocations
    * 
    */
   TR::AOTRelocationRuntime* _reloRuntime;
   /**
    * @brief A pointer to the CodeCache manager—this class manages memory that contains 
    * executable code, which is necessary if the AOT module is used to load code
    */
   TR::CodeCacheManager*    _codeCacheManager;


};
}
#endif // !defined(OMR_AOTADAPTER_INCL)
