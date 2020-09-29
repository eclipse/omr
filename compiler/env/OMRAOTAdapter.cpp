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
#include "env/AOTAdapter.hpp"
#include "env/AOTStorageInterface.hpp"
#include "env/CompilerEnv.hpp"
#include <fstream>
#include "runtime/CodeCacheManager.hpp"
#include "compile/Compilation.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/AOTRelocationRuntime.hpp"
#include "infra/STLUtils.hpp"
#include <string.h>

OMR::AOTAdapter::AOTAdapter():
_methodNameToHeaderMap(str_comparator)
{}

TR::AOTAdapter *
OMR::AOTAdapter::self()
    {
    return static_cast<TR::AOTAdapter *>(this);
    }

void
OMR::AOTAdapter::initializeAOTClasses(TR::CodeCacheManager* cc)
    {
    _storage = new (PERSISTENT_NEW) TR::AOTStorageInterface();
    _reloRuntime = new (PERSISTENT_NEW) TR::AOTRelocationRuntime();
    _codeCacheManager = cc;
    }
 
void
OMR::AOTAdapter::storeExternalItem(const char *itemName, void* itemAddress)
    {
    _reloRuntime->registerPersistedItemAddress(itemName,itemAddress);
    }

void
OMR::AOTAdapter::storeAOTMethodAndDataInTheCache(const char* methodName)
    {
    TR::AOTMethodHeader* hdr = _methodNameToHeaderMap[methodName];
    _storage->storeEntry(methodName,hdr);
    }

TR::AOTMethodHeader*
OMR::AOTAdapter::loadAOTMethodAndDataFromTheCache(const char* methodName)
    {
    TR::AOTMethodHeader* methodHeader = _storage->loadEntry(methodName);
    if (methodHeader!=NULL)
        {
        self()->registerAOTMethodHeader(methodName,methodHeader);
        }

    return methodHeader;
    }

void
OMR::AOTAdapter::registerAOTMethodHeader(const char*  methodName,TR::AOTMethodHeader* header)
    {
    _methodNameToHeaderMap[methodName]=header;
    }


TR::RelocationRuntime*
OMR::AOTAdapter::getRelocationRuntime()
    {
    return _reloRuntime;
    }

TR::AOTStorageInterface*
OMR::AOTAdapter::getAOTStorageInterface()
    {
    return _storage;
    }

TR::AOTMethodHeader* 
OMR::AOTAdapter::createAndRegisterAOTMethodHeader(const char* methodName, uint8_t* codeStart,
                                                  uint32_t codeSize,uint8_t* dataStart, uint32_t dataSize)
    {
    TR::AOTMethodHeader* hdr = (TR::AOTMethodHeader*)new (TR::AOTMethodHeader) (codeStart,codeSize,dataStart,dataSize);
    self()->registerAOTMethodHeader(methodName,hdr );
    return hdr;
    }

void
OMR::AOTAdapter::relocateRegisteredMethod(const char* methodName)
    {
    TR::AOTMethodHeader* hdr = self()-> getRegisteredAOTMethodHeader(methodName);
    _reloRuntime->relocateMethodAndData(hdr);
    }

TR::AOTMethodHeader*
OMR::AOTAdapter::getRegisteredAOTMethodHeader(const char* methodName)
    {
    TR::AOTMethodHeader* result =_methodNameToHeaderMap[methodName];
    if (result==NULL)
        {
        result = self()->loadAOTMethodAndDataFromTheCache(methodName);
        }
    return result;
    }

void
OMR::AOTAdapter::storeHeaderForCompiledMethod(const char* methodName)
    {

    if (_methodNameToHeaderMap[methodName] != NULL)
        {
        self()->storeAOTMethodAndDataInTheCache(methodName);
        }
    }

void*
OMR::AOTAdapter::getMethodCode(const char* methodName)
    {
    TR::AOTMethodHeader* methodHeader = self()->getRegisteredAOTMethodHeader(methodName);
    if(void *codeStart = _reloRuntime->persistedItemAddress(const_cast<char*>(methodName))) {
        return codeStart;
    }
    if (methodHeader == NULL)
        return NULL;
    int32_t numReserved;
    TR::CodeCache *codeCache = _codeCacheManager->reserveCodeCache(false, methodHeader->getCompiledCodeSize(), 0, &numReserved);
    if(!codeCache)
        {
        return NULL;
        }
    size_t  methodLength = methodHeader->sizeOfSerializedVersion();
    uint8_t * coldCode = NULL;
    void * warmCode =  _codeCacheManager->allocateCodeMemory(methodLength, 0, &codeCache, &coldCode, false);
    if(!warmCode){
        codeCache->unreserve();
        return NULL;
    }
    memcpy(warmCode,methodHeader->getCompiledCodeStart(),methodLength);
    methodHeader->setCompiledCodeStart((uint8_t*)warmCode);
    _reloRuntime->registerPersistedItemAddress(methodName,warmCode);
    _codeCacheManager->unreserveCodeCache(codeCache);
    return warmCode;
    }
