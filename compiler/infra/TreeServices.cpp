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

#include "infra/TreeServices.hpp"

#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"

bool
TR_AddressTree::isILLoad(TR::Node * node)
   {
   if (node->getOpCodeValue() != TR::iload  && node->getOpCodeValue() != TR::lload &&
       node->getOpCodeValue() != TR::iloadi && node->getOpCodeValue() != TR::lloadi)
      {
      return false;
      }
   else
      {
      return true;
      }
   }

bool
TR_AddressTree::findComplexAddressGenerationTree(TR::Node *node, vcount_t visitCount, TR::Node *parent)
   {
   if (node->getVisitCount()==visitCount)
      return false;
   node->setVisitCount(visitCount);

   if (isILLoad(node))
      {
      int32_t childNumber;
      for (childNumber = 0; childNumber < parent->getNumChildren(); childNumber++)
         if (parent->getChild(childNumber)==node)
            break;
      _indVarNode.setParentAndChildNumber(parent, childNumber);
      _indexBaseNode.setParentAndChildNumber(node, 0);
      return true;
      }
   else
      return false;
   }


bool
TR_AddressTree::processBaseAndIndex(TR::Node* parent)
   {
   TR::Node * lhs = parent->getFirstChild();
   TR::Node * rhs = parent->getSecondChild();
   bool isValid = true;
   if (isILLoad(lhs) && isILLoad(rhs))
      {
      isValid = false; // exactly one child should be a variable
      }
   else if (isILLoad(lhs))
      {
      _indexBaseNode.setParentAndChildNumber(lhs, 0);
      _indVarNode.setParentAndChildNumber(parent, 0);
      }
   else if (isILLoad(rhs))
      {
      _indexBaseNode.setParentAndChildNumber(rhs, 0);
      _indVarNode.setParentAndChildNumber(parent, 1);
      }
   else
      {
      isValid = false; // one of the children must be variable
      }
   if (isValid)
      return isValid;
   else
      {
         return findComplexAddressGenerationTree(parent, comp()->incVisitCount(), parent);
      }
   }



bool
TR_AddressTree::processMultiplyNode(TR::Node * multiplyNode)
   {

   // (64-bit)
   // for aladds multiplysubTree
   // lmul
   //   i2l
   //     <index>
   //   lconst
   TR::Node * secondMulChild = multiplyNode->getSecondChild();
   TR::ILOpCodes opCodeSecondMulChild = secondMulChild->getOpCodeValue();
   if (opCodeSecondMulChild != TR::iconst && opCodeSecondMulChild != TR::lconst)
      {
      dumpOptDetails(comp(), "AddressTree: second node of multiply is not iconst\n");
      return false;
      }

   if (secondMulChild->getType().isInt64())
      {
      _multiplier = (int32_t) secondMulChild->getLongInt();
      }
   else
      {
      _multiplier = secondMulChild->getInt();
      }

   TR::Node * firstMulChild = multiplyNode->getFirstChild()->skipConversions();

   TR::ILOpCodes opCodeFirstMulChild = firstMulChild->getOpCodeValue();

   if (opCodeFirstMulChild == TR::iadd || opCodeFirstMulChild == TR::ladd)
      {
      if (!processBaseAndIndex(firstMulChild))
         {
         dumpOptDetails(comp(), "AddressTree: first node of multiply is iadd/ladd but children are not ok\n");
         return false;
         }
      }
   else if (isILLoad(firstMulChild))
      {
      _indexBaseNode.setParentAndChildNumber(firstMulChild, 0);
      _indVarNode.setParentAndChildNumber(multiplyNode, 0);
      }
   else
      {
      dumpOptDetails(comp(), "AddressTree: first node of multiply is not iadd/ladd/iload/lload\n");
      return false;
      }

   return true;
   }

//
// process looks for an address generation sub-tree. The tree typically is headed up with an aiadd
// but can also be an iadd or ladd tree in the case of unsafe get/put being used.
//
// aiadd <memory reference>
//    aload <address>
//    isub <index expression>
//       imul
//          iload <index>
//          iconst <stride>
//       iconst <displacement>
//
// -or-
// (32-bit)
// iadd
//    x2i
//       ixload
//          aiadd <memory reference sub-tree>
//    l2i
//       ilload <DirectByteBuffer>
//          aload <Buffer>
//
// (64-bit)
// ladd
//    x2i
//       ixload
//          aladd <memory reference sub-tree>
//    ilload <DirectByteBuffer>
//       aload <Buffer>
bool
TR_AddressTree::process(TR::Node * aiaddNode, bool onlyConsiderConstAiaddSecondChild)
   {
   TR::Node * multiplySubTree = NULL;
   bool validAiaddSubTree = false;

   _offset = 0;
   _rootNode = aiaddNode;

   TR::ILOpCodes opCodeaiAdd = aiaddNode->getOpCodeValue();

   if (opCodeaiAdd != TR::aiadd && opCodeaiAdd != TR::aladd)
      {
      dumpOptDetails(comp(), "AddressTree: Can not construct an address tree without an address node\n");
      return false;
      }

   // Use 'UnconvertedChild' instead of 'Child' below to skip over potential l2i's that can be strewn about for
   // 64-bit sign extension work.
   TR::Node * aiaddFirstChild = aiaddNode->getFirstChild()->skipConversions();
   TR::ILOpCodes opCodeAiaddFirstChild = aiaddFirstChild->getOpCodeValue();
   TR::Node * aiaddSecondChild = aiaddNode->getSecondChild()->skipConversions();
   TR::ILOpCodes opCodeAiaddSecondChild = aiaddSecondChild->getOpCodeValue();

   if (opCodeAiaddFirstChild == TR::aload || opCodeAiaddFirstChild == TR::aloadi)
      {
      _baseVarNode.setParentAndChildNumber(aiaddNode, 0);
      if ((opCodeAiaddSecondChild == TR::isub || opCodeAiaddSecondChild == TR::lsub ||
           opCodeAiaddSecondChild == TR::iadd || opCodeAiaddSecondChild == TR::ladd) &&
         !onlyConsiderConstAiaddSecondChild)
         {
         TR::Node * isubFirstChild = aiaddSecondChild->getFirstChild()->skipConversions();
         TR::ILOpCodes opCodeIsubFirstChild = isubFirstChild->getOpCodeValue();
         TR::Node * isubSecondChild = aiaddSecondChild->getSecondChild()->skipConversions();
         TR::ILOpCodes opCodeIsubSecondChild = isubSecondChild->getOpCodeValue();

         // (64-bit, for 32-bit, change the l to i in the opcodes below and remove the i2l...
         // aladd
         //   aload
         //   lsub
         //     lmul
         //       i2l
         //         <index>
         //       lconst
         //     lconst
         if (opCodeIsubSecondChild != TR::iconst && opCodeIsubSecondChild != TR::lconst)
            {
            dumpOptDetails(comp(), "AddressTree: i(l)sub second child is not constant\n");
            }
         else
            {
            if (opCodeIsubFirstChild == TR::imul || opCodeIsubFirstChild == TR::lmul)
               {
               multiplySubTree = isubFirstChild;
               TR::Node * indSymFromSubTree = isubFirstChild->getFirstChild()->skipConversions();

               _multiplyNode.setParentAndChildNumber(aiaddSecondChild, 0);
               if (indSymFromSubTree->getOpCodeValue() == TR::iload || indSymFromSubTree->getOpCodeValue() == TR::lload)
                  {
                  validAiaddSubTree = true;
                  _indVarNode.setParentAndChildNumber(isubFirstChild, 0);
                  _indexBaseNode.setParentAndChildNumber(indSymFromSubTree, 0);
                  }
               else if (indSymFromSubTree->getOpCodeValue() == TR::iadd || indSymFromSubTree->getOpCodeValue() == TR::ladd)
                  {
                  // the following check is safe because there should only be one induction
                  // variable active for loops we consider, but it does however require that
                  // the induction variable be the first child, so we will catch a case of:
                  //   a[indVar + j]
                  // but not:
                  //   a[j + indVar]
                  if (
                      (indSymFromSubTree->getFirstChild()->getOpCodeValue() == TR::iload || indSymFromSubTree->getFirstChild()->getOpCodeValue() == TR::lload)
                      &&
                      (indSymFromSubTree->getSecondChild()->getOpCodeValue() == TR::iload || indSymFromSubTree->getSecondChild()->getOpCodeValue() == TR::lload ||
                       indSymFromSubTree->getSecondChild()->getOpCodeValue() == TR::iconst || indSymFromSubTree->getSecondChild()->getOpCodeValue() == TR::lconst)
                     )
                     {
                     validAiaddSubTree = true;
                     _indVarNode.setParentAndChildNumber(indSymFromSubTree, 0);
                     _indexBaseNode.setParentAndChildNumber(indSymFromSubTree->getFirstChild(), 0);
                     }
                  }
               }
            else if (isILLoad(isubFirstChild))
               {
               _multiplyNode.setParentAndChildNumber(aiaddSecondChild, 0);
               _indVarNode.setParentAndChildNumber(aiaddSecondChild, 0);
               _indexBaseNode.setParentAndChildNumber(isubFirstChild, 0);
               validAiaddSubTree = true;
               }
            else if (opCodeIsubFirstChild == TR::iadd || opCodeIsubFirstChild == TR::ladd)
               {
               _multiplyNode.setParentAndChildNumber(aiaddSecondChild, 0);
               validAiaddSubTree = processBaseAndIndex(isubFirstChild);
               }
            else
               {
               validAiaddSubTree = findComplexAddressGenerationTree(isubFirstChild, comp()->incVisitCount(),  aiaddSecondChild);
               if (validAiaddSubTree)
                  _multiplyNode.setParentAndChildNumber(aiaddSecondChild, 0);
               else
                  dumpOptDetails(comp(), "AddressTree: i(l)sub children are not i(l)mul or i(l)const\n");
               }

            if (opCodeIsubSecondChild == TR::iconst)
               {
               _offset = (isubSecondChild->getInt());
               }
            else
               {
               _offset = (isubSecondChild->getLongInt());
               }
            if(opCodeAiaddSecondChild == TR::isub || opCodeAiaddSecondChild == TR::lsub)
               {
               _offset = -_offset;
               }
            }
         }
      else if (opCodeAiaddSecondChild == TR::iconst || opCodeAiaddSecondChild == TR::lconst)
         {
         validAiaddSubTree = true;
         if (opCodeAiaddSecondChild == TR::iconst)
            {
            _offset = (aiaddSecondChild->getInt());
            }
         else
            {
            _offset = (aiaddSecondChild->getLongInt());
            }
         }
      else if ((opCodeAiaddSecondChild == TR::imul || opCodeAiaddSecondChild == TR::lmul) &&
               !onlyConsiderConstAiaddSecondChild)
         {
         validAiaddSubTree = true;
         multiplySubTree = aiaddSecondChild;
         }
      else if (opCodeAiaddSecondChild == TR::iload)
         {
         validAiaddSubTree = true;
         _offset = 0;
         _indexBaseNode.setParentAndChildNumber(aiaddSecondChild, 0);
         }
      else
         {
         dumpOptDetails(comp(), "AddressTree: second child of aiadd/aladd is not iload/i(l)sub/i(l)mul\n");
         }
      }
   else
      {
      dumpOptDetails(comp(), "AddressTree: first child of aiadd/aladd is not aiload\n");
      }

   if (validAiaddSubTree && (multiplySubTree != NULL))
      {
      validAiaddSubTree = processMultiplyNode(multiplySubTree);
      }

   return validAiaddSubTree;
   }

void TR_Pattern::tracePattern(TR::Node *node) {
   traceMsg(TR::comp(), "{ Trying %s pattern on %s n%dn\n", getName(), node->getOpCode().getName(), node->getGlobalIndex());
}

void TR_OpCodePattern::tracePattern(TR::Node *node) {
   traceMsg(TR::comp(), "{ Trying %s [%s] pattern on %s n%dn\n", getName(), TR::ILOpCode(_opCode).getName(), node->getOpCode().getName(), node->getGlobalIndex());
}

bool TR_Pattern::matches(TR::Node *node, TR_Unification &uni, TR::Compilation *comp)
   {
   // Note: ideally, if TR_DisableTreePatternMatching is set, we shouldn't even
   // have wasted memory on TR_Pattern objects in the first place.  Users should
   // guard their pattern builder code with a test of TR_DisableTreePatternMatching.
   // However, just in case they forgot, the following test at least ensures that
   // the patterns have no effect on generated code.
   //
   if (comp->getOption(TR_DisableTreePatternMatching))
      return false;

   // Attempts to match this pattern as well as _next.
   // On failure, uni is restored to its original state.

   if (comp->getOption(TR_TraceTreePatternMatching))
      tracePattern(node);

   bool result = false;
   TR_Unification::TR_Mark mark = uni.mark();
   if (thisMatches(node, uni, comp))
      result = (!_next || _next->matches(node, uni, comp));
   else
      uni.undoTo(mark);

   if (comp->getOption(TR_TraceTreePatternMatching))
      traceMsg(comp, "} result: %s\n", result? "true":"false");

   return result;
   }

bool TR_UnifyPattern::thisMatches(TR::Node *node, TR_Unification &uni, TR::Compilation *comp)
   {
   if (comp->getOption(TR_TraceTreePatternMatching))
      {
      traceMsg(comp, "Unify %d with %s in state ", _index, comp->getDebug()->getName(node));
      uni.dump(comp);
      traceMsg(comp, "\n");
      }

   if (uni.node(_index))
      {
      // Already unified; match succeeds only if it's the same node again
      return node == uni.node(_index);
      }
   else
      {
      uni.add(_index, node);
      return true;
      }
   }

bool TR_CommutativePattern::thisMatches(TR::Node *node, TR_Unification &uni, TR::Compilation *comp)
   {
   if (node->getNumChildren() < 2)
      return false;

   TR_Unification::TR_Mark mark = uni.mark();
   if (_leftPattern->matches(node->getFirstChild(), uni, comp) && _rightPattern->matches(node->getSecondChild(), uni, comp))
      {
      return true;
      }
   else
      {
      // If the left one matched and the right one failed, then we need to undo the unifications from the left one
      uni.undoTo(mark);

      // Now try again with the children swapped
      return (_leftPattern->matches(node->getSecondChild(), uni, comp) && _rightPattern->matches(node->getFirstChild(), uni, comp));
      }
   }
