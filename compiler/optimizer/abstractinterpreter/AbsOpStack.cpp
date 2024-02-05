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

#include "optimizer/abstractinterpreter/AbsOpStack.hpp"

TR::AbsOpStack* TR::AbsOpStack::clone(TR::Region &region) const
   {
   TR::AbsOpStack* copy = new (region) TR::AbsOpStack(region);
   for (size_t i = 0; i < _container.size(); i ++)
      {
      copy->_container.push_back(_container[i] ? _container[i]->clone(region) : NULL);
      }
   return copy;
   }

TR::AbsValue* TR::AbsOpStack::pop()
   {
   TR_ASSERT_FATAL(size() > 0, "Pop an empty stack!");
   TR::AbsValue *value = _container.back();
   _container.pop_back();
   return value;
   }

void TR::AbsOpStack::merge(const TR::AbsOpStack* other, TR::Region& region)
   {
   TR_ASSERT_FATAL(other->_container.size() == _container.size(), "Stacks have different sizes! other: %d vs self: %d", other->_container.size(), _container.size());

   for (size_t i = 0; i < _container.size(); i ++)
      {
      if (_container[i] == NULL)
         _container[i] = other->_container[i]->clone(region);
      else
         _container[i]->merge(other->_container[i]);
      }
   }

void TR::AbsOpStack::print(TR::Compilation* comp) const
   {
   traceMsg(comp, "Contents of Abstract Operand Stack:\n");

   const size_t stackSize = size();

   if (stackSize == 0)
      {
      traceMsg(comp, "<empty>\n\n");
      return;
      }

   traceMsg(comp, "<top>\n");

   for (size_t i = 0; i < stackSize; i++)
      {
      TR::AbsValue *value = _container[stackSize - i -1 ];
      traceMsg(comp, "S[%d] = ", stackSize - i - 1);
      if (value)
         value->print(comp);
      else
         traceMsg(comp, "Uninitialized");
      traceMsg(comp, "\n");
      }

   traceMsg(comp, "<bottom>\n\n");
   }
