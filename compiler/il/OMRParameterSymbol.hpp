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

#ifndef OMR_PARAMETERSYMBOL_INCL
#define OMR_PARAMETERSYMBOL_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_PARAMETERSYMBOL_CONNECTOR
#define OMR_PARAMETERSYMBOL_CONNECTOR
namespace OMR { class ParameterSymbol; }
namespace OMR { typedef OMR::ParameterSymbol ParameterSymbolConnector; }
#endif

#include <stddef.h>
#include <stdint.h>
#include "env/KnownObjectTable.hpp"
#include "il/DataTypes.hpp"
#include "il/RegisterMappedSymbol.hpp"

namespace TR { class ParameterSymbol; }

namespace OMR
{

class OMR_EXTENSIBLE ParameterSymbol : public TR::RegisterMappedSymbol
   {

protected:

   ParameterSymbol(TR::DataType d, int32_t slot);

   ParameterSymbol(TR::DataType d, int32_t slot, size_t size);

   TR::ParameterSymbol * self();

public:

   template <typename AllocatorType>
   static TR::ParameterSymbol * create(AllocatorType, TR::DataType, int32_t);

   template <typename AllocatorType>
   static TR::ParameterSymbol * create(AllocatorType, TR::DataType, int32_t, size_t);

   int32_t  getParameterOffset()               { return _mappedOffset; }
   void     setParameterOffset(int32_t o);

   int32_t  getSlot();

   int32_t  getOrdinal()                       { return _ordinal; }
   void     setOrdinal(int32_t o)              { _ordinal = o; }

   int8_t   getLinkageRegisterIndex()          { return _registerIndex; }
   void     setLinkageRegisterIndex(int8_t li) { _registerIndex = li; }
   bool     isParmPassedInRegister()           { return (_registerIndex >= 0) ? true : false; }

   /**
    * @return The global register index assigned to this parameter, or -1 if
    *         a global register has not been assigned.
    */
   int8_t   getAssignedGlobalRegisterIndex()   { return _allocatedHigh; }

   /**
    * @brief Sets the global register index assigned to this parameter symbol.
    * @param[in] gr : the global register index
    */
   void     setAssignedGlobalRegisterIndex(int8_t gr) { _allocatedHigh = gr; }

   /**
    * @return The high global register index assigned to this parameter, or -1 if
    *         a global register index has not been assigned.
    */
   int8_t   getAssignedHighGlobalRegisterIndex() { return _allocatedHigh; }

   /**
    * @brief Sets the high global register index assigned to this parameter symbol.
    * @param[in] gr : the global register index
    */
   void     setAssignedHighGlobalRegisterIndex(int8_t gr) { _allocatedHigh = gr; }

   /**
    * @return The low global register index assigned to this parameter, or -1 if
    *         a global register index has not been assigned.
    */
   int8_t   getAssignedLowGlobalRegisterIndex() { return _allocatedLow; }

   /**
    * @brief Sets the low global register index assigned to this parameter symbol.
    * @param[in] gr : the global register index
    */
   void     setAssignedLowGlobalRegisterIndex(int8_t gr) { _allocatedLow = gr; }

   void*    getFixedType()                     { return _fixedType; }
   void     setFixedType(void* t)              { _fixedType = t; }

   bool     getIsPreexistent()                 { return _isPreexistent; }
   void     setIsPreexistent(bool t)           { _isPreexistent = t; }

   bool     hasKnownObjectIndex() { return _knownObjectIndex != TR::KnownObjectTable::UNKNOWN; }
   void     setKnownObjectIndex(TR::KnownObjectTable::Index i) { _knownObjectIndex = i; }
   TR::KnownObjectTable::Index getKnownObjectIndex() { return _knownObjectIndex; }

   void           setTypeSignature(const char * s, int32_t l) { _typeLength = l; _typeSignature = s; }
   const char *   getTypeSignature(int32_t & len) { len = _typeLength; return _typeSignature; }

private:

   const char *                _typeSignature;
   void *                      _fixedType;
   int32_t                     _typeLength;
   int32_t                     _ordinal;
   int8_t                      _registerIndex;
   int8_t                      _allocatedHigh;
   int8_t                      _allocatedLow;
   bool                        _isPreexistent;
   uintptr_t                   _addressSize;
   TR::KnownObjectTable::Index _knownObjectIndex;
   };

}

#endif
