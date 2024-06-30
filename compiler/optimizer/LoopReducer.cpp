/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "optimizer/LoopReducer.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/NodeUtils.hpp"
#include "il/Node_inlines.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "infra/TreeServices.hpp"
#include "optimizer/LoopCanonicalizer.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TranslateTable.hpp"
#include "optimizer/VPConstraint.hpp"
#include "ras/Debug.hpp"

#define OPT_DETAILS "O^O LOOP TRANSFORMATION: "


static TR::Node * testNode(TR::Compilation * comp, TR::Node * node, TR::ILOpCodes value, const char* msg)
   {
   if (node->getOpCodeValue() != value)
      {
      if (msg) dumpOptDetails(comp, msg);
      return NULL;
      }
   else
      {
      return node;
      }
   }

static TR::Node * testUnary(TR::Compilation * comp, TR::Node * parent, TR::ILOpCodes value, const char* msg)
   {
   return testNode(comp, parent->getFirstChild(), value, msg);
   }

static TR::Node * testBinary(
   TR::Compilation * comp, TR::Node * parent, TR::ILOpCodes value, TR::ILOpCodes leftChildValue, TR::ILOpCodes rightChildValue, const char* msg)
   {
   TR::Node * child = parent->getFirstChild();
   if (child->getOpCodeValue() != value || child->getFirstChild()->getOpCodeValue() != leftChildValue ||
       child->getSecondChild()->getOpCodeValue() != rightChildValue)
      {
      if (msg) dumpOptDetails(comp, msg);
      return NULL;
      }
   else
      {
      return child;
      }
   }

static TR::Node * testBinaryIConst(TR::Compilation * comp, TR::Node * parent, TR::ILOpCodes value, TR::ILOpCodes leftChildValue,
                                  int32_t rightChildValue, const char* msg)
   {
   TR::Node * child = parent->getFirstChild();
   if (child->getOpCodeValue() != value || child->getFirstChild()->getOpCodeValue() != leftChildValue ||
       child->getSecondChild()->getOpCodeValue() != TR::iconst || child->getSecondChild()->getInt() != rightChildValue)
      {
      if (msg) dumpOptDetails(comp, msg);
      return NULL;
      }
   else
      {
      return child;
      }
   }


TR_LRAddressTree::TR_LRAddressTree(TR::Compilation * comp, TR_InductionVariable * indVar)
   : TR_AddressTree(heapAlloc, comp), _indVar(indVar), _increment(indVar->getIncr()->getLowInt()), _indVarLoad(NULL)
   {
   }

TR_ArrayLoop::TR_ArrayLoop(TR::Compilation * comp, TR_InductionVariable * firstIndVar, TR_InductionVariable * secondIndVar)
   : _comp(comp), _firstAddress(comp, firstIndVar), _secondAddress(comp, secondIndVar), _thirdAddress(comp, secondIndVar),
     _finalNode(NULL), _addInc(false), _forwardLoop(false)
   {
   }

TR_ArrayLoop::TR_ArrayLoop(TR::Compilation * comp, TR_InductionVariable * indVar)
   : _comp(comp), _firstAddress(comp, indVar), _secondAddress(comp, indVar), _thirdAddress(comp, indVar), _finalNode(NULL), _addInc(false), _forwardLoop(false)
   {
   }

TR_Arrayset::TR_Arrayset(TR::Compilation * comp, TR_InductionVariable * indVar)
   : TR_ArrayLoop(comp, indVar)
   {
   }
TR_Arraycopy::TR_Arraycopy(TR::Compilation * comp, TR_InductionVariable * indVar)
   : TR_ArrayLoop(comp, indVar)
   {
   }
TR_ByteToCharArraycopy::TR_ByteToCharArraycopy(TR::Compilation * comp, TR_InductionVariable * firstIndVar, TR_InductionVariable * secondIndVar, bool bigEndian)
   : TR_ArrayLoop(comp, firstIndVar, secondIndVar), _bigEndian(bigEndian)
   {
   }
TR_CharToByteArraycopy::TR_CharToByteArraycopy(TR::Compilation * comp, TR_InductionVariable * firstIndVar, TR_InductionVariable * secondIndVar, bool bigEndian)
   : TR_ArrayLoop(comp, firstIndVar, secondIndVar), _bigEndian(bigEndian)
   {
   }

TR_Arraycmp::TR_Arraycmp(TR::Compilation * comp, TR_InductionVariable * indVar)
   : _targetOfGotoBlock(NULL), _firstLoad(NULL), _secondLoad(NULL), TR_ArrayLoop(comp, indVar)
   {
   }

TR_Arraytranslate::TR_Arraytranslate(TR::Compilation * comp, TR_InductionVariable * indVar, bool hasBranch, bool hasBreak)
   : TR_ArrayLoop(comp, indVar), _inputNode(NULL), _outputNode(NULL), _termCharNode(NULL), _tableNode(NULL), _resultNode(NULL),
   _resultUnconvertedNode(NULL), _usesRawStorage(false), _compilerGeneratedTable(false), _hasBranch(hasBranch), _hasBreak(hasBreak), _compareOp(TR::BadILOp)
   {
   }

TR_ArraytranslateAndTest::TR_ArraytranslateAndTest(TR::Compilation * comp, TR_InductionVariable * indVar)
   : TR_ArrayLoop(comp, indVar), _termCharNode(NULL)
   {
   }

bool
TR_LRAddressTree::processBaseAndIndex(TR::Node* parent)
   {
   TR::Node * lhs = parent->getFirstChild();
   TR::Node * rhs = parent->getSecondChild();

   TR::RegisterMappedSymbol * indSym = _indVar->getLocal();
   if (isILLoad(lhs) && (lhs->getSymbol()->getRegisterMappedSymbol() == indSym))
      {
      _indVarNode.setParentAndChildNumber(parent, 0);
      if (isILLoad(rhs))
         {
         _baseVarNode.setParentAndChildNumber(parent, 1);
         }
      }
   else if (isILLoad(rhs) && (rhs->getSymbol()->getRegisterMappedSymbol() == indSym))
      {
      _indVarNode.setParentAndChildNumber(parent, 1);
      if (isILLoad(lhs))
         {
         _baseVarNode.setParentAndChildNumber(parent, 0);
         }
      }
   else
      {
      return false;
      }

   return true;
   }

bool
TR_LRAddressTree::checkAiadd(TR::Node * aiaddNode, int32_t elementSize)
   {
   if (!process(aiaddNode))
      {
      dumpOptDetails(comp(), "checkAiadd: base processing of node did not match criteria\n");
      return false;
      }

   TR::RegisterMappedSymbol * indSym = _indVar->getLocal();

   if (_indVarNode.isNull() || (_indVarNode.getChild()->skipConversions()->getSymbol()->getRegisterMappedSymbol() != indSym))
      {
      dumpOptDetails(comp(), "checkAiadd: induction variable does not match index variable\n");
      return false;
      }

   TR::RegisterMappedSymbol * loadSym = _indVarNode.getChild()->skipConversions()->getSymbol()->getRegisterMappedSymbol();
   if (loadSym != indSym)
      {
      if (_matIndVarSymRef)
         {
         if (loadSym != _matIndVarSymRef->getSymbol()->getRegisterMappedSymbol())
            {
            dumpOptDetails(comp(), "checkAiadd: load in the aiadd tree does not match materialized induction variable\n");
            return false;
            }
         }
      else
         {
         dumpOptDetails(comp(), "checkAiadd: induction variable does not match index variable\n");
         return false;
         }
      }

   if (_multiplyNode.isNull() && ((elementSize != _increment) && (elementSize != -_increment)))
      {
      dumpOptDetails(comp(), "checkAiadd: sub-tree does not have induction variable change consistent with increment of multiplier (%d %d)\n",
         elementSize, _increment);
      return false;
      }

   switch (_multiplier)
      {
      case 1:
         if (elementSize == 1 && (_increment == 1 || _increment == -1))
            {
            return true;
            }
         break;
      case 2:
         if (elementSize == 2 && (_increment == 1 || _increment == -1))
            {
            return true;
            }
         break;
      case 4:
         if (elementSize == 4 && (_increment == 1 || _increment == -1))
            {
            return true;
            }
         break;

      case 8:
         if (elementSize == 8 && (_increment == 1 || _increment == -1))
            {
            return true;
            }
         break;

      default:
         break;
   }

   return false;
   }

bool
TR_Arrayset::checkArrayStore(TR::Node * storeNode)
   {
   if (!storeNode->getOpCode().isStoreIndirect())
      {
      dumpOptDetails(comp(), "arraystore tree does not have an indirect store as root\n");
      return false;
      }
   TR::Node * storeFirstChild = storeNode->getFirstChild();
   TR::ILOpCodes opCodeStoreFirstChild = storeFirstChild->getOpCodeValue();
   TR::Node * storeSecondChild = storeNode->getSecondChild();
   TR::ILOpCodes opCodeStoreSecondChild = storeSecondChild->getOpCodeValue();

   if (opCodeStoreSecondChild == TR::iload && storeSecondChild->getSymbol()->getRegisterMappedSymbol() == getIndVar()->getLocal())
      {
      dumpOptDetails(comp(), "arraystore tree has induction variable on rhs\n");
      return false;
      }

   if (!storeSecondChild->getOpCode().isLoadDirectOrReg())
      {
      dumpOptDetails(comp(), "arraystore tree does not have a constant load, or constant load is an address\n");
      return false;
      }
   return (getStoreAddress()->checkAiadd(storeFirstChild, storeNode->getSize()));
   }

bool
TR_Arraycopy::checkArrayStore(TR::Node * storeNode)
   {
   if (!storeNode->getOpCode().isStoreIndirect() && !(storeNode->getOpCodeValue() == TR::treetop && storeNode->getFirstChild()->getOpCodeValue() == TR::awrtbari))
      {
      dumpOptDetails(comp(), "arraycopy arraystore tree does not have an indirect store as root\n");
      return false;
      }
   if (storeNode->getOpCodeValue() == TR::treetop)
      {
      storeNode = storeNode->getFirstChild();
      _hasWriteBarrier = true;
      }
   else
      {
      _hasWriteBarrier = false;
      }
   TR::Node * storeFirstChild = storeNode->getFirstChild();
   TR::ILOpCodes opCodeStoreFirstChild = storeFirstChild->getOpCodeValue();
   TR::Node * storeSecondChild = storeNode->getSecondChild();
   TR::ILOpCodes opCodeStoreSecondChild = storeSecondChild->getOpCodeValue();

   TR::Node * loadNode = storeSecondChild;
   if (!loadNode->getOpCode().isLoadIndirect())
      {
      dumpOptDetails(comp(), "arraycopy arraystore tree does not have an indirect load as the second child\n");
      return false;
      }
   if (loadNode->getSize() != storeNode->getSize())
      {
      dumpOptDetails(comp(), "arraycopy src and dst trees are not of the same size\n");
      return false;
      }
   _copySize = loadNode->getSize();

   TR::Node * loadFirstChild = loadNode->getFirstChild();


   if (storeFirstChild->getNumChildren() == 0 || loadFirstChild->getNumChildren() == 0   ||
       !storeFirstChild->getFirstChild()->getOpCode().hasSymbolReference() ||
       !loadFirstChild->getFirstChild()->getOpCode().hasSymbolReference() ||
       storeFirstChild->getFirstChild()->getSymbol()->getRegisterMappedSymbol() == loadFirstChild->getFirstChild()->getSymbol()->getRegisterMappedSymbol())
      {
      dumpOptDetails(comp(), "arraycopy src and dst are against same object - punt for now\n");
      return false;
      }

   bool checkStore = getStoreAddress()->checkAiadd(storeFirstChild, storeNode->getSize());
   bool checkLoad  = getLoadAddress()->checkAiadd(loadFirstChild, loadNode->getSize());
   _storeNode = storeNode;

   return checkStore && checkLoad;
   }

bool
TR_ByteToCharArraycopy::checkArrayStore(TR::Node * storeNode)
   {
   if (storeNode->getOpCodeValue() != TR::sstorei)
      {
      dumpOptDetails(comp(), "byte to char arraycopy arraystore tree does not have an indirect store as root\n");
      return false;
      }
   TR::Node * storeFirstChild = storeNode->getFirstChild();
   TR::ILOpCodes opCodeStoreFirstChild = storeFirstChild->getOpCodeValue();

   bool checkStore = getStoreAddress()->checkAiadd(storeFirstChild, storeNode->getSize());

   return checkStore;
   }

// Need to verify that the byte loads sub-tree looks like the following:
// Will use checkAiadd() to verify the address children but need an additional
// check to ensure that the only difference between the two address trees is
// that the memory offset of the second child is one more than the memory offset
// of the first child (in this case, 16 and 17).
//  i2s
//    ior
//      imul
//       bu2i
//          ibload #132[0x005f1c80] Shadow[<array-shadow>]
//            aiadd
//              aload #221[0x00703b50] Auto[<temp slot 11>]
//              isub
//                iload #175[0x005f1444] Auto[<auto slot 4>]
//                ==>iconst -16 at [0x005f21f8]
//       iconst 256
//      bu2i
//        ibload #132[0x005f1c80] Shadow[<array-shadow>]
//          aiadd
//            ==>aload at [0x005f1ac8]
//            isub
//              ==>iload at [0x005f1af4]
//             iconst -17
bool
TR_ByteToCharArraycopy::checkByteLoads(TR::Node * loadNodes)
   {
   if (loadNodes->getOpCodeValue() != TR::i2s)
      {
      dumpOptDetails(comp(), "checkByteLoads: byte to char arraycopy byte loads is not headed with i2c\n");
      return false;
      }
   TR::Node * joinNode = loadNodes->getFirstChild();
   if (joinNode->getOpCodeValue() != TR::ior && joinNode->getOpCodeValue() != TR::iadd)
      {
      dumpOptDetails(comp(), "checkByteLoads: byte to char arraycopy byte loads not joined with OR or ADD\n");
      return false;
      }
   TR::Node * highByteNode = joinNode->getFirstChild();
   TR::Node * lowByteNode  = joinNode->getSecondChild();

   if (highByteNode->getOpCodeValue() != TR::imul &&
       (highByteNode->getOpCodeValue() == TR::bu2i && lowByteNode->getOpCodeValue() == TR::imul))
      {
      // operands may be reversed - swap
      dumpOptDetails(comp(), "checkByteLoads: try swapping the 2 OR/ADD children\n");
      TR::Node * tmpNode = highByteNode;
      highByteNode = lowByteNode;
      lowByteNode  = tmpNode;
      }
   else
      {
      if (highByteNode->getOpCodeValue() != TR::imul || lowByteNode->getOpCodeValue() != TR::bu2i)
         {
         dumpOptDetails(comp(), "checkByteLoads: byte to char arraycopy byte loads do not have imul/bu2i children\n");
         return false;
         }
      }

   if (highByteNode->getFirstChild()->getOpCodeValue() != TR::bu2i || highByteNode->getFirstChild()->getFirstChild()->getOpCodeValue() != TR::bloadi)
      {
      dumpOptDetails(comp(), "checkByteLoads: high byte load does not have bu2i/ibload\n");
      return false;
      }
   if (lowByteNode->getFirstChild()->getOpCodeValue() != TR::bloadi)
      {
      dumpOptDetails(comp(), "checkByteLoads: low byte load does not have ibload\n");
      return false;
      }

   if (highByteNode->getSecondChild()->getOpCodeValue() != TR::iconst || highByteNode->getSecondChild()->getInt() != 256)
      {
      dumpOptDetails(comp(), "checkByteLoads: multiplier for high value is not 256\n");
      return false;
      }

   TR::Node * lowByteAddressNode  = lowByteNode->getFirstChild()->getFirstChild();
   TR::Node * highByteAddressNode = highByteNode->getFirstChild()->getFirstChild()->getFirstChild();
   TR_ParentOfChildNode lowLoadMultiplyNode;
   TR_ParentOfChildNode lowLoadIndVarNode;

   bool checkHighLoad = getHighLoadAddress()->checkAiadd(highByteAddressNode, 2);
   bool checkLowLoad  = getLowLoadAddress()->checkAiadd(lowByteAddressNode, 2);

   if (!checkHighLoad || !checkLowLoad)
      {
      dumpOptDetails(comp(), "checkByteLoads: aiadd tree in error (%d,%d)\n", checkHighLoad, checkLowLoad);
      return false;
      }

   if (getLowLoadAddress()->getOffset() != getHighLoadAddress()->getOffset() + 1)
      {
      dumpOptDetails(comp(), "checkByteLoads: second offset is not one greater than first offset (%d %d)\n",
         (int32_t)getLowLoadAddress()->getOffset(), (int32_t)getHighLoadAddress()->getOffset());
      return false;
      }

   TR::RegisterMappedSymbol * hbs = getHighLoadAddress()->getBaseVarNode()->getChild() ? getHighLoadAddress()->getBaseVarNode()->getChild()->getSymbol()->getRegisterMappedSymbol() : NULL;
   TR::RegisterMappedSymbol * lbs = getLowLoadAddress()->getBaseVarNode()->getChild() ? getLowLoadAddress()->getBaseVarNode()->getChild()->getSymbol()->getRegisterMappedSymbol() : NULL;
   if (hbs || lbs)
      {
      // if either has a base var, both must, and they both must be the same
      if (!hbs || !lbs || hbs != lbs)
         {
         dumpOptDetails(comp(), "checkByteLoads: at least one tree has a base sym, but both trees do not have the same sym (%p %p)\n",
            lbs, hbs);
         return false;
         }
      }

   return true;
   }

bool
TR_CharToByteArraycopy::checkArrayStores(TR::Node * origHighStoreNode, TR::Node * origLowStoreNode)
   {
   TR::Node * highStoreNode;
   TR::Node * lowStoreNode;
   if (_bigEndian)
      {
      highStoreNode = origHighStoreNode;
      lowStoreNode = origLowStoreNode;
      }
   else
      {
      highStoreNode = origLowStoreNode;
      lowStoreNode = origHighStoreNode;
      }

   if (highStoreNode->getOpCodeValue() != TR::bstorei)
      {
      dumpOptDetails(comp(), "checkArrayStores: char to byte arraycopy high arraystore tree does not have an indirect store as root\n");
      return false;
      }
   if (lowStoreNode->getOpCodeValue() != TR::bstorei)
      {
      dumpOptDetails(comp(), "checkArrayStores: char to byte arraycopy low arraystore tree does not have an indirect store as root\n");
      return false;
      }
   TR::Node * highStoreAddressNode = highStoreNode->getFirstChild();
   TR::Node * lowStoreAddressNode  = lowStoreNode->getFirstChild();
   bool checkHighStore = getHighStoreAddress()->checkAiadd(highStoreAddressNode, 2);
   bool checkLowStore  = getLowStoreAddress()->checkAiadd(lowStoreAddressNode, 2);

   if (!checkHighStore || !checkLowStore)
      {
      return false;
      }

   if ((int32_t)getLowStoreAddress()->getOffset() != (int32_t)getHighStoreAddress()->getOffset() + 1)
      {
      dumpOptDetails(comp(), "checkArrayStores: second offset is not 1 greater than first offset (%d %d)\n",
         (int32_t)getLowStoreAddress()->getOffset(), (int32_t)getHighStoreAddress()->getOffset());
      return false;
      }

   TR::Node * ishr = testNode(comp(), origHighStoreNode->getSecondChild(), TR::i2b, "checkArrayStores: high store child is not i2b\n");
   if (!ishr) return false;
   TR::Node * iand = testBinaryIConst(comp(), ishr, TR::ishr, TR::iand, 8, "checkArrayStores: high store child is not ishr of iand and 8\n");
   if (!iand) return false;
   TR::Node * c2i = testBinaryIConst(comp(), iand, TR::iand, TR::su2i, 0xFF00, "checkArrayStores: high store child is not iand of su2i and 0xFF00\n");
   if (!c2i) return false;
   TR::Node * isload = testUnary(comp(), c2i->getFirstChild(), TR::sloadi, "checkArrayStores: high store child is not isload\n");
   if (!isload) return false;

   bool checkLoad = getLoadAddress()->checkAiadd(isload->getFirstChild(), 2);
   if (!checkLoad)
      {
      return false;
      }

   iand = testNode(comp(), origLowStoreNode->getSecondChild(), TR::i2b, "checkArrayStores: low store child is not i2b\n");
   if (!iand) return false;
   c2i = testBinaryIConst(comp(), iand, TR::iand, TR::su2i, 0xFF, "checkArrayStores: low store child is not iand of su2i and 0xFF\n");
   if (!c2i) return false;
   TR::Node * isloadDup = testUnary(comp(), c2i->getFirstChild(), TR::sloadi, "checkArrayStores: low store child is not isload\n");
   if (!isloadDup) return false;

   if (isloadDup != isload)
      {
      dumpOptDetails(comp(), "checkArrayStores: two isload addresses are not the same\n");
      return false;
      }

   return true;
   }

//
// This method verifies that the sub-tree passed in conforms to an increment of a
// variable and that the variable being incremented is the induction variable for
// the loop.
//
// istore <ind var>
//   iadd
//     iload <ind var>
//     iconst <increment>
//
// Also - the iconst increment must match the increment of the loop
//
bool
TR_LRAddressTree::checkIndVarStore(TR::Node * indVarNode)
   {
   if (!indVarNode->getOpCode().isStoreDirect())
      {
      dumpOptDetails(comp(), "induction variable tree does not have a direct store as root\n");
      return false;
      }

   TR::Node * storeFirstChild = indVarNode->getFirstChild();
   TR::ILOpCodes opCodeStoreFirstChild = storeFirstChild->getOpCodeValue();

   if (opCodeStoreFirstChild != TR::iadd && opCodeStoreFirstChild != TR::isub)
      {
      dumpOptDetails(comp(), "first child of istore is not TR::iadd/TR::isub\n");
      return false;
      }

   TR::Node * iaddFirstChild = storeFirstChild->getFirstChild();
   TR::ILOpCodes opCodeIaddFirstChild = iaddFirstChild->getOpCodeValue();
   TR::Node * iaddSecondChild = storeFirstChild->getSecondChild();
   TR::ILOpCodes opCodeIaddSecondChild = iaddSecondChild->getOpCodeValue();

   if (opCodeIaddFirstChild != TR::iload || opCodeIaddSecondChild != TR::iconst)
      {
      dumpOptDetails(comp(), "first child of iadd is not TR::iload or second child is not TR::iconst\n");
      return false;
      }
   TR::RegisterMappedSymbol * indSym = _indVar->getLocal();
   if (indSym != iaddFirstChild->getSymbol()->getRegisterMappedSymbol())
      {
      dumpOptDetails(comp(), "iload symbol for aload does not match induction variable\n");
      return false;
      }

   _indVarSymRef = iaddFirstChild->getSymbolReference();

   int32_t realIncr = iaddSecondChild->getInt();
   if (realIncr < 0 && (opCodeStoreFirstChild == TR::isub))
      {
      realIncr = -realIncr;
      }
   if (_increment != realIncr)
      {
      dumpOptDetails(comp(), "increment does not match induction variable increment\n");
      return false;
      }
   else
      {
      _indVarLoad = iaddFirstChild;
      return true;
      }
   }

//
// This method verifies that the sub-tree passed in conforms to a conditional
// branch on the induction variable
//
// ificmpge --> OutOfLoop
//   iload #InductionVariable
//   iconst 0 (or iload <val> if loop invariant)
// -or-
// ificmpge --> OutOfLoop
//   <indvar node expression from indvar node tree>
//   iconst 0 (or iload <val> if loop invariant)

bool
TR_ArrayLoop::checkLoopCmp(TR::Node * loopCmpNode, TR::Node * indVarStoreNode, TR_InductionVariable * indVar)
   {
   if (!loopCmpNode->getOpCode().isIf())
      {
      dumpOptDetails(comp(), "loop compare tree does not have an if as root\n");
      return false;
      }

   TR::ILOpCodes compareOp = loopCmpNode->getOpCode().getOpCodeValue();
   // if the comparison to leave the loop is equality, add (subtract) one to ind var
   // on exit
   if (compareOp == TR::ificmpeq || compareOp == TR::ificmpge || compareOp == TR::ificmple ||
       compareOp == TR::ifiucmpge || compareOp == TR::ifiucmple)
      {
      _addInc = true;
      }

   if (compareOp == TR::ificmplt || compareOp == TR::ificmple ||
       compareOp == TR::ifiucmplt || compareOp == TR::ifiucmple)
      {
      _forwardLoop = true;
      }

   TR::Node * cmpFirstChild = loopCmpNode->getFirstChild();
   TR::ILOpCodes opCodeCmpFirstChild = cmpFirstChild->getOpCodeValue();
   TR::Node * cmpSecondChild = loopCmpNode->getSecondChild();
   TR::ILOpCodes opCodeCmpSecondChild = cmpSecondChild->getOpCodeValue();

   if (opCodeCmpFirstChild != TR::iload && cmpFirstChild != indVarStoreNode->getFirstChild())
      {
      dumpOptDetails(comp(), "loop compare does not have iload or indvarnode expr as first child\n");
      return false;
      }

   if (opCodeCmpSecondChild != TR::iconst && opCodeCmpSecondChild != TR::iload && !cmpSecondChild->getOpCode().isArrayLength())
      {
      dumpOptDetails(comp(), "loop compare does not have iconst/iload/arraylength as second child\n");
      return false;
      }

   if (opCodeCmpFirstChild == TR::iload)
      {
      TR::RegisterMappedSymbol * indSym = indVar->getLocal();
      if (indSym != cmpFirstChild->getSymbol()->getRegisterMappedSymbol())
         {
         dumpOptDetails(comp(), "loop compare does not use induction variable\n");
         return false;
         }
      }

   _finalNode = cmpSecondChild;
   return true;
   }

int32_t
TR_ArrayLoop::checkForPostIncrement(TR::Block *loopHeader, TR::Node *indVarStoreNode,
                                             TR::Node *loopCmpNode, TR::Symbol *ivSym)
   {
   TR::TreeTop *startTree = loopHeader->getFirstRealTreeTop();
   bool storeFound = false;
   vcount_t visitCount = comp()->incVisitCount();
   TR_ScratchList<TR::Node> ivLoads(comp()->trMemory());
   for (TR::TreeTop *tt = startTree; !storeFound && tt != loopHeader->getExit(); tt = tt->getNextTreeTop())
      findIndVarLoads(tt->getNode(), indVarStoreNode, storeFound, &ivLoads, ivSym, visitCount);

   TR::Node *cmpFirstChild = loopCmpNode->getFirstChild();

   TR::Node *storeIvLoad = indVarStoreNode->getFirstChild();
   if (storeIvLoad->getOpCode().isAdd() || storeIvLoad->getOpCode().isSub())
      storeIvLoad = storeIvLoad->getFirstChild();

   // simple case
   // the loopCmp uses the un-incremented value
   // of the iv
   //
   if (storeIvLoad == cmpFirstChild)
      return 1;

   // the loopCmp uses some load of the iv that
   // was commoned
   //
   if (ivLoads.find(cmpFirstChild))
      return 1;

   // uses a brand new load of the iv
   return 0;
   }

void
TR_ArrayLoop::findIndVarLoads(TR::Node *node, TR::Node *indVarStoreNode, bool &storeFound,
                                             List<TR::Node> *ivLoads, TR::Symbol *ivSym, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;
   node->setVisitCount(visitCount);

   if (node == indVarStoreNode)
      storeFound = true;

   if (node->getOpCodeValue() == TR::iload &&
        node->getSymbolReference()->getSymbol() == ivSym)
      {
      if (!ivLoads->find(node))
         ivLoads->add(node);
      }

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      findIndVarLoads(node->getChild(i), indVarStoreNode, storeFound, ivLoads, ivSym, visitCount);
   }

// updateIndVarStore updates the indVarStore tree to have a multiplier
// so that the final value of the induction variable will be correct.
TR::Node *
TR_ArrayLoop::updateIndVarStore(TR_ParentOfChildNode * indVarNode, TR::Node * indVarStoreNode,
                                    TR_LRAddressTree* tree, int32_t postIncrement)
   {
   int32_t endInc = tree->getIncrement() * tree->getMultiplier();
   TR::Node * startNode;
   TR::Node * endNode;
   if (endInc < 0)
      {
      startNode = _finalNode;
      endNode = tree->getIndVarLoad();
      endInc = -endInc;
      }
   else
      {
      startNode = tree->getIndVarLoad();
      endNode = _finalNode;
      }

   //
   // adjust the element size by <element-size> if the loop was ==, <=, >= and
   // then multiply by the size of the induction variable (which could be 1)
   //
   TR::Node * isub = TR::Node::create(TR::isub, 2, endNode->duplicateTree(), startNode->duplicateTree());
   if (postIncrement != 0)
      isub = TR::Node::create(TR::iadd, 2, isub, TR::Node::create(isub, TR::iconst, 0, postIncrement));
   if (_addInc)
      {
      int32_t positiveIncrement = (tree->getIncrement() < 0) ? -(tree->getIncrement()) : tree->getIncrement();
      TR::Node * elemSizeInc = TR::Node::create(_finalNode, TR::iconst, 0, positiveIncrement);
      isub = TR::Node::create(TR::iadd, 2, isub, elemSizeInc);
      }


   //TR::Node * incMul = TR::Node::create(_finalNode, TR::iconst, 0, endInc);
   //TR::Node * imul = TR::Node::create(TR::imul, 2, isub, incMul);
   //
   TR::Node * incMul = NULL;
   TR::Node * imul = NULL;
   if (comp()->target().is64Bit())
      {
      incMul = TR::Node::create(_finalNode, TR::lconst);
      incMul->setLongInt(endInc);
      isub = TR::Node::create(TR::i2l, 1, isub);
      imul = TR::Node::create(TR::lmul, 2, isub, incMul);
      }
   else
      {
      incMul = TR::Node::create(_finalNode, TR::iconst, 0, endInc);
      imul = TR::Node::create(TR::imul, 2, isub, incMul);
      }

   //
   // the istore for the induction variable that was originally
   // in the loop can be re-used as an istore to ensure the induction
   // variable has the right value after the arrayset by just
   // changing the iload on the isub's first child to be an
   // iload of the final node. The subtraction of the increment
   // remains to keep the original semantics of the loop, if the
   // loop was a <=, >=, or == type of loop (as opposed to < and >)
   //
   TR::Node * replacedNode = indVarStoreNode->getFirstChild()->getFirstChild();
   // first child of the indVarStoreNode might be commoned with array indexes
   // before we change the child of the isub, check if it is commoned, and create a copy if so.
   if (indVarStoreNode->getFirstChild()->getReferenceCount() > 1)
      {
      replacedNode = indVarStoreNode->getFirstChild();
      //duplicate the isub tree
      indVarStoreNode->setAndIncChild(0, indVarStoreNode->getFirstChild()->duplicateTree());
      }

   indVarStoreNode->getFirstChild()->setAndIncChild(0, _finalNode->duplicateTree());
   replacedNode->recursivelyDecReferenceCount();

   if (!_addInc && (postIncrement == 0))
      {
      TR_ParentOfChildNode secondChild(indVarStoreNode->getFirstChild(), 1);
      secondChild.setChild(TR::Node::create(endNode, TR::iconst, 0, (int32_t) 0));
      }

   return imul;
   }

// updateAiaddSubTree will update the aiadd tree to have the start value in
// the base address calculation instead of the induction variable, if the
// loop was originally run backwards.
void
TR_LRAddressTree::updateAiaddSubTree(TR_ParentOfChildNode * indVarNode, TR_ArrayLoop* loop)
   {
   TR::Node * finalNode = loop->getFinalNode();
   bool addInc = loop->getAddInc();
   int32_t endInc = _increment;
   if (endInc < 0 && !indVarNode->isNull())
      {
      // array is being run backwards - need to update the start calculation
      // to not use the indVarLoad but instead use the finalNode
      if (indVarNode->getParent()->getType().isInt64() && !finalNode->getType().isInt64())
         {
         TR::Node * i2lNode = TR::Node::create(TR::i2l, 1, finalNode->duplicateTree());
         indVarNode->setChild(i2lNode);
         }
      else
         {
         indVarNode->setChild(finalNode->duplicateTree());
         }

      if (!addInc)
         {
         TR::Node * elemSizeInc = TR::Node::create(finalNode, TR::iconst, 0, endInc);
         TR::Node * isub = TR::Node::create(TR::isub, 2, finalNode->duplicateTree(), elemSizeInc);
         // (64-bit)
         // use the same check for aladds as above
         if (indVarNode->getParent()->getType().isInt64())
            {
            TR::Node * i2lNode = TR::Node::create(TR::i2l, 1, isub);
            indVarNode->setChild(i2lNode);
            }
         else
            {
            indVarNode->setChild(isub);
            }
         }
      }
   }

TR::Node *
TR_LRAddressTree::updateMultiply(TR_ParentOfChildNode * multiplyNode)
   {
   TR::Node * newMul = NULL;
   if (!multiplyNode->isNull())
      {
      // need to convert simple iload into
      // imul
      //   <original iload>
      //   abs(increment)

      if (multiplyNode->getParent()->getType().isInt32())
         {
         TR::Node * initMulVal = TR::Node::create(multiplyNode->getParent(), TR::iconst, 0,
                                   (_increment > 0 ? _increment : -_increment));
         newMul = TR::Node::create(TR::imul, 2, multiplyNode->getChild(), initMulVal);
         }
      else
         {
         TR::Node * initMulVal = TR::Node::create(multiplyNode->getParent(), TR::lconst);
         initMulVal->setLongInt((_increment > 0 ? _increment : -_increment));

         newMul = TR::Node::create(TR::lmul, 2, multiplyNode->getChild(), initMulVal);
         }

      multiplyNode->setChild(newMul);
      }
   return newMul;
   }

// The code recognizes the following arraycopy patterns is similar to arrayset pattern:
// iTstore #ArrayStore Shadow[<array-shadow>] (where T is the type - one of b, c, s, i, l, d, f)
//   aiadd
//     aload #ArrayStoreBase (loop invariant)
//     isub
//       imul
//         iload #InductionVariable
//         iconst sizeof(T) (imul not present if size is 1)
//       iconst -16
//   iXload #ArrayLoad Shadow[<array-shadow>] (where T is the type - one of b, c, s, i, l, d, f)
//     aiadd
//       aload #ArrayLoadBase (loop invariant)
//       isub
//         imul
//           iload #InductionVariable
//           iconst sizeof(X) (imul not present if size is 1)
//         iconst -16
// istore #InductionVariable
//   iadd
//   ==>iload #InductionVariable
//     iconst -1
// ificmpge --> OutOfLoop
//   iload #InductionVariable
//   iconst 0 (or iload <val> if loop invariant)
//
bool
TR_LoopReducer::generateArraycopy(TR_InductionVariable * indVar, TR::Block * loopHeader)
   {
   // for aladds
   if (!comp()->cg()->getSupportsReferenceArrayCopy() && !comp()->cg()->getSupportsPrimitiveArrayCopy())
      {
      dumpOptDetails(comp(), "arraycopy not enabled for this platform\n");
      return false;
      }

   if (loopHeader->getNumberOfRealTreeTops() != 3) // array store, induction variable store, loop comparison
      {
      dumpOptDetails(comp(), "Loop has %d tree tops - no arrayset reduction\n", loopHeader->getNumberOfRealTreeTops());
      return false;
      }


   TR::TreeTop * arrayStoreTree = loopHeader->getFirstRealTreeTop();
   TR::Node * storeNode = arrayStoreTree->getNode();

   TR_Arraycopy arraycopyLoop(comp(), indVar);

   if (!arraycopyLoop.checkArrayStore(storeNode))
      {
      return false;
      }

   TR::TreeTop * indVarStoreTree = arrayStoreTree->getNextTreeTop();
   TR::Node * indVarStoreNode = indVarStoreTree->getNode();
   if (!arraycopyLoop.getStoreAddress()->checkIndVarStore(indVarStoreNode))
      {
      return false;
      }

   TR::TreeTop * loopCmpTree = indVarStoreTree->getNextTreeTop();
   TR::Node * loopCmpNode = loopCmpTree->getNode();
   if (!arraycopyLoop.checkLoopCmp(loopCmpNode, indVarStoreNode, arraycopyLoop.getStoreAddress()->getIndVar()))
      {
      return false;
      }

   bool needWriteBarrier = false;
   switch (TR::Compiler->om.writeBarrierType())
      {
      case gc_modron_wrtbar_oldcheck:
      case gc_modron_wrtbar_cardmark:
      case gc_modron_wrtbar_cardmark_and_oldcheck:
      case gc_modron_wrtbar_cardmark_incremental:
         needWriteBarrier = true;
         break;
      default:
         break;
      }
   //FUTURE: can eliminate wrtbar when src and dest are equal. Currently we don't reduce arraycopy like this.
   if (arraycopyLoop.hasWriteBarrier() && needWriteBarrier && !comp()->cg()->getSupportsReferenceArrayCopy())
      {
      dumpOptDetails(comp(), "arraycopy arraystore tree has write barrier as root and write barriers are enabled but no support for this platform- no arraycopy reduction\n");
      return false;
      }

   int32_t postIncrement = arraycopyLoop.checkForPostIncrement(loopHeader, indVarStoreNode, loopCmpNode, arraycopyLoop.getStoreAddress()->getIndVar()->getLocal());

   int32_t storeSize = storeNode->getSize();
#ifdef J9_PROJECT_SPECIFIC
   if (storeNode->getType().isBCD() &&
       !(storeSize == 1 || storeSize == 2 || storeSize == 4 || storeSize == 8))
      {
      dumpOptDetails(comp(), "arraycopy storeNode %p is a BCD type (%s) and the storeSize (%d) is not 1,2,4 or 8 so do not reduce arraycopy\n",
         storeNode,storeNode->getDataType().toString(),storeSize);
      return false;
      }
#endif

   if (!performTransformation(comp(), "%sReducing arraycopy %d\n", OPT_DETAILS, loopHeader->getNumber()))
      {
      return false;
      }

   //
   // At this point, this is an ArrayCopy loop.
   // Create a new tree with a primitive arraycopy call node followed by an istore node to store the induction variable
   // The parameters will be:
   //   The address to store into (the store aiadd tree - with the iload variable changed if negative increment)
   //   The address to load from  (the load aiadd tree - with the iload variable changed if negative increment)
   //   The number of bytes to copy(induction variable initial value - comparison value) * stride
   //
   //Tree-Top
   // ArrayCopy call
   //   aiadd
   //     aload #ArrayStoreBase (loop invariant)
   //     isub
   //       imul (this imul may need to be generated if not in original code and stride not 1)
   //         iload #InductionVariable (or #EndVariable if negative increment)
   //         iconst sizeof(T)
   //       iconst -16
   //   aiadd
   //     aload #ArrayLoadBase (loop invariant)
   //     isub
   //       imul (this imul may need to be generated if not in original code and stride not 1)
   //         iload #InductionVariable (or #EndVariable if negative increment)
   //         iconst sizeof(T)
   //       iconst -16
   //  imul
   //    isub
   //      iload #EndVariable
   //      iload #InductionVariable
   //    iconst <increment * multiplier>
   //istore
   //   iadd (could be isub)
   //   ==>iload #EndVariable
   //     iconst increment
   //
   arraycopyLoop.getStoreAddress()->updateAiaddSubTree(arraycopyLoop.getStoreIndVarNode(), &arraycopyLoop);
   arraycopyLoop.getLoadAddress()->updateAiaddSubTree(arraycopyLoop.getLoadIndVarNode(), &arraycopyLoop);
   TR::Node * imul = arraycopyLoop.updateIndVarStore(arraycopyLoop.getStoreIndVarNode(), indVarStoreNode, arraycopyLoop.getStoreAddress(), postIncrement);
   arraycopyLoop.getStoreAddress()->updateMultiply(arraycopyLoop.getStoreMultiplyNode());

   TR::Node * storeAddr = arraycopyLoop.getStoreNode()->getFirstChild();
   TR::Node * loadAddr  = arraycopyLoop.getStoreNode()->getSecondChild()->getFirstChild();
   TR::Node * arraycopy;
   if (!(arraycopyLoop.hasWriteBarrier() && needWriteBarrier))
      {
      //treat as primitive - create only 3 children
      TR::Node *src = loadAddr;
      TR::Node *dst = storeAddr;
      intptr_t offset;
      TR::ILOpCodes op_add   = comp()->target().is64Bit() ? TR::aladd : TR::aiadd;
      TR::ILOpCodes op_const = comp()->target().is64Bit() ? TR::lconst : TR::iconst;

      offset = arraycopyLoop.getStoreNode()->getSymbolReference()->getOffset();
      if (offset != 0)
         dst = TR::Node::create(op_add, 2, dst, TR::Node::create(dst, op_const, 0, static_cast<int32_t>(offset)));

      offset = arraycopyLoop.getStoreNode()->getSecondChild()->getSymbolReference()->getOffset();
      if (offset != 0)
         src = TR::Node::create(op_add, 2, src, TR::Node::create(src, op_const, 0, static_cast<int32_t>(offset)));

      arraycopy = TR::Node::createArraycopy(src, dst, imul->duplicateTree());
      TR::DataType arraycopyElementType = storeNode->getDataType();
#ifdef J9_PROJECT_SPECIFIC
      if (arraycopyElementType.isBCD())
         {
         switch (storeNode->getSize())
            {
            case 1: arraycopyElementType = TR::Int8;   break;
            case 2: arraycopyElementType = TR::Int16;  break;
            case 4: arraycopyElementType = TR::Int32;  break;
            case 8: arraycopyElementType = TR::Int64;  break;
            default:
               // a pre-transformation check just above guarantees only sizes 1,2,4 or 8 are transformed
               TR_ASSERT(false,"unsupported size (%d) for reduced arraycopy\n",storeNode->getSize());
            }
         }
#endif
      arraycopy->setArrayCopyElementType(arraycopyElementType);
      }
   else
      {
      //create the arraycopy and set the flag.
      arraycopy = TR::Node::createArraycopy(loadAddr->getFirstChild(), storeAddr->getFirstChild(),loadAddr, storeAddr, imul->duplicateTree());
      arraycopy->setNumChildren(5);
      ////arraycopy->setReferenceArrayCopy(true);
      arraycopy->setNoArrayStoreCheckArrayCopy(true);

      }
   storeAddr->decReferenceCount();
   loadAddr->decReferenceCount();

   TR::SymbolReference *arrayCopySymRef;
   arrayCopySymRef = comp()->getSymRefTab()->findOrCreateArrayCopySymbol();
   arraycopy->setSymbolReference(arrayCopySymRef);
   // if incr is negative, it should be a backward arraycopy
   if (arraycopyLoop.getStoreAddress()->getIncrement() < 0)
      //&& (storeAddr->getFirstChild()->getSymbol()->getRegisterMappedSymbol() == loadAddr->getFirstChild()->getSymbol()->getRegisterMappedSymbol()))
      {
      arraycopy->setBackwardArrayCopy(true);
      }
   else
      {
      arraycopy->setForwardArrayCopy(true);
      }

   switch (arraycopyLoop.getCopySize())
      {
      case 2:
         arraycopy->setHalfWordElementArrayCopy(true);
         break;

      case 4:
      case 8:
         arraycopy->setWordElementArrayCopy(true);
         break;
      }

   TR::Node * top = TR::Node::create(TR::treetop, 1, arraycopy);
   arrayStoreTree->setNode(top);

   TR::TreeTop * deadCmpChild1 = TR::TreeTop::create(comp(), indVarStoreTree, loopCmpNode);
   TR::TreeTop * deadCmpChild2 = TR::TreeTop::create(comp(), deadCmpChild1, loopCmpNode);

   deadCmpChild1->setNode(TR::Node::create(TR::treetop, 1, loopCmpTree->getNode()->getFirstChild()));
   deadCmpChild2->setNode(TR::Node::create(TR::treetop, 1, loopCmpTree->getNode()->getSecondChild()));

   deadCmpChild1->getNode()->getFirstChild()->decReferenceCount();
   deadCmpChild2->getNode()->getFirstChild()->decReferenceCount();

   deadCmpChild2->join(loopHeader->getExit());
   if (arraycopyLoop.hasWriteBarrier())
      {
      TR::TreeTop * deadCmpChild = TR::TreeTop::create(comp(), deadCmpChild2, loopCmpNode);
      deadCmpChild->setNode(TR::Node::create(TR::treetop, 1, arraycopyLoop.getStoreNode()->getChild(2)));
      deadCmpChild->getNode()->getFirstChild()->decReferenceCount();
      }

   return true;
   }

//
// The code recognizes the following arrayset patterns:
// iTstore #ArrayStore Shadow[<array-shadow>] (where T is the type - one of b, c, s, i, l, d, f
//   aiadd
//     aload #ArrayBase (loop invariant)
//     isub (isub not present if internal pointers supported)
//       imul
//         iload #InductionVariable
//         iconst sizeof(T) (imul not present if loop striding or size is 1)
//       iconst -16
//   Tconst  <val> (or Tload <val> if loop invariant)
// istore #InductionVariable
//   iadd
//   ==>iload #InductionVariable
//     iconst -1
// ificmpge --> OutOfLoop
//   iload #InductionVariable
//   iconst 0 (or iload <val> if loop invariant)
//
bool
TR_LoopReducer::generateArrayset(TR_InductionVariable * indVar, TR::Block * loopHeader)
   {
   // for aladds
   if (!comp()->cg()->getSupportsArraySet())
      {
      dumpOptDetails(comp(), "arrayset not enabled for this platform\n");
      return false;
      }

   if (loopHeader->getNumberOfRealTreeTops() != 3) // array store, induction variable store, loop comparison
      {
      dumpOptDetails(comp(), "Loop has %d tree tops - no arrayset reduction\n", loopHeader->getNumberOfRealTreeTops());
      return false;
      }

   TR::TreeTop * arrayStoreTree = loopHeader->getFirstRealTreeTop();
   TR::Node * storeNode = arrayStoreTree->getNode();

   TR_Arrayset arraysetLoop(comp(), indVar);

   if (!arraysetLoop.checkArrayStore(storeNode))
      {
      return false;
      }

   TR::TreeTop * indVarStoreTree = arrayStoreTree->getNextTreeTop();
   TR::Node * indVarStoreNode = indVarStoreTree->getNode();
   if (!arraysetLoop.getStoreAddress()->checkIndVarStore(indVarStoreNode))
      {
      return false;
      }

   TR::TreeTop * loopCmpTree = indVarStoreTree->getNextTreeTop();
   TR::Node * loopCmpNode = loopCmpTree->getNode();
   if (!arraysetLoop.checkLoopCmp(loopCmpNode, indVarStoreNode, indVar))
      {
      return false;
      }

   TR::Node *copyValueNode = storeNode->getSecondChild();

   if (copyValueNode->getType().isFloatingPoint())
      {
      // TODO: Evaluators need to support these types for an arrayset, if they're intended to be supported
      dumpOptDetails(comp(), "Loop has unsupported copyValueNode type %s so do not transform\n", TR::DataType::getName(copyValueNode->getDataType()));
      return false;
      }

   if (!performTransformation(comp(), "%sReducing arrayset %d from storeNode [" POINTER_PRINTF_FORMAT "] and copyValueNode [" POINTER_PRINTF_FORMAT "]\n",
         OPT_DETAILS, loopHeader->getNumber(),storeNode,storeNode->getSecondChild()))
      {
      return false;
      }


   //
   // At this point, this is an ArraySet loop.
   // Create a new tree with an ArraySet call node followed by an istore node to store the induction variable
   // The parameters will be:
   //   The address to store into (the aiadd tree - with the iload variable changed if negative increment)
   //   The value to store (the result of the 2nd child of the iTstore tree)
   //   The number of elements to set to the value (induction variable initial value - comparison value) / increment / multiplier
   // (alternate would be to have just a arrayset call and multiply the size)
   //
   //Tree-Top
   // ArraySet call
   //   aiadd
   //     aload #ArrayBase (loop invariant)
   //     isub (isub not present if internal pointers supported. may be an iadd)
   //       imul (this imul may need to be generated if not in original code and stride not 1)
   //         iload #InductionVariable (or #EndVariable if negative increment)
   //         iconst sizeof(T)
   //       iconst -16
   //  iconst 97
   //  imul
   //    isub
   //      iload #EndVariable
   //      iload #InductionVariable
   //    iconst <increment * multiplier>
   //istore
   //   iadd (could be isub)
   //   ==>iload #EndVariable
   //     iconst increment
   //
   arraysetLoop.getStoreAddress()->updateAiaddSubTree(arraysetLoop.getIndVarNode(), &arraysetLoop);
   TR::Node * imul = arraysetLoop.updateIndVarStore(arraysetLoop.getIndVarNode(), indVarStoreNode, arraysetLoop.getStoreAddress());
   arraysetLoop.getStoreAddress()->updateMultiply(arraysetLoop.getMultiplyNode());

   TR::Node *dst = storeNode->getFirstChild();
   intptr_t offset;
   TR::ILOpCodes op_add   = comp()->target().is64Bit() ? TR::aladd : TR::aiadd;
   TR::ILOpCodes op_const = comp()->target().is64Bit() ? TR::lconst : TR::iconst;
   offset = storeNode->getSymbolReference()->getOffset();
   if (offset != 0)
      dst = TR::Node::create(op_add, 2, dst, TR::Node::create(dst, op_const, 0, static_cast<int32_t>(offset)));

   TR::Node * arrayset = TR::Node::create(TR::arrayset, 3,
                                        dst,
                                        copyValueNode,
                                        imul->duplicateTree());

   storeNode->getFirstChild()->decReferenceCount();
   storeNode->getSecondChild()->decReferenceCount();

   arrayset->setSymbolReference(comp()->getSymRefTab()->findOrCreateArraySetSymbol());



   TR::Node * top = TR::Node::create(TR::treetop, 1, arrayset);
   arrayStoreTree->setNode(top);

   TR::TreeTop * deadCmpChild1 = TR::TreeTop::create(comp(), indVarStoreTree, loopCmpNode);
   TR::TreeTop * deadCmpChild2 = TR::TreeTop::create(comp(), deadCmpChild1, loopCmpNode);

   deadCmpChild1->setNode(TR::Node::create(TR::treetop, 1, loopCmpTree->getNode()->getFirstChild()));
   deadCmpChild2->setNode(TR::Node::create(TR::treetop, 1, loopCmpTree->getNode()->getSecondChild()));

   deadCmpChild1->getNode()->getFirstChild()->decReferenceCount();
   deadCmpChild2->getNode()->getFirstChild()->decReferenceCount();

   deadCmpChild2->join(loopHeader->getExit());
   return true;
   }

bool
TR_Arraycmp::checkElementCompare(TR::Node * elemCmpNode)
   {
   TR::ILOpCodes elemOp = elemCmpNode->getOpCodeValue();
   if (elemOp != TR::ificmpne &&
      elemOp != TR::iffcmpne &&
      elemOp != TR::iffcmpneu &&
      elemOp != TR::ifdcmpne &&
      elemOp != TR::ifdcmpneu &&
      elemOp != TR::iflcmpne)
      {
      dumpOptDetails(comp(), "element compare tree does not have an ifxcmpne as root\n");
      return false;
      }

   TR::Node * cmpFirstChild = elemCmpNode->getFirstChild();
   TR::ILOpCodes opCodeCmpFirstChild = cmpFirstChild->getOpCodeValue();
   TR::Node * cmpSecondChild = elemCmpNode->getSecondChild();
   TR::ILOpCodes opCodeCmpSecondChild = cmpSecondChild->getOpCodeValue();

   cmpFirstChild = cmpFirstChild->skipConversions();
   cmpSecondChild = cmpSecondChild->skipConversions();

   if (cmpFirstChild->getOpCode().isLoadIndirect())
      {
      setFirstLoad(cmpFirstChild);
      }
   else
      {
      dumpOptDetails(comp(), "no array element load encountered on first cmp child\n");
      return false;
      }

   if (cmpSecondChild->getOpCode().isLoadIndirect())
      {
      setSecondLoad(cmpSecondChild);
      }
   else
      {
      dumpOptDetails(comp(), "no array element load encountered on second cmp child\n");
      return false;
      }

   if (!getFirstAddress()->checkAiadd(cmpFirstChild->getFirstChild(), cmpFirstChild->getSize()))
      {
      dumpOptDetails(comp(), "firstAddress check failed on checkElementCompare\n");
      return false;
      }
   if (!getFirstAddress()->checkAiadd(cmpSecondChild->getFirstChild(), cmpSecondChild->getSize()))
      {
      dumpOptDetails(comp(), "secondAddress check failed on checkElementCompare\n");
      return false;
      }

   TR::TreeTop * targetTree = elemCmpNode->getBranchDestination();
   _targetOfGotoBlock = targetTree->getEnclosingBlock();

   return true;
   }

bool
TR_Arraycmp::checkGoto(TR::Block * gotoBlock, TR::Node * gotoNode, TR::Node * finalNode)
   {
   TR::Node * comparisonNode = NULL;
   TR::Node * extraStoreNode = NULL;

   TR::ILOpCodes gotoOpCode = gotoNode->getOpCode().getOpCodeValue();
   if (gotoOpCode != TR::Goto && gotoOpCode != TR::istore)
      {
      dumpOptDetails(comp(), "goto tree does not have a goto or istore\n");
      return false;
      }

   if (gotoOpCode == TR::istore)
      {
      if (gotoBlock->getNumberOfRealTreeTops() != 2)
         {
         dumpOptDetails(comp(), "goto tree has istore but too many subsequent nodes\n");
         return false;
         }

      if (gotoNode->getFirstChild()->getOpCode().getOpCodeValue() != TR::iconst)
         {
         dumpOptDetails(comp(), "goto tree has istore without iconst child\n");
         return false;
         }

      comparisonNode = gotoNode;
      extraStoreNode = gotoNode;

      gotoNode = gotoBlock->getFirstRealTreeTop()->getNextTreeTop()->getNode();
      TR::ILOpCodes gotoOpCode = gotoNode->getOpCode().getOpCodeValue();
      if (gotoOpCode != TR::Goto)
         {
         dumpOptDetails(comp(), "goto tree has istore but not subsequent goto\n");
         return false;
         }
      }
   else
      {
      comparisonNode = finalNode;
      extraStoreNode = NULL;
      }

   TR::TreeTop * targetTree = gotoNode->getBranchDestination()->getNextTreeTop();
   TR::Node * targetNode = targetTree->getNode();
   if (targetNode->getOpCode().getOpCodeValue() != TR::ificmpne)
      {
      if (targetNode->getOpCode().getOpCodeValue() == TR::istore)
         {
         targetNode = targetNode->getFirstChild()->skipConversions();
         if (targetNode->getOpCode().getOpCodeValue() != TR::icmpeq)
            {
            dumpOptDetails(comp(), "target of goto is not an ificmpne/istore. It is %s\n", targetNode->getOpCode().getName());
            return false;
            }
         }
      }
   if (targetNode->getNumChildren() < 1)
      {
      dumpOptDetails(comp(), "end of block\n");
      return false;
      }
   TR::ILOpCodes opCodeFirstChild = targetNode->getFirstChild()->getOpCode().getOpCodeValue();
   if (opCodeFirstChild != TR::iload)
      {
      dumpOptDetails(comp(), "first child: goto comparison does have iload\n");
      return false;
      }
   TR::ILOpCodes opCodeSecondChild = targetNode->getSecondChild()->getOpCode().getOpCodeValue();
   if (opCodeSecondChild != TR::iload && opCodeSecondChild != TR::iconst)
      {
      dumpOptDetails(comp(), "second child: goto comparison does have iload/iconst\n");
      return false;
      }

   TR::RegisterMappedSymbol * indSym = getFirstAddress()->getIndVar()->getLocal();
   TR::RegisterMappedSymbol * comparisonSym = (comparisonNode->getOpCode().hasSymbolReference()) ?
      comparisonNode->getSymbol()->getRegisterMappedSymbol() :
      NULL;
   TR::RegisterMappedSymbol * firstChildSym = targetNode->getFirstChild()->getSymbol()->getRegisterMappedSymbol();
   TR::RegisterMappedSymbol * secondChildSym = (opCodeSecondChild == TR::iload) ?
      targetNode->getSecondChild()->getSymbol()->getRegisterMappedSymbol() :
      NULL;

   if (extraStoreNode != NULL)
      {
      if (!(firstChildSym == comparisonSym && secondChildSym == NULL))
         {
         dumpOptDetails(comp(), "first/second child: alternate goto comparison not to comparison sym\n");
         return false;
         }
      }
   else if (!(firstChildSym == indSym && secondChildSym == comparisonSym) &&
      !(firstChildSym == comparisonSym && secondChildSym == indSym) &&
      !(firstChildSym == indSym && secondChildSym == NULL && comparisonNode->getInt() == targetNode->getSecondChild()->getInt()))
      {
      dumpOptDetails(comp(), "first/second child: goto comparison are not final/induction variable syms\n");
      return false;
      }

   _targetOfGotoBlock = targetTree->getEnclosingBlock();
   return true;
   }

//
// skip over a multiply by 2
//
TR::Node *
TR_Arraytranslate::getMulChild(TR::Node * child)
   {
   // match the tree
   // lmul
   //   i2l
   //     <...>
   //   lconst 2
   intptr_t constValue;
   if ((child->getOpCodeValue() == TR::imul || child->getOpCodeValue() == TR::lmul) &&
      (child->getSecondChild()->getOpCodeValue() == TR::iconst || child->getSecondChild()->getOpCodeValue() == TR::lconst))
      {
      if (child->getSecondChild()->getType().isInt32())
         {
         constValue = child->getSecondChild()->getInt();
         }
      else
         {
         constValue = child->getSecondChild()->getLongInt();
         }
      if (constValue == 2)
         {
         if (child->getFirstChild()->getOpCodeValue() == TR::i2l || child->getFirstChild()->getOpCodeValue() == TR::bu2l)
            {
            return child->getFirstChild()->getFirstChild();
            }
         else
            {
            return child->getFirstChild();
            }
         }
      else
         {
         return child;
         }
      }
   else
      {
      return child;
      }
   }


static void swapIfNecessary(TR::Node *&aiaddFirstChild, TR::Node *&aiaddSecondChild)
   {
   TR::Node *tempNode = NULL;
   if (aiaddFirstChild->getOpCodeValue() == TR::l2i &&
         (aiaddFirstChild->getFirstChild()->getOpCodeValue() == TR::lloadi ||
          aiaddFirstChild->getFirstChild()->getOpCodeValue() == TR::lload))
      {
      tempNode = aiaddFirstChild;
      aiaddFirstChild = aiaddSecondChild;
      aiaddSecondChild = tempNode;
      }
   }



//
//Load tree should look as follows:
//
//istore #transchar (store translated character)
//  s2i
//    isload <translate-char> (load character)
//      aiadd
//        aload #table (translation table)
//        isub (add displacement to get to start of array with index into translation table)
//          imul (multiply by stride of target - may not be present if byte-to-byte)
//            bu2i (widen byte (unsigned) to int - could be
//              ibload #byte (load the byte - could also be isload)
//                aiadd
//                  aload #base (load base pointer)
//                  isub (add displacement to get to relative index)
//                    iload #i (induction variable)
//                    iconst -16
//            iconst 2
//          iconst -16
//
// -or- (in the case of an unsafe reference)
//istore #transchar (store translated character)
//  b2i
//    ibload <translate-char> (load character)
//      iadd
//       c2i (widen byte (unsigned) to int - could be
//         isload #byte (load the byte - could also be isload)
//            aiadd
//              aload #base (load base pointer)
//                isub (add displacement to get to relative index)
//                  imul
//                    iload #i (induction variable)
//                    iconst 2
//                  iconst -16
//        l2i
//          ilload <DirectByteBuffer>
//            aload <ButeBuffer>
//
// -or (in the case of a compiler-generated table look-up)
//istore #transchar (store byte)
//  b2i
//    ibload <translate-char> (load character)
//      aiadd
//        aload #base (load base pointer)
//        isub (add displacement to get to relative index)
//          iload #i (induction variable)
//          iconst -16
//

bool
TR_Arraytranslate::checkLoad(TR::Node * loadNode)
   {
   TR::Node * transLoadNode;
   if (hasBranch())
      {
      if (loadNode->getOpCodeValue() != TR::istore)
         {
         dumpOptDetails(comp(), "...load tree does not have store - no arraytranslate reduction\n");
         return false;
         }
      _resultNode = loadNode;

      transLoadNode = loadNode->getFirstChild();
      }
   else
      {
      _resultNode = loadNode;
      transLoadNode = loadNode;
      }

   transLoadNode = transLoadNode->skipConversions();

   // these cast-down conversions can be ignored
   //
   if (transLoadNode->getOpCodeValue() == TR::i2b ||
       transLoadNode->getOpCodeValue() == TR::i2s ||
       transLoadNode->getOpCodeValue() == TR::s2b)
      {
      transLoadNode = transLoadNode->getFirstChild();
      transLoadNode = transLoadNode->skipConversions();
      }

   if (transLoadNode->getOpCodeValue() != TR::bloadi)
      {
      dumpOptDetails(comp(), "...load tree does not have ibload - no arraytranslate reduction\n");
      return false;
      }
   _resultUnconvertedNode = transLoadNode;

   TR::Node * aiaddNode = transLoadNode->getFirstChild();

   if (aiaddNode->getOpCodeValue() != TR::aiadd &&
      aiaddNode->getOpCodeValue() != TR::aladd &&
      aiaddNode->getOpCodeValue() != TR::iadd &&
      aiaddNode->getOpCodeValue() != TR::ladd)
      {
      dumpOptDetails(comp(), "...load tree does not have aiadd/aladd/iadd/ladd - no arraytranslate reduction\n");
      return false;
      }

   TR::Node * aiaddFirstChild = aiaddNode->getFirstChild();
   TR::Node * aiaddSecondChild = aiaddNode->getSecondChild();
   TR::Node * inLoadNode;

   if (aiaddNode->getOpCodeValue() == TR::aiadd || aiaddNode->getOpCodeValue() == TR::aladd)
      {
      if (aiaddFirstChild->getOpCodeValue() != TR::aload && aiaddFirstChild->getOpCodeValue() != TR::aloadi)
         {
         dumpOptDetails(comp(), "...aiadd load tree does not have aload - no arraytranslate reduction\n");
         return false;
         }
      _tableNode = aiaddFirstChild;

      if (aiaddSecondChild->getOpCodeValue() != TR::isub && aiaddSecondChild->getOpCodeValue() != TR::lsub)
         {
         dumpOptDetails(comp(), "...load tree does not have isub - no arraytranslate reduction\n");
         return false;
         }
      inLoadNode = aiaddSecondChild->getFirstChild();
      }
   else
      {
      // TR::iadd - unsafe sub-tree
      swapIfNecessary(aiaddFirstChild, aiaddSecondChild);

      if (aiaddSecondChild->getOpCodeValue() == TR::l2i) // skip over l2i if present
         {
         aiaddSecondChild = aiaddSecondChild->getFirstChild();
         }
      if (aiaddSecondChild->getOpCodeValue() != TR::lloadi && aiaddSecondChild->getOpCodeValue() != TR::lload)
         {
         dumpOptDetails(comp(), "...iadd load tree does not have ilload - no arraytranslate reduction\n");
         return false;
         }
      _usesRawStorage = true;
      _tableNode = aiaddSecondChild;
      inLoadNode = aiaddFirstChild;
      }

   inLoadNode = getMulChild(inLoadNode);
   inLoadNode = inLoadNode->skipConversions();

   if (inLoadNode->getOpCodeValue() != TR::bloadi)
      {
      dumpOptDetails(comp(), "...load tree does not have 2nd bloadi - check if compiler-generated table lookup match\n");
      inLoadNode = transLoadNode;
      _tableNode = NULL;
      _compilerGeneratedTable = true;
      }

   if (inLoadNode->getOpCodeValue() == TR::bloadi)
      {
      _byteInput = true;
      }
   else
      {
      _byteInput = false;
      }

   _inputNode = inLoadNode->getFirstChild();
   return (getLoadAddress()->checkAiadd(_inputNode, inLoadNode->getSize()));
   }

//
//Store tree should look as follows:
//
//isstore #reschar (result char stored into output array)
//   aiadd
//     aload #outbase (load output base ptr)
//     isub (add displacement to get to relative index
//       imul (multiply by stride of target)
//         iload #i <induction variable>
//         iconst 2
//       iconst -16
//   i2c
//     iload #transchar (translated character from first block)

bool
TR_Arraytranslate::checkStore(TR::Node * storeNode)
   {
   if (storeNode->getOpCodeValue() != TR::sstorei && storeNode->getOpCodeValue() != TR::bstorei)
      {
      dumpOptDetails(comp(), "...store tree does not have isstore/ibstore - no arraytranslate reduction\n");
      return false;
      }

   TR::Node * aiaddNode = storeNode->getFirstChild();

   if (aiaddNode->getOpCodeValue() != TR::aiadd && aiaddNode->getOpCodeValue() != TR::aladd)
      {
      dumpOptDetails(comp(), "...store tree does not have aiadd/aladd - no arraytranslate reduction\n");
      return false;
      }

   _outputNode = aiaddNode;

   if (hasBranch())
      {
      TR::Node * shrinkNode = storeNode->getSecondChild();

      if (shrinkNode->getOpCodeValue() != TR::i2s &&
          shrinkNode->getOpCodeValue() != TR::i2b &&
          shrinkNode->getOpCodeValue() != TR::sconst &&
          shrinkNode->getOpCodeValue() != TR::bconst)
         {
         dumpOptDetails(comp(), "...store tree does not have i2c/i2b/sconst/bconst - no arraytranslate reduction\n");
         return false;
         }

      if (shrinkNode->getOpCodeValue() == TR::i2b || shrinkNode->getOpCodeValue() == TR::bconst)
         {
         _byteOutput = true;
         }
      else
         {
         _byteOutput = false;
         }

      if (shrinkNode->getOpCodeValue() == TR::i2b || shrinkNode->getOpCodeValue() == TR::i2s)
         {
         TR::Node * iloadNode = shrinkNode->getFirstChild();
         if (iloadNode->getOpCodeValue() != TR::iload)
            {
            dumpOptDetails(comp(), "...store tree does not have iload - no arraytranslate reduction\n");
            return false;
            }

         if (iloadNode->getSymbolReference() != _resultNode->getSymbolReference())
            {
            dumpOptDetails(comp(), "...store tree reference does not match load tree reference - no arraytranslate reduction\n");
            return false;
            }
         }
      }
   else
      {
      if (storeNode->getOpCodeValue() == TR::sstorei)
         {
         _byteOutput = false;
         }
      else
         {
         _byteOutput = true;
         }
      }

   return (getStoreAddress()->checkAiadd(_outputNode, storeNode->getSize()));
   }

//Break tree should look as follows:
//
//ifscmpeq --> block <no-stop>
//  ==>isload at <translate-char>
//  sconst <termination char>
//-or-
//ifXcmpYY --> block <no-stop>
//  ==>isload at <translate-char>
//  iconst <termination char>
//
bool
TR_Arraytranslate::checkBreak(TR::Block * breakBlock, TR::Node * breakNode, TR::Block * nextBlock)
   {
   if (breakNode->getOpCodeValue() != TR::ificmpeq)
      {
      if (compilerGeneratedTable())
         {
         // if the compiler is generating the table, we can handle any compare, but only to a constant
         if (!breakNode->getOpCode().isBooleanCompare())
            {
            dumpOptDetails(comp(), "...break tree does not have expected compare operator\n");
            return false;
            }
         }
      else
         {
         dumpOptDetails(comp(), "...break tree does not have ificmpeq\n");
         return false;
         }
      }

   TR::Node * loadNode = breakNode->getFirstChild()->skipConversions();
   if (loadNode->getSymbolReference() != _resultUnconvertedNode->getSymbolReference() &&
      loadNode->getSymbolReference() != _resultNode->getFirstChild()->getSymbolReference() &&
      loadNode->getSymbolReference() != _resultNode->getSymbolReference())
      {
      dumpOptDetails(comp(), "...break tree reference does not match load tree reference - no arraytranslate reduction\n");
      return false;
      }

   TR::Node * valNode = breakNode->getSecondChild();
   if (valNode->getOpCodeValue() != TR::iconst || (compilerGeneratedTable() && (valNode->getInt() > 32766 || valNode->getInt() < -32766)))
      {
      //msf - this can be loosened up to just ensure the expression is a byte or character value
      //      that is loop invariant iff it is not a compilerGeneratedTable() loop
      //      we also conservatively ensure the range is inside the represented ranges such that we can
      //      add or subtract 1 from the value and still represent it in 2**16 bits for a default value
      //      if we need to generate a table.
      dumpOptDetails(comp(), "...break tree does not have iconst, or not in range - no arraytranslate reduction\n");
      return false;
      }

   _termCharNode = valNode;
   _compareOp = breakNode->getOpCodeValue();

   return true;
   }

// the materialized induction variable must be
// expressed in terms of the primary induction variable
// istore <matIndVar>
//      iadd/isub
//              increment of the indVar
//              iconst/invariant iload
//
bool
TR_Arraytranslate::checkMatIndVarStore(TR::Node * matIndVarStoreNode, TR::Node * indVarStoreNode)
   {
   if (!matIndVarStoreNode->getOpCode().isStoreDirect())
      {
      dumpOptDetails(comp(), "materialized induction variable tree %p does not have a direct store as root\n", matIndVarStoreNode);
      return false;
      }

   TR::Node *storeFirstChild = matIndVarStoreNode->getFirstChild();

   if (storeFirstChild->getOpCodeValue() != TR::iadd &&
         storeFirstChild->getOpCodeValue() != TR::isub)
      {
      dumpOptDetails(comp(), "first child %p of materialized induction variable store is not TR::iadd/TR::isub\n", storeFirstChild);
      return false;
      }

   TR::Node *iaddFirstChild = storeFirstChild->getFirstChild();
   TR::Node *iaddSecondChild = storeFirstChild->getSecondChild();

   // this may perhaps be too conservative, but maybe thats good
   // alternatively, we can work a bit harder and look for the load
   // of the indVar
   //
   //if (iaddFirstChild != indVarStoreNode)
   //   return;

   if (iaddFirstChild->getOpCodeValue() != TR::iadd &&
         iaddFirstChild->getOpCodeValue() != TR::isub)
      {
      dumpOptDetails(comp(), "materialized variable is not expressed in terms of primary iv %p\n", iaddFirstChild);
      return false;
      }
   TR::Node *indVarLoad = iaddFirstChild->getFirstChild();
   if (indVarLoad->getOpCodeValue() != TR::iload ||
         iaddFirstChild->getSecondChild()->getOpCodeValue() != TR::iconst)
      {
      dumpOptDetails(comp(), "primary iv in the materialized tree is not TR::iload %p or second child is not TR::iconst %p\n", indVarLoad, iaddFirstChild->getSecondChild());
      return false;
      }

   // primary induction variable
   //
   TR_InductionVariable *indVar = getLoadAddress()->getIndVar();
   if (indVarLoad->getSymbol()->getRegisterMappedSymbol() != indVar->getLocal())
      {
      dumpOptDetails(comp(), "materialized iv is not actually materialized at all\n");
      return false;
      }

   if (iaddSecondChild->getOpCodeValue() != TR::iload &&
         iaddSecondChild->getOpCodeValue() != TR::iconst)
      {
      dumpOptDetails(comp(), "second child of the materialized tree is not delta or a constant %p\n", iaddSecondChild);
      return false;
      }

   getStoreAddress()->setMaterializedIndVarSymRef(matIndVarStoreNode->getSymbolReference());

   return true;
   }

bool
TR_Arraytranslate::checkGoto(TR::Block * gotoBlock, TR::Node * gotoNode, TR::Block * nextBlock)
   {
   TR::ILOpCodes gotoOpCode = gotoNode->getOpCode().getOpCodeValue();

   // make sure it really is just a goto block
   if (gotoOpCode != TR::Goto)
      {
      dumpOptDetails(comp(), "...goto tree does not have a goto\n");
      return false;
      }

   // make sure the target block is the block after our 3-block sequence
   TR::Block * targetBlock = gotoNode->getBranchDestination()->getEnclosingBlock();
   if (targetBlock != nextBlock)
      {
      dumpOptDetails(comp(), "...goto tree does not goto the first block after the loop %p %p\n", targetBlock, nextBlock);
      return false;
      }
   return true;
   }


// msf - should move to opcodes file probably
TR::ILOpCodes
TR_LoopReducer::convertIf(TR::ILOpCodes ifCmp)
   {
   switch (ifCmp)
      {
      case TR::ifbcmpeq:
         return TR::bcmpeq;
      case TR::ifscmpeq:
         return TR::scmpeq;
      case TR::ificmpeq:
         return TR::icmpeq;
      case TR::iflcmpeq:
         return TR::lcmpeq;
      case TR::iffcmpeq:
         return TR::fcmpeq;
      case TR::ifdcmpeq:
         return TR::dcmpeq;
      case TR::ifacmpeq:
         return TR::acmpeq;
      default:
         TR_ASSERT(0, "Unexpected TR_ifxcmp operator"); return TR::BadILOp;
      }
   }


bool
TR_LoopReducer::blockInVersionedLoop(List<TR::CFGEdge> otherLoopVersionBlockList, TR::Block * block)
   {
   ListIterator<TR::CFGEdge> succIt(&otherLoopVersionBlockList);
   TR::CFGEdge * edge;
   TR::Block * b;

   for (edge = succIt.getCurrent(); edge != NULL; edge = succIt.getNext())
      {
      b = toBlock(edge->getTo());
      if (b->getNumber() == block->getNumber())
         {
         return true;
         }
      }

   return false;
   }

bool
TR_LoopReducer::generateArraycmp(TR_RegionStructure * whileLoop, TR_InductionVariable * indVar,
   TR::Block * branchBlock, TR::Block * incrementBlock)
   {
   //
   //The general loop that we need to recognize for an array compare is as follows:
   //
   //BBStart (block #1)
   //ificmpne --> block #3 <break block>
   //  b2i
   //    ibload <array #1 element>
   //      aiadd
   //        aload <array #1 base>
   //        isub (this could be variable based on internal pointers, striding, type being loaded) [common base offset]
   //          iload <induction variable>
   //          iconst -16
   //  b2i
   //    ibload <array #2 element>
   //      aiadd
   //        aload <array #2 base>
   //        ==>isub at [common base offset]
   //BBEnd (block #1)
   //BBStart (block #2)
   //istore <induction variable>
   //  isub
   //    iload <induction variable>
   //    iconst -1
   //ificmplt --> block #1
   //  iload <induction variable>
   //  iload <final value>
   //BBEnd (block #2)

   if (!comp()->cg()->getSupportsArrayCmp())
      {
      dumpOptDetails(comp(), "arraycmp not enabled for this platform\n");
      return false;
      }

   int32_t compNum = branchBlock ? branchBlock->getNumberOfRealTreeTops() : 0;
   int32_t incrNum = incrementBlock ? incrementBlock->getNumberOfRealTreeTops() : 0;
   if (compNum != 1 || incrNum != 2)
      {
      dumpOptDetails(comp(), "Loop has wrong number of tree tops (%d,%d) - no arraycmp reduction\n", compNum, incrNum);
      return false;
      }

   TR::TreeTop * branchTree = branchBlock->getFirstRealTreeTop();
   TR::Node * branchNode = branchTree->getNode();

   TR_Arraycmp arraycmpLoop(comp(), indVar);
   if (!arraycmpLoop.checkElementCompare(branchNode))
      {
      return false;
      }

   TR::TreeTop * indVarStoreTree = incrementBlock->getFirstRealTreeTop();
   TR::Node * indVarStoreNode = indVarStoreTree->getNode();
   if (!arraycmpLoop.getFirstAddress()->checkIndVarStore(indVarStoreNode))
      {
      return false;
      }

   TR::TreeTop * loopCmpTree = indVarStoreTree->getNextTreeTop();
   TR::Node * loopCmpNode = loopCmpTree->getNode();
   if (!arraycmpLoop.checkLoopCmp(loopCmpNode, indVarStoreNode, indVar))
      {
      return false;
      }

   //
   // The following check is to ensure someone doesn't read the induction variable
   // other than for comparison to the 'final' value (which is in the known blocks I've examined)
   // in any successor block to the loop. If the induction variable is written to first, then
   // I can ignore that path.
   // The reason this is important is because the reduced arraycmp will not calculate the
   // induction variable's final value correctly if it terminates early, only if it runs to
   // completion, so I have to ensure that I don't reduce a loop like:
   // for (int i=0; i<len; ++i)
   //    {
   //    if (arrA[i] != arrB[i]) break;
   //    }
   // if (i == len/2) { <do something> }
   //
   // Since I will not know the value of i other than i==len.
   //
   TR_Queue<TR::Block> queue(trMemory());
   whileLoop->collectExitBlocks(&queue);
   comp()->incVisitCount();
   TR::RegisterMappedSymbol * indVarSym = indVar->getLocal();

   vcount_t visitCount = comp()->getVisitCount();

   while (!queue.isEmpty())
      {
      TR::Block * block = queue.dequeue();
      TR::CFGEdge * edge;

      if (block->getVisitCount() == visitCount)
         {
         continue;
         }

      block->setVisitCount(visitCount);

      if (block != _cfg->getEnd() && block != branchBlock && block != incrementBlock)
         {
         TR_TransformerDefUseState blockState = getSymbolDefUseStateInBlock(block, indVarSym);
         if (blockState == transformerReadFirst)
            {
            dumpOptDetails(comp(), "induction variable is read before write in block_%d after compare loop - no arraycmp reduction\n",
               block->getNumber());
            return false;
            }

         if (blockState == transformerWrittenFirst)
            {
            dumpOptDetails(comp(), "induction variable is written before read in block_%d after compare loop - pruning successor\n",
               block->getNumber());
            continue;
            }
         }

      TR_SuccessorIterator sit(block);
      for (edge = sit.getFirst(); edge; edge = sit.getNext())
         {
         TR::Block * succ = edge->getTo()->asBlock();
         if (block)
            {
            queue.enqueue(succ);
            }
         }
      }


   if  (
        (arraycmpLoop.getFirstAddress ()->getIncrement() * arraycmpLoop.getFirstAddress ()->getMultiplier() < 0) ||
        (arraycmpLoop.getSecondAddress()->getIncrement() * arraycmpLoop.getSecondAddress()->getMultiplier() < 0)
       )
      {
      dumpOptDetails(comp(), "Can not reduce an arraycmp loop that runs backwards\n");
      return false;
      }

   if (!performTransformation(comp(), "%sReducing arraycmp %d\n", OPT_DETAILS, branchBlock->getNumber()))
      {
      return false;
      }

   //
   // At this point, this is an ArrayCmp loop.
   // Create a new tree with:
   //   an ArrayCmp call node, which sets a boolean result (true/false)
   //   a conditional branch based on the arraycmp result
   //   an istore node to store the induction variable
   //     (the istore can not be moved above the arraycmp because the ind var may be used in the arraycmp)
   //     (the istore must be after the conditional branch since it is used in exactly one subsequent compare)
   //
   // The parameters to ArrayCmp will be:
   //   The first address to compare to (the aiadd tree - with the iload variable changed if negative increment)
   //   The second address to compare to (the aiadd tree - this may reference the first iload in which case it can't be updated)
   //   The number of elements to compare (induction variable initial value - comparison value) / increment / multiplier
   //
   //Tree-Top
   // ArrayCmp call
   //   aiadd (for first address)
   //     aload #ArrayBase (loop invariant)
   //     isub (isub not present if internal pointers supported. may be an iadd)
   //       imul (this imul may need to be generated if not in original code and stride not 1)
   //         iload #InductionVariable (or #EndVariable if negative increment)
   //         iconst sizeof(T)
   //       iconst -16
   //  iconst 97
   //  imul
   //    isub
   //      iload #EndVariable
   //      iload #InductionVariable
   //    iconst <increment * multiplier>
   //istore
   //   iadd (could be isub)
   //   ==>iload #EndVariable
   //     iconst increment
   //
   arraycmpLoop.getFirstAddress()->updateAiaddSubTree(arraycmpLoop.getFirstIndVarNode(), &arraycmpLoop);
   arraycmpLoop.getSecondAddress()->updateAiaddSubTree(arraycmpLoop.getSecondIndVarNode(), &arraycmpLoop);
   TR::Node * mul = arraycmpLoop.updateIndVarStore(arraycmpLoop.getFirstIndVarNode(), indVarStoreNode, arraycmpLoop.getFirstAddress());
   if (!comp()->target().is64Bit())
      {
      // updateIndVarStore returns an imul on 32 bit, extend as arraycmp takes 64 bit length
      mul = TR::Node::create(TR::i2l, 1, mul);
      // extending the imul is technically unneccessary since the length of an array on 32 bit cannot exceed 32 bit range
      mul->setUnneededConversion(true);
      }

   arraycmpLoop.getFirstAddress()->updateMultiply(arraycmpLoop.getFirstMultiplyNode());
   arraycmpLoop.getFirstAddress()->updateMultiply(arraycmpLoop.getSecondMultiplyNode());

   TR::Node * firstBase = branchNode->getFirstChild()->skipConversions()->getFirstChild();
   TR::Node * secondBase = branchNode->getSecondChild()->skipConversions()->getFirstChild();


   TR_ASSERT(arraycmpLoop.getFirstLoad(),"firstLoad is null for arraycmp reduction\n");
   TR_ASSERT(arraycmpLoop.getSecondLoad(),"secondLoad is null for arraycmp reduction\n");
   TR_ASSERT(arraycmpLoop.getFirstLoad()->getOpCode().isLoadVar(),"firstLoad %s (%p) is not a loadVar for arraycmp reduction\n",
      arraycmpLoop.getFirstLoad()->getOpCode().getName(),arraycmpLoop.getFirstLoad());
   TR_ASSERT(arraycmpLoop.getSecondLoad()->getOpCode().isLoadVar(),"secondLoad %s (%p) is not a loadVar for arraycmp reduction\n",
      arraycmpLoop.getSecondLoad()->getOpCode().getName(),arraycmpLoop.getSecondLoad());

   TR::Node * arraycmp = TR::Node::create(TR::arraycmp, 3, firstBase, secondBase, mul);

   TR::SymbolReference *arraycmpSymRef = comp()->getSymRefTab()->findOrCreateArrayCmpSymbol();
   arraycmp->setSymbolReference(arraycmpSymRef);

   firstBase->decReferenceCount();
   secondBase->decReferenceCount();

   TR::SymbolReference * compareTemp = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Int32);
   TR::Node * storeCompare = TR::Node::createStore(compareTemp, arraycmp);

   TR::Node * loadCompare = TR::Node::createLoad(arraycmp, compareTemp);
   TR::Node * constBoolNode = TR::Node::create(arraycmp, TR::iconst, 0, (int32_t) 0);

   // create induction variable store block to replace the old one
   // since I no longer need the comparison / branch back loop back edge

   indVarStoreTree->setNode(indVarStoreNode->duplicateTree());

   TR::TreeTop * indVarEntry = incrementBlock->getEntry();
   TR::TreeTop * indVarExit = incrementBlock->getExit();

   TR::TreeTop * cmpTargetTree = arraycmpLoop.targetOfGotoBlock()->getEntry();
   int32_t cmpTargetBlockNumber = cmpTargetTree->getEnclosingBlock()->getNumber();
   TR::Node * compareNode = TR::Node::createif(TR::ificmpne, loadCompare, constBoolNode, cmpTargetTree);

   // create arraycmp block
   TR::TreeTop * newEntry = branchBlock->getEntry();
   TR::TreeTop * newExit = branchBlock->getExit();

   TR::TreeTop * arraycmpTreeTop = TR::TreeTop::create(comp(), storeCompare);
   newEntry->join(arraycmpTreeTop);
   TR::TreeTop * compareTreeTop = TR::TreeTop::create(comp(), compareNode);
   arraycmpTreeTop->join(compareTreeTop);
   compareTreeTop->join(newExit);

   newExit->join(indVarEntry);

   indVarEntry->join(indVarStoreTree);
   indVarStoreTree->join(indVarExit);

   // update the CFG
   _cfg->setStructure(NULL);
   _cfg->removeEdge(incrementBlock->getSuccessors(), incrementBlock->getNumber(), branchBlock->getNumber());

   return true;
   }

int32_t
TR_Arraytranslate::getTermValue()
   {
   int32_t defaultValue;

   if (getByteInput())
      defaultValue = -1;
   else
      defaultValue = 0xFFFF;

   if (_termCharNode)
      defaultValue = _termCharNode->getInt();

   if (TR::ILOpCode::isStrictlyLessThanCmp(_compareOp))
      {
      --defaultValue; // a value that will be outside the range
      }
   if (TR::ILOpCode::isStrictlyGreaterThanCmp(_compareOp))
      {
      ++defaultValue; // a value that will be outside the range
      }
   return defaultValue;
   }

TR::Node*
TR_Arraytranslate::getTermCharNode()
   {
   if (compilerGeneratedTable())
      {
      TR::Node * n = _termCharNode;
      if (!n)
         n = _tableNode;
      return TR::Node::create(n, TR::iconst, 0, getTermValue());
      }
   else if  (!hasBranch())
      {
      return TR::Node::create(_tableNode, TR::iconst, 0, 0); // a guess
      }
   return _termCharNode;
   }

TR::Node *
TR_Arraytranslate::getTableNode()
   {
   if (compilerGeneratedTable() && (_tableNode == NULL))
      {
      // need to generate a _tableNode - compiler generated table required
      int32_t defaultValue = getTermValue();
      int8_t inputBits = getByteInput() ? 8 : 16;
      int8_t outputBits= getByteOutput() ? 8 : 16;
      int32_t max       = (1 << inputBits);
      int32_t signedMax = (inputBits == 16) ? max : (max >> 1);
      uint32_t startA, startB, endA, endB;

      if (TR::ILOpCode::isLessCmp(_compareOp))
         {
         if (defaultValue < 0)
            {
            // need 2 ranges to initialize to non-default, 0 to signed-max and  max + termVal
            startA = 0;
            endA   = signedMax;
            startB = max + defaultValue + 1;
            endB   = max;
            }
         else
            {
            startA = 0;
            endA   = defaultValue;
            startB = endB = 0; // not required
            }
         }
      else if (TR::ILOpCode::isGreaterCmp(_compareOp))
         {
         if (defaultValue < 0)
            {
            startA = signedMax;
            endA   = max + defaultValue;
            startB = endB = 0; // not required
            }
         else
            {
            // need 2 ranges to initialize to non-default, 0 to termVal and signed-max+1 to max
            startA = 0;
            endA   = defaultValue;
            startB = signedMax;
            endB   = max;
            }
         }
      else if (TR::ILOpCode::isEqualCmp(_compareOp))
         {
         if (defaultValue < 0)
            {
            startA = 0;
            endA = max + defaultValue;
            startB = max + defaultValue+1;
            endB = max;
            }
         else
            {
            startA = 0;
            endA = defaultValue;
            startB = defaultValue+1;
            endB = max;
            }
         }
      else if (TR::ILOpCode::isNotEqualCmp(_compareOp))
         {
         if (defaultValue < 0)
            {
            startA = max + defaultValue;
            endA = max + defaultValue+1;
            startB = endB = 0;
            }
         else
            {
            startA = defaultValue;
            endA = defaultValue+1;
            startB = endB = 0;
            }
         }
      else
         {
         if ((_compareOp == TR::BadILOp) &&
               (!hasBranch() && !hasBreak()))
            {
            startA = 0;
            endA = max;
            startB = endB = 0;
            }
         else
            return NULL; // no table available
         }

      if (outputBits == 8 && defaultValue < 0)
         {
         defaultValue += 256;
         }
      else if (outputBits == 16 && defaultValue < 0)
         {
         defaultValue += 65536;
         }

      TR::SymbolReference *tableSymRef;
      if (inputBits == 8)
         {
         if (startB != endB)
            {
            TR_RangeTranslateTable rangeTable(comp(), inputBits, outputBits, (uint16_t)startA, (uint16_t)endA, (uint16_t)startB, (uint16_t)endB, defaultValue);
            tableSymRef = rangeTable.createSymbolRef();
            rangeTable.dumpTable();
            }
         else
            {
            TR_RangeTranslateTable rangeTable(comp(), inputBits, outputBits, (uint16_t)startA, (uint16_t)endA, defaultValue);
            tableSymRef = rangeTable.createSymbolRef();
            rangeTable.dumpTable();
            }
         }
      else
         {
         if (startB != endB)
            {
            TR_RangeTranslateTable rangeTable(comp(), inputBits, outputBits, startA, endA, startB, endB, defaultValue);
            tableSymRef = rangeTable.createSymbolRef();
            rangeTable.dumpTable();
            }
         else
            {
            TR_RangeTranslateTable rangeTable(comp(), inputBits, outputBits, startA, endA, defaultValue);
            tableSymRef = rangeTable.createSymbolRef();
            rangeTable.dumpTable();
            }
         }
      TR::Node * n = _termCharNode;
      if (!n)
         n = _resultNode;
      _tableNode = TR::Node::createWithSymRef(n, TR::loadaddr, 0, tableSymRef);
      }

   return _tableNode;
   }

bool
TR_LoopReducer::replaceInductionVariable(TR::Node *parent, TR::Node *node, int32_t childNum,
                                          int32_t indVarSymRefNumber, TR::Node *replacingNode,
                                          vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return false;

   node->setVisitCount(visitCount);
   if (node->getOpCode().hasSymbolReference())
      {
      if (node->getSymbolReference()->getReferenceNumber() == indVarSymRefNumber)
         {
         parent->setAndIncChild(childNum, replacingNode);
         return true;
         }
      }

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      if (replaceInductionVariable(node, node->getChild(i), i, indVarSymRefNumber, replacingNode, visitCount))
         return true;
      }
   return false;
   }

bool
TR_LoopReducer::generateArraytranslate(TR_RegionStructure * whileLoop, TR_InductionVariable * indVar,
   TR::Block * firstBlock, TR::Block * secondBlock, TR::Block * thirdBlock, TR::Block * fourthBlock)
   {
   // The following is the general loop you can expect for a translate type instruction.
   // Note that you will probably need to run with recompilation for all the bounds checks
   // to get eliminated - the profile-directed feedback is required to eliminate the
   // bounds check on the translation table reference.
   //
   // There are 4 types of translation loops - b2b, b2s, s2b, c2c
   // b2b - byte to byte - table lookup is 256 byte table
   // b2s - byte to short - table lookup is 256 character table
   // s2b - short to byte - table lookup is 65536 byte table
   // s2s - short to short - table lookup is 65536 character table
   //
   // Each loop is of the form:
   //   for (int i=<start>; i< <end>; ++i)
   //      {
   //      item = table[in[i]];
   //      if (item == <termination-character>) break;
   //      out[i] = item;
   //      }
   //BBStart <load-char>
   //istore #transchar (store translated character)
   //  c2i
   //    isload <translate-char> (load character)
   //      aiadd
   //        aload #table (translation table)
   //        isub (add displacement to get to start of array with index into translation table)
   //          imul (multiply by stride of target - may not be present if byte-to-byte)
   //            bu2i (widen byte (unsigned) to int - could be
   //              ibload #byte (load the byte - could also be isload)
   //                aiadd
   //                  aload #base (load base pointer)
   //                  isub (add displacement to get to relative index)
   //                    iload #i (induction variable)
   //                    iconst -16
   //            iconst 2
   //          iconst -16
   //ifscmpne --> block <no-stop>
   //  ==>isload at <translate-char>
   //  sconst <termination char>
   //BBEnd <load-char>
   //BBStart <escape>
   //   goto <escape-block>
   //BBEnd <escape>
   //
   //BBStart <store-char>
   //isstore #reschar (result char stored into output array)
   //   aiadd
   //     aload #outbase (load output base ptr)
   //     isub (add displacement to get to relative index
   //       imul (multiply by stride of target)
   //         iload #i <induction variable>
   //         iconst 2
   //       iconst -16
   //   i2c
   //     iload #transchar (translated character from first block)
   //istore #i (induction variable)
   //   isub
   //     iload #i
   //     iconst -1
   //ificmplt --> <load-char>
   //   iload #i
   //   <termination value>
   //
   //BBEnd <store-char>
   //
   // -or-
   //
   // ---------------------------------------------------------------------------------------------------------
   //
   //BBStart <load-char>
   //istore #transchar (store translated character)
   //  c2i
   //    isload <translate-char> (load character)
   //      aiadd
   //        aload #table (translation table)
   //        isub (add displacement to get to start of array with index into translation table)
   //          imul (multiply by stride of target - may not be present if byte-to-byte)
   //            bu2i (widen byte (unsigned) to int - could be
   //              ibload #byte (load the byte - could also be isload)
   //                aiadd
   //                  aload #base (load base pointer)
   //                  isub (add displacement to get to relative index)
   //                    iload #i (induction variable)
   //                    iconst -16
   //            iconst 2
   //          iconst -16
   //ifscmpeq --> block <escape-block>
   //  ==>isload at <translate-char>
   //  sconst <termination char>
   //BBEnd <load-char>
   //
   //BBStart <store-char>
   //isstore #reschar (result char stored into output array)
   //   aiadd
   //     aload #outbase (load output base ptr)
   //     isub (add displacement to get to relative index
   //       imul (multiply by stride of target)
   //         iload #i <induction variable>
   //         iconst 2
   //       iconst -16
   //   i2c
   //     iload #transchar (translated character from first block)
   //istore #i (induction variable)
   //   isub
   //     iload #i
   //     iconst -1
   //ificmplt --> <load-char>
   //   iload #i
   //   <termination value>
   //
   //BBEnd <store-char>
   //
   // ---------------------------------------------------------------------------------------------------------
   //
   // Another similar loop is of the form:
   //   for (int i=<start>; i< <end>; ++i)
   //      {
   //      item = in[i];
   //      if (item <comparison> <termination-character>) break; [<comparison> can be <, <=, >, >=]
   //      out[i] = item;
   //      }
   //BBStart <load-char>
   //istore #transchar (store translated character)
   //  b2i
   //    ibload <byte>
   //     aiadd
   //       aload #base (load base pointer)
   //         isub (add displacement to get to relative index)
   //            iload #i (induction variable)
   //            iconst -16
   //       iconst -16
   //ifbcmpXX --> block <no-stop>
   //  ==>ibload <byte>
   //  bconst <termination char> [+/- 1]
   //BBEnd <load-char>
   //BBStart <escape>
   //   goto <escape-block>
   //BBEnd <escape>
   //
   //BBStart <store-char>
   //isstore #reschar (result char stored into output array)
   //   aiadd
   //     aload #outbase (load output base ptr)
   //     isub (add displacement to get to relative index
   //       imul (multiply by stride of target)
   //         iload #i <induction variable>
   //         iconst 2
   //       iconst -16
   //   i2c
   //     iload #transchar (widened byte from first block)
   //istore #i (induction variable)
   //   isub
   //     iload #i
   //     iconst -1
   //ificmplt --> <load-char>
   //   iload #i
   //   <termination value>
   //
   //BBEnd <store-char>
   //
   //Simplest form of the loop is:
   //BBStart <load/store-char>
   // isstore #reschar (result char stored into output array)
   //   aiadd
   //     aload #outbase (load output base ptr)
   //     isub (add displacement to get to relative index
   //       imul (multiply by stride of target)
   //         iload #i <induction variable>
   //         iconst 2
   //       iconst -16
   //    ibload <byte>
   //     aiadd
   //       aload #base (load base pointer)
   //         isub (add displacement to get to relative index)
   //            iload #i (induction variable)
   //            iconst -16
   //       iconst -16
   //istore #i (induction variable)
   //   isub
   //     iload #i
   //     iconst -1
   //ificmplt --> <load-char>
   //   iload #i
   //   <termination value>
   //
   //BBEnd <load/store-char>

   if (!cg()->getSupportsArrayTranslateTRxx())
      {
      dumpOptDetails(comp(), "arraytranslate not enabled for this platform\n");
      return false;
      }

   // test conditions to see if loop matches a translate loop

   int32_t firstNum = firstBlock ? firstBlock->getNumberOfRealTreeTops() : 0;
   int32_t secondNum = secondBlock ? secondBlock->getNumberOfRealTreeTops() : 0;
   int32_t thirdNum = thirdBlock ? thirdBlock->getNumberOfRealTreeTops() : 0;
   int32_t fourthNum = fourthBlock ? fourthBlock->getNumberOfRealTreeTops() : 0;
   if (
       ((firstNum == 3 || firstNum == 4 || firstNum == 5) && secondNum == 0) ||
       ((firstNum == 3 || firstNum == 2) && secondNum == 3) ||
       ((firstNum == 3 || firstNum == 2) && secondNum == 1 && thirdNum == 2 && fourthNum == 2)
      )
      {
      ; // this is ok - matches one of our loop forms
      }
   else
      {
      dumpOptDetails(comp(), "Loop has wrong number of tree tops (%d,%d,%d,%d) - no arraytranslate reduction\n", firstNum, secondNum,
         thirdNum, fourthNum);
      return false;
      }

   bool hasBreak;

   bool hasBranch = true;
   if (
       ((firstNum == 2 || firstNum == 3) && secondNum == 3)
      )
      {
      hasBreak = true;
      dumpOptDetails(comp(), "Processing blocks %d %d\n", firstNum, secondNum);
      }
   else
      {
      hasBreak = false;
      if (secondNum == 0)
         {
         hasBranch = false;
         }
      dumpOptDetails(comp(), "Processing blocks %d %d %d %d\n", firstNum, secondNum, thirdNum, fourthNum);
      }

   TR::TreeTop * loadTree = NULL;
   TR::Node * loadNode = NULL;
   TR::TreeTop * storeTree = NULL;
   TR::Node * storeNode = NULL;
   TR::TreeTop * indVarStoreTree = NULL;
   TR::Node * indVarStoreNode = NULL;
   // materialized induction variable, created when one or more
   // induction variables are eliminated as redundant (and expressed
   // in terms of the main iv)
   //
   bool mayHaveMaterializedIV = false;
   bool indVarStoreIsFirstTreeTop = false;
   TR::TreeTop * matIndVarStoreTree = NULL;
   TR::Node * matIndVarStoreNode = NULL;
   TR::TreeTop * loopCmpTree = NULL;
   TR::Node * loopCmpNode = NULL;
   TR::TreeTop * nextTree = NULL;
   TR::Block * nextBlock = NULL;

   TR::TreeTop * branchAroundTree = NULL;
   TR::Node * branchAroundNode = NULL;

   TR::TreeTop * branchOverStoreTree = NULL;
   TR::Node * branchOverStoreNode = NULL;

   TR::TreeTop * otherStoreTree = NULL;
   TR::Node * otherStoreNode = NULL;

   TR::Block * loadBlock = NULL;
   TR::Block * storeBlock = NULL;
   TR::Block * otherStoreBlock = NULL; // without break only
   TR::Block * cmpBlock = NULL; // without break only
   if (hasBreak)
      {
      loadBlock = firstBlock;
      storeBlock = secondBlock;

      loadTree = loadBlock->getFirstRealTreeTop();
      loadNode = loadTree->getNode();
      if (loadNode->getOpCodeValue() == TR::asynccheck && firstNum == 3)
         {
         loadTree = loadTree->getNextTreeTop();
         loadNode = loadTree->getNode();
         }

      branchAroundTree = loadTree->getNextTreeTop();
      branchAroundNode = branchAroundTree->getNode();

      storeTree = storeBlock->getFirstRealTreeTop();
      storeNode = storeTree->getNode();

      indVarStoreTree = storeTree->getNextTreeTop();
      indVarStoreNode = indVarStoreTree->getNode();

      loopCmpTree = indVarStoreTree->getNextTreeTop();
      loopCmpNode = loopCmpTree->getNode();

      nextTree = storeBlock->getExit()->getNextTreeTop();
      nextBlock = (nextTree) ? nextTree->getEnclosingBlock() : NULL;
      }
   else
      {
      loadBlock = firstBlock;

      loadTree = loadBlock->getFirstRealTreeTop();
      loadNode = loadTree->getNode();
      if (loadNode->getOpCodeValue() == TR::asynccheck)
         {
         loadTree = loadTree->getNextTreeTop();
         }

      if (hasBranch)
         {
         storeBlock = secondBlock;
         cmpBlock = thirdBlock;
         otherStoreBlock = fourthBlock;

         loadNode = loadTree->getNode();

         branchAroundTree = loadTree->getNextTreeTop();
         branchAroundNode = branchAroundTree->getNode();

         storeTree = storeBlock->getFirstRealTreeTop();
         storeNode = storeTree->getNode();

         otherStoreTree = otherStoreBlock->getFirstRealTreeTop();
         otherStoreNode = otherStoreTree->getNode();

         branchOverStoreTree = otherStoreTree->getNextTreeTop();
         branchOverStoreNode = branchAroundTree->getNode();

         indVarStoreTree = cmpBlock->getFirstRealTreeTop();
         nextTree = cmpBlock->getExit()->getNextTreeTop();
         }
      else
         {
         storeTree = loadTree;
         // check for the loop iv
         //
         if (storeTree->getNode()->getOpCode().isStoreDirect())
            {
            mayHaveMaterializedIV = true;
            indVarStoreTree = storeTree;
            storeTree = storeTree->getNextTreeTop();
            }
         storeNode = (storeTree->getNode()->getNumChildren() == 2) ? storeTree->getNode() : NULL;
         loadTree  = (storeNode != NULL && storeNode->getNumChildren() == 2) ? storeTree : NULL;
         loadNode  = (loadTree) ? storeNode->getSecondChild() : NULL;
         if (storeNode == NULL || loadNode == NULL)
            {
            dumpOptDetails(comp(), "Load node is %p and store node is %p\n", loadNode, storeNode);
            return false;
            }

         if (mayHaveMaterializedIV)
            {
            matIndVarStoreTree = storeTree->getNextTreeTop();
            matIndVarStoreNode = matIndVarStoreTree->getNode();
            indVarStoreIsFirstTreeTop = true;
            }
         else
            {
            indVarStoreTree = storeTree->getNextTreeTop();
            TR::TreeTop * nextTT = indVarStoreTree->getNextTreeTop();
            if (nextTT->getNode()->getOpCode().isStoreDirect())
               {
               mayHaveMaterializedIV = true;
               matIndVarStoreTree = indVarStoreTree->getNextTreeTop();
               matIndVarStoreNode = matIndVarStoreTree->getNode();
               }
            }
         nextTree = loadBlock->getExit()->getNextTreeTop();
         }
      nextBlock = (nextTree) ? nextTree->getEnclosingBlock() : NULL;

      indVarStoreNode = indVarStoreTree->getNode();

      if (mayHaveMaterializedIV)
         loopCmpTree = matIndVarStoreTree->getNextTreeTop();
      else
         loopCmpTree = indVarStoreTree->getNextTreeTop();
      loopCmpNode = loopCmpTree->getNode();

      }

   if (!nextBlock)
      {
      dumpOptDetails(comp(), "Loop exit block is method exit - no arraytranslate reduction\n");
      return false;
      }

   TR_Arraytranslate arraytranslateLoop(comp(), indVar, hasBranch, hasBreak);

   if (!arraytranslateLoop.checkLoad(loadNode))
      {
      dumpOptDetails(comp(), "Loop does not have load tree - no arraytranslate reduction\n");
      return false;
      }

   // hasBreak can never be true if hasBranch is false
   // the test is used to detect the case when loop has a single block
   // resulting in both to be false (the loop can still be reduced)
   //
   if (arraytranslateLoop.compilerGeneratedTable() && !hasBranch && hasBreak)
      {
      dumpOptDetails(comp(), "Attempt to reduce loop marked with no branch but also comp gen'ed table - no arraytranslate reduction\n");
      return false;
      }

   // if hasBreak and hasBranch are both false, the loop is a simple byte->char or char->byte conversion
   // the termChar can be ignored by the instruction if only the hardware supports it. if there is no
   // support for ignoring the termChar, then char->byte conversions cannot be reduced
   // (since the termChar is treated as byte from 0x00 - 0xFF, this effectively means there is no good
   // defaultValue that can be specified)
   //
   if (!hasBranch && !arraytranslateLoop.getByteInput() && !cg()->getSupportsTestCharComparisonControl())
      {
      dumpOptDetails(comp(), "Cannot reduce char->byte loops since hardware does not support termination character control\n");
      return false;
      }

   if (!arraytranslateLoop.getStoreAddress()->checkIndVarStore(indVarStoreNode))
      {
      dumpOptDetails(comp(), "Loop does not have indvar tree - no arraytranslate reduction\n");
      return false;
      }

   if (mayHaveMaterializedIV &&
         !arraytranslateLoop.checkMatIndVarStore(matIndVarStoreNode, indVarStoreNode))
      {
      dumpOptDetails(comp(), "Loop does not match materialized indvar requirements - no arraytranslate reduction\n");
      return false;
      }

   if (!arraytranslateLoop.checkStore(storeNode))
      {
      dumpOptDetails(comp(), "Loop does not have store tree - no arraytranslate reduction\n");
      return false;
      }

   // check for src & dest arrays being different
   // if they are same, the range of indices may overlap. in this case,
   // the result of the arrayTranslate instruction in the hardware is unpredictable
   //
   TR::Node *inputBase = arraytranslateLoop.getInputNode(); //returns aiadd
   TR::Node *outputBase = arraytranslateLoop.getOutputNode();
   if (inputBase && outputBase)
      {
      inputBase = inputBase->getFirstChild(); //returns iaload/aload base
      outputBase = outputBase->getFirstChild();
      if ((inputBase == outputBase) ||
            (inputBase->getOpCode().hasSymbolReference() && outputBase->getOpCode().hasSymbolReference() &&
              inputBase->getSymbolReference()->getSymbol() == outputBase->getSymbolReference()->getSymbol()))
         {
         dumpOptDetails(comp(), "Loop has same src and dst arrays -- no arraytranslate reduction\n");
         return false;
         }
      }

   if (hasBranch)
      {
      if (!arraytranslateLoop.checkBreak(storeBlock, branchAroundNode, nextBlock))
         {
         dumpOptDetails(comp(), "Loop does not have branch-around tree - no arraytranslate reduction\n");
         return false;
         }
      }
   else
      {
      if (arraytranslateLoop.getTableNode() == NULL)
         {
         dumpOptDetails(comp(), "Loop does not have a translation table - no arraytranslate reduction\n");
         return false;
         }
      }

   // if there is no compiler generated table for the arraytranslate,
   // then the datatypes of the output array and the translation table (node)
   // should match
   //
   if (!arraytranslateLoop.compilerGeneratedTable() &&
         (arraytranslateLoop.getResultUnconvertedNode()->getDataType() != storeNode->getDataType()))
      {
      dumpOptDetails(comp(), "Output array and translation table datatype mismatch - no arraytranslate reduction\n");
      return false;
      }

   if (!arraytranslateLoop.checkLoopCmp(loopCmpNode, indVarStoreNode, indVar))
      {
      dumpOptDetails(comp(), "Loop does not have loopcmp tree - no arraytranslate reduction\n");
      return false;
      }

   if (!arraytranslateLoop.forwardLoop())
      {
      dumpOptDetails(comp(), "Loop does not have forward loop - no arraytranslate reduction\n");
      return false;
      }

   if (!hasBreak && hasBranch)
      {
      if (!arraytranslateLoop.checkStore(otherStoreNode))
         {
         dumpOptDetails(comp(), "Loop does not have store tree - no arraytranslate reduction\n");
         return false;
         }
      }

   if (!performTransformation(comp(), "%sReducing arraytranslate %d\n", OPT_DETAILS, loadBlock->getNumber()))
      {
      return false;
      }

   // modify tree with arraytranslate loop
   // Output tree needs to be of the form:
   //
   //BBStart <test block>
   //ificmple --> block <original-code>
   //  <termination value>
   //  iconst <hardware-minimum-length>
   //ificmpne --> block <original-code>  *** this is only generated for s2b/s2s if required ***
   //  iand
   //     <table address>
   //     iconst <hardware-page-mask>
   //BBEnd <test block>
   //BBStart <translate block>
   //istore #i (induction variable - number of bytes translated)
   //   arraytranslate
   //     <aiadd input tree>
   //     <aiadd output tree>
   //     <aload translation table>
   //     <isload translate char>
   //     <termination value>
   //
   //isstore #reschar (result char stored into output array)
   //   <original aiadd input tree>
   //
   //goto <escape-block>
   //BBEnd <translate block>
   //BBStart <original-code>
   // ...
   //BBEnd <original-code>
   //BBStart <escape-block>
   // ...
   // duplicate some trees for use further down
   TR::Node * resCharDupNode = NULL;
   if (hasBranch)
      {
      resCharDupNode = loadNode->duplicateTree();
      }

   TR::Node * indVarDupNode = indVarStoreNode->duplicateTree();

   // establish new 'load' tree and block and connect it up to the old load/break code
   TR::TreeTop * newLoadTreeTop;
   if (hasBranch)
      {
      newLoadTreeTop = TR::TreeTop::create(comp(), loadNode);
      }
   else
      {
      newLoadTreeTop = TR::TreeTop::create(comp(), storeNode);
      }
   TR::Block * newLoadBlock = TR::Block::createEmptyBlock(loadNode, comp(), firstBlock->getFrequency(), firstBlock);
   _cfg->addNode(newLoadBlock);
   TR::TreeTop * newLoadEntry = newLoadBlock->getEntry();
   TR::TreeTop * newLoadExit = newLoadBlock->getExit();

   TR::Block * gotoBlock = TR::Block::createEmptyBlock(loadNode, comp(), firstBlock->getFrequency(), firstBlock);
   _cfg->addNode(gotoBlock);
   TR::TreeTop * gotoEntry = gotoBlock->getEntry();
   TR::TreeTop * gotoExit = gotoBlock->getExit();

   // comparison of translate length

   int32_t minimumSize = cg()->arrayTranslateMinimumNumberOfElements(arraytranslateLoop.getByteInput(),
                                  arraytranslateLoop.getByteOutput());
   TR::Node * hardwareMinimumNode = TR::Node::create(loadNode, TR::iconst, 0, minimumSize);
   TR::Node * finalNode = arraytranslateLoop.getFinalNode()->duplicateTree();
   TR::Node * compareNode = TR::Node::createif(TR::ificmple, finalNode, hardwareMinimumNode, newLoadEntry);
   TR::TreeTop * branchNewOldTreeTop = loadTree; // necessary to keep old edges consistent
   branchNewOldTreeTop->setNode(compareNode);

   if (mayHaveMaterializedIV)
      loadBlock->getEntry()->join(branchNewOldTreeTop);

   TR::Block * branchNewOldBlock = branchNewOldTreeTop->getEnclosingBlock();
   TR::TreeTop * branchNewOldExit = branchNewOldBlock->getExit();

   TR::Node * tableNode = arraytranslateLoop.getTableNode()->duplicateTree();
   if (tableNode->getType().isInt64() && comp()->target().is32Bit())
      {
      TR::Node * shrunkTableNode = TR::Node::create(TR::l2i, 1, tableNode);
      tableNode = shrunkTableNode;
      }

   // generate the new translate node for use in creating next 'if' block below
   TR::Block * translateBlock = TR::Block::createEmptyBlock(loadNode, comp(), firstBlock->getFrequency(), firstBlock);
   _cfg->addNode(translateBlock);
   TR::TreeTop * translateEntry = translateBlock->getEntry();
   TR::TreeTop * translateExit = translateBlock->getExit();

   /* this is what is going to happen if hasBreak is true
      "The arraytranslate instruction returns the index (into the array) of the
       last element that was successfully translated."
      What does this mean?
      a) if the entire array was successfully translated, the return value is length-1.
      So the exit values of
         --> i must be: i = length
         --> c must be: c = charA[i-1]

      b) if the break condition was triggered (entire array not traversed), the return
      value is the index of the last element translated (call it j).
      So exit values of
         --> i must be: i = j
         --> c must be: c = charA[i]

      we need a conditional to update the value of 'c'.

      This means that the loop should look like:
          arraytranslate
          i = i + arraytranslate    // number of elements translated
          if (i < length) {         // lengthTestTree
             c = charA[i]           // load the element read at index i (loadElementBlock)
          }
          else {
             c = charA[i-1]         // load the last element (loadLastElementBlock)
          }
          if (c > termChar) exit    // check for the termChar (originalbreak condition) (matchBlock)
          goto normal_loop_exit     // gotoBlock
   */

   TR::Block *loadElementBlock = NULL;
   TR::Block *loadLastElementBlock = NULL;
   if (hasBreak)
      {
      loadElementBlock = TR::Block::createEmptyBlock(loadNode, comp(), firstBlock->getFrequency(), firstBlock);
      loadLastElementBlock = TR::Block::createEmptyBlock(loadNode, comp(), firstBlock->getFrequency(), firstBlock);
      _cfg->addNode(loadElementBlock);
      _cfg->addNode(loadLastElementBlock);
      }

   // comparison of translate table alignment
   TR::Block * alignmentBlock = NULL;
   TR::TreeTop * alignmentEntry = NULL;
   TR::TreeTop * alignmentExit = NULL;

   int32_t mask = cg()->arrayTranslateTableRequiresAlignment(arraytranslateLoop.getByteInput(), arraytranslateLoop.getByteOutput());
   if (mask && !arraytranslateLoop.compilerGeneratedTable()) // compiler-generated tables aligned at allocation
      {
      alignmentBlock = TR::Block::createEmptyBlock(loadNode, comp(), firstBlock->getFrequency(), firstBlock);
      alignmentEntry = alignmentBlock->getEntry();
      alignmentExit = alignmentBlock->getExit();
      _cfg->addNode(alignmentBlock);

      TR::Node * compareNode = NULL;
      if (arraytranslateLoop.tableBackedByRawStorage())
         {
         if (comp()->target().is64Bit())
            {
            TR::Node * zeroNode = TR::Node::create(loadNode, TR::lconst);
            zeroNode->setLongInt(0);
            TR::Node * pageNode = TR::Node::create(loadNode, TR::lconst);
            pageNode->setLongInt(mask);
            TR::Node * dupTableNode = tableNode->duplicateTree();
            TR::Node * andNode = TR::Node::create(TR::land, 2, dupTableNode, pageNode);

            compareNode = TR::Node::createif(TR::iflcmpne, zeroNode, andNode, newLoadEntry);
            }
         else
            {
            TR::Node * zeroNode = TR::Node::create(loadNode, TR::iconst, 0, 0);
            TR::Node * pageNode = TR::Node::create(loadNode, TR::iconst, 0, mask);
            TR::Node * dupTableNode = tableNode->duplicateTree();

            TR::Node * andNode = TR::Node::create(TR::iand, 2, dupTableNode, pageNode);

            compareNode = TR::Node::createif(TR::ificmpne, zeroNode, andNode, newLoadEntry);
            }
         }
      else
         {
         TR::Node * zeroNode = TR::Node::create(loadNode, TR::iconst, 0, 0);
         TR::Node * pageNode = TR::Node::create(loadNode, TR::iconst, 0, mask);
         TR::Node * dupTableNode = tableNode->duplicateTree();

         TR::Node * hdrSizeNode = TR::Node::create(loadNode, TR::iconst, 0, static_cast<int32_t>(TR::Compiler->om.contiguousArrayHeaderSizeInBytes()));
         TR::Node * addNode = TR::Node::create(TR::iadd, 2, dupTableNode, hdrSizeNode);
         TR::Node * andNode = TR::Node::create(TR::iand, 2, addNode, pageNode);

         compareNode = TR::Node::createif(TR::ificmpne, zeroNode, andNode, newLoadEntry);
         }

      TR::TreeTop * pageSizeTreeTop = TR::TreeTop::create(comp(), compareNode);
      pageSizeTreeTop->setNode(compareNode);

      branchNewOldTreeTop->join(branchNewOldExit);
      branchNewOldExit->join(alignmentEntry);
      alignmentEntry->join(pageSizeTreeTop);
      pageSizeTreeTop->join(alignmentExit);
      alignmentExit->join(translateEntry);
      }
   else
      {
      branchNewOldTreeTop->join(branchNewOldExit);
      branchNewOldExit->join(translateEntry);
      }

   TR::Node * termCharNode = arraytranslateLoop.getTermCharNode()->duplicateTree();
   TR::Node * inputNode = arraytranslateLoop.getInputNode()->duplicateTree();
   TR::Node * outputNode = arraytranslateLoop.getOutputNode()->duplicateTree();
   TR::Node * dupFinalNode = arraytranslateLoop.getFinalNode()->duplicateTree();
   TR::Node * stoppingNode = TR::Node::create( loadNode, TR::iconst, 0, 0xffffffff);


   TR::Node * translateNode = TR::Node::create(loadNode, TR::arraytranslate, 6);
   translateNode->setSymbolReference(comp()->getSymRefTab()->findOrCreateArrayTranslateSymbol());

   translateNode->setAndIncChild(0, inputNode);
   translateNode->setAndIncChild(1, outputNode);
   translateNode->setAndIncChild(2, tableNode);
   translateNode->setAndIncChild(3, termCharNode);

   TR::SymbolReference * indVarSymRef = arraytranslateLoop.getStoreAddress()->getIndVarSymRef();
   TR::Node * indVarLengthNode = arraytranslateLoop.getStoreAddress()->getIndVarLoad()->duplicateTree();
   TR::Node * lengthNode = TR::Node::create(TR::isub, 2, dupFinalNode, indVarLengthNode);

   translateNode->setAndIncChild(4, lengthNode);
   translateNode->setAndIncChild(5, stoppingNode);


   translateNode->setSourceIsByteArrayTranslate(arraytranslateLoop.getByteInput());
   translateNode->setTargetIsByteArrayTranslate(arraytranslateLoop.getByteOutput());
   // if this flag is set, then the termChar is ignored by the TRXX instruction
   //
   translateNode->setTermCharNodeIsHint(hasBreak ? false : !hasBranch);
   translateNode->setTableBackedByRawStorage(arraytranslateLoop.tableBackedByRawStorage() || arraytranslateLoop.compilerGeneratedTable());

   TR::SymbolReference * translateTemp = comp()->getSymRefTab()->
         createTemporary(comp()->getMethodSymbol(), TR::Int32);
   TR::Node * storeTranslateNode = TR::Node::createStore(translateTemp, translateNode);
   TR::TreeTop * translateTreeTop = TR::TreeTop::create(comp(), storeTranslateNode);

   // add number of elements translated to induction variable total

   TR::Node * indVarValueNode = arraytranslateLoop.getStoreAddress()->getIndVarLoad()->duplicateTree();
   TR::Node * addCountNode = TR::Node::create(TR::iadd, 2, indVarValueNode, translateNode);

   TR::Node * indVarUpdateNode = TR::Node::createWithSymRef(TR::istore, 1, 1, addCountNode, indVarSymRef);

   TR::TreeTop * indVarUpdateTreeTop = TR::TreeTop::create(comp(), indVarUpdateNode);

   translateEntry->join(translateTreeTop);
   translateTreeTop->join(indVarUpdateTreeTop);

   // add one to the induction variable to get it to the right final value
   //FIXME: not done anymore. see s390/TreeEvaluator.cpp
   //
   TR::TreeTop * indVarTreeTop = indVarUpdateTreeTop; //TR::TreeTop::create(comp(), indVarDupNode);

   // store last character translated into #reschar

   TR::TreeTop *dupMatIndVarStore = NULL;
   TR::TreeTop *resCharTreeTop = NULL;
   if (hasBranch)
      {
      resCharTreeTop = TR::TreeTop::create(comp(), resCharDupNode);
      if (!hasBreak)
         {
         // update the induction variable in the array access
         // i could be arraylength, so should be updated accordingly
         //
         // i' = i - (i == length) ? 1 : 0;
         //
         TR::Node *iNode = TR::Node::createLoad(loadNode, indVarSymRef);
         TR::Node *icmpNode = TR::Node::create(TR::icmpeq, 2, iNode, arraytranslateLoop.getFinalNode()->duplicateTree());
         TR::Node *decNode = TR::Node::create(TR::isub, 2, iNode, icmpNode);
         vcount_t visitCount = comp()->incVisitCount();
         replaceInductionVariable(NULL, resCharDupNode->getFirstChild()->skipConversions(), -1,
                                    indVarSymRef->getReferenceNumber(),
                                    decNode,
                                    visitCount);
         indVarUpdateTreeTop->join(resCharTreeTop);
         ///resCharTreeTop->join(indVarTreeTop);
         indVarTreeTop = resCharTreeTop;
         }
      }
   else
      {
      ///indVarUpdateTreeTop->join(indVarTreeTop);
      if (mayHaveMaterializedIV)
         {
         // at this point the matIV and primaryIV have been incremented in lockstep
         // so final value of matIV = primaryIV + delta
         // the tree generated here is the dup of the matIV store ie. matIV = primaryIV + delta + 1
         // so subtract by 1 here to get the right final value
         //
         TR::Node *updatedMatIndVar = matIndVarStoreNode->duplicateTree();
         TR::Node *firstChild = updatedMatIndVar->getFirstChild();
         TR::Node *decNode = TR::Node::create(TR::isub, 2, firstChild, TR::Node::create(updatedMatIndVar, TR::iconst, 0, 1));
         firstChild->decReferenceCount();
         updatedMatIndVar->setAndIncChild(0, decNode);
         dupMatIndVarStore = TR::TreeTop::create(comp(), updatedMatIndVar);
         indVarTreeTop->join(dupMatIndVarStore);
         }
      }

   TR::Block * breakExitBlock = NULL;
   TR::Block * matchBlock = NULL;
   if (hasBreak)
      {
      TR::TreeTop * matchTree = branchAroundNode->getBranchDestination();
      breakExitBlock = matchTree->getEnclosingBlock();

      // setup matchBlock
      //
      matchBlock = TR::Block::createEmptyBlock(loadNode, comp(), firstBlock->getFrequency(), firstBlock);
      _cfg->addNode(matchBlock);
      TR::Node *matchBreakNode = branchAroundNode->duplicateTree();
      TR::Node *matchResultNode = TR::Node::createLoad(branchAroundNode, arraytranslateLoop.getResultNode()->getSymbolReference());
      matchBreakNode->getFirstChild()->recursivelyDecReferenceCount();
      matchBreakNode->setAndIncChild(0, matchResultNode);

      TR::TreeTop * matchTreeTop = TR::TreeTop::create(comp(), matchBreakNode);
      matchBlock->getEntry()->join(matchTreeTop);
      matchTreeTop->join(matchBlock->getExit());

      // create the lengthTest which controls the right
      // updates to the iv and the loadChar
      //
      TR::Node *lengthTestNode = TR::Node::createLoad(loadNode, indVarSymRef);
      ///lengthTestNode = TR::Node::create(TR::iadd, 2, lengthTestNode, TR::Node::create(loadNode, TR::iconst, 0, 1));
      TR::Node * dupFinalNode = arraytranslateLoop.getFinalNode()->duplicateTree();
      TR::TreeTop *lengthTestTree = TR::TreeTop::create(comp(), TR::Node::createif(TR::ificmplt,
                                                                                        lengthTestNode,
                                                                                        dupFinalNode, loadElementBlock->getEntry()));
      indVarUpdateTreeTop->join(lengthTestTree);
      lengthTestTree->join(translateExit);

      // setup loadElementBlock and loadLastElementBlock
      //
      translateExit->join(loadLastElementBlock->getEntry());
      loadLastElementBlock->getExit()->join(loadElementBlock->getEntry());
      loadElementBlock->getExit()->join(matchBlock->getEntry());

      // fixup loadElementBlock
      //
      loadElementBlock->getEntry()->join(resCharTreeTop);
      resCharTreeTop->join(loadElementBlock->getExit());

      // fixup loadLastElementBlock
      //
      TR::Node *dupResCharNode = resCharDupNode->duplicateTree();
      TR::Node *ivLoadNode = dupResCharNode;
      // find the arraybase and replace the induction variable by i-1
      // this is because i is already updated to be equal to the arraylength
      //
      for (; !ivLoadNode->getOpCode().isArrayRef() && ivLoadNode->getNumChildren() >= 1; ivLoadNode = ivLoadNode->getFirstChild());
      TR::Node *parentIV = NULL;
      bool foundIV = true;
      if (ivLoadNode->getOpCode().isArrayRef())
         {
         vcount_t visitCount = comp()->incVisitCount();
         TR::Node *decNode = TR::Node::create(TR::isub, 2, TR::Node::createLoad(loadNode, indVarSymRef),
                                             TR::Node::iconst(loadNode, 1));
         replaceInductionVariable(NULL, ivLoadNode, -1, indVarSymRef->getReferenceNumber(), decNode, visitCount);
         }
      TR::TreeTop *dupResCharTreeTop = TR::TreeTop::create(comp(), dupResCharNode);
      loadLastElementBlock->getEntry()->join(dupResCharTreeTop);
      TR::TreeTop *gotoMatchTree = TR::TreeTop::create(comp(), TR::Node::create(loadNode, TR::Goto, 0, matchBlock->getEntry()));

      dupResCharTreeTop->join(gotoMatchTree);
      gotoMatchTree->join(loadLastElementBlock->getExit());


      //indVarTreeTop->join(matchTreeTop);
      //matchTreeTop->join(translateExit);
      }
   else
      {
      if (dupMatIndVarStore)
         dupMatIndVarStore->join(translateExit);
      else
         indVarTreeTop->join(translateExit);
      }

   TR::Node * newGotoNode = TR::Node::create(loadNode, TR::Goto, 0, nextTree);

   TR::TreeTop * branchAroundTreeTop = TR::TreeTop::create(comp(), newGotoNode);

   if (hasBreak)
      matchBlock->getExit()->join(gotoEntry);
   else
      translateExit->join(gotoEntry);

   gotoEntry->join(branchAroundTreeTop);
   branchAroundTreeTop->join(gotoExit);
   gotoExit->join(newLoadEntry);

   newLoadEntry->join(newLoadTreeTop);

   if (hasBranch)
      {
      newLoadTreeTop->join(branchAroundTree);
      branchAroundTree->join(newLoadExit);
      newLoadExit->join(storeBlock->getEntry());
      }
   else
      {
      if (mayHaveMaterializedIV &&
            indVarStoreIsFirstTreeTop)
         {
         newLoadEntry->join(indVarStoreTree);
         indVarStoreTree->join(newLoadTreeTop);
         newLoadTreeTop->join(matIndVarStoreTree);
         }
      else
         newLoadTreeTop->join(indVarStoreTree);

      loopCmpTree->join(newLoadExit);
      newLoadExit->join(nextBlock->getEntry());
      }
   loopCmpNode->setBranchDestination(newLoadBlock->getEntry());

   // update the CFG with these new blocks

   _cfg->setStructure(NULL);

   if (alignmentBlock)
      {
      _cfg->addEdge(TR::CFGEdge::createEdge(loadBlock,  newLoadBlock, trMemory()));
      _cfg->addEdge(TR::CFGEdge::createEdge(loadBlock,  alignmentBlock, trMemory()));
      _cfg->addEdge(TR::CFGEdge::createEdge(alignmentBlock,  newLoadBlock, trMemory()));
      _cfg->addEdge(TR::CFGEdge::createEdge(alignmentBlock,  translateBlock, trMemory()));
      }
   else
      {
      _cfg->addEdge(TR::CFGEdge::createEdge(loadBlock,  newLoadBlock, trMemory()));
      _cfg->addEdge(TR::CFGEdge::createEdge(loadBlock,  translateBlock, trMemory()));
      }

   if (hasBreak)
      _cfg->addEdge(TR::CFGEdge::createEdge(matchBlock,  gotoBlock, trMemory()));
   else
      _cfg->addEdge(TR::CFGEdge::createEdge(translateBlock,  gotoBlock, trMemory()));
   _cfg->addEdge(TR::CFGEdge::createEdge(gotoBlock,  nextBlock, trMemory()));

   if (hasBreak)
      {
      _cfg->addEdge(TR::CFGEdge::createEdge(translateBlock,  loadElementBlock, trMemory()));
      _cfg->addEdge(TR::CFGEdge::createEdge(translateBlock,  loadLastElementBlock, trMemory()));
      _cfg->addEdge(TR::CFGEdge::createEdge(loadElementBlock,  matchBlock, trMemory()));
      _cfg->addEdge(TR::CFGEdge::createEdge(loadLastElementBlock,  matchBlock, trMemory()));
      _cfg->addEdge(TR::CFGEdge::createEdge(matchBlock,  breakExitBlock, trMemory()));

      _cfg->addEdge(TR::CFGEdge::createEdge(newLoadBlock,  storeBlock, trMemory()));
      _cfg->addEdge(TR::CFGEdge::createEdge(newLoadBlock,  breakExitBlock, trMemory()));
      _cfg->addEdge(TR::CFGEdge::createEdge(storeBlock,  newLoadBlock, trMemory()));
      }
   else
      {
      if (hasBranch)
         {
         _cfg->addEdge(TR::CFGEdge::createEdge(newLoadBlock,  storeBlock, trMemory()));
         _cfg->addEdge(TR::CFGEdge::createEdge(newLoadBlock,  otherStoreBlock, trMemory()));
         _cfg->addEdge(TR::CFGEdge::createEdge(cmpBlock,  newLoadBlock, trMemory()));
         }
      else
         {
         _cfg->addEdge(TR::CFGEdge::createEdge(newLoadBlock,  newLoadBlock, trMemory()));
         _cfg->addEdge(TR::CFGEdge::createEdge(newLoadBlock,  nextBlock, trMemory()));
         }
      }

   TR::CFGEdgeList &loadSuccList = loadBlock->getSuccessors();
   TR::CFGEdgeList &storeSuccList = storeBlock->getSuccessors();
   TR::CFGEdgeList &cmpSuccList = cmpBlock->getSuccessors();
   if (hasBranch)
      {

      _cfg->removeEdge(loadSuccList, loadBlock->getNumber(), storeBlock->getNumber());

      if (hasBreak)
         {
         _cfg->removeEdge(loadSuccList, loadBlock->getNumber(), breakExitBlock->getNumber());
         }
      else
         {
         _cfg->removeEdge(loadSuccList, loadBlock->getNumber(), otherStoreBlock->getNumber());
         _cfg->removeEdge(loadSuccList, loadBlock->getNumber(), storeBlock->getNumber());
         _cfg->removeEdge(storeSuccList, storeBlock->getNumber(), loadBlock->getNumber());

         _cfg->removeEdge(cmpSuccList, cmpBlock->getNumber(), loadBlock->getNumber());
         }

      _cfg->removeEdge(storeSuccList, storeBlock->getNumber(), loadBlock->getNumber());

      return false; // do not remove self edge
      }
   else
      {
      _cfg->removeEdge(loadSuccList, loadBlock->getNumber(), nextBlock->getNumber());
      return true;
      }
   }

//
//Load tree should look as follows:
//
// BBStart block 14
//  ifbcmpeq --> block <after>
//   ibload #146[0x03F1527C] Shadow[<array-shadow>]
//     aiadd
//       aload #176[0x03F08DC4] Static[Simple.data [B]
//       isub
//         iload #177[0x03F15380] Auto[<auto slot 4>]
//         iconst -16
//   bconst <termChar>
// BBEnd (block 14)

bool
TR_ArraytranslateAndTest::checkLoad(TR::Block * loadBlock, TR::Node * loadNode)
   {
   if (loadNode->getOpCodeValue() != TR::ifbcmpeq && loadNode->getOpCodeValue() != TR::ificmpeq)
      {
      dumpOptDetails(comp(), "...load tree does not have ifbcmpeq/ificmpeq - no arraytranslateAndTest reduction\n");
      return false;
      }

   TR::Node * bLoadNode;
   if (loadNode->getOpCodeValue() == TR::ificmpeq)
      {
      if (loadNode->getFirstChild()->getOpCodeValue() != TR::b2i)
         {
         dumpOptDetails(comp(), "...load tree has ificmpeq but no widening from byte - no arraytranslateAndTest reduction\n");
         return false;
         }
      else
         {
         bLoadNode = loadNode->getFirstChild()->getFirstChild();
         }
      }
   else
      {
      bLoadNode = loadNode->getFirstChild();
      }

   if (bLoadNode->getOpCodeValue() != TR::bloadi)
      {
      dumpOptDetails(comp(), "...load tree does not have ibload - no arraytranslateAndTest reduction\n");
      return false;
      }

   TR::Node * aiaddNode = bLoadNode->getFirstChild();

   if (aiaddNode->getOpCodeValue() != TR::aiadd && aiaddNode->getOpCodeValue() != TR::aladd)
      {
      dumpOptDetails(comp(), "...load tree does not have aiadd/aladd - no arraytranslate reduction\n");
      return false;
      }
   _inputNode = aiaddNode;

   TR::Node * aiaddFirstChild = aiaddNode->getFirstChild();
   TR::Node * aiaddSecondChild = aiaddNode->getSecondChild();

   if (aiaddFirstChild->getOpCodeValue() != TR::aload && aiaddFirstChild->getOpCodeValue() != TR::aloadi)
      {
      dumpOptDetails(comp(), "...load tree does not have aload - no arraytranslateAndTest reduction\n");
      return false;
      }

   TR::Node * cmpVal = loadNode->getSecondChild();
   if (cmpVal->getOpCodeValue() != TR::bconst && cmpVal->getOpCodeValue() != TR::iconst)
      {
      dumpOptDetails(comp(), "...load tree does not have bconst/iconst - no arraytranslateAndTest reduction\n");
      return false;
      }
   _termCharNode = loadNode->getSecondChild();
   return (getStoreAddress()->checkAiadd(_inputNode, bLoadNode->getSize()));
   }

bool
TR_ArraytranslateAndTest::checkFrequency(TR::CodeGenerator * cg, TR::Block * loadBlock, TR::Node * loadNode)
   {
   int16_t loopHeaderFrequency = loadBlock->getFrequency();
   int16_t fallThroughFrequency = loadBlock->getNextBlock()->getFrequency();
   int16_t breakFrequency = loadNode->getBranchDestination()->getEnclosingBlock()->getFrequency();

   if (((fallThroughFrequency <= 0) &&
        ((loopHeaderFrequency > 0))) ||
       (fallThroughFrequency < breakFrequency * cg->arrayTranslateAndTestMinimumNumberOfIterations()))
      {
      return false;
      }
   return true;
   }

// BBStart block 14
//  ifbcmpeq --> block <after>
//   ibload #146[0x03F1527C] Shadow[<array-shadow>]
//     aiadd
//       aload #176[0x03F08DC4] Static[Simple.data [B]
//       isub
//         iload #177[0x03F15380] Auto[<auto slot 4>]
//         iconst -16
//   bconst <termChar>
// BBEnd (block 14)
//
// BBStart (block 16) (frequency 2064) (is in loop 14)
// istore #177[0x03F15380] Auto[<auto slot 4>]
//   isub
//     iload #177[0x03F15380] Auto[<auto slot 4>]
//     iconst -1
// <async - optional>
// ificmplt --> block 14 BBStart at [0x03F08870]
//   iload #177[0x03F15380] Auto[<auto slot 4>]
//   iconst 1024
// BBEnd (block 16)
bool
TR_LoopReducer::generateArraytranslateAndTest(TR_RegionStructure * whileLoop, TR_InductionVariable * indVar,
   TR::Block * firstBlock, TR::Block * secondBlock)
   {
   if (!cg()->getSupportsArrayTranslateAndTest())
      {
      dumpOptDetails(comp(), "arrayTranslateAndTest not enabled for this platform\n");
      return false;
      }

   // test conditions to see if loop matches a translate loop

   int32_t firstNum = firstBlock ? firstBlock->getNumberOfRealTreeTops() : 0;
   int32_t secondNum = secondBlock ? secondBlock->getNumberOfRealTreeTops() : 0;
   if (firstNum != 1 || (secondNum != 2 && secondNum != 3))
      {
      dumpOptDetails(comp(), "Loop has wrong number of tree tops (%d,%d) - no arraytranslateAndTest reduction\n", firstNum, secondNum);
      return false;
      }

   dumpOptDetails(comp(), "Processing blocks %d %d\n", firstBlock->getNumber(), secondBlock->getNumber());

   TR::Block * loadBlock = firstBlock;
   TR::Block * cmpBlock = secondBlock;

   TR::TreeTop * loadTree = NULL;
   TR::Node * loadNode = NULL;
   TR::TreeTop * indVarStoreTree = NULL;
   TR::Node * indVarStoreNode = NULL;
   TR::TreeTop * loopCmpTree = NULL;
   TR::Node * loopCmpNode = NULL;
   TR::TreeTop * nextTree = NULL;
   TR::Block * nextBlock = NULL;

   loadTree = loadBlock->getFirstRealTreeTop();
   loadNode = loadTree->getNode();


   indVarStoreTree = cmpBlock->getFirstRealTreeTop();
   indVarStoreNode = indVarStoreTree->getNode();

   if (secondNum == 2)
      {
      loopCmpTree = indVarStoreTree->getNextTreeTop();
      loopCmpNode = loopCmpTree->getNode();
      }
   else
      {
      TR::TreeTop * asyncTree = indVarStoreTree;
      TR::Node * asyncNode = asyncTree->getNode();
      bool asyncFound = false;
      for (int i=0; i<secondNum; ++i)
         {
         if (asyncNode->getOpCodeValue() == TR::asynccheck)
            {
            asyncFound = true;
            }
         asyncTree = asyncTree->getNextTreeTop();
         }
      if (!asyncFound)
         {
         dumpOptDetails(comp(), "Loop has wrong number of tree tops for no async-check (%d,%d) - no arraytranslateAndTest reduction\n",
            firstNum, secondNum);
         return false;
         }
      if (indVarStoreTree->getNode()->getOpCodeValue() == TR::asynccheck)
         {
         indVarStoreTree = indVarStoreTree->getNextTreeTop();
         }
      loopCmpTree = indVarStoreTree->getNextTreeTop();
      if (loopCmpTree->getNode()->getOpCodeValue() == TR::asynccheck)
         {
         loopCmpTree = loopCmpTree->getNextTreeTop();
         }
      indVarStoreNode = indVarStoreTree->getNode();
      loopCmpNode = loopCmpTree->getNode();
      }

   nextTree = cmpBlock->getExit()->getNextTreeTop();
   nextBlock = (nextTree) ? nextTree->getEnclosingBlock() : NULL;
   if (!nextBlock)
      {
      dumpOptDetails(comp(), "Loop exit block is method exit - no arraytranslateAndTest reduction\n");
      return false;
      }

   TR_ArraytranslateAndTest arraytranslateAndTestLoop(comp(), indVar);

   if (!arraytranslateAndTestLoop.checkLoad(loadBlock, loadNode))
      {
      dumpOptDetails(comp(), "Loop does not have load tree - no arraytranslateAndTest reduction\n");
      return false;
      }

   if (!arraytranslateAndTestLoop.getStoreAddress()->checkIndVarStore(indVarStoreNode))
      {
      dumpOptDetails(comp(), "Loop does not have indvar tree - no arraytranslateAndTest reduction\n");
      return false;
      }

   if (!arraytranslateAndTestLoop.checkLoopCmp(loopCmpNode, indVarStoreNode, indVar))
      {
      dumpOptDetails(comp(), "Loop does not have loopcmp tree - no arraytranslateAndTest reduction\n");
      return false;
      }

   if (!arraytranslateAndTestLoop.forwardLoop())
      {
      dumpOptDetails(comp(), "Loop does not have forward loop - no arraytranslateAndTest reduction\n");
      return false;
      }

   if (!arraytranslateAndTestLoop.checkFrequency(cg(), loadBlock, loadNode))
      {
      dumpOptDetails(comp(), "Loop frequency on fall-through not high enough - no arraytranslateAndTest reduction\n");
      return false;
      }
   if (!performTransformation(comp(), "%sReducing arraytranslateAndTest %d\n", OPT_DETAILS, loadBlock->getNumber()))
      {
      return false;
      }

   // reduce the loop to a call to arrayTranslateAndTest, returning number of bytes translated, stored into indVar
   TR::Node * termCharNode = arraytranslateAndTestLoop.getTermCharNode()->duplicateTree();
   TR::Node * inputNode = arraytranslateAndTestLoop.getInputNode()->duplicateTree();
   TR::Node * finalNode = arraytranslateAndTestLoop.updateIndVarStore(arraytranslateAndTestLoop.getIndVarNode(), indVarStoreNode, arraytranslateAndTestLoop.getFirstAddress());

   TR::Node * translateNode = TR::Node::create(loadNode, TR::arraytranslateAndTest, 3);
   translateNode->setSymbolReference(comp()->getSymRefTab()->findOrCreateArrayTranslateAndTestSymbol());

   translateNode->setAndIncChild(0, inputNode);
   translateNode->setAndIncChild(1, termCharNode);
   translateNode->setAndIncChild(2, finalNode);

   TR::SymbolReference * indVarSymRef = arraytranslateAndTestLoop.getStoreAddress()->getIndVarSymRef();
   TR::Node * indVarLengthNode = arraytranslateAndTestLoop.getStoreAddress()->getIndVarLoad()->duplicateTree();
   TR::Node * addNode = TR::Node::create(TR::iadd, 2, translateNode, TR::Node::createLoad(loadNode, indVarSymRef));
   TR::Node * storeToIndVar = TR::Node::createWithSymRef(TR::istore, 1, 1, addNode, indVarSymRef);

   loadTree->setNode(storeToIndVar);

   TR::TreeTop * cmpTargetTree = loadNode->getBranchDestination();
   int32_t cmpTargetBlockNumber = cmpTargetTree->getEnclosingBlock()->getNumber();

   if (cmpTargetBlockNumber != nextBlock->getNumber())
      {
      TR::Node * compareNode = TR::Node::createif(TR::ificmpne, arraytranslateAndTestLoop.getStoreAddress()->getIndVarLoad()->duplicateTree(),
                                 arraytranslateAndTestLoop.getFinalNode()->duplicateTree(), cmpTargetTree);
      TR::TreeTop * compareTreeTop = TR::TreeTop::create(comp(), compareNode);

      loadTree->join(compareTreeTop);
      compareTreeTop->join(loadBlock->getExit());

      _cfg->addEdge(TR::CFGEdge::createEdge(loadBlock,  nextBlock, trMemory()));
      }
   _cfg->setStructure(NULL);

   _cfg->removeEdge(loadBlock->getSuccessors(), loadBlock->getNumber(), cmpBlock->getNumber());
   _cfg->removeEdge(cmpBlock->getSuccessors(), cmpBlock->getNumber(), nextBlock->getNumber());
   return true;
   }

//The following is the type of loop that needs to be reduced to an arraycopy (byte to char arraycopy)
//
//BBStart (block 22) (frequency 840) (is in loop 22)
//isstore #134[0x005f227c] Shadow[<array-shadow>]
//  aiadd
//    aload #219[0x00703ac8] Auto[<temp slot 9>]
//    isub
//      imul
//        iload #176[0x005f14f0] Auto[<auto slot 5>]
//        iconst 2
//      iconst -16
//  i2c
//    ior
//      imul
//       bu2i
//          ibload #132[0x005f1c80] Shadow[<array-shadow>]
//            aiadd
//              aload #221[0x00703b50] Auto[<temp slot 11>]
//              isub
//                iload #175[0x005f1444] Auto[<auto slot 4>]
//                ==>iconst -16 at [0x005f21f8]
//       iconst 256
//      bu2i
//        ibload #132[0x005f1c80] Shadow[<array-shadow>]
//          aiadd
//            ==>aload at [0x005f1ac8]
//            isub
//              ==>iload at [0x005f1af4]
//             iconst -17
//istore #175[0x005f1444] Auto[<auto slot 4>]
//  isub
//    ==>iload at [0x005f1af4]
//    iconst -2
//istore #176[0x005f14f0] Auto[<auto slot 5>]
//  isub
//    ==>iload at [0x005f1a9c]
//    iconst -1
//ificmplt --> block 22 BBStart at [0x00903f0c]
//  ==>isub at [0x005f23f8]
//  iconst 2024
//BBEnd (block 22)

bool
TR_LoopReducer::generateByteToCharArraycopy(TR_InductionVariable * byteIndVar, TR_InductionVariable * charIndVar, TR::Block * loopHeader)
   {
   if (!comp()->cg()->getSupportsReferenceArrayCopy() && !comp()->cg()->getSupportsPrimitiveArrayCopy())
      {
      dumpOptDetails(comp(), "arraycopy not enabled for this platform\n");
      return false;
      }

   int32_t incrA = byteIndVar->getIncr()->getLowInt();
   int32_t incrB = charIndVar->getIncr()->getLowInt();

   if (incrA * incrB != 2) /* e.g. 1 and 2, -1 and -2, 2 and 1, -2 and -1 */
      {
      dumpOptDetails(comp(), "Loop does not have an increment of +/-1 and +/-2, but instead %d and %d - no byte to char arraycopy reduction\n", incrA, incrB);
      return false;
      }

   if (incrA == 1 || incrA == -1)
      {
      /* swap the induction variables around */
      TR_InductionVariable * temp = byteIndVar;
      byteIndVar = charIndVar;
      charIndVar = temp;
      }

   if (loopHeader->getNumberOfRealTreeTops() != 4) // array store, induction variable store, induction variable store, loop comparison
      {
      dumpOptDetails(comp(), "Loop has %d tree tops - no byte to char arraycopy reduction\n", loopHeader->getNumberOfRealTreeTops());
      return false;
      }

   TR::TreeTop * arrayStoreTree = loopHeader->getFirstRealTreeTop();
   TR::Node * storeNode = arrayStoreTree->getNode();

   TR_ByteToCharArraycopy arraycopyLoop(comp(), charIndVar, byteIndVar, comp()->target().cpu.isBigEndian());

   if (!arraycopyLoop.checkArrayStore(storeNode))
      {
      return false;
      }

   if (!arraycopyLoop.checkByteLoads(storeNode->getSecondChild()))
      {
      dumpOptDetails(comp(), "second child of store does not look like OR of 2 byte loads - no byte to char arraycopy performed\n");
      return false;
      }

   TR::TreeTop * charIndVarStoreTree = arrayStoreTree->getNextTreeTop();
   TR::Node    * charIndVarStoreNode = charIndVarStoreTree->getNode();

   TR::TreeTop * byteIndVarStoreTree = charIndVarStoreTree->getNextTreeTop();
   TR::Node    * byteIndVarStoreNode = byteIndVarStoreTree->getNode();

   TR::TreeTop * lastIndVarStoreTree = byteIndVarStoreTree;

   TR::TreeTop * loopCmpTree = byteIndVarStoreTree->getNextTreeTop();
   TR::Node    * loopCmpNode = loopCmpTree->getNode();

   if (!arraycopyLoop.getStoreAddress()->checkIndVarStore(charIndVarStoreNode))
      {
      bool swap = arraycopyLoop.getHighLoadAddress()->checkIndVarStore(charIndVarStoreNode);
      if (swap && arraycopyLoop.getStoreAddress()->checkIndVarStore(byteIndVarStoreNode))
         {
         dumpOptDetails(comp(), "try swapping the 2 induction variables\n");
         TR::TreeTop * tmpTree = byteIndVarStoreTree;
         TR::Node    * tmpNode = byteIndVarStoreNode;
         byteIndVarStoreTree = charIndVarStoreTree;
         byteIndVarStoreNode = charIndVarStoreNode;
         charIndVarStoreTree = tmpTree;
         charIndVarStoreNode = tmpNode;
         }
      else
         {
         dumpOptDetails(comp(), "Did not encounter byte array induction variable increment - no byte to char arraycopy performed\n");
         return false;
         }
      }
   else if (!arraycopyLoop.getHighLoadAddress()->checkIndVarStore(byteIndVarStoreNode))
      {
      dumpOptDetails(comp(), "Did not encounter char array induction variable increment - no byte to char arraycopy performed\n");
      return false;
      }

   if (!arraycopyLoop.checkLoopCmp(loopCmpNode, charIndVarStoreNode, charIndVar))
      {
      // For now, we require that the comparison is for the character array induction variable.
      // We should make this work if it was comparing the byte array induction variable as an enhancement,
      // e.g.
      // for (byteInd=start; byteInd<end; byteInd+=2)
      //   { ... }
      dumpOptDetails(comp(), "Loop comparison does not match byte or char induction variable - no byte to char arraycopy performed\n");
      return false;
      }

   if (!performTransformation(comp(), "%sReducing byte to char arraycopy %d\n", OPT_DETAILS, loopHeader->getNumber()))
      {
      return false;
      }


   // In addition to the arraycopy we generate, we also will need to generate an update of the
   // second induction variable - this will require a new node.
   // There is no 'final' value as in the case of the primary induction variable, so the final
   // value will have to be calculated.
   // Ignoring the fact that they could have different starting values, the primary induction variable
   // is either twice the value of the secondary induction variable, or vice versa. The final value
   // of the secondary induction variable is thus (primaryEnd - primaryStart) <operator> 2,
   // where <operator> may be multiplication or division.

   //For details on arraycopy call to be generated, see generateArrayCopy. The core tree
   //is repeated here for convenience.
   //Tree-Top
   // ArrayCopy call
   //     ArrayStoreBase tree
   //     ArrayLoadBase tree
   //     number of bytes to copy
   // first induction variable update tree (this tree is new - see note above)
   // second induction variable update tree (this re-uses comparison node from original tree)

   //First, set the increment for the byte array addresses to 1 so that the code doesn't insert
   //a multiplier in to the base address calculation of the src node on the arraycopy.
   arraycopyLoop.getHighLoadAddress()->setIncrement(1);
   arraycopyLoop.getLowLoadAddress()->setIncrement(1);

   arraycopyLoop.getStoreAddress()->updateAiaddSubTree(arraycopyLoop.getStoreIndVarNode(), &arraycopyLoop);
   arraycopyLoop.getHighLoadAddress()->updateAiaddSubTree(arraycopyLoop.getHighLoadIndVarNode(), &arraycopyLoop);
   TR::Node * charImul = arraycopyLoop.updateIndVarStore(arraycopyLoop.getStoreIndVarNode(), charIndVarStoreNode, arraycopyLoop.getStoreAddress());
   TR::Node * byteImul = arraycopyLoop.updateIndVarStore(arraycopyLoop.getHighLoadIndVarNode(), byteIndVarStoreNode, arraycopyLoop.getHighLoadAddress());
   arraycopyLoop.getStoreAddress()->updateMultiply(arraycopyLoop.getStoreMultiplyNode());
   arraycopyLoop.getHighLoadAddress()->updateMultiply(arraycopyLoop.getHighLoadMultiplyNode());

   TR::Node * loadAddr = arraycopyLoop.getHighLoadAddress()->getRootNode();
   TR::Node * storeAddr= arraycopyLoop.getStoreAddress()->getRootNode();
   TR::Node * arraycopy = TR::Node::createArraycopy(loadAddr, storeAddr, charImul->duplicateTree());

   loadAddr->decReferenceCount();
   storeAddr->decReferenceCount();
   arraycopyLoop.getLowLoadAddress()->getRootNode()->recursivelyDecReferenceCount();


   arraycopy->setSymbolReference(comp()->getSymRefTab()->findOrCreateArrayCopySymbol());
   arraycopy->setForwardArrayCopy(true);

   arraycopy->setArrayCopyElementType(TR::Int8);

   TR::Node * top = TR::Node::create(TR::treetop, 1, arraycopy);
   arrayStoreTree->setNode(top);

   TR::TreeTop * deadStore     = byteIndVarStoreTree;
   TR::TreeTop * deadCmpChild1 = TR::TreeTop::create(comp(), lastIndVarStoreTree, loopCmpNode);
   TR::TreeTop * deadCmpChild2 = TR::TreeTop::create(comp(), deadCmpChild1, loopCmpNode);

   deadStore->setNode(TR::Node::create(TR::treetop, 1, byteIndVarStoreNode->getFirstChild()));
   deadCmpChild1->setNode(TR::Node::create(TR::treetop, 1, loopCmpTree->getNode()->getFirstChild()));
   deadCmpChild2->setNode(TR::Node::create(TR::treetop, 1, loopCmpTree->getNode()->getSecondChild()));

   deadStore->getNode()->getFirstChild()->recursivelyDecReferenceCount();
   deadCmpChild1->getNode()->getFirstChild()->decReferenceCount();
   deadCmpChild2->getNode()->getFirstChild()->decReferenceCount();
   deadCmpChild2->join(loopHeader->getExit());

   TR::SymbolReference * charIndVarTemp = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Int32);
   TR::Node * charIndVarTempLoadNode = TR::Node::createLoad(storeAddr, arraycopyLoop.getStoreAddress()->getIndVarSymRef());
   TR::Node * charIndVarTempStoreNode = TR::Node::createStore(charIndVarTemp, charIndVarTempLoadNode);

   TR::TreeTop * charIndVarTempStoreTree = TR::TreeTop::create(comp(), charIndVarTempStoreNode);
   arrayStoreTree->insertAfter(charIndVarTempStoreTree);

   TR::Node * origTempReLoadNode   = TR::Node::createLoad(storeAddr, charIndVarTemp);
   TR::Node * charIndVarReLoadNode = TR::Node::createLoad(storeAddr, arraycopyLoop.getStoreAddress()->getIndVarSymRef());
   TR::Node * subNode = TR::Node::create(TR::isub, 2, charIndVarReLoadNode, origTempReLoadNode);

   TR::Node * charStrideConst = TR::Node::create(storeAddr, TR::iconst, 0, 2);
   TR::Node * mulNode = TR::Node::create(TR::imul, 2, subNode, charStrideConst);

   TR::Node * byteIndVarReLoadNode = TR::Node::createLoad(storeAddr, arraycopyLoop.getHighLoadAddress()->getIndVarSymRef());
   TR::Node * addNode = TR::Node::create(TR::iadd, 2, byteIndVarReLoadNode, mulNode);
   TR::Node * byteIndVarReStoreNode = TR::Node::createStore(arraycopyLoop.getHighLoadAddress()->getIndVarSymRef(), addNode);

   TR::TreeTop * byteIndVarReStoreTree = TR::TreeTop::create(comp(), byteIndVarReStoreNode);
   deadCmpChild2->insertAfter(byteIndVarReStoreTree);

   return true;
   }

//The following loop:
//            for (int i=0; i<end; ++i)
//               {
//               byteArrA[byteIndex]    = (byte) ((charArrA[i] & 0xFF00) >> 8);
//               byteArrA[byteIndex+1]  = (byte) (charArrA[i] & 0xFF);
//               byteIndex += 2;
//               }
//generates:
//
// BBStart (block 33) (frequency 0) (is in loop 33)
// ibstore #154[0x019D11B0] Shadow[<array-shadow>]
//   aiadd
//     aload #240[0x015D9D5C] Auto[<temp slot 7>]
//     isub
//       iload #197[0x019D0814] Auto[<auto slot 4>]
//       iconst -16
//  i2b
//     ishr
//       iand
//         c2i
//           isload #156[0x019D0EC0] Shadow[<array-shadow>]
//             aiadd
//              aload #241[0x015D9DA4] Auto[<temp slot 8>]
//               isub
//                 imul
//                   iload #198[0x019D08C4] Auto[<auto slot 5>]
//                   iconst 2
//                 ==>iconst -16 at [0x019D112C]
//         iconst 65280
//       iconst 8
// ibstore #154[0x019D11B0] Shadow[<array-shadow>]
//   aiadd
//     ==>aload at [0x019D0C58]
//     isub
//       ==>iload at [0x019D0C84]
//       iconst -17
//   i2b
//     iand
//       ==>c2i at [0x019D0F28]
//       iconst 255
// istore #197[0x019D0814] Auto[<auto slot 4>]
//   isub
//     ==>iload at [0x019D0C84]
//     iconst -2
// istore #198[0x019D08C4] Auto[<auto slot 5>]
//   isub
//     ==>iload at [0x019D0CDC]
//     iconst -1
// ificmplt --> block 33 BBStart at [0x014A12F4]
//   iload #198[0x019D08C4] Auto[<auto slot 5>]
//   iconst 2024
// BBEnd (block 33)
//
// which we can reduce to an arraycopy (memcopy)
bool
TR_LoopReducer::generateCharToByteArraycopy(TR_InductionVariable * byteIndVar, TR_InductionVariable * charIndVar, TR::Block * loopHeader)
   {
   if (!comp()->cg()->getSupportsReferenceArrayCopy() && !comp()->cg()->getSupportsPrimitiveArrayCopy())
      {
      dumpOptDetails(comp(), "arraycopy not enabled for this platform\n");
      return false;
      }

   int32_t incrA = byteIndVar->getIncr()->getLowInt();
   int32_t incrB = charIndVar->getIncr()->getLowInt();

   if (incrA * incrB != 2) /* e.g. 1 and 2, -1 and -2, 2 and 1, -2 and -1 */
      {
      dumpOptDetails(comp(), "Loop does not have an increment of +/-1 and +/-2, but instead %d and %d - no byte to char arraycopy reduction\n", incrA, incrB);
      return false;
      }

   if (incrA == 1 || incrA == -1)
      {
      /* swap the induction variables around */
      TR_InductionVariable * temp = byteIndVar;
      byteIndVar = charIndVar;
      charIndVar = temp;
      }

   if (loopHeader->getNumberOfRealTreeTops() != 5) // array store low byte, array store high byte, induction variable store, induction variable store, loop comparison
      {
      dumpOptDetails(comp(), "Loop has %d tree tops - no char to byte arraycopy reduction\n", loopHeader->getNumberOfRealTreeTops());
      return false;
      }

   TR::TreeTop * highArrayStoreTree;
   TR::Node * highStoreNode;
   TR::TreeTop * lowArrayStoreTree;
   TR::Node * lowStoreNode;
   TR::TreeTop * nextTreeTop;

   highArrayStoreTree = loopHeader->getFirstRealTreeTop();
   highStoreNode = highArrayStoreTree->getNode();
   lowArrayStoreTree = highArrayStoreTree->getNextTreeTop();
   lowStoreNode = lowArrayStoreTree->getNode();
   nextTreeTop = lowArrayStoreTree->getNextTreeTop();

   TR_CharToByteArraycopy arraycopyLoop(comp(), charIndVar, byteIndVar, comp()->target().cpu.isBigEndian());

   if (!arraycopyLoop.checkArrayStores(highStoreNode, lowStoreNode))
      {
      dumpOptDetails(comp(), "... no match - switch around high and low array store trees and try again...\n");
      lowArrayStoreTree = loopHeader->getFirstRealTreeTop();
      lowStoreNode = lowArrayStoreTree->getNode();
      highArrayStoreTree = lowArrayStoreTree->getNextTreeTop();
      highStoreNode = highArrayStoreTree->getNode();
      nextTreeTop = highArrayStoreTree->getNextTreeTop();

      if (!arraycopyLoop.checkArrayStores(highStoreNode, lowStoreNode))
         {
         return false;
         }
      }

   TR::TreeTop * charIndVarStoreTree = nextTreeTop;
   TR::Node    * charIndVarStoreNode = charIndVarStoreTree->getNode();

   TR::TreeTop * byteIndVarStoreTree = charIndVarStoreTree->getNextTreeTop();
   TR::Node    * byteIndVarStoreNode = byteIndVarStoreTree->getNode();

   TR::TreeTop * lastIndVarStoreTree = byteIndVarStoreTree;

   TR::TreeTop * loopCmpTree = byteIndVarStoreTree->getNextTreeTop();
   TR::Node    * loopCmpNode = loopCmpTree->getNode();

   if (!arraycopyLoop.getLoadAddress()->checkIndVarStore(charIndVarStoreNode))
      {
      bool swap = arraycopyLoop.getHighStoreAddress()->checkIndVarStore(charIndVarStoreNode);
      if (swap && arraycopyLoop.getLoadAddress()->checkIndVarStore(byteIndVarStoreNode))
         {
         dumpOptDetails(comp(), "try swapping the 2 induction variables\n");
         TR::TreeTop * tmpTree = byteIndVarStoreTree;
         TR::Node    * tmpNode = byteIndVarStoreNode;
         byteIndVarStoreTree = charIndVarStoreTree;
         byteIndVarStoreNode = charIndVarStoreNode;
         charIndVarStoreTree = tmpTree;
         charIndVarStoreNode = tmpNode;
         }
      else
         {
         dumpOptDetails(comp(), "Did not encounter byte array induction variable increment - no char to byte arraycopy performed\n");
         return false;
         }
      }
   else if (!arraycopyLoop.getHighStoreAddress()->checkIndVarStore(byteIndVarStoreNode))
      {
      dumpOptDetails(comp(), "Did not encounter char array induction variable increment - no char to byte arraycopy performed\n");
      return false;
      }

   if (!arraycopyLoop.checkLoopCmp(loopCmpNode, charIndVarStoreNode, charIndVar))
      {
      // msf - for now, we require that the comparison is for the character array induction variable.
      // We should make this work if it was comparing the byte array induction variable as an enhancement,
      // e.g.
      // for (byteInd=start; byteInd<end; byteInd+=2)
      //   { ... }
      dumpOptDetails(comp(), "Loop comparison does not match byte or char induction variable - no char to byte arraycopy performed\n");
      return false;
      }

   if (!performTransformation(comp(), "%sReducing char to byte arraycopy %d\n", OPT_DETAILS, loopHeader->getNumber()))
      {
      return false;
      }


   // In addition to the arraycopy we generate, we also will need to generate an update of the
   // second induction variable - this will require a new node.
   // There is no 'final' value as in the case of the primary induction variable, so the final
   // value will have to be calculated.
   // Ignoring the fact that they could have different starting values, the primary induction variable
   // is either twice the value of the secondary induction variable, or vice versa. The final value
   // of the secondary induction variable is thus (primaryEnd - primaryStart) <operator> 2,
   // where <operator> may be multiplication or division.

   //For details on arraycopy call to be generated, see generateArrayCopy. The core tree
   //is repeated here for convenience.
   //Tree-Top
   // ArrayCopy call
   //     ArrayStoreBase tree
   //     ArrayLoadBase tree
   //     number of bytes to copy
   // first induction variable update tree (this tree is new - see note above)
   // second induction variable update tree (this re-uses comparison node from original tree)

   //First, set the increment for the byte array address to 1 so that the code doesn't insert
   //a multiplier in to the base address calculation of the src node on the arraycopy.
   arraycopyLoop.getHighStoreAddress()->setIncrement(1);

   arraycopyLoop.getHighStoreAddress()->updateAiaddSubTree(arraycopyLoop.getHighStoreIndVarNode(), &arraycopyLoop);
   arraycopyLoop.getLoadAddress()->updateAiaddSubTree(arraycopyLoop.getLoadIndVarNode(), &arraycopyLoop);
   TR::Node * charImul = arraycopyLoop.updateIndVarStore(arraycopyLoop.getLoadIndVarNode(), charIndVarStoreNode, arraycopyLoop.getLoadAddress());
   TR::Node * byteImul = arraycopyLoop.updateIndVarStore(arraycopyLoop.getHighStoreIndVarNode(), byteIndVarStoreNode, arraycopyLoop.getHighStoreAddress());
   arraycopyLoop.getHighStoreAddress()->updateMultiply(arraycopyLoop.getHighStoreMultiplyNode());
   arraycopyLoop.getLoadAddress()->updateMultiply(arraycopyLoop.getLoadMultiplyNode());

   TR::Node * loadAddr = arraycopyLoop.getLoadAddress()->getRootNode()->duplicateTree();
   TR::Node * storeAddr= arraycopyLoop.getHighStoreAddress()->getRootNode()->duplicateTree();

   TR::Node * lenNode = byteImul->duplicateTree();
   // need to adjust the multiplier to be 2 instead of 1...
   TR::Node * lenChild = lenNode;
   while (lenChild->getOpCodeValue() != TR::imul)
      {
      lenChild = lenChild->getFirstChild();
      }
   lenChild->setAndIncChild(1, TR::Node::create(lenNode, TR::iconst, 0, 2));

   TR::Node * arraycopy = TR::Node::createArraycopy(loadAddr, storeAddr, lenNode);

   arraycopy->setSymbolReference(comp()->getSymRefTab()->findOrCreateArrayCopySymbol());
   arraycopy->setForwardArrayCopy(true);

   arraycopy->setArrayCopyElementType(TR::Int8);

   TR::Node * top = TR::Node::create(TR::treetop, 1, arraycopy);
   TR::TreeTop * arraycopyTree = TR::TreeTop::create(comp(), top);

   TR::TreeTop* treeList[] = { byteIndVarStoreTree, charIndVarStoreTree, highArrayStoreTree, lowArrayStoreTree, loopCmpTree, NULL };

   TR::TreeTop::removeDeadTrees(comp(), treeList); // make these trees into nops, but leave them around to keep ref counts right

   TR::SymbolReference * charIndVarTemp = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Int32);
   TR::Node * charIndVarTempLoadNode = TR::Node::createLoad(storeAddr, arraycopyLoop.getLoadAddress()->getIndVarSymRef());
   TR::Node * charIndVarTempStoreNode = TR::Node::createStore(charIndVarTemp, charIndVarTempLoadNode);

   TR::TreeTop * charIndVarTempStoreTree = TR::TreeTop::create(comp(), charIndVarTempStoreNode);

   // following line is a bit messy - need to skip over the multiply-by-2, since want an unstrided value for ind var
   TR::Node * charIndVarReStoreNode = TR::Node::createStore(arraycopyLoop.getLoadAddress()->getIndVarSymRef(), charImul->getFirstChild()->duplicateTree());
   TR::TreeTop * charIndVarReStoreTree = TR::TreeTop::create(comp(), charIndVarReStoreNode);

   TR::Node * origTempReLoadNode   = TR::Node::createLoad(storeAddr, charIndVarTemp);
   TR::Node * charIndVarReLoadNode = TR::Node::createLoad(storeAddr, arraycopyLoop.getLoadAddress()->getIndVarSymRef());
   TR::Node * subNode = TR::Node::create(TR::isub, 2, charIndVarReLoadNode, origTempReLoadNode);

   TR::Node * charStrideConst = TR::Node::create(storeAddr, TR::iconst, 0, 2);
   TR::Node * mulNode = TR::Node::create(TR::imul, 2, subNode, charStrideConst);

   TR::Node * byteIndVarReLoadNode = TR::Node::createLoad(storeAddr, arraycopyLoop.getHighStoreAddress()->getIndVarSymRef());
   TR::Node * addNode = TR::Node::create(TR::iadd, 2, byteIndVarReLoadNode, mulNode);
   TR::Node * byteIndVarReStoreNode = TR::Node::createStore(arraycopyLoop.getHighStoreAddress()->getIndVarSymRef(), addNode);

   TR::TreeTop * byteIndVarReStoreTree = TR::TreeTop::create(comp(), byteIndVarReStoreNode);

   loopHeader->getEntry()->insertAfter(arraycopyTree);
   arraycopyTree->insertAfter(charIndVarTempStoreTree);
   charIndVarTempStoreTree->insertAfter(charIndVarReStoreTree);
   charIndVarReStoreTree->insertAfter(byteIndVarReStoreTree);

   return true;
   }


bool
TR_LoopReducer::constrainedIndVar(TR_InductionVariable * indVar)
   {
   if (!indVar)
      {
      return true;
      }

   if (!indVar->getIncr()->asIntConst() && !indVar->getIncr()->asLongConst())
      {
      dumpOptDetails(comp(), "Loop has non-constant induction variable increment\n");
      return false;
      }

   int32_t increment = indVar->getIncr()->getLowInt();
   switch (increment)
      {
      case  1:
      case -1:
      case  2:
      case -2:
      case  4:
      case -4:
      case  8:
      case -8:
         break;

      default:
         dumpOptDetails(comp(), "Loop has constant induction variable other than +/-1/2/4/8\n");
         return false;
      }
   return true;
   }

void
TR_LoopReducer::removeSelfEdge(TR::CFGEdgeList &succList, int32_t selfNumber)
   {
   _cfg->removeEdge(succList, selfNumber, selfNumber);
   }

int
TR_LoopReducer::addBlock(TR::Block * newBlock, TR::Block ** blockList, int numBlocks, const int maxNumBlocks)
   {
   if (numBlocks > maxNumBlocks)
      {
      dumpOptDetails(comp(), "Loop has more than 4 blocks. Punting after block:%d\n", newBlock->getNumber());
      }
   else
      {
      blockList[numBlocks] = newBlock;
      }
   ++numBlocks;
   return numBlocks;
   }

int
TR_LoopReducer::addRegionBlocks(TR_RegionStructure * region, TR::Block ** blockList, int numBlocks, const int maxNumBlocks)
   {
   TR_RegionStructure::Cursor it(*region);
   TR_StructureSubGraphNode * node;
   for (node = it.getFirst(); node != NULL; node = it.getNext())
      {
      TR_BlockStructure * sbs = node->getStructure() ? node->getStructure()->asBlock() : NULL;
      if (sbs)
         {
         numBlocks = addBlock(sbs->getBlock(), blockList, numBlocks, maxNumBlocks);
         }
      else
         {
         //numBlocks = addRegionBlocks(node->getStructure()->asRegion(), blockList, numBlocks, maxNumBlocks);
         dumpOptDetails(comp(), "Nested blocks in loop. No reduction performed\n");
         }
      }
   return numBlocks;
   }

bool
TR_LoopReducer::mayNeedGlobalDeadStoreElimination(TR::Block * storeBlock, TR::Block * compareBlock)
   {
   int32_t storeNum = storeBlock->getNumberOfRealTreeTops();
   int32_t compareNum = compareBlock->getNumberOfRealTreeTops();

   if (storeNum != 3 || compareNum != 3)
      {
      //dumpOptDetails(comp(), "storenum:%d comparenum:%d\n", storeNum, compareNum);
      return false; // for now, just the pattern for arraytranslate is what we are worried about
      }

   const int numBlocks = 2;
   const int numTreeTops = 3; // take advantage of both blocks having 3 trees
   const TR::ILOpCodes pattern[numBlocks][numTreeTops] =
      {
      { TR::istore,  TR::istore, TR::ificmpeq }, { TR::bstorei, TR::istore, TR::ificmplt }
      };
   TR::Block * blocks[numBlocks] =
      {
      storeBlock, compareBlock
      };

   for (int b = 0; b < numBlocks; ++b)
      {
      TR::TreeTop * tree = blocks[b]->getFirstRealTreeTop();
      for (int t = 0; t < numTreeTops; ++t)
         {
         TR::Node * node = tree->getNode();
         if (node->getOpCodeValue() == TR::treetop)
            {
            node = node->getFirstChild();
            }
         if (node->getOpCodeValue() != pattern[b][t])
            {
            //dumpOptDetails(comp(), "fail on tree %d %d %d %d\n", b, t, node->getOpCodeValue(), pattern[b][t]);
            return false;
            }
         tree = tree->getNextTreeTop();
         }
      }

   //dumpOptDetails(comp(), "match tree\n");
   return true;
   }

void
TR_LoopReducer::reduceNaturalLoop(TR_RegionStructure * whileLoop)
   {
   dumpOptDetails(comp(), "Reducer while loop %d\n", whileLoop->getNumber());
   TR_StructureSubGraphNode * entryGraphNode = whileLoop->getEntry();
   if (!entryGraphNode->getStructure()->asBlock())
      {
      dumpOptDetails(comp(), "Header is not a block\n");
      return;
      }
   TR::Block * loopHeader = entryGraphNode->getStructure()->asBlock()->getBlock();

   // Find the basic blocks that represent the loop header and the
   // first block in the loop body.
   //
   TR_ScratchList<TR::Block> blocksInReferencedLoop(trMemory());
   whileLoop->getBlocks(&blocksInReferencedLoop);
   int32_t numBlocks = blocksInReferencedLoop.getSize();

   blocksInReferencedLoop.remove(loopHeader);

   ListIterator<TR::Block> blocksIt(&blocksInReferencedLoop);
   TR::Block * nextBlock;

   if (trace())
      {
      dumpOptDetails(comp(), "Blocks in loop %p,%d ( ", loopHeader, loopHeader->getNumber());
      for (nextBlock = blocksIt.getFirst(); nextBlock; nextBlock = blocksIt.getNext())
         {
         dumpOptDetails(comp(), "%p,%d ", nextBlock, nextBlock->getNumber());
         }
      dumpOptDetails(comp(), ")\n");
      }

   TR_InductionVariable * indVar = whileLoop->getFirstInductionVariable();

   if (indVar == NULL)
      {
      dumpOptDetails(comp(), "Loop has no induction variable\n");
      return;
      }


   TR_InductionVariable * nextIndVar;
   nextIndVar = indVar->getNext();

   if (!constrainedIndVar(indVar) || !constrainedIndVar(nextIndVar))
      {
      dumpOptDetails(comp(), "Induction Variable(s) not constrained\n");
      return;
      }
   int32_t increment = indVar->getIncr()->getLowInt();

   TR::Block * block2 = blocksInReferencedLoop.popHead();
   TR::Block * block3 = blocksInReferencedLoop.popHead();
   TR::Block * block4 = blocksInReferencedLoop.popHead();

   if ((block2 && (loopHeader->getNextBlock() != block2)) ||
       (block2 && block3 && (block2->getNextBlock() != block3)) ||
       (block3 && block4 && (block3->getNextBlock() != block4)))
      {
      dumpOptDetails(comp(), "Blocks are not in succession\n");
      return;
      }

   //
   // note that the blocks are in backwards order (4,3,2) because they are popped from the stack
   //
   if (nextIndVar) /* these reductions require 2 induction variables to be considered */
      {
      if (((numBlocks == 1) && generateByteToCharArraycopy(indVar, nextIndVar, loopHeader)) ||
          ((numBlocks == 1) && generateCharToByteArraycopy(indVar, nextIndVar, loopHeader)))
         {
         removeSelfEdge(loopHeader->getSuccessors(), whileLoop->getNumber());
         }
      else
         {
         dumpOptDetails(comp(), "Multiple Induction Variable loop %d has %d blocks and is not reduced\n", loopHeader->getNumber(), numBlocks);
         }
      }
   else
      {
      if ((numBlocks == 2 /* || (numBlocks == 3 && block3->endsInGoto()) */) && mayNeedGlobalDeadStoreElimination(loopHeader, block2))
         {
         dumpOptDetails(comp(), "Loop matches possible arraytranslate - global deadstore elimination to be performed\n");
         requestOpt(OMR::globalDeadStoreElimination, true);
         requestOpt(OMR::localCSE, true);
         requestOpt(OMR::deadTreesElimination, true);
         requestOpt(OMR::loopReduction, true);
         return;
         }
      if (((numBlocks == 1) && generateArrayset(indVar, loopHeader)) ||
         ((numBlocks == 1) && generateArraycopy(indVar, loopHeader)) ||
          (((numBlocks == 2) /* || (numBlocks == 3 && block3->endsInGoto()) */) && generateArraycmp(whileLoop, indVar, loopHeader, block2)) ||
         ((numBlocks == 1) && generateArraytranslate(whileLoop, indVar, loopHeader, NULL, NULL, NULL)) ||
          (((numBlocks == 2) /* || (numBlocks == 3 && block3->endsInGoto()) */) && generateArraytranslate(whileLoop, indVar, loopHeader, block2, NULL, NULL)) ||
         ((numBlocks == 3) && generateArraytranslate(whileLoop, indVar, loopHeader, block4, block3, NULL)) ||
         ((numBlocks == 4) && generateArraytranslate(whileLoop, indVar, loopHeader, block4, block3, block2)) ||
         ((numBlocks == 2) && generateArraytranslateAndTest(whileLoop, indVar, loopHeader, block2)))
         {
         removeSelfEdge(loopHeader->getSuccessors(), whileLoop->getNumber());
         }
      else
         {
         dumpOptDetails(comp(), "Loop %d has %d blocks and is not reduced\n", loopHeader->getNumber(), numBlocks);
         }
      }
   return;
   }

TR_LoopReducer::TR_LoopReducer(TR::OptimizationManager *manager)
   : TR_LoopTransformer(manager)
   {}

int32_t
TR_LoopReducer::perform()
   {

   // enable only if the new loop reduction framework is
   // disabled
   //
   if (optimizer()->isEnabled(OMR::idiomRecognition))
      {
      dumpOptDetails(comp(), "idiom recognition is enabled, skipping loopReducer\n");
      return 0;
      }

   if (!comp()->cg()->getSupportsArraySet() &&
      !comp()->cg()->getSupportsReferenceArrayCopy() &&
      !comp()->cg()->getSupportsPrimitiveArrayCopy() &&
      !comp()->cg()->getSupportsArrayCmp() &&
      !comp()->cg()->getSupportsArrayCmpLen() &&
      !comp()->cg()->getSupportsArrayTranslateTRxx() &&
      !comp()->cg()->getSupportsArrayTranslateAndTest())
      {
      dumpOptDetails(comp(), "No Loop Reduction Optimizations Enabled for this platform\n");
      return 0;
      }

   if (!comp()->mayHaveLoops())
      {
      dumpOptDetails(comp(), "Method has no loops\n");
      return 0;
      }

   _cfg = comp()->getFlowGraph();
   if (trace())
      {
      traceMsg(comp(), "Starting LoopReducer\n");
      traceMsg(comp(), "\nCFG before loop reduction:\n");
      getDebug()->print(comp()->getOutFile(), _cfg);
      }

   // From this point on, stack memory allocations will die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   TR_ScratchList<TR_Structure> whileLoops(trMemory());

   createWhileLoopsList(&whileLoops);

   ListIterator<TR_Structure> whileLoopsIt(&whileLoops);

   if (whileLoops.isEmpty())
      {
      dumpOptDetails(comp(), "Method has no while loops\n");
      return false;
      }

   TR_Structure * nextWhileLoop;

   for (nextWhileLoop = whileLoopsIt.getFirst(); nextWhileLoop != NULL; nextWhileLoop = whileLoopsIt.getNext())
      {
      // Can be used for debugging; to stop a particular loop from being
      // and see if the problem persists or not.
      //
      // if (_counter == 1)
      //   break;
      // else
      //   _counter++;

      TR_RegionStructure * naturalLoop = nextWhileLoop->asRegion();
      TR_ASSERT(naturalLoop && naturalLoop->isNaturalLoop(), "Loop reducer, expecting natural loop");
      TR::Block *entryBlock = naturalLoop->getEntryBlock();
      if (entryBlock->isCold())
         continue;

      reduceNaturalLoop(naturalLoop);
      }

   // Use/def info and value number info are now bad.
   //
   optimizer()->setUseDefInfo(NULL);
   optimizer()->setValueNumberInfo(NULL);


   if (trace())
      {
      traceMsg(comp(), "\nCFG after loop reduction:\n");
      getDebug()->print(comp()->getOutFile(), _cfg);
      traceMsg(comp(), "Ending LoopReducer\n");
      }

   return 1; // actual cost
   }

const char *
TR_LoopReducer::optDetailString() const throw()
   {
   return "O^O LOOP REDUCER: ";
   }
