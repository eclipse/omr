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

#include <stddef.h>
#include "optimizer/DataFlowAnalysis.hpp"


class TR_BitVector;

// #define MAX_BLOCKS_FOR_STACK_ALLOCATION 128

// This file defines the methods in class UnionBitVectorAnalysis.
//
//
template<class Container>void TR_UnionDFSetAnalysis<Container *>::compose(Container *firstBitVector, Container *secondBitVector)
   {
   *firstBitVector |= *secondBitVector;
   }

template<class Container>void TR_UnionDFSetAnalysis<Container *>::inverseCompose(Container *firstBitVector, Container *secondBitVector)
   {
   *firstBitVector &= *secondBitVector;
   }

template<class Container>void TR_UnionDFSetAnalysis<Container *>::initializeInSetInfo()
   {
   this->_currentInSetInfo->empty();
   }

template<class Container>void TR_UnionDFSetAnalysis<Container *>::initializeCurrentGenKillSetInfo()
   {
   this->_currentRegularGenSetInfo->empty();
   this->_currentRegularKillSetInfo->setAll(this->_numberOfBits);
   }

template<class Container>Container * TR_UnionDFSetAnalysis<Container *>::initializeInfo(Container *info)
   {
   Container *result = info;
   if (result == NULL)
      this->allocateContainer(&result, true);
   else
      result->empty();
   return result;
   }

template<class Container>Container * TR_UnionDFSetAnalysis<Container *>::inverseInitializeInfo(Container *info)
   {
   Container *result = info;
   if (result == NULL)
      this->allocateContainer(&result, false);
   result->setAll(this->_numberOfBits);
   return result;
   }


template<class Container>TR_DataFlowAnalysis::Kind TR_UnionDFSetAnalysis<Container *>::getKind()
   {
   return TR_DataFlowAnalysis::UnionDFSetAnalysis;
   }


template class TR_UnionDFSetAnalysis<TR_BitVector *>;
template class TR_UnionDFSetAnalysis<TR_SingleBitContainer *>;
