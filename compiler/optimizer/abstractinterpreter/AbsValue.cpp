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

#include "optimizer/abstractinterpreter/AbsValue.hpp"

TR::AbsValue* TR::AbsVPValue::clone(TR::Region& region) const
   {
   TR::AbsVPValue* copy = new (region) TR::AbsVPValue(_vp, _constraint, _dataType, _paramPos);
   return copy;
   }

TR::AbsValue* TR::AbsVPValue::merge(const TR::AbsValue *other)
   {
   if (other == NULL)
      return this;

   if (_paramPos != other->getParameterPosition())
      _paramPos = -1;

   if (other->getDataType() != _dataType)
      {
      _dataType = TR::NoType;
      setToTop();
      return this;
      }

   if (isTop())
      return this;

   if (other->isTop())
      {
      setToTop();
      return this;
      }

   TR::VPConstraint *mergedConstraint = _constraint->merge(static_cast<const TR::AbsVPValue*>(other)->getConstraint(), _vp);

   _constraint = mergedConstraint;
   return this;
   }

void TR::AbsVPValue::print(TR::Compilation* comp) const
   {
   traceMsg(comp, "AbsValue: Type: %s ", TR::DataType::getName(_dataType));

   if (_constraint)
      {
      traceMsg(comp, "Constraint: ");
      _constraint->print(_vp);
      }
   else
      {
      traceMsg(comp, "TOP (unknown) ");
      }

   traceMsg(comp, " param position: %d", _paramPos);
   }
