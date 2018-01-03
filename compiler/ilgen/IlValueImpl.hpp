/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#ifndef OMR_ILVALUE_IMPL_INCL
#define OMR_ILVALUE_IMPL_INCL

#ifndef TR_ILVALUE_IMPL_DEFINED
#define TR_ILVALUE_IMPL_DEFINED
#define PUT_OMR_ILVALUE_IMPL_INTO_TR
#endif // !defined(TR_ILVALUE_IMPL_DEFINED)

#include "ilgen/IlValue.hpp"

#include "il/DataTypes.hpp" // for TR::DataType

namespace TR { class Node; }
namespace TR { class TreeTop; }
namespace TR { class Block; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }

namespace TR { class IlValueImpl; }
namespace TR { class MethodBuilderImpl; }

namespace OMR
{

class IlValueImpl : public TR::IlValue
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   /**
    * @brief initial state assumes value will only be used locally
    */
   IlValueImpl(TR::Node *node, TR::TreeTop *treeTop, TR::Block *block, TR::MethodBuilderImpl *methodBuilder);

   /**
    * @brief return a TR::Node that computes this value, either directly if in same block or by loading a local variable if not
    * @param block TR::Block for the load point so it can be compared to the TR::Block where value is computed
    */
   TR::Node *load(TR::Block *block);

   /**
    * @brief ensures that the provided value replaces the current value, but only affects subsequent loads
    * @param value the new value that should be returned whenever the current value is loaded
    * @param block TR::Block for the replacement point, so that either the node pointer is changed or what's stored in the symbol reference
    */
   void storeOver(TR::IlValueImpl *value, TR::Block *block);

   /**
    * @brief return the data type of this value
    */
   TR::DataType getDataType();

   /**
    * @brief returns the TR::SymbolReference holding the value, or NULL if willBeUsedInAnotherBlock() has not yet been called
    * Caller should ensure the current block is different than the block that computes the value; if the current block is the
    * same then it is better to use the value as a node pointer via GetNodePointerIfLegal().
    */
   TR::SymbolReference *getSymbolReference()
      {
      return _symRefThatCanBeUsedInOtherBlocks;
      }

protected:
   /**
    * @brief ensures this value is accessible via an auto symbol, but only generates store if hasn't already been stored
    */
   void storeToAuto();

private:

   /**
    * @brief TR::Node pointer that computes the actual value
    */
   TR::Node            * _nodeThatComputesValue;

   /**
    * @brief TR::TreeTop that anchors this node (though not necessarily directly)
    */
   TR::TreeTop         * _treeTopThatAnchorsValue;

   /**
    * @brief TR::Block where the value is computed
    */
   TR::Block           * _blockThatComputesValue;

   /**
    * @brief TR::SymbolReference for temp holding value if it needs to be used outside basic block that computes it
    */
   TR::SymbolReference * _symRefThatCanBeUsedInOtherBlocks;
   };

} // namespace OMR



#if defined(PUT_OMR_ILVALUE_IMPL_INTO_TR)

namespace TR
{
   class IlValueImpl : public OMR::IlValueImpl
      {
      public:
         IlValueImpl(TR::Node *node, TR::TreeTop *treeTop, TR::Block *block, TR::MethodBuilderImpl *methodBuilder)
            : OMR::IlValueImpl(node, treeTop, block, methodBuilder)
            { }
      };

} // namespace TR

#endif // defined(PUT_OMR_ILVALUE_IMPL_INTO_TR)

#endif // !defined(OMR_ILVALUE_IMPL_INCL)
