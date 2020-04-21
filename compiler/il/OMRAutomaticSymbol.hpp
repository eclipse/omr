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

#ifndef OMR_AUTOMATICSYMBOL_INCL
#define OMR_AUTOMATICSYMBOL_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_AUTOMATICSYMBOL_CONNECTOR
#define OMR_AUTOMATICSYMBOL_CONNECTOR
namespace OMR { class AutomaticSymbol; }
namespace OMR { typedef OMR::AutomaticSymbol AutomaticSymbolConnector; }
#endif

#include <stddef.h>
#include <stdint.h>
#include "codegen/RegisterConstants.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"

class TR_FrontEnd;
namespace TR { class AutomaticSymbol; }
namespace TR { class SymbolReference; }

namespace OMR
{

class OMR_EXTENSIBLE AutomaticSymbol : public TR::RegisterMappedSymbol
   {

public:

   template <typename AllocatorType>
   static TR::AutomaticSymbol * create(AllocatorType);

   template <typename AllocatorType>
   static TR::AutomaticSymbol * create(AllocatorType, TR::DataType);

   template <typename AllocatorType>
   static TR::AutomaticSymbol * create(AllocatorType, TR::DataType, uint32_t);

   template <typename AllocatorType>
   static TR::AutomaticSymbol * create(AllocatorType, TR::DataType, uint32_t, const char *);

protected:

   AutomaticSymbol();

   AutomaticSymbol(TR::DataType d);

   AutomaticSymbol(TR::DataType d, uint32_t s);

   AutomaticSymbol(TR::DataType d, uint32_t s, const char *name);

   void init();

   TR::AutomaticSymbol * self();

public:

   /**
    * Reference counting methods.
    *
    * \note Since this data is only actually consumed and manipulated at
    *       codegen time, it should be incumbent on the code generators to
    *       maintain the reference count information.
    *
    * @{
    */
   rcount_t getReferenceCount()           { return _referenceCount; }
   rcount_t decReferenceCount()           { return --_referenceCount; }
   rcount_t setReferenceCount(rcount_t i);
   rcount_t incReferenceCount();
   /** @} */

private:

   rcount_t _referenceCount;

/**
 * TR_LocalObjectSymbol
 *
 * On stack object symbol
 *
 * @{
 */
public:

   /**
    * Local object symbol factory
    */
   template <typename AllocatorType>
   static TR::AutomaticSymbol * createLocalObject(AllocatorType  m,
                                                 int32_t          arrayType,
                                                 TR::DataType    d,
                                                 uint32_t         s,
                                                 TR_FrontEnd *    fe);

   /**
    * Local object symbol factory with specified kind.
    *
    * Currently, only supported kinds are TR::New, TR::newarray
    * and TR::anewarray
    */
   template <typename AllocatorType>
   static TR::AutomaticSymbol * createLocalObject(AllocatorType          m,
                                                   TR::ILOpCodes          kind,
                                                   TR::SymbolReference *  classSymRef,
                                                   TR::DataType          d,
                                                   uint32_t               s,
                                                   TR_FrontEnd *          fe);

   TR::ILOpCodes getOpCodeKind();

   TR::SymbolReference *getClassSymbolReference();
   TR::SymbolReference *setClassSymbolReference(TR::SymbolReference *s);

   int32_t getArrayType();

   int32_t *getReferenceSlots()                { return _referenceSlots; }
   void     setReferenceSlots(int32_t *fields) { _referenceSlots = fields; }


private:
   //! Array of indices of collected reference slots, zero terminated
   int32_t * _referenceSlots;
   union
      {
      TR::SymbolReference *_classSymRef;
      int32_t               _arrayType;
      };


   /**
    * Store the kind of this local object.
    *
    * \todo Since there are only a small subset of the ILOpCode types that are
    *       valid here, we should really map the opcode to an internal enum,
    *       and then handle these types using a switch. This would allow the
    *       compiler to help us catch unhandled cases.
    */
   TR::ILOpCodes  _kind;
/** @} */

/**
 * TR_InternalPointerAutomaticSymbol
 *
 * @{
 */
public:

   template <typename AllocatorType>
   static TR::AutomaticSymbol * createInternalPointer(AllocatorType m, TR::AutomaticSymbol *pinningArrayPointer = 0);

   template <typename AllocatorType>
   static TR::AutomaticSymbol * createInternalPointer(AllocatorType m, TR::DataType d, TR::AutomaticSymbol *pinningArrayPointer = 0);

   template <typename AllocatorType>
   static TR::AutomaticSymbol * createInternalPointer(AllocatorType m, TR::DataType d, uint32_t s, TR_FrontEnd * fe);

   TR::AutomaticSymbol *getPinningArrayPointer();

   // Expose base name on derived calls.
   using TR::Symbol::setPinningArrayPointer;

   TR::AutomaticSymbol *setPinningArrayPointer(TR::AutomaticSymbol *s);

private:

   TR::AutomaticSymbol *_pinningArrayPointer;

   /** @} */

   /**
    * TR_VariableSizeSymbol
    *
    * @{
    */

public:

   #define TR_VSS_NAME "VTS"

   template <typename AllocatorType>
   static TR::AutomaticSymbol * createVariableSized(AllocatorType m, uint32_t s);

   uint32_t getActiveSize();

   uint32_t setActiveSize(uint32_t s);

   /**
    * Flags for variable size symbols
    */
   enum
      {
      IsReferenced                  = 0x01, ///< was this symbol ever actually used in any instruction? -- if not then it does not have to be mapped
      IsAddressTaken                = 0x02, ///< address taken symbols have to be kept alive longer
      IsSingleUse                   = 0x04, ///< a temp ref created for a one off privatization in a single evaluator (doesn't use refCounts so automatically freed)
      };

   bool isReferenced();

   void setIsReferenced(bool b = true);

   bool isAddressTaken();

   void setIsAddressTaken(bool b = true);

   bool isSingleUse();

   void setIsSingleUse(bool b = true);

   TR::Node *getNodeToFreeAfter();

   TR::Node *setNodeToFreeAfter(TR::Node *n);

private:

   TR::Node *  _nodeToFreeAfter;
   uint32_t    _activeSize;
   flags8_t    _variableSizeSymbolFlags;

   /** @} */
   };
}

#endif
