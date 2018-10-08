/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#ifndef OMR_JITBUILDERREPLAY_INCL
#define OMR_JITBUILDERREPLAY_INCL

#include <iostream>
#include <fstream>
#include <map>
#include "ilgen/StatementNames.hpp"

namespace TR { class IlBuilderReplay; }
namespace TR { class MethodBuilder; }
namespace TR { class MethodBuilder; }
namespace TR { class IlBuilder; }

namespace OMR
{

class JitBuilderReplay
   {
   public:
   
   enum MethodFlag { CONSTRUCTOR_FLAG, BUILDIL_FLAG };

   JitBuilderReplay();
   virtual ~JitBuilderReplay();

   /**
    * @brief Subclasses override these functions to replay from different input formats
    * (helpers)
    */

    virtual void start();
    virtual void initializeMethodBuilder(TR::MethodBuilder * replay);
    virtual bool parseConstructor()              { return false; }
    virtual bool parseBuildIL()                  { return false; }
    virtual void StoreReservedIDs();

    virtual void handleMethodBuilder(uint32_t serviceID, char * tokens)         { }
    virtual void handleDefineLine(TR::MethodBuilder * mb, char * tokens)        { }
    virtual void handleDefineFile(TR::MethodBuilder * mb, char * tokens)        { }
    virtual void handleDefineName(TR::MethodBuilder * mb, char * tokens)        { }
    virtual void handleDefineParameter(TR::MethodBuilder * mb, char * tokens)   { }
    virtual void handleDefineArrayParameter(TR::MethodBuilder * mb, char * tokens)   { }
    virtual void handlePrimitiveType(TR::MethodBuilder * mb, char * tokens)     { }
    virtual void handleDefineReturnType(TR::MethodBuilder * mb, char * tokens)  { }
    virtual void handleDefineFunction(TR::MethodBuilder * mb, char * tokens)    { }
    virtual void handleDoneLocal(TR::MethodBuilder * mb, char * tokens)         { }

    virtual void handleAdd(TR::IlBuilder * ilmb, char * tokens)                 { }
    virtual void handleSub(TR::IlBuilder * ilmb, char * tokens)                 { }
    virtual void handleMul(TR::IlBuilder * ilmb, char * tokens)                 { }
    virtual void handleDiv(TR::IlBuilder * ilmb, char * tokens)                 { }
    virtual void handleCreateLocalArray(TR::IlBuilder * ilmb, char * tokens)    { }
    virtual void handleAnd(TR::IlBuilder * ilmb, char * tokens)                 { }
    virtual void handleOr(TR::IlBuilder * ilmb, char * tokens)                  { }
    virtual void handleXor(TR::IlBuilder * ilmb, char * tokens)                 { }
    virtual void handleLoad(TR::IlBuilder * ilmb, char * tokens)                { }
    virtual void handleLoadAt(TR::IlBuilder * ilmb, char * tokens)              { }
    virtual void handleStore(TR::IlBuilder * ilmb, char * tokens)               { }
    virtual void handleStoreAt(TR::IlBuilder * ilmb, char * tokens)             { }
    virtual void handleIndexAt(TR::IlBuilder * ilmb, char * tokens)             { }
    virtual void handleConstInt8(TR::IlBuilder * ilmb, char * tokens)           { }
    virtual void handleConstInt32(TR::IlBuilder * ilmb, char * tokens)          { }
    virtual void handleConstInt64(TR::IlBuilder * ilmb, char * tokens)          { }
    virtual void handleConstDouble(TR::IlBuilder * ilmb, char * tokens)         { }
    virtual void handleConstAddress(TR::IlBuilder * ilmb, char * tokens)         { }
    virtual void handleConvertTo(TR::IlBuilder * ilmb, char * tokens)           { }
    virtual void handleCall(TR::IlBuilder * ilmb, char * tokens)                { }
    virtual void handleForLoop(TR::IlBuilder * ilmb, char * tokens)             { }
    virtual void handleReturn(TR::IlBuilder * ilmb, char * tokens)              { }
    virtual void handleReturnValue(TR::IlBuilder * ilmb, char * tokens)         { }
    virtual bool handleService(char * service)                                  { return false; }
    virtual void handleLessThan(TR::IlBuilder * ilmb, char * tokens)            { }
    virtual void handleGreaterThan(TR::IlBuilder * ilmb, char * tokens)         { }
    virtual void handleNotEqualTo(TR::IlBuilder * ilmb, char * tokens)          { }
    virtual void handleIfThenElse(TR::IlBuilder * ilmb, char * tokens)          { }
    virtual void handleAllLocalsHaveBeenDefined(TR::IlBuilder * ilmb, char * tokens) { }
    virtual void handleUnsignedShiftR(TR::IlBuilder * ilmb, char * tokens)      { }
    virtual void handleIfCmpEqualZero(TR::IlBuilder * ilmb, char * tokens)      { }
    virtual void handleNewIlBuilder(TR::IlBuilder * ilmb, char * tokens)        { }

    // Define Map that maps IDs to pointers

    typedef const uint32_t                      TypeID;
    typedef void *                              TypePointer;
    virtual void StoreIDPointerPair(TypeID, TypePointer);
    typedef std::map<TypeID, TypePointer> TypeMapPointer;

    protected:
    const TR::MethodBuilder *   _mb;
    TypeMapPointer                    _pointerMap;
    uint8_t                           _idSize;

    TypePointer lookupPointer(TypeID id);
    
   };

} // namespace OMR

 #endif // !defined(OMR_JITBUILDERREPLAY_INCL)
