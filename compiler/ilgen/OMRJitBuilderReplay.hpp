/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

namespace TR { class MethodBuilder; }
namespace TR { class IlBuilder; }
namespace TR { class IlType; }
namespace TR { class IlValue; }

namespace OMR
{

class JitBuilderReplay
   {
   public:
   
   typedef const uint32_t                      TypeID;
   typedef void *                              TypePointer;
   typedef std::map<TypeID, TypePointer>       TypeMapPointer;

   JitBuilderReplay(const char *fileName);
   virtual ~JitBuilderReplay();

   void start();
   void StoreReservedIDs();

   /**
    * @brief Consumers for what has been recorded 
    * (consumers)
    */

    virtual void ConsumeStart()                         { }
    virtual const char * const ConsumeString()          { return NULL; }
    virtual int8_t Consume8bitNumber()                  { return 0; }
    virtual int16_t Consume16bitNumber()                { return 0; }
    virtual int32_t Consume32bitNumber()                { return 0; }
    virtual int64_t Consume64bitNumber()                { return 0; }
    virtual float ConsumeFloatNumber()                  { return 0.0F; }
    virtual double ConsumeDoubleNumber()                { return 0.0; }
    virtual TypeID ConsumeID()                          { return 0; }
    virtual const char * ConsumeStatement()             { return NULL; } 
    virtual TR::IlType * ConsumeType()                  { return static_cast<TR::IlType *>(0); }
    virtual TR::IlValue * ConsumeValue()                { return static_cast<TR::IlValue *>(0); }
    virtual TR::IlBuilder * ConsumeBuilder()            { return NULL; }
    virtual const void * ConsumeLocation()              { return NULL; }

    // Define Map that maps IDs to pointers

    virtual void registerMapping(TypeID, TypePointer);

    protected:
    TypeMapPointer                    _pointerMap;

    TypePointer lookupPointer(TypeID id);

    std::fstream _file;
    
   };

} // namespace OMR

 #endif // !defined(OMR_JITBUILDERREPLAY_INCL)
