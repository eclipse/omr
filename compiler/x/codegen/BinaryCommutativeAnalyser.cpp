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

#include "x/codegen/BinaryCommutativeAnalyser.hpp"

#include <stddef.h>                                 // for NULL
#include <stdint.h>                                 // for uint8_t, int32_t
#include "codegen/Analyser.hpp"                     // for NUM_ACTIONS
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator, etc
#include "codegen/FrontEnd.hpp"                     // for feGetEnv
#include "codegen/Machine.hpp"                      // for Machine
#include "codegen/MemoryReference.hpp"
#include "codegen/Register.hpp"                     // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"                 // for RegisterPair
#include "codegen/TreeEvaluator.hpp"                // for TreeEvaluator, etc
#include "codegen/X86Evaluator.hpp"
#include "compile/Compilation.hpp"                  // for Compilation, etc
#include "env/IO.hpp"                               // for POINTER_PRINTF_FORMAT
#include "env/CompilerEnv.hpp"
#include "il/ILOpCodes.hpp"                         // for ILOpCodes::lushr, etc
#include "il/ILOps.hpp"                             // for ILOpCode
#include "il/ILProps.hpp"
#include "il/Node.hpp"                              // for Node
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                            // for Symbol
#include "il/SymbolReference.hpp"                   // for SymbolReference
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"                     // for ::MOV4RegReg, etc

static void
zeroExtendTo32BitRegister(TR::Node          *node,
                          TR::Register      *reg,
                          int32_t           sourceSize,
                          TR::CodeGenerator *cg)
   {
   TR_X86OpCodes op;

   switch (sourceSize)
      {
      case 1:
         op = MOVZXReg4Reg1;
         break;
      case 2:
         op = MOVZXReg4Reg2;
         break;
      default:
         op = BADIA32Op;
         break;
      }

   if (op != BADIA32Op)
      generateRegRegInstruction(op, node, reg, reg, cg);
   }

/*
 * \brief 
 * this API is for check nodes(like OverflowCHK) with certain operation where the operands 
 * are given explicitly by the caller and are not the first and second child of the given root node
 *
 * \param root
 *     the check node
 * \param firstChild, secondChild 
 *     the operands for the operation 
 */
void TR_X86BinaryCommutativeAnalyser::genericAnalyserWithExplicitOperands(TR::Node      *root,
                                                                          TR::Node      *firstChild,
                                                                          TR::Node      *secondChild,
                                                                          TR_X86OpCodes regRegOpCode,
                                                                          TR_X86OpCodes regMemOpCode,
                                                                          TR_X86OpCodes copyOpCode,
                                                                          bool          nonClobberingDestination) //false by default
   {
   TR_ASSERT(root->getOpCodeValue() == TR::OverflowCHK, "unsupported  opcode %s for genericAnalyserWithExplicitOperands on node %p\n", _cg->comp()->getDebug()->getName(root->getOpCodeValue()), root);
   TR::Register *tempReg = genericAnalyserImpl(root, firstChild, secondChild, regRegOpCode, regMemOpCode, copyOpCode, nonClobberingDestination);
   _cg->decReferenceCount(firstChild);
   _cg->decReferenceCount(secondChild);
   _cg->stopUsingRegister(tempReg);
   }

/*
 * \brief 
 * this API is for regular operation nodes where the first child and second child are the operands by default
 */
void TR_X86BinaryCommutativeAnalyser::genericAnalyser(TR::Node      *root,
                                                      TR_X86OpCodes regRegOpCode,
                                                      TR_X86OpCodes regMemOpCode,
                                                      TR_X86OpCodes copyOpCode,
                                                      bool          nonClobberingDestination) //false by default
   {
   TR::Node *firstChild = NULL;
   TR::Node *secondChild = NULL;
   TR::Register *targetRegister = NULL;
   if (_cg->whichChildToEvaluate(root) == 0)
      {
      firstChild  = root->getFirstChild();
      secondChild = root->getSecondChild();
      setReversedOperands(false);
      }
   else
      {
      setReversedOperands(true);
      firstChild  = root->getSecondChild();
      secondChild = root->getFirstChild();
      }
   targetRegister = genericAnalyserImpl(root, firstChild, secondChild, regRegOpCode, regMemOpCode, copyOpCode, nonClobberingDestination);
   root->setRegister(targetRegister);
   _cg->decReferenceCount(firstChild);
   _cg->decReferenceCount(secondChild);
   }

/*
 * users should call the genericAnalyser or genericAnalyserWithExplicitOperands APIs instead of calling this one directly 
 */
TR::Register* TR_X86BinaryCommutativeAnalyser::genericAnalyserImpl(TR::Node      *root,
                                                                   TR::Node      *firstChild,
                                                                   TR::Node      *secondChild,
                                                                   TR_X86OpCodes regRegOpCode,
                                                                   TR_X86OpCodes regMemOpCode,
                                                                   TR_X86OpCodes copyOpCode,
                                                                   bool           nonClobberingDestination)
   {
   TR::Register *targetRegister;

   TR::Register *firstRegister  = firstChild->getRegister();
   TR::Register *secondRegister = secondChild->getRegister();

   setInputs(firstChild, firstRegister, secondChild, secondRegister, nonClobberingDestination);

   if (getEvalChild1())
      {
      firstRegister = _cg->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      secondRegister = _cg->evaluate(secondChild);
      firstRegister = firstChild->getRegister();
      }

   if (getOpReg1Reg2())
      {
      generateRegRegInstruction(regRegOpCode, root, firstRegister, secondRegister, _cg);
      targetRegister = firstRegister;
      }
   else if (getOpReg2Reg1())
      {
      generateRegRegInstruction(regRegOpCode, root, secondRegister, firstRegister, _cg);
      targetRegister = secondRegister;
      notReversedOperands();
      }
   else if (getCopyReg1())
      {
      TR::Register *tempReg;
      if (TR_X86OpCode::fprOp(copyOpCode) == 0)
         {
         tempReg = _cg->allocateRegister();
         }
      else if (TR_X86OpCode::singleFPOp(copyOpCode))
         {
         tempReg = _cg->allocateSinglePrecisionRegister(TR_X87);
         }
      else
         {
         tempReg = _cg->allocateRegister(TR_X87);
         }
      targetRegister = tempReg;
      generateRegRegInstruction(copyOpCode, root, tempReg, firstRegister, _cg);
      generateRegRegInstruction(regRegOpCode, root, tempReg, secondRegister, _cg);
      }
   else if (getCopyReg2())
      {
      TR::Register *tempReg;
      if (TR_X86OpCode::fprOp(copyOpCode) == 0)
         {
         tempReg = _cg->allocateRegister();
         }
      else if (TR_X86OpCode::singleFPOp(copyOpCode))
         {
         tempReg = _cg->allocateSinglePrecisionRegister(TR_X87);
         }
      else
         {
         tempReg = _cg->allocateRegister(TR_X87);
         }
      targetRegister = tempReg;
      generateRegRegInstruction(copyOpCode, root, tempReg, secondRegister, _cg);
      generateRegRegInstruction(regRegOpCode, root, tempReg, firstRegister, _cg);
      notReversedOperands();
      }
   else if (getOpReg1Mem2())
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(secondChild, _cg);
      if (regMemOpCode == TEST4MemReg || regMemOpCode == TEST8MemReg)
         {
         generateMemRegInstruction(regMemOpCode, root, tempMR, firstRegister, _cg);
         }
      else
         {
         generateRegMemInstruction(regMemOpCode, root, firstRegister, tempMR, _cg);
         }
      targetRegister = firstRegister;
      tempMR->decNodeReferenceCounts(_cg);
      }
   else
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(firstChild, _cg);
      if (regMemOpCode == TEST4MemReg || regMemOpCode == TEST8MemReg)
         {
         generateMemRegInstruction(regMemOpCode, root, tempMR, secondRegister, _cg);
         }
      else
         {
         generateRegMemInstruction(regMemOpCode, root, secondRegister, tempMR, _cg);
         }
      targetRegister = secondRegister;
      tempMR->decNodeReferenceCounts(_cg);
      notReversedOperands();
      }

   return targetRegister;
   }

void TR_X86BinaryCommutativeAnalyser::genericLongAnalyser(TR::Node       *root,
                                                           TR_X86OpCodes lowRegRegOpCode,
                                                           TR_X86OpCodes highRegRegOpCode,
                                                           TR_X86OpCodes lowRegMemOpCode,
                                                           TR_X86OpCodes lowRegMemOpCode2Byte,
                                                           TR_X86OpCodes lowRegMemOpCode1Byte,
                                                           TR_X86OpCodes highRegMemOpCode,
                                                           TR_X86OpCodes copyOpCode)
   {
   TR::Node *firstChild;
   TR::Node *secondChild;
   if (_cg->whichChildToEvaluate(root) == 0)
      {
      firstChild  = root->getFirstChild();
      secondChild = root->getSecondChild();
      setReversedOperands(false);
      }
   else
      {
      setReversedOperands(true);
      firstChild  = root->getSecondChild();
      secondChild = root->getFirstChild();
      }

   TR::Register *firstRegister  = firstChild->getRegister();
   TR::Register *secondRegister = secondChild->getRegister();

   bool firstHighZero      = false;
   bool secondHighZero     = false;
   bool useFirstHighOrder  = false;
   bool useSecondHighOrder = false;

   TR::Node *bypassedFirstUnsignedConversionChild = NULL;
   TR::Node *bypassedSecondUnsignedConversionChild = NULL;

   // Can generate better code for long operations when one or more children have their
   // high order word as a zero. Examples are xu2l conversion operators, right unsigned shifts by 32 or more bits
   // and just things that value propagation has found are in the range of an unsigned int.
   // we can avoid the evaluation when we don't need the result of such nodes for another parent.
   //
   if (firstChild->isHighWordZero())
      {
      firstHighZero = true;
      TR::ILOpCodes firstOp = firstChild->getOpCodeValue();
      if (firstChild->getReferenceCount() == 1 && firstRegister == 0)
         {
         // We cannot use a non-long grandchild that has not been zero-extended.
         if ((firstOp == TR::lushr &&
              firstChild->getSecondChild()->getOpCodeValue() == TR::iconst &&
              (firstChild->getSecondChild()->getInt() & TR::TreeEvaluator::shiftMask(true)) == 32))
            {
            bypassedFirstUnsignedConversionChild = firstChild;
            //only decrease RC for bypassed node and the constant node because we bypass the lushr node but keep the reference to its first child
            _cg->decReferenceCount(bypassedFirstUnsignedConversionChild);
            _cg->decReferenceCount(firstChild->getSecondChild());
            firstChild    = firstChild->getFirstChild();
            firstRegister = firstChild->getRegister();
            if (firstOp == TR::lushr)
               {
               useFirstHighOrder = true;
               }

            // adjust low opcode based on size of the child we're loading from mem (66327)
            switch (firstChild->getSize())
               {
               case 1 :
                  lowRegMemOpCode = lowRegMemOpCode1Byte;
                  break;

               case 2 :
                  lowRegMemOpCode = lowRegMemOpCode2Byte;
                  break;
               }
            }
         }
      }

   if (secondChild->isHighWordZero())
      {
      secondHighZero = true;
      TR::ILOpCodes secondOp = secondChild->getOpCodeValue();
      if (secondChild->getReferenceCount() == 1 && secondRegister == 0)
         {
         // We cannot use a non-long grandchild that has not been zero-extended.
         if ((secondOp == TR::lushr &&
              secondChild->getSecondChild()->getOpCodeValue() == TR::iconst &&
              (secondChild->getSecondChild()->getInt() & TR::TreeEvaluator::shiftMask(true)) == 32))
            {
            bypassedSecondUnsignedConversionChild = secondChild;
            //only decrease RC for bypassed node and the constant node because we bypass the lushr node but keep the reference to its first child
            _cg->decReferenceCount(bypassedSecondUnsignedConversionChild);
            _cg->decReferenceCount(secondChild->getSecondChild());
            secondChild    = secondChild->getFirstChild();
            secondRegister = secondChild->getRegister();
            if (secondOp == TR::lushr)
               {
               useSecondHighOrder = true;
               }

            // adjust low opcode based on size of the child we're loading from mem
            switch (secondChild->getSize())
               {
               case 1 :
                  lowRegMemOpCode = lowRegMemOpCode1Byte;
                  break;

               case 2 :
                  lowRegMemOpCode = lowRegMemOpCode2Byte;
                  break;
               }
            }
         }
      }

   // Bypassing a conversion child makes it behave a bit like a
   // pseudo-PassThrough node, and so we have to get very conservative about
   // when we clobber (at least until x86 codegen gets smarter about
   // pseudo-PassThroughs generally).
   //
   const bool dontClobberAnything = bypassedFirstUnsignedConversionChild || bypassedSecondUnsignedConversionChild;
   setInputs(firstChild, firstRegister, secondChild, secondRegister, false, dontClobberAnything);

   if (getEvalChild1())
      {
      firstRegister = _cg->evaluate(firstChild);

      if (bypassedFirstUnsignedConversionChild)
         zeroExtendTo32BitRegister(bypassedFirstUnsignedConversionChild, firstRegister, firstChild->getSize(), _cg);
      }

   if (getEvalChild2())
      {
      secondRegister = _cg->evaluate(secondChild);

      if (bypassedSecondUnsignedConversionChild)
         zeroExtendTo32BitRegister(bypassedSecondUnsignedConversionChild, secondRegister, secondChild->getSize(), _cg);
      }

   if (firstHighZero && firstRegister && firstRegister->getRegisterPair())
      {
      if (!useFirstHighOrder)
         {
         firstRegister = firstRegister->getLowOrder();
         }
      else
         {
         firstRegister = firstRegister->getHighOrder();
         }
      }

   if (secondHighZero && secondRegister && secondRegister->getRegisterPair())
      {
      if (!useSecondHighOrder)
         {
         secondRegister = secondRegister->getLowOrder();
         }
      else
         {
         secondRegister = secondRegister->getHighOrder();
         }
      }

   TR::Register *twoLow;
   TR::Register *twoHigh;
   TR::Register *oneLow;
   TR::Register *oneHigh;

   if (getOpReg1Reg2())
      {
      if (!firstHighZero)
         {
         oneLow  = firstRegister->getLowOrder();
         oneHigh = firstRegister->getHighOrder();
         }
      else
         {
         oneLow  = firstRegister;
         oneHigh = 0;
         }

      if (!secondHighZero)
         {
         twoLow  = secondRegister->getLowOrder();
         twoHigh = secondRegister->getHighOrder();
         }
      else
         {
         twoLow  = secondRegister;
         twoHigh = 0;
         }
      generateRegRegInstruction(lowRegRegOpCode, root, oneLow, twoLow, _cg);

      if (firstHighZero)
         {
         if (secondHighZero)
            {
            oneHigh = _cg->allocateRegister();
            generateRegRegInstruction(XOR4RegReg, root, oneHigh, oneHigh, _cg);
            }
         else
            {
            if (root->getOpCodeValue() == TR::land)
               {
               oneHigh = _cg->allocateRegister();
               generateRegRegInstruction(XOR4RegReg, root, oneHigh, oneHigh, _cg);
               }
            else
               {
               if (secondChild->getReferenceCount() == 1)
                  {
                  oneHigh = twoHigh;
                  }
               else
                  {
                  oneHigh = _cg->allocateRegister();
                  generateRegRegInstruction(copyOpCode, root, oneHigh, twoHigh, _cg);
                  }
               }
            }
         }
      else if (secondHighZero)
         {
         if (root->getOpCodeValue() == TR::land)
            {
            generateRegRegInstruction(XOR4RegReg, root, oneHigh, oneHigh, _cg);
            }
         }
      else
         {
         generateRegRegInstruction(highRegRegOpCode, root, oneHigh, twoHigh, _cg);
         }
      root->setRegister(_cg->allocateRegisterPair(oneLow, oneHigh));
      }
   else if (getOpReg2Reg1())
      {
      if (!firstHighZero)
         {
         oneLow  = firstRegister->getLowOrder();
         oneHigh = firstRegister->getHighOrder();
         }
      else
         {
         oneLow  = firstRegister;
         oneHigh = 0;
         }

      if (!secondHighZero)
         {
         twoLow  = secondRegister->getLowOrder();
         twoHigh = secondRegister->getHighOrder();
         }
      else
         {
         twoLow  = secondRegister;
         twoHigh = 0;
         }

      generateRegRegInstruction(lowRegRegOpCode, root, twoLow, oneLow, _cg);

      if (firstHighZero)
         {
         if (secondHighZero)
            {
            twoHigh = _cg->allocateRegister();
            generateRegRegInstruction(XOR4RegReg, root, twoHigh, twoHigh, _cg);
            }
         else
            {
            if (root->getOpCodeValue() == TR::land)
               {
               generateRegRegInstruction(XOR4RegReg, root, twoHigh, twoHigh, _cg);
               }
            }
         }
      else if (secondHighZero)
         {
         twoHigh = _cg->allocateRegister();
         if (root->getOpCodeValue() == TR::land)
            {
            generateRegRegInstruction(XOR4RegReg, root, twoHigh, twoHigh, _cg);
            }
         else
            {
            generateRegRegInstruction(copyOpCode, root, twoHigh, oneHigh, _cg);
            }
         }
      else
         {
         generateRegRegInstruction(highRegRegOpCode, root, twoHigh, oneHigh, _cg);
         }
      root->setRegister(_cg->allocateRegisterPair(twoLow, twoHigh));
      notReversedOperands();
      }
   else if (getCopyRegs())
      {
      TR::Register *copyReg;
      TR::Register *copyLow;
      TR::Register *copyHigh;
      TR::Register *sourceReg;
      TR::Register *sourceLow;
      TR::Register *sourceHigh    = NULL;
      bool         copyHighZero   = false;
      bool         sourceHighZero = false;
      if (getCopyReg1())
         {
         copyReg        = firstRegister;
         sourceReg      = secondRegister;
         copyHighZero   = firstHighZero;
         sourceHighZero = secondHighZero;
         }
      else
         {
         copyReg        = secondRegister;
         sourceReg      = firstRegister;
         copyHighZero   = secondHighZero;
         sourceHighZero = firstHighZero;
         notReversedOperands();
         }
      copyLow  = _cg->allocateRegister();

      TR::Register *temp;
      if (copyHighZero)
         {
         temp = copyReg;
         }
      else
         {
         temp = copyReg->getLowOrder();
         }
      generateRegRegInstruction(copyOpCode, root, copyLow, temp, _cg);

      if (sourceHighZero)
         {
         sourceLow  = sourceReg;
         }
      else
         {
         sourceLow  = sourceReg->getLowOrder();
         sourceHigh = sourceReg->getHighOrder();
         }
      generateRegRegInstruction(lowRegRegOpCode, root, copyLow, sourceLow, _cg);

      copyHigh = _cg->allocateRegister();
      if (copyHighZero)
         {
         if (sourceHighZero)
            {
            generateRegRegInstruction(XOR4RegReg, root, copyHigh, copyHigh, _cg);
            }
         else
            {
            if (root->getOpCodeValue() == TR::land)
               {
               generateRegRegInstruction(XOR4RegReg, root, copyHigh, copyHigh, _cg);
               }
            else
               {
               generateRegRegInstruction(copyOpCode, root, copyHigh, sourceHigh, _cg);
               }
            }
         }
      else if (sourceHighZero)
         {
         if (root->getOpCodeValue() == TR::land)
            {
            generateRegRegInstruction(XOR4RegReg, root, copyHigh, copyHigh, _cg);
            }
         else
            {
            generateRegRegInstruction(copyOpCode, root, copyHigh, copyReg->getHighOrder(), _cg);
            }
         }
      else
         {
         generateRegRegInstruction(copyOpCode, root, copyHigh, copyReg->getHighOrder(), _cg);
         generateRegRegInstruction(highRegRegOpCode, root, copyHigh, sourceHigh, _cg);
         }
      root->setRegister(_cg->allocateRegisterPair(copyLow, copyHigh));
      }
   else
      {
      TR::MemoryReference  *lowMR           = NULL;
      TR::MemoryReference  *highMR          = NULL;
      TR::Register         *targetReg       = NULL;
      TR::Register         *targetLow       = NULL;
      TR::Register         *targetHigh      = NULL;
      bool                  targetHighZero  = false;
      bool                  memHighZero     = false;
      bool                  useMemHighOrder = false;
      if (getOpReg1Mem2())
         {
         lowMR           = generateX86MemoryReference(secondChild, _cg);
         targetReg       = firstRegister;
         targetHighZero  = firstHighZero;
         memHighZero     = secondHighZero;
         useMemHighOrder = useSecondHighOrder;
         }
      else
         {
         lowMR           = generateX86MemoryReference(firstChild, _cg);
         targetReg       = secondRegister;
         targetHighZero  = secondHighZero;
         memHighZero     = firstHighZero;
         useMemHighOrder = useFirstHighOrder;
         notReversedOperands();
         }

      if (targetHighZero)
         {
         targetLow = targetReg;
         }
      else
         {
         targetLow  = targetReg->getLowOrder();
         targetHigh = targetReg->getHighOrder();
         }

      if (useMemHighOrder)
         {
         lowMR->getSymbolReference().addToOffset(4);
         }
      generateRegMemInstruction(lowRegMemOpCode, root, targetLow, lowMR, _cg);

      if (memHighZero)
         {
         if (targetHighZero)
            targetHigh = _cg->allocateRegister();
         if (root->getOpCodeValue() == TR::land || targetHighZero)
            {
            generateRegRegInstruction(XOR4RegReg, root, targetHigh, targetHigh, _cg);
            }
         }
      else if (targetHighZero)
         {
         targetHigh = _cg->allocateRegister();
         if (root->getOpCodeValue() == TR::land)
            {
            generateRegRegInstruction(XOR4RegReg, root, targetHigh, targetHigh, _cg);
            }
         else
            {
            highMR = generateX86MemoryReference(*lowMR, 4, _cg);
            generateRegMemInstruction(L4RegMem, root, targetHigh, highMR, _cg);
            }
         }
      else
         {
         highMR = generateX86MemoryReference(*lowMR, 4, _cg);
         generateRegMemInstruction(highRegMemOpCode, root, targetHigh, highMR, _cg);
         }
      root->setRegister(_cg->allocateRegisterPair(targetLow, targetHigh));
      lowMR->decNodeReferenceCounts(_cg);
      }

   _cg->decReferenceCount(firstChild);
   _cg->decReferenceCount(secondChild);
   }

/*
 * \brief 
 * this API is intended for regular add operation nodes where the first child and second child are the operands by default
 */
void TR_X86BinaryCommutativeAnalyser::integerAddAnalyser(TR::Node      *root,
                                                         TR_X86OpCodes regRegOpCode,
                                                         TR_X86OpCodes regMemOpCode,
                                                         bool          needsEflags,     // false by default
                                                         TR::Node      *carry )// 0 by default
   {
   TR::Node *firstChild = NULL;
   TR::Node *secondChild = NULL;
   TR::Register *targetRegister;
  if (_cg->whichChildToEvaluate(root) == 0)
      {
      firstChild  = root->getFirstChild();
      secondChild = root->getSecondChild();
      setReversedOperands(false);
      }
   else
      {
      firstChild  = root->getSecondChild();
      secondChild = root->getFirstChild();
      setReversedOperands(true);
      }
   targetRegister = integerAddAnalyserImpl(root, firstChild, secondChild, regRegOpCode, regMemOpCode, needsEflags, carry);
   root->setRegister(targetRegister);
   _cg->decReferenceCount(firstChild);
   _cg->decReferenceCount(secondChild);
   }

/*
 * \brief 
 * this API is for check nodes(like OverflowCHK) with an add operation where the operands 
 * are given explicitly by the caller and are not the first and second child of the given root node
 *
 * \param root
 *     the check node
 * \param firstChild, secondChild 
 *     the operands for the add operation 
 */
void TR_X86BinaryCommutativeAnalyser::integerAddAnalyserWithExplicitOperands(TR::Node      *root,
                                                                             TR::Node      *firstChild, 
                                                                             TR::Node      *secondChild, 
                                                                             TR_X86OpCodes regRegOpCode,
                                                                             TR_X86OpCodes regMemOpCode,
                                                                             bool          needsEflags, // false by default
                                                                             TR::Node      *carry)// 0 by default
   {
   TR_ASSERT(root->getOpCodeValue() == TR::OverflowCHK, "unsupported opcode %s for integerAddAnalyserWithGivenOperands on node %p\n", _cg->comp()->getDebug()->getName(root->getOpCodeValue()), root);
   TR::Register *tempReg = integerAddAnalyserImpl(root, firstChild, secondChild, regRegOpCode, regMemOpCode, needsEflags, carry);
   _cg->decReferenceCount(firstChild);
   _cg->decReferenceCount(secondChild);
   _cg->stopUsingRegister(tempReg);
   }
 
/*
 * users should call the integerAddAnalyser or integerAddAnalyserWithGivenOperands APIs instead of calling this one directly 
 */
TR::Register *TR_X86BinaryCommutativeAnalyser::integerAddAnalyserImpl(TR::Node      *root,
                                                                      TR::Node      *firstChild, 
                                                                      TR::Node      *secondChild, 
                                                                      TR_X86OpCodes regRegOpCode,
                                                                      TR_X86OpCodes regMemOpCode,
                                                                      bool          needsEflags,     
                                                                      TR::Node      *carry)
   {
   TR::Register *targetRegister;
   TR::Compilation* comp = TR::comp();
   TR::Register *firstRegister  = firstChild->getRegister();
   TR::Register *secondRegister = secondChild->getRegister();


   setInputs(firstChild, firstRegister, secondChild, secondRegister);

   // Must prevent creating registers that are marked as containing
   // both collected reference and internal pointers at the same time
   //
   if (root->isInternalPointer())
      {
      // If a register is marked as containing a collected reference, adjust
      // the _inputs fields as to mark it unclobberable
      //
      if (firstRegister &&
          ((!firstRegister->containsInternalPointer()) || (firstRegister->getPinningArrayPointer() != root->getPinningArrayPointer())))
         resetClob1();

      if (secondRegister &&
          ((!secondRegister->containsInternalPointer()) || (secondRegister->getPinningArrayPointer() != root->getPinningArrayPointer())))
         resetClob2();
      }

   if (comp->generateArraylets() && root->getOpCodeValue() == TR::aiadd)
      {
      // result of this add is probably an internal pointer into the arraylet spine
      // so make sure that, if either child is marked as collectible, we won't
      // reuse its register for this internal pointer
      if (firstRegister && firstRegister->containsCollectedReference())
         resetClob1();
      if (secondRegister && secondRegister->containsCollectedReference())
         resetClob2();
      }

   if (getEvalChild1())
      {
      firstRegister = _cg->evaluate(firstChild);
      }

   if (getEvalChild2())
      {
      secondRegister = _cg->evaluate(secondChild);
      firstRegister = firstChild->getRegister();
      }

   // comp()->useCompressedPointers
   // ladd
   //    iu2l
   // the firstChild is not commoned
   //
   TR::TreeEvaluator::genNullTestSequence(root, firstRegister, firstRegister, _cg);

   // maybe a register that did not exist before has just been created,
   // and this new register contains a collected reference
   // arraylets: aiadd is technically internal pointer into spine object, but isn't marked as internal pointer
   if (root->isInternalPointer() || comp->generateArraylets() && root->getOpCodeValue() == TR::aiadd)
      {
      // check whether a newly created register contains collected reference
      if ((getEvalChild1() && ((!firstRegister->containsInternalPointer()) || (firstRegister->getPinningArrayPointer() != root->getPinningArrayPointer()))) ||
          (getEvalChild2() && ((!secondRegister->containsInternalPointer()) || (secondRegister->getPinningArrayPointer() != root->getPinningArrayPointer()))))
         {
         // need to call again setInputs to mark the fact that
         // the newly created register cannot be clobbered
         setInputs(firstChild, firstRegister, secondChild, secondRegister, false, true);

         // if we need to copy registers, make sure we have both registers available
         if (getCopyRegs())
            {
            if (!firstRegister)
               firstRegister = _cg->evaluate(firstChild);
            if (!secondRegister)
               secondRegister = _cg->evaluate(secondChild);
            }
         }
      }

   if (carry != 0)
      TR_X86ComputeCC::setCarryBorrow(carry, false, _cg);

   if (getOpReg1Reg2())
      {
      generateRegRegInstruction(regRegOpCode, root, firstRegister, secondRegister, _cg);
      targetRegister = firstRegister;
      }
   else if (getOpReg2Reg1())
      {
      generateRegRegInstruction(regRegOpCode, root, secondRegister, firstRegister, _cg);
      targetRegister = secondRegister;
      notReversedOperands();
      }
   else if (getCopyRegs())
      {
      TR::Register *tempReg;
      if (firstRegister->containsCollectedReference() ||
          secondRegister->containsCollectedReference() ||
          firstRegister->containsInternalPointer() ||
          secondRegister->containsInternalPointer())
         {
         if (root->isInternalPointer())
            {
            tempReg = _cg->allocateRegister();
            if (root->getPinningArrayPointer())
               {
               tempReg->setContainsInternalPointer();
               tempReg->setPinningArrayPointer(root->getPinningArrayPointer());
               }
            }
         else if (comp->generateArraylets() && root->getOpCodeValue() == TR::aiadd)
            // arraylets: aiadd is technically internal pointer into spine object, but isn't marked as internal pointer
            tempReg = _cg->allocateRegister();
         else
            tempReg = _cg->allocateCollectedReferenceRegister();
         }
      else
         {
         tempReg = _cg->allocateRegister();
         }

      targetRegister = tempReg;
      bool is64Bit = TR_X86OpCode(regRegOpCode).hasLongSource();

      // if eflags are required then we cannot use LEA as it doesn't set or use them
      if (needsEflags || (carry != 0))
         {
         generateRegRegInstruction(MOVRegReg(is64Bit), root, tempReg, firstRegister, _cg);
         generateRegRegInstruction(regRegOpCode, root, tempReg, secondRegister, _cg);
         }
      else
         {
         TR::MemoryReference  *tempMR = generateX86MemoryReference(_cg);
         tempMR->setBaseRegister(firstRegister);
         tempMR->setIndexRegister(secondRegister);
         generateRegMemInstruction(LEARegMem(is64Bit), root, tempReg, tempMR, _cg);
         }
      }
   else if (getOpReg1Mem2())
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(secondChild, _cg);
      generateRegMemInstruction(regMemOpCode, root, firstRegister, tempMR, _cg);
      targetRegister = firstRegister;
      tempMR->decNodeReferenceCounts(_cg);
      }
   else
      {
      TR::MemoryReference  *tempMR = generateX86MemoryReference(firstChild, _cg);
      generateRegMemInstruction(regMemOpCode, root, secondRegister, tempMR, _cg);
      targetRegister = secondRegister;
      tempMR->decNodeReferenceCounts(_cg);
      notReversedOperands();
      }
   return targetRegister;
   }


// Volatile memory operands are not allowed in long additions
// if we are compiling for an SMP machine, as the carry flag can
// get clobbered by the memory barrier immediately preceding the
// ADC4RegMem instruction.
//
static bool isVolatileMemoryOperand(TR::Node *node)
   {
   if (TR::Compiler->target.isSMP() && node->getOpCode().isMemoryReference())
      {
      TR_ASSERT(node->getSymbolReference(), "expecting a symbol reference\n");
      TR::Symbol *sym = node->getSymbolReference()->getSymbol();
      return (sym && sym->isVolatile());
      }
   return false;
   }

/*
 * \brief 
 * this API is for check nodes(like OverflowCHK) an ladd operation where the operands 
 * are given explicitly by the caller and are not the first and second child of the given root node
 *
 * \param root
 *     the check node
 * \param firstChild, secondChild 
 *     the operands for the add operation 
 */
void TR_X86BinaryCommutativeAnalyser::longAddAnalyserWithExplicitOperands(TR::Node *root, TR::Node *firstChild, TR::Node *secondChild)
   {
   TR_ASSERT(root->getOpCodeValue() == TR::OverflowCHK, "unsupported opcode %s for longAddAnalyserWithExplicitOperands on node %p\n", _cg->comp()->getDebug()->getName(root->getOpCodeValue()), root);
   TR::Register *tempReg= longAddAnalyserImpl(root, firstChild, secondChild);
   _cg->decReferenceCount(firstChild);
   _cg->decReferenceCount(secondChild);
   _cg->stopUsingRegister(tempReg);
   }

/*
 * \brief 
 * this API is intended for regular ladd operation nodes where the first child and second child are the operands by default
 */
void TR_X86BinaryCommutativeAnalyser::longAddAnalyser(TR::Node *root)
   {
   TR::Node *firstChild = NULL; 
   TR::Node *secondChild = NULL;
   if (_cg->whichChildToEvaluate(root) == 0)
      {
      firstChild  = root->getFirstChild();
      secondChild = root->getSecondChild();
      setReversedOperands(false);
      }
   else
      {
      firstChild  = root->getSecondChild();
      secondChild = root->getFirstChild();
      setReversedOperands(true);
      }
   TR::Register * targetRegister = longAddAnalyserImpl(root, firstChild, secondChild);
   root->setRegister(targetRegister);
   _cg->decReferenceCount(firstChild);
   _cg->decReferenceCount(secondChild);
   }

/*
 * users should call the longAddAnalyser or longAddAnalyserWithExplicitOperands APIs instead of calling this one directly 
 */
TR::Register* TR_X86BinaryCommutativeAnalyser::longAddAnalyserImpl(TR::Node *root, TR::Node *&firstChild, TR::Node *&secondChild)
   {
   TR::Register *twoLow       = NULL;
   TR::Register *twoHigh      = NULL;
   TR::Register *oneLow       = NULL;
   TR::Register *oneHigh      = NULL;
   TR::Register *targetRegister = NULL;
   TR_X86OpCodes regRegOpCode = ADD4RegReg;
   TR_X86OpCodes regMemOpCode = ADD4RegMem;

   TR::Register *firstRegister  = firstChild->getRegister();
   TR::Register *secondRegister = secondChild->getRegister();

   bool firstHighZero      = false;
   bool secondHighZero     = false;
   bool useFirstHighOrder  = false;
   bool useSecondHighOrder = false;
   bool needsEflags = NEED_CC(root) || (root->getOpCodeValue() == TR::luaddc);

   TR::ILOpCodes firstOp  = firstChild->getOpCodeValue();
   TR::ILOpCodes secondOp = secondChild->getOpCodeValue();

   // Can generate better code for long adds when one or more children have a high order zero word
   // can avoid the evaluation when we don't need the result of such nodes for another parent.
   //
   TR::Node *bypassedFirstUnsignedConversionChild = NULL;
   TR::Node *bypassedSecondUnsignedConversionChild = NULL;

   if (firstChild->isHighWordZero() && !needsEflags)
      {
      firstHighZero = true;
      if (firstChild->getReferenceCount() == 1 && firstRegister == 0)
         {
         if (firstOp == TR::iu2l || firstOp == TR::su2l ||
             firstOp == TR::bu2l ||
             (firstOp == TR::lushr &&
              firstChild->getSecondChild()->getOpCodeValue() == TR::iconst &&
              (firstChild->getSecondChild()->getInt() & TR::TreeEvaluator::shiftMask(true)) == 32))
            {
            bypassedFirstUnsignedConversionChild = firstChild;
            firstChild    = firstChild->getFirstChild();
            firstRegister = firstChild->getRegister();
            if (firstOp == TR::lushr)
               {
               useFirstHighOrder = true;
               }
            }
         }
      }

   if (secondChild->isHighWordZero() && !needsEflags)
      {
      secondHighZero = true;
      if (secondChild->getReferenceCount() == 1 && secondRegister == 0)
         {
         if (secondOp == TR::iu2l || secondOp == TR::su2l ||
             secondOp == TR::bu2l ||
             (secondOp == TR::lushr &&
              secondChild->getSecondChild()->getOpCodeValue() == TR::iconst &&
              (secondChild->getSecondChild()->getInt() & TR::TreeEvaluator::shiftMask(true)) == 32))
            {
            bypassedSecondUnsignedConversionChild = secondChild;
            secondChild    = secondChild->getFirstChild();
            secondRegister = secondChild->getRegister();
            if (secondOp == TR::lushr)
               {
               useSecondHighOrder = true;
               }
            }
         }
      }

   setInputs(firstChild, firstRegister, secondChild, secondRegister);

   if (isVolatileMemoryOperand(firstChild) || firstChild->getSize() != ILTypeProp::Size_4)
      resetMem1();

   if (isVolatileMemoryOperand(secondChild) || secondChild->getSize() != ILTypeProp::Size_4)
      resetMem2();

   if (getEvalChild1())
      {
      firstRegister = _cg->evaluate(firstChild);

      if (bypassedFirstUnsignedConversionChild)
         zeroExtendTo32BitRegister(bypassedFirstUnsignedConversionChild, firstRegister, firstChild->getSize(), _cg);
      }

   if (getEvalChild2())
      {
      secondRegister = _cg->evaluate(secondChild);

      if (bypassedSecondUnsignedConversionChild)
         zeroExtendTo32BitRegister(bypassedSecondUnsignedConversionChild, secondRegister, secondChild->getSize(), _cg);
      }

   if (firstHighZero && firstRegister && firstRegister->getRegisterPair())
      {
      if (!useFirstHighOrder)
         {
         firstRegister = firstRegister->getLowOrder();
         }
      else
         {
         firstRegister = firstRegister->getHighOrder();
         }
      }
   if (secondHighZero && secondRegister && secondRegister->getRegisterPair())
      {
      if (!useSecondHighOrder)
         {
         secondRegister = secondRegister->getLowOrder();
         }
      else
         {
         secondRegister = secondRegister->getHighOrder();
         }
      }

   if (root->getOpCodeValue() == TR::luaddc &&
       TR_X86ComputeCC::setCarryBorrow(root->getChild(2), false, _cg))
      {
      // use ADC rather than ADD
      //
      regRegOpCode = ADC4RegReg;
      regMemOpCode = ADC4RegMem;
      }

   if (getOpReg1Reg2())
      {
      if (!firstHighZero)
         {
         oneLow  = firstRegister->getLowOrder();
         oneHigh = firstRegister->getHighOrder();
         }
      else
         {
         oneLow  = firstRegister;
         oneHigh = 0;
         }

      if (!secondHighZero)
         {
         twoLow  = secondRegister->getLowOrder();
         twoHigh = secondRegister->getHighOrder();
         }
      else
         {
         twoLow  = secondRegister;
         twoHigh = 0;
         }

      generateRegRegInstruction(regRegOpCode, root, oneLow, twoLow, _cg);

      if (firstHighZero)
         {
         if (secondHighZero)
            {
            oneHigh = _cg->allocateRegister();
            // xor would reset the carry we need for the subsequent adc instruction
            generateRegImmInstruction(MOV4RegImm4, root, oneHigh, 0, _cg);
            generateRegRegInstruction(ADC4RegReg, root, oneHigh, oneHigh, _cg);
            }
         else
            {
            if (getOpReg2Reg1() == false)
               {
               oneHigh = _cg->allocateRegister();
               generateRegRegInstruction(MOV4RegReg, root, oneHigh, twoHigh, _cg);
               generateRegImmInstruction(ADC4RegImms, root, oneHigh, 0, _cg);
               }
            else
               {
               generateRegImmInstruction(ADC4RegImms, root, twoHigh, 0, _cg);
               oneHigh = twoHigh;
               }
            }
         }
      else if (secondHighZero)
         {
         generateRegImmInstruction(ADC4RegImms, root, oneHigh, 0, _cg);
         }
      else
         {
         generateRegRegInstruction(ADC4RegReg, root, oneHigh, twoHigh, _cg);
         }

      TR::Register *target = _cg->allocateRegisterPair(oneLow, oneHigh);
      targetRegister = target;
      }
   else if (getOpReg2Reg1())
      {
      if (!firstHighZero)
         {
         oneLow  = firstRegister->getLowOrder();
         oneHigh = firstRegister->getHighOrder();
         }
      else
         {
         oneLow  = firstRegister;
         oneHigh = 0;
         }

      if (!secondHighZero)
         {
         twoLow  = secondRegister->getLowOrder();
         twoHigh = secondRegister->getHighOrder();
         }
      else
         {
         twoLow  = secondRegister;
         twoHigh = 0;
         }

      generateRegRegInstruction(regRegOpCode, root, twoLow, oneLow, _cg);
      if (firstHighZero)
         {
         if (secondHighZero)
            {
            twoHigh = _cg->allocateRegister();
            // xor would reset the carry we need for the subsequent adc instruction
            generateRegImmInstruction(MOV4RegImm4, root, twoHigh, 0, _cg);
            generateRegRegInstruction(ADC4RegReg, root, twoHigh, twoHigh, _cg);
            }
         else
            {
            generateRegImmInstruction(ADC4RegImms, root, twoHigh, 0, _cg);
            }
         }
      else if (secondHighZero)
         {
         twoHigh = _cg->allocateRegister();
         generateRegRegInstruction(MOV4RegReg, root, twoHigh, oneHigh, _cg);
         generateRegImmInstruction(ADC4RegImms, root, twoHigh, 0, _cg);
         }
      else
         {
         generateRegRegInstruction(ADC4RegReg, root, twoHigh, oneHigh, _cg);
         }
      targetRegister = _cg->allocateRegisterPair(twoLow, twoHigh);
      notReversedOperands();
      }
   else if (getCopyRegs())
      {
      oneLow  = _cg->allocateRegister();
      oneHigh = _cg->allocateRegister();

      TR::Register *temp;
      if (firstHighZero)
         {
         temp = firstRegister;
         }
      else
         {
         temp = firstRegister->getLowOrder();
         }
      generateRegRegInstruction(MOV4RegReg, root, oneLow, temp, _cg);
      if (secondHighZero)
         {
         twoLow  = secondRegister;
         }
      else
         {
         twoLow  = secondRegister->getLowOrder();
         twoHigh = secondRegister->getHighOrder();
         }
      generateRegRegInstruction(regRegOpCode, root, oneLow, twoLow, _cg);
      if (firstHighZero)
         {
         // xor would reset the carry we need for the subsequent adc instruction
         generateRegImmInstruction(MOV4RegImm4, root, oneHigh, 0, _cg);
         }
      else
         {
         generateRegRegInstruction(MOV4RegReg, root, oneHigh, firstRegister->getHighOrder(), _cg);
         }
      if (secondHighZero)
         {
         generateRegImmInstruction(ADC4RegImms, root, oneHigh, 0, _cg);
         }
      else
         {
         generateRegRegInstruction(ADC4RegReg, root, oneHigh, twoHigh, _cg);
         }
      targetRegister = _cg->allocateRegisterPair(oneLow, oneHigh);
      }
   else
      {
      TR::MemoryReference  *lowMR;
      TR::MemoryReference  *highMR;
      TR::Register            *targetReg;
      TR::Register            *targetLow;
      TR::Register            *targetHigh;
      TR::ILOpCodes            memOp;
      bool                    targetHighZero;
      bool                    memHighZero;
      bool                    useMemHighOrder = false;
      if (getOpReg1Mem2())
         {
         lowMR           = generateX86MemoryReference(secondChild, _cg);
         targetReg       = firstRegister;
         targetHighZero  = firstHighZero;
         memOp           = secondOp;
         memHighZero     = secondHighZero;
         useMemHighOrder = useSecondHighOrder;
         }
      else
         {
         lowMR           = generateX86MemoryReference(firstChild, _cg);
         targetReg       = secondRegister;
         targetHighZero  = secondHighZero;
         memOp           = firstOp;
         memHighZero     = firstHighZero;
         useMemHighOrder = useFirstHighOrder;
         notReversedOperands();
         }
      if (targetHighZero)
         {
         targetLow  = targetReg;
         targetHigh = _cg->allocateRegister();
         generateRegRegInstruction(XOR4RegReg, root, targetHigh, targetHigh, _cg);
         }
      else
         {
         targetLow  = targetReg->getLowOrder();
         targetHigh = targetReg->getHighOrder();
         }
      if (memOp == TR::bu2l)
         {
         TR::Register *tempReg = _cg->allocateRegister();
         generateRegMemInstruction(MOVZXReg4Mem1, root, tempReg, lowMR, _cg);
         generateRegRegInstruction(regRegOpCode, root, targetLow, tempReg, _cg);
         _cg->stopUsingRegister(tempReg);
         }
      else if (memOp == TR::su2l)
         {
         TR::Register *tempReg = _cg->allocateRegister();
         generateRegMemInstruction(MOVZXReg4Mem2, root, tempReg, lowMR, _cg);
         generateRegRegInstruction(regRegOpCode, root, targetLow, tempReg, _cg);
         _cg->stopUsingRegister(tempReg);
         }
      else
         {
         if (useMemHighOrder)
            {
            lowMR->getSymbolReference().addToOffset(4);
            }
         generateRegMemInstruction(regMemOpCode, root, targetLow, lowMR, _cg);
         }
      if (memHighZero)
         {
         generateRegImmInstruction(ADC4RegImms, root, targetHigh, 0, _cg);
         }
      else
         {
         highMR = generateX86MemoryReference(*lowMR, 4, _cg);
         generateRegMemInstruction(ADC4RegMem, root, targetHigh, highMR, _cg);
         }
      targetRegister = _cg->allocateRegisterPair(targetLow, targetHigh);
      lowMR->decNodeReferenceCounts(_cg);
      }

   return targetRegister;
   }

// Multiply for:
// lumulh
//   a
//   b
//   lmul
//     ==> a
//     ==> b
void TR_X86BinaryCommutativeAnalyser::longDualMultiplyAnalyser(TR::Node *root)
   {
   bool needsHighMulOnly = (root->getOpCodeValue() == TR::lumulh) && !root->isDualCyclic();
   TR_ASSERT(root->isDualCyclic() || needsHighMulOnly, "Should be either calculating cyclic dual or just the high part of the lmul");

   TR::Node *lmulNode;
   TR::Node *lumulhNode;
   if (!needsHighMulOnly)
      {
      diagnostic("Found lmul/lumulh for node = %p\n", root);
      lmulNode = (root->getOpCodeValue() == TR::lmul) ? root : root->getChild(2);
      lumulhNode = lmulNode->getChild(2);
      TR_ASSERT((lumulhNode->getReferenceCount() > 1) && (lmulNode->getReferenceCount() > 1), "Expected both lumulh and lmul have external references.");

      // we only evaluate the lumulh children, and internal cycle does not indicate evaluation
      _cg->decReferenceCount(lmulNode->getFirstChild());
      _cg->decReferenceCount(lmulNode->getSecondChild());
      _cg->decReferenceCount(lmulNode->getChild(2));
      _cg->decReferenceCount(lumulhNode->getChild(2));
      }
   else
      {
      diagnostic("Found lumulh only node = %p\n", root);
      lumulhNode = root;
      lmulNode = NULL;
      }

   TR::Node *firstChild  = 0;
   TR::Node *secondChild = 0;

   // this may cause a problem, because both lmul and lumulh have 3 children.
   if (_cg->whichChildToEvaluate(lumulhNode) == 0)
      {
      firstChild  = lumulhNode->getFirstChild();
      secondChild = lumulhNode->getSecondChild();
      setReversedOperands(false);
      }
   else
      {
      firstChild  = lumulhNode->getSecondChild();
      secondChild = lumulhNode->getFirstChild();
      setReversedOperands(true);
      }

   TR::Register *firstRegister  = firstChild->getRegister();
   TR::Register *secondRegister = secondChild->getRegister();
   setInputs(firstChild, firstRegister, secondChild, secondRegister);

   if (getEvalChild1())
      firstRegister = _cg->evaluate(firstChild);

   if (getEvalChild2())
      secondRegister = _cg->evaluate(secondChild);

   TR::Register *one;
   TR::Register *two;
   TR::Register *r1;
   TR::Register *r2;
   TR::Register *r3;
   TR::Register *r4;
   TR::Register *r5;
   TR::Register *eax;
   TR::Register *edx;
   TR::Node *oneChild;
   TR::Register *resultHigh;

   TR::MemoryReference  *oneLowMR  = NULL;
   bool copyTwo;
   bool copyOne;
   bool memOne;
   if (getOpReg1Reg2())
      {
      one = firstRegister;
      two = secondRegister;
      copyTwo = (getOpReg2Reg1() == false) ? true : false;
      copyOne = false;
      memOne = false;
      }
   else if (getOpReg2Reg1())
      {
      two = firstRegister;
      one = secondRegister;
      copyTwo = true;
      copyOne = false;
      memOne = false;
      notReversedOperands();
      }
   else if (getCopyRegs())
      {
      one = firstRegister;
      two = secondRegister;
      copyTwo = true;
      copyOne = true;
      memOne = false;
      }
   else if (getOpReg1Mem2())
      {
      two = firstRegister;
      oneChild = secondChild;
      copyTwo = false;
      copyOne = true;
      memOne = true;
      notReversedOperands();
      }
   else
      {
      two = secondRegister;
      oneChild = firstChild;
      copyTwo = false;
      copyOne = true;
      memOne = true;
      }

   if (copyOne)
      {
      // allocate register for the low result, need to copy oneLow into r1
      r1 = _cg->allocateRegister();
      if (memOne)
         {
         r5 = _cg->allocateRegister();
         oneLowMR  = generateX86MemoryReference(oneChild, _cg);
         TR::MemoryReference  *oneHighMR = generateX86MemoryReference(*oneLowMR, 4, _cg);
         generateRegMemInstruction(L4RegMem, root, r1, oneLowMR, _cg);
         generateRegMemInstruction(L4RegMem, root, r5, oneHighMR, _cg);
         }
      else
         {
         r5 = one->getHighOrder();
         generateRegRegInstruction(MOV4RegReg, root, r1, one->getLowOrder(), _cg);
         }
      }
   else
      {
      r1 = one->getLowOrder();
      r5 = one->getHighOrder();
      }

   if (copyTwo)
      {
      r4 = _cg->allocateRegister();
      r3 = _cg->allocateRegister();
      generateRegRegInstruction(MOV4RegReg, root, r4, two->getLowOrder(), _cg);
      generateRegRegInstruction(MOV4RegReg, root, r3, two->getHighOrder(), _cg);
      }
   else
      {
      r4 = two->getLowOrder();
      r3 = two->getHighOrder();
      }

   r2 = _cg->allocateRegister();

   eax = _cg->allocateRegister();
   edx = _cg->allocateRegister();

   // requires:
   //   7 registers, five general purpose: R1, R2, R3, R4, R5, two special registers: EAX, EDX
   // entry:
   //   R5:R1 = a
   //   R3:R4 = b
   //   R1,R3,R4 are overwritten with result; EAX, EDX are overwritten. R5 is preserved.
   // exit
   //   R4:R3:R2:R1 = r = a * b

   // conditions on the four MUL instructions
   TR::RegisterDependencyConditions  *depsMul[4];
   for (int i = 0; i < 4; i++)
      {
      depsMul[i] = generateRegisterDependencyConditions(2, 2, _cg);
      depsMul[i]->addPreCondition( eax, TR::RealRegister::eax, _cg);
      depsMul[i]->addPostCondition(eax, TR::RealRegister::eax, _cg);
      depsMul[i]->addPreCondition( edx, TR::RealRegister::edx, _cg);
      depsMul[i]->addPostCondition(edx, TR::RealRegister::edx, _cg);
      }

   generateRegRegInstruction(MOV4RegReg,  root, eax,  r1,             _cg);    //   1  MOV   EAX,  R1     // EAX = al
   generateRegRegInstruction(MUL4AccReg,  root, eax,  r4, depsMul[0], _cg);    //   2  MUL    R4          // EDX:EAX = al * bl
   generateRegRegInstruction(XCHG4RegReg, root,  r1, eax,             _cg);    //   3  XCHG   R1, EAX     // R1 = (al * bl)l; EAX = al
   generateRegRegInstruction(MOV4RegReg,  root,  r2, edx,             _cg);    //   4  MOV    R2, EDX     // R2 = (al * bl)h
   generateRegRegInstruction(MUL4AccReg,  root, eax,  r3, depsMul[1], _cg);    //   5  MUL    R3          // EDX:EAX = al * bh
   generateRegRegInstruction(XCHG4RegReg, root,  r3, edx,             _cg);    //   6  XCHG   R3, EDX     // R3 = (al * bh)h; EDX = bh
   generateRegRegInstruction(ADD4RegReg,  root,  r2, eax,             _cg);    //   7  ADD    R2, EAX     // R2 = (al * bl)h + (al * bh)l
   generateRegImmInstruction(ADC4RegImms, root,  r3,   0,             _cg);    //   8  ADC    R3, 0       // R3 = (al * bh)h + [carry]
   generateRegRegInstruction(MOV4RegReg,  root, eax,  r5,             _cg);    //   9  MOV   EAX, R5      // EAX = ah
   generateRegRegInstruction(MUL4AccReg,  root, eax, edx, depsMul[2], _cg);    //  10  MUL   EDX          // EDX:EAX = ah * bh
   generateRegRegInstruction(XCHG4RegReg, root,  r4, edx,             _cg);    //  11  XCHG   R4, EDX     // R4 = (ah * bh)h; EDX = bl
   generateRegRegInstruction(ADD4RegReg,  root,  r3, eax,             _cg);    //  12  ADD    R3, EAX     // R3 = (al * bh)l + (ah * bh)l
   generateRegImmInstruction(ADC4RegImms, root,  r4,   0,             _cg);    //  13  ADC    R4, 0       // R4 = (ah * bh)h + [carry]
   generateRegRegInstruction(MOV4RegReg,  root, eax,  r5,             _cg);    //  14  MOV   EAX, R5      // EAX = ah
   generateRegRegInstruction(MUL4AccReg,  root, eax, edx, depsMul[3], _cg);    //  15  MUL   EDX          // EDX:EAX = ah * bl
   generateRegRegInstruction(ADD4RegReg,  root,  r2, eax,             _cg);    //  16  ADD    R2, EAX     // R2 = (al * bl)h + (al * bh)l              + (ah * bl)l
   generateRegRegInstruction(ADC4RegReg,  root,  r3, edx,             _cg);    //  17  ADC    R3, EDX     // R3 =              (al * bh)l + (ah * bh)l + (ah * bl)h + [carry]
   generateRegImmInstruction(ADC4RegImms, root,  r4,   0,             _cg);    //  18  ADC    R4, 0       // R4 =                           (ah * bh)h              + [carry]

   if (memOne)
      {
      // r5 and oneLowMR are temps, otherwise r5 is oneHigh
      _cg->stopUsingRegister(r5);
      oneLowMR->decNodeReferenceCounts(_cg);
      }

   _cg->stopUsingRegister(eax);
   _cg->stopUsingRegister(edx);

   if (needsHighMulOnly)
      {
      _cg->stopUsingRegister(r1);
      _cg->stopUsingRegister(r2);
      }
   else
      {
      TR::Register *resultLow = _cg->allocateRegisterPair(r1, r2);
      lmulNode->setRegister(resultLow);
      }
   resultHigh = _cg->allocateRegisterPair(r3, r4);
   lumulhNode->setRegister(resultHigh);
   _cg->decReferenceCount(lumulhNode->getFirstChild());
   _cg->decReferenceCount(lumulhNode->getSecondChild());
   }

void TR_X86BinaryCommutativeAnalyser::longMultiplyAnalyser(TR::Node *root)
   {
   TR::Node *firstChild  = 0;
   TR::Node *secondChild = 0;
   if (_cg->whichChildToEvaluate(root) == 0)
      {
      firstChild  = root->getFirstChild();
      secondChild = root->getSecondChild();
      setReversedOperands(false);
      }
   else
      {
      firstChild  = root->getSecondChild();
      secondChild = root->getFirstChild();
      setReversedOperands(true);
      }

   TR::Register *firstRegister  = firstChild->getRegister();
   TR::Register *secondRegister = secondChild->getRegister();

   bool firstHighZero      = false;
   bool secondHighZero     = false;
   bool useFirstHighOrder  = false;
   bool useSecondHighOrder = false;

   TR::Node *bypassedFirstUnsignedConversionChild = NULL;
   TR::Node *bypassedSecondUnsignedConversionChild = NULL;

   // Can generate better code for multiplies when one or more children have a high order zero word
   // can avoid the evaluation when we don't need the result of such nodes for another parent.
   //
   if (firstChild->isHighWordZero())
      {
      firstHighZero = true;
      TR::ILOpCodes firstOp = firstChild->getOpCodeValue();
      if (firstChild->getReferenceCount() == 1 && firstRegister == 0)
         {
         if (firstOp == TR::iu2l || firstOp == TR::su2l ||
             firstOp == TR::bu2l ||
             (firstOp == TR::lushr &&
              firstChild->getSecondChild()->getOpCodeValue() == TR::iconst &&
              (firstChild->getSecondChild()->getInt() & TR::TreeEvaluator::shiftMask(true)) == 32))
            {
            bypassedFirstUnsignedConversionChild = firstChild;
            firstChild    = firstChild->getFirstChild();
            firstRegister = firstChild->getRegister();
            if (firstOp == TR::lushr)
               {
               useFirstHighOrder = true;
               }
            }
         }
      }

   static char * reportHighWordZero = feGetEnv("TR_ReportHighWordZero");
   if (reportHighWordZero)
      {
      diagnostic("long multiply analyser: firstChild  [" POINTER_PRINTF_FORMAT "] has a zero high word: %d\n", firstChild, firstHighZero);
      }

   if (secondChild->isHighWordZero())
      {
      secondHighZero = true;
      TR::ILOpCodes secondOp = secondChild->getOpCodeValue();
      if (secondChild->getReferenceCount() == 1 && secondRegister == 0)
         {
         if (secondOp == TR::iu2l || secondOp == TR::su2l ||
             secondOp == TR::bu2l ||
             (secondOp == TR::lushr &&
              secondChild->getSecondChild()->getOpCodeValue() == TR::iconst &&
              (secondChild->getSecondChild()->getInt() & TR::TreeEvaluator::shiftMask(true)) == 32))
            {
            bypassedSecondUnsignedConversionChild = secondChild;
            secondChild    = secondChild->getFirstChild();
            secondRegister = secondChild->getRegister();
            if (secondOp == TR::lushr)
               {
               useSecondHighOrder = true;
               }
            }
         }
      }

   if (reportHighWordZero)
      {
      diagnostic("long multiply analyser: secondChild [" POINTER_PRINTF_FORMAT "] has a zero high word: %d\n", secondChild, secondHighZero);
      }

   setInputs(firstChild, firstRegister, secondChild, secondRegister);

   if (isVolatileMemoryOperand(firstChild) || firstChild->getSize() != ILTypeProp::Size_4)
      resetMem1();

   if (isVolatileMemoryOperand(secondChild) || secondChild->getSize() != ILTypeProp::Size_4)
      resetMem2();

   if (getEvalChild1())
      {
      firstRegister = _cg->evaluate(firstChild);

      if (bypassedFirstUnsignedConversionChild)
         zeroExtendTo32BitRegister(bypassedFirstUnsignedConversionChild, firstRegister, firstChild->getSize(), _cg);
      }

   if (getEvalChild2())
      {
      secondRegister = _cg->evaluate(secondChild);

      if (bypassedSecondUnsignedConversionChild)
         zeroExtendTo32BitRegister(bypassedSecondUnsignedConversionChild, secondRegister, secondChild->getSize(), _cg);
      }

   if (firstHighZero && firstRegister && firstRegister->getRegisterPair())
      {
      if (!useFirstHighOrder)
         {
         firstRegister = firstRegister->getLowOrder();
         }
      else
         {
         firstRegister = firstRegister->getHighOrder();
         }
      }
   if (secondHighZero && secondRegister && secondRegister->getRegisterPair())
      {
      if (!useSecondHighOrder)
         {
         secondRegister = secondRegister->getLowOrder();
         }
      else
         {
         secondRegister = secondRegister->getHighOrder();
         }
      }

   TR::Register *intermediateResultReg = 0;

   TR::Register *twoLow;
   TR::Register *twoHigh;
   TR::Register *oneLow;
   TR::Register *oneHigh;
   TR::Register *resultReg;

   static char *reportInlineMultiply = feGetEnv("TR_ReportInlineMultiply");

   if (getOpReg1Reg2())
      {
      if (!firstHighZero)
         {
         oneLow    = firstRegister->getLowOrder();
         oneHigh   = firstRegister->getHighOrder();
         resultReg = firstRegister;
         }
      else
         {
         oneLow    = firstRegister;
         oneHigh   = _cg->allocateRegister();
         resultReg = _cg->allocateRegisterPair(oneLow, oneHigh);
         }
      if (!secondHighZero)
         {
         twoLow = secondRegister->getLowOrder();
         if (getOpReg2Reg1() == false)
            {
            twoHigh = _cg->allocateRegister();
            generateRegRegInstruction(MOV4RegReg, root, twoHigh, secondRegister->getHighOrder(), _cg);
            }
         else
            {
            twoHigh = secondRegister->getHighOrder();
            }
         }
      else
         {
         twoLow  = secondRegister;
         twoHigh = 0;
         }

      if (!secondHighZero)
         {
         generateRegRegInstruction(IMUL4RegReg, root, twoHigh, oneLow, _cg);
         intermediateResultReg = twoHigh;
         }
      if (!firstHighZero)
         {
         generateRegRegInstruction(IMUL4RegReg, root, oneHigh, twoLow, _cg);
         intermediateResultReg = oneHigh;
         }
      if (!firstHighZero)
         {
         if (!secondHighZero)
            {
            generateRegRegInstruction(ADD4RegReg, root, twoHigh, oneHigh, _cg);
            intermediateResultReg = twoHigh;
            }
         else
            {
            intermediateResultReg = _cg->allocateRegister();
            generateRegRegInstruction(MOV4RegReg, root, intermediateResultReg, oneHigh, _cg);
            }
         }

      TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)3, 3, _cg);
      deps->addPreCondition (oneLow,  TR::RealRegister::eax,   _cg);
      deps->addPostCondition(oneLow,  TR::RealRegister::eax,   _cg);
      deps->addPreCondition (oneHigh, TR::RealRegister::edx,   _cg);
      deps->addPostCondition(oneHigh, TR::RealRegister::edx,   _cg);
      deps->addPreCondition (twoLow,  TR::RealRegister::NoReg, _cg);
      deps->addPostCondition(twoLow,  TR::RealRegister::NoReg, _cg);

      generateRegRegInstruction(MUL4AccReg, root, oneLow, twoLow, deps, _cg);

      if (intermediateResultReg)
         {
         generateRegRegInstruction(ADD4RegReg, root, oneHigh, intermediateResultReg, _cg);
         }

      if (!secondHighZero)
         {
         if (twoHigh != secondRegister->getHighOrder())
            _cg->stopUsingRegister(twoHigh);
         }
      else if (!firstHighZero)
         {
         _cg->stopUsingRegister(intermediateResultReg);
         }

      if (reportInlineMultiply)
         diagnostic("OpReg1Reg2 path in long multiply inline\n");

      root->setRegister(resultReg);
      }
   else if (getOpReg2Reg1())
      {
      if (!secondHighZero)
         {
         twoLow    = secondRegister->getLowOrder();
         twoHigh   = secondRegister->getHighOrder();
         resultReg = secondRegister;
         }
      else
         {
         twoLow    = secondRegister;
         twoHigh   = _cg->allocateRegister();
         resultReg = _cg->allocateRegisterPair(twoLow, twoHigh);
         }

      if (!firstHighZero)
         {
         oneLow  = firstRegister->getLowOrder();
         oneHigh = _cg->allocateRegister();
         generateRegRegInstruction(MOV4RegReg, root, oneHigh, firstRegister->getHighOrder(), _cg);
         }
      else
         {
         oneLow  = firstRegister;
         oneHigh = 0;
         }

      if (!firstHighZero)
         {
         generateRegRegInstruction(IMUL4RegReg, root, oneHigh, twoLow, _cg);
         intermediateResultReg = oneHigh;
         }
      if (!secondHighZero)
         {
         generateRegRegInstruction(IMUL4RegReg, root, twoHigh, oneLow, _cg);
         intermediateResultReg = twoHigh;
         }
      if (!secondHighZero)
         {
         if (!firstHighZero)
            {
            generateRegRegInstruction(ADD4RegReg, root, oneHigh, twoHigh, _cg);
            intermediateResultReg = oneHigh;
            }
         else
            {
            intermediateResultReg = _cg->allocateRegister();
            generateRegRegInstruction(MOV4RegReg, root, intermediateResultReg, twoHigh, _cg);
            }
         }

      TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)3, 3, _cg);
      deps->addPreCondition (twoLow,  TR::RealRegister::eax,   _cg);
      deps->addPostCondition(twoLow,  TR::RealRegister::eax,   _cg);
      deps->addPreCondition (twoHigh, TR::RealRegister::edx,   _cg);
      deps->addPostCondition(twoHigh, TR::RealRegister::edx,   _cg);
      deps->addPreCondition (oneLow,  TR::RealRegister::NoReg, _cg);
      deps->addPostCondition(oneLow,  TR::RealRegister::NoReg, _cg);

      generateRegRegInstruction(MUL4AccReg, root, twoLow, oneLow, deps, _cg);

      if (intermediateResultReg)
         {
         generateRegRegInstruction(ADD4RegReg, root, twoHigh, intermediateResultReg, _cg);
         }

      if (!firstHighZero)
         {
         _cg->stopUsingRegister(oneHigh);
         }
      else if (!secondHighZero)
         {
         _cg->stopUsingRegister(intermediateResultReg);
         }

      root->setRegister(resultReg);

      if (reportInlineMultiply)
         diagnostic("OpReg2Reg1 path in long multiply inline\n");

      notReversedOperands();
      }
   else if (getCopyRegs())
      {
      // Ah:Al=Al*Bl notation means that Ah:Al contains the result of the MUL instructon between Al and Bl
      //
      // both high zero: compute                               Ah:Al=Al*Bl            (clobber Ah,Al)
      // Ah zero:        compute Bh=Bh*Al,                     Ah:Al=Al*Bl, Ah=Ah+Bh  (clobber Ah,Al,Bh)
      // Bh zero:        compute Ah=Ah*Bl,                     Bh:Bl=Bl*Al, Bh=Bh+Ah  (clobber Ah,Bh,Bl)
      // neither zero:   compute Ah=Ah*Bl, Bh=Bh*Al, Bh=Bh+Ah, Ah:Al=Al*Bl, Ah=Ah+Bh  (clobber Ah,Bh,Al)

      // Allocate and copy the registers that we are going to clobber
      //
      TR::Register *resultHigh, *resultLow, *otherHigh, *otherLow;
      oneHigh = _cg->allocateRegister();
      if (firstHighZero)
         {
         // no need to copy oneHigh since it is zero
         oneLow  = _cg->allocateRegister();
         generateRegRegInstruction(MOV4RegReg, root, oneLow,  firstRegister,  _cg);
         if (secondHighZero)
            {
            twoHigh = 0;
            twoLow = secondRegister;
            }
         else
            {
            twoHigh = _cg->allocateRegister();
            generateRegRegInstruction(MOV4RegReg, root, twoHigh, secondRegister->getHighOrder(), _cg);
            twoLow  = secondRegister->getLowOrder();
            }

         resultHigh = oneHigh;
         resultLow  = oneLow;
         otherHigh  = twoHigh;
         otherLow   = twoLow;
         }
      else
         {
         generateRegRegInstruction(MOV4RegReg, root, oneHigh, firstRegister->getHighOrder(), _cg);

         twoHigh = _cg->allocateRegister();
         if (secondHighZero)
            {
            // no need to copy twoHigh since it is zero
            oneLow = firstRegister->getLowOrder();
            twoLow = _cg->allocateRegister();
            generateRegRegInstruction(MOV4RegReg, root, twoLow, secondRegister, _cg);
            }
         else
            {
            generateRegRegInstruction(MOV4RegReg, root, twoHigh, secondRegister->getHighOrder(), _cg);
            oneLow = _cg->allocateRegister();
            generateRegRegInstruction(MOV4RegReg, root, oneLow, firstRegister->getLowOrder(), _cg);
            twoLow = secondRegister->getLowOrder();
            }

         resultHigh = secondHighZero ? twoHigh : oneHigh;
         resultLow  = secondHighZero ? twoLow  : oneLow;
         otherHigh  = secondHighZero ? oneHigh : twoHigh;
         otherLow   = secondHighZero ? oneLow  : twoLow;
         }
      resultReg = _cg->allocateRegisterPair(resultLow, resultHigh);

      if (!firstHighZero)
         {
         generateRegRegInstruction(IMUL4RegReg, root, oneHigh, twoLow, _cg);
         intermediateResultReg = oneHigh;
         }
      if (!secondHighZero)
         {
         generateRegRegInstruction(IMUL4RegReg, root, twoHigh, oneLow, _cg);
         intermediateResultReg = twoHigh;
         }
      if (!firstHighZero && !secondHighZero)
         generateRegRegInstruction(ADD4RegReg, root, twoHigh, oneHigh, _cg);

      TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)2, 2, _cg);
      deps->addPreCondition (resultHigh, TR::RealRegister::edx, _cg);
      deps->addPostCondition(resultHigh, TR::RealRegister::edx, _cg);
      deps->addPreCondition (resultLow,  TR::RealRegister::eax, _cg);
      deps->addPostCondition(resultLow,  TR::RealRegister::eax, _cg);
      generateRegRegInstruction(MUL4AccReg, root, resultLow, otherLow, deps, _cg);
      if (intermediateResultReg)
         generateRegRegInstruction(ADD4RegReg, root, resultHigh, intermediateResultReg, _cg);

      // stop using the registers that we allocated
      //
      if (!firstHighZero && secondHighZero)
         _cg->stopUsingRegister(oneHigh);

      if (firstHighZero)
         {
         if (!secondHighZero)
            _cg->stopUsingRegister(twoHigh);
         }

      else
         {
         if (!secondHighZero)
            _cg->stopUsingRegister(twoHigh);
         }

      if (reportInlineMultiply)
         diagnostic("CopyRegs path in long multiply inline\n");

      root->setRegister(resultReg);
      }
   else if (getOpReg1Mem2())
      {
      TR::MemoryReference  *lowMR  = generateX86MemoryReference(secondChild, _cg);
      TR::MemoryReference  *highMR = generateX86MemoryReference(*lowMR, 4, _cg);

      if (!firstHighZero)
         {
         oneLow  = firstRegister->getLowOrder();
         oneHigh = firstRegister->getHighOrder();
         }
      else
         {
         oneLow  = firstRegister;
         oneHigh = _cg->allocateRegister();
         }

      TR::Register *oneLowCopy = 0;
      if (useSecondHighOrder)
         {
         lowMR = highMR;
         }
      if (!secondHighZero)
         {
         oneLowCopy = _cg->allocateRegister();
         generateRegRegInstruction(MOV4RegReg, root, oneLowCopy, oneLow, _cg);
         intermediateResultReg = oneLowCopy;
         generateRegMemInstruction(IMUL4RegMem, root, oneLowCopy, highMR, _cg);
         }

      TR::Register *oneHighCopy = 0;
      if (!firstHighZero)
         {
         if (secondHighZero)
            {
            oneHighCopy = _cg->allocateRegister();
            generateRegRegInstruction(MOV4RegReg, root, oneHighCopy, oneHigh, _cg);
            }
         else
            oneHighCopy = oneHigh;

         generateRegMemInstruction(IMUL4RegMem, root, oneHighCopy, lowMR, _cg);
         intermediateResultReg = oneHighCopy;
         }

      if (!firstHighZero && !secondHighZero)
         {
         generateRegRegInstruction(ADD4RegReg, root, oneLowCopy, oneHigh, _cg);
         intermediateResultReg = oneLowCopy;
         }

      TR::MemoryReference               *lowMR2 = generateX86MemoryReference(*lowMR, 0, _cg);
      TR::RegisterDependencyConditions  *deps   = generateRegisterDependencyConditions((uint8_t)2, 2, _cg);
      deps->addPreCondition (oneLow,  TR::RealRegister::eax, _cg);
      deps->addPostCondition(oneLow,  TR::RealRegister::eax, _cg);
      deps->addPreCondition (oneHigh, TR::RealRegister::edx, _cg);
      deps->addPostCondition(oneHigh, TR::RealRegister::edx, _cg);
      resultReg = _cg->allocateRegisterPair(oneLow, oneHigh);
      generateRegMemInstruction(MUL4AccMem, root, oneLow, lowMR2, deps, _cg);
      if (intermediateResultReg)
         {
         generateRegRegInstruction(ADD4RegReg, root, oneHigh, intermediateResultReg, _cg);
         }

      if (oneLowCopy)
         {
         _cg->stopUsingRegister(oneLowCopy);
         }

      if (oneHighCopy &&
          (oneHighCopy != oneHigh))
         {
         _cg->stopUsingRegister(oneHighCopy);
         }

      if (reportInlineMultiply)
         diagnostic("OpReg1Mem2 path in long multiply inline\n");

      root->setRegister(resultReg);
      lowMR->decNodeReferenceCounts(_cg);
      }
   else
      {
      TR::MemoryReference  *lowMR   = generateX86MemoryReference(firstChild, _cg);
      TR::MemoryReference  *highMR  = generateX86MemoryReference(*lowMR, 4, _cg);
      oneLow  = _cg->allocateRegister();
      oneHigh = 0;

      if (useFirstHighOrder)
         {
         lowMR = highMR;
         }
      generateRegMemInstruction(L4RegMem, root, oneLow, lowMR, _cg);
      if (!firstHighZero)
         {
         oneHigh = _cg->allocateRegister();
         generateRegMemInstruction(L4RegMem, root, oneHigh, highMR, _cg);
         }

      if (!secondHighZero)
         {
         twoHigh = secondRegister->getHighOrder();
         twoLow  = secondRegister->getLowOrder();
         generateRegRegInstruction(IMUL4RegReg, root, twoHigh, oneLow, _cg);
         intermediateResultReg = twoHigh;
         }
      else
         {
         twoLow  = secondRegister;
         twoHigh = 0;
         }

      if (!firstHighZero)
         {
         generateRegRegInstruction(IMUL4RegReg, root, oneHigh, twoLow, _cg);
         intermediateResultReg = oneHigh;
         }

      if (!firstHighZero)
         {
         if (!secondHighZero)
            {
            generateRegRegInstruction(ADD4RegReg, root, twoHigh, oneHigh, _cg);
            }
         else
            {
            twoHigh = _cg->allocateRegister();
            generateRegRegInstruction(MOV4RegReg, root, twoHigh, oneHigh, _cg);
            }
         intermediateResultReg = twoHigh;
         }


      TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)2, 2, _cg);
      deps->addPreCondition (oneLow,  TR::RealRegister::eax, _cg);
      deps->addPostCondition(oneLow,  TR::RealRegister::eax, _cg);
      if (oneHigh == 0)
         {
         oneHigh = _cg->allocateRegister();
         }
      deps->addPreCondition (oneHigh, TR::RealRegister::edx, _cg);
      deps->addPostCondition(oneHigh, TR::RealRegister::edx, _cg);
      resultReg = _cg->allocateRegisterPair(oneLow, oneHigh);
      generateRegRegInstruction(MUL4AccReg, root, oneLow, twoLow, deps, _cg);
      if (intermediateResultReg)
         {
         generateRegRegInstruction(ADD4RegReg, root, oneHigh, intermediateResultReg, _cg);
         }

      if (!firstHighZero &&
           secondHighZero)
         {
         _cg->stopUsingRegister(twoHigh);
         }

      if (reportInlineMultiply)
         diagnostic("OpReg2Mem1 path in long multiply inline\n");

      root->setRegister(resultReg);
      lowMR->decNodeReferenceCounts(_cg);
      notReversedOperands();
      }

   _cg->decReferenceCount(firstChild);
   _cg->decReferenceCount(secondChild);
   }

const uint8_t TR_X86BinaryCommutativeAnalyser::_actionMap[NUM_ACTIONS] =
   {                  // Reg1 Mem1 Clob1 Reg2 Mem2 Clob2
   EvalChild1 |       //  0    0     0    0    0     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  0    0     0    0    0     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  0    0     0    0    1     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  0    0     0    0    1     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  0    0     0    1    0     0
   EvalChild2 |
   CopyReg2,

   EvalChild1 |       //  0    0     0    1    0     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  0    0     0    1    1     0
   EvalChild2 |
   CopyReg2,

   EvalChild1 |       //  0    0     0    1    1     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  0    0     1    0    0     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  0    0     1    0    0     1
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  0    0     1    0    1     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  0    0     1    0    1     1
   OpReg1Mem2,

   EvalChild1 |       //  0    0     1    1    0     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  0    0     1    1    0     1
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  0    0     1    1    1     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  0    0     1    1    1     1
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  0    1     0    0    0     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  0    1     0    0    0     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  0    1     0    0    1     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  0    1     0    0    1     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  0    1     0    1    0     0
   EvalChild2 |
   CopyReg2,

   EvalChild1 |       //  0    1     0    1    0     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  0    1     0    1    1     0
   EvalChild2 |
   CopyReg2,

   EvalChild1 |       //  0    1     0    1    1     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  0    1     1    0    0     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild2 |       //  0    1     1    0    0     1
   OpReg2Mem1,

   EvalChild1 |       //  0    1     1    0    1     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  0    1     1    0    1     1
   OpReg1Mem2,

   EvalChild1 |       //  0    1     1    1    0     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild2 |       //  0    1     1    1    0     1
   OpReg2Mem1,

   EvalChild1 |       //  0    1     1    1    1     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild2 |       //  0    1     1    1    1     1
   OpReg2Mem1,

   EvalChild1 |       //  1    0     0    0    0     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  1    0     0    0    0     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  1    0     0    0    1     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  1    0     0    0    1     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  1    0     0    1    0     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  1    0     0    1    0     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  1    0     0    1    1     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  1    0     0    1    1     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  1    0     1    0    0     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  1    0     1    0    0     1
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  1    0     1    0    1     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  1    0     1    0    1     1
   OpReg1Mem2,

   EvalChild1 |       //  1    0     1    1    0     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  1    0     1    1    0     1
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  1    0     1    1    1     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  1    0     1    1    1     1
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  1    1     0    0    0     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  1    1     0    0    0     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  1    1     0    0    1     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  1    1     0    0    1     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  1    1     0    1    0     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  1    1     0    1    0     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  1    1     0    1    1     0
   EvalChild2 |
   CopyReg1,

   EvalChild1 |       //  1    1     0    1    1     1
   EvalChild2 |
   OpReg2Reg1,

   EvalChild1 |       //  1    1     1    0    0     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  1    1     1    0    0     1
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  1    1     1    0    1     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  1    1     1    0    1     1
   OpReg1Mem2,

   EvalChild1 |       //  1    1     1    1    0     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  1    1     1    1    0     1
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  1    1     1    1    1     0
   EvalChild2 |
   OpReg1Reg2,

   EvalChild1 |       //  1    1     1    1    1     1
   EvalChild2 |
   OpReg1Reg2
   };
