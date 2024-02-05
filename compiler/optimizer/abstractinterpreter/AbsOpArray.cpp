/*******************************************************************************
 * Copyright IBM Corp. and others 2020
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

#include "optimizer/abstractinterpreter/AbsOpArray.hpp"

TR::AbsOpArray* TR::AbsOpArray::clone(TR::Region& region) const
   {
   TR::AbsOpArray* copy = new (region) TR::AbsOpArray(static_cast<uint32_t>(_container.size()), region);
   for (auto i = 0; i < _container.size(); i ++)
      {
      copy->_container[i] = _container[i] ? _container[i]->clone(region) : NULL;
      }
   return copy;
   }

void TR::AbsOpArray::merge(const TR::AbsOpArray* other, TR::Region& region)
   {
   TR_ASSERT_FATAL(other->size() == size(), "Op Array Size not equal! other:%d vs self:%d\n", other->size(), size());

   for (auto i = 0; i < size(); i++)
      {
      TR::AbsValue *selfValue = at(i);
      TR::AbsValue *otherValue = other->at(i);

      if (!selfValue && !otherValue)
         {
         continue;
         }
      else if (selfValue && otherValue)
         {
         TR::AbsValue* mergedVal = selfValue->merge(otherValue);
         set(i, mergedVal);
         }
      else if (selfValue)
         {
         set(i, selfValue);
         }
      else
         {
         set(i, otherValue->clone(region));
         }
      }
   }

void TR::AbsOpArray::set(uint32_t index, TR::AbsValue *value)
   {
   TR_ASSERT_FATAL(index < size(), "Index out of range! Max array size: %d, Index: %d\n", size(), index);
   _container[index] = value;
   }

TR::AbsValue* TR::AbsOpArray::at(uint32_t index) const
   {
   TR_ASSERT_FATAL(index < size(), "Index out of range! Max array size: %d, Index: %d\n", size(), index);
   return _container[index];
   }

void TR::AbsOpArray::print(TR::Compilation* comp) const
   {
   traceMsg(comp, "Contents of Abstract Local Variable Array:\n");
   for (auto i = 0; i < size(); i++)
      {
      traceMsg(comp, "A[%d] = ", i);
      if (!at(i))
         traceMsg(comp, "Uninitialized");
      else
         at(i)->print(comp);

      traceMsg(comp, "\n");
      }
   traceMsg(comp, "\n");
   }
