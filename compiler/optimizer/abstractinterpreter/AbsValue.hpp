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

#ifndef ABS_VALUE_INCL
#define ABS_VALUE_INCL

#include "il/OMRDataTypes.hpp"
#include "compile/Compilation.hpp"
#include "optimizer/VPConstraint.hpp"
#include "optimizer/ValuePropagation.hpp"

namespace TR {

/**
 * AbsValue is the abstract representation of a 'value'.
 * It is the basic unit used to perform abstract interpretation.
 * This class is an abstract class.
 */
class AbsValue
   {
   public:
   explicit AbsValue(TR::DataType dataType) :
         _dataType(dataType),
         _paramPos(-1)
   {}

   /**
    * @brief Clone an abstract value
    *
    * @param region The region where the cloned value will be allocated on.
    * @return the cloned abstract value
    */
   virtual TR::AbsValue* clone(TR::Region& region) const =0;

   /**
    * @brief Merge with another AbsValue.
    * @note This is an in-place merge. Self should modify other.
    * Also, self should not store any mutable references from other during the merge
    * but immutable references are allowed.
    *
    *
    * @param other Another AbsValue to be merged with
    * @return Self after the merge
    */
   virtual TR::AbsValue* merge(const TR::AbsValue *other)=0;

   /**
    * @brief Check whether the AbsValue is least precise abstract value.
    * @note Top denotes the least precise representation in lattice theory.
    *
    * @return true if it is top. false otherwise
    */
   virtual bool isTop() const =0;

   /**
    * @brief Set to the least precise abstract value.
    */
   virtual void setToTop()=0;

   /**
    * @brief Check if the AbsValue is a parameter.
    *
    * @return true if it is a parameter. false if not.
    */
   bool isParameter() const { return _paramPos >= 0; }

   int32_t getParameterPosition() const { return _paramPos; }
   void setParameterPosition(int32_t paramPos) { _paramPos = paramPos; }

   TR::DataType getDataType() const { return _dataType; }

   virtual void print(TR::Compilation* comp) const = 0;

   protected:

   AbsValue(TR::DataType dataType, int32_t paramPos) :
         _dataType(dataType),
         _paramPos(paramPos)
      {}

   int32_t _paramPos;
   TR::DataType _dataType;
   };

/**
 * An AbsValue which uses VPConstraint as the constraint.
 */
class AbsVPValue : public AbsValue
   {
   public:
   AbsVPValue(TR::ValuePropagation*vp, TR::VPConstraint* constraint, TR::DataType dataType) :
         AbsValue(dataType),
         _vp(vp),
         _constraint(constraint)
      {}

   TR::VPConstraint* getConstraint() const { return _constraint; }

   virtual bool isTop() const { return _constraint == NULL; }
   virtual void setToTop() { _constraint = NULL; }

   virtual TR::AbsValue* clone(TR::Region& region) const;
   virtual TR::AbsValue* merge(const TR::AbsValue* other);
   virtual void print(TR::Compilation* comp) const;

   private:
   AbsVPValue(TR::ValuePropagation*vp, TR::VPConstraint* constraint, TR::DataType dataType, int32_t paramPos) :
         AbsValue(dataType, paramPos),
         _vp(vp),
         _constraint(constraint)
      {}

   TR::ValuePropagation* _vp;
   TR::VPConstraint* _constraint;
   };

}

#endif
