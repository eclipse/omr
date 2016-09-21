/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#ifndef OMR_TREE_EVALUATOR_INCL
#define OMR_TREE_EVALUATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_TREE_EVALUATOR_CONNECTOR
#define OMR_TREE_EVALUATOR_CONNECTOR
namespace OMR { class TreeEvaluator; }
namespace OMR { typedef OMR::TreeEvaluator TreeEvaluatorConnector; }
#endif

#include <stddef.h>     // for NULL
#include <stdint.h>     // for int32_t, etc
#include "il/Node.hpp"  // for vcount_t
#include "il/SymbolReference.hpp"
#include "env/CompilerEnv.hpp"   // for TR::Compiler

class TR_OpaqueClassBlock;
namespace TR { class SymbolReference; }
namespace TR { class CodeGenerator; }
namespace TR { class Register; }

typedef TR::Register *(* TR_TreeEvaluatorFunctionPointer)(TR::Node *node, TR::CodeGenerator *codeGen);

namespace OMR
{

class TreeEvaluator
   {
   public:
   static bool instanceOfOrCheckCastNeedEqualityTest(TR::Node * castClassNode, TR::CodeGenerator *cg);
   static bool instanceOfOrCheckCastNeedSuperTest(TR::Node * castClassNode, TR::CodeGenerator *cg);

   static int32_t classDepth(TR::Node * castClassNode, TR::CodeGenerator * cg);
   static int32_t checkNonNegativePowerOfTwo(int32_t value);
   static int32_t checkNonNegativePowerOfTwo(int64_t value);
   static int32_t checkPositiveOrNegativePowerOfTwo(int32_t value);
   static int32_t checkPositiveOrNegativePowerOfTwo(int64_t value);

   static TR::Register *compressedRefsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *computeCCEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *unImpOpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *badILOpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   struct TR_ArithmeticOverflowCheckNodes { TR::Node *operationNode, *leftChild, *rightChild; };
   static bool nodeIsIArithmeticOverflowCheck(TR::Node *node, TR_ArithmeticOverflowCheckNodes *u);
   static bool nodeIsLArithmeticOverflowCheck(TR::Node *node, TR_ArithmeticOverflowCheckNodes *u);
   static bool nodeIsIAddOverflowCheck(TR::Node *node, TR_ArithmeticOverflowCheckNodes *u);
   static bool nodeIsISubOverflowCheck(TR::Node *node, TR_ArithmeticOverflowCheckNodes *u);
   static bool nodeIsLAddOverflowCheck(TR::Node *node, TR_ArithmeticOverflowCheckNodes *u);
   static bool nodeIsLSubOverflowCheck(TR::Node *node, TR_ArithmeticOverflowCheckNodes *u);
   static void evaluateNodesWithFutureUses(TR::Node *node, TR::CodeGenerator *cg);
   static void initializeStrictlyFutureUseCounts(TR::Node *node, vcount_t visitCount, TR::CodeGenerator *cg);

   /**
   * Checks whether memory barriers must be created for a given memory read operation.
   *
   * On SMP, memory barriers are need to avoid potential race conditions when reading
   * from a volatile variable.
   */
   static bool isReadMemoryBarrierNeeded(TR::Node* node) {
      return TR::Compiler->target.isSMP() && node->getSymbolReference()->getSymbol()->isSyncVolatile();
   }

   /**
    * Checks whether memory barriers must be created for a given memory write operation.
    *
    * On SMP, memory barriers are need to avoid potential race conditions when writing
    * to a volatile variable. They are also needed when a write operation is sensitive
    * to re-ordering with other non-volatile memory operations. This is the case when
    * atomically reading from a so called "shadow" symbols, which is an indirect memory
    * references (e.g. de-referencing an array element by index).
    */
   static bool isWriteMemoryBarrierNeeded(TR::Node* node) {
      auto symbol = node->getSymbolReference()->getSymbol();
      return TR::Compiler->target.isSMP() && (symbol->isSyncVolatile() || (symbol->isShadow() && symbol->isOrdered()));
   }

   protected:
   static bool isStaticClassSymRef(TR::SymbolReference * symRef);

   };

}

#define IS_8BIT_SIGNED(x)    ((x) == (  int8_t)(x))
#define IS_8BIT_UNSIGNED(x)  ((x) == ( uint8_t)(x))
#define IS_16BIT_SIGNED(x)   ((x) == ( int16_t)(x))
#define IS_16BIT_UNSIGNED(x) ((x) == (uint16_t)(x))
#define IS_32BIT_SIGNED(x)   ((x) == ( int32_t)(x))
#define IS_32BIT_UNSIGNED(x) ((x) == (uint32_t)(x))

#endif
