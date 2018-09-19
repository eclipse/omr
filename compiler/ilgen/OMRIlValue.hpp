/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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

#ifndef OMR_ILVALUE_INCL
#define OMR_ILVALUE_INCL

#include "il/DataTypes.hpp" // for TR::DataType
#ifndef TR_ALLOC
#define TR_ALLOC(x)
#endif

namespace TR { class Node; }
namespace TR { class TreeTop; }
namespace TR { class Block; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class MethodBuilder; }
namespace TR { class IlValue; }

namespace OMR
{

class IlValue
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   /**
    * @brief initial state assumes value will only be used locally
    */
   IlValue(TR::Node *node, TR::TreeTop *treeTop, TR::Block *block, TR::MethodBuilder *methodBuilder);

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
   void storeOver(TR::IlValue *value, TR::Block *block);

   /**
    * @brief return the data type of this value
    */
   TR::DataType getDataType();

   int32_t get32bitConstValue();
   int64_t get64bitConstValue();

   /**
    * @brief returns the TR::SymbolReference holding the value, or NULL if willBeUsedInAnotherBlock() has not yet been called
    * Caller should ensure the current block is different than the block that computes the value; if the current block is the
    * same then it is better to use the value as a node pointer via GetNodePointerIfLegal().
    */
   TR::SymbolReference *getSymbolReference()
      {
      return _symRefThatCanBeUsedInOtherBlocks;
      }

   /**
    * @brief returns a unique identifier for this IlValue object within its MethodBuilder
    * Note that IlValues from different MethodBuilders can have the same ID.
    */
   int32_t getID()
      {
      return _id;
      }

protected:
   /**
    * @brief ensures this value is accessible via an auto symbol, but only generates store if hasn't already been stored
    */
   void storeToAuto();

   /**
    * @brief identifying number for values guaranteed to be unique per MethodBuilder
    */
   int32_t               _id;

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
    * @brief Method builder object where value is computed
    */
   TR::MethodBuilder   * _methodBuilder;

   /**
    * @brief TR::SymbolReference for temp holding value if it needs to be used outside basic block that computes it
    */
   TR::SymbolReference * _symRefThatCanBeUsedInOtherBlocks;
   };

} // namespace OMR

#endif // !defined(OMR_ILVALUE_INCL)
