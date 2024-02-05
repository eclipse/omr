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

#ifndef ABS_OP_ARRAY
#define ABS_OP_ARRAY

#include "env/Region.hpp"
#include "optimizer/abstractinterpreter/AbsValue.hpp"
#include "infra/vector.hpp"

namespace TR {

/**
.* Abstract representation of the operand array.
 */
class AbsOpArray
   {
   public:

   AbsOpArray(uint32_t maxArraySize, TR::Region& region) :
         _container(maxArraySize, NULL, region)
      {}

   /**
    * @brief Clone the operand array
    *
    * @param region The memory region where the cloned operand array should be allocated.
    * @return the cloned operand array
    */
   TR::AbsOpArray *clone(TR::Region& region) const;

   /**
    * @brief Perform an in-place merge with another operand array.
    * The merge operation does not modify the state of another state
    * or store any references of abstract values from another state to be merged with
    *
    * @param other The operand array to be merged with.
    */
   void merge(const TR::AbsOpArray* other, TR::Region& region);

   /**
    * @brief Get the abstract value at index i.
    *
    * @param i the array index
    * @return the abstract value
    */
   TR::AbsValue *at(uint32_t i) const;

   /**
    * @brief Set the abstract value at index i.
    *
    * @param i the array index
    * @param value the abstract value to be set
    */
   void set(uint32_t i, TR::AbsValue* value);

   size_t size() const { return _container.size(); }
   void print(TR::Compilation* comp) const;

   private:
   TR::vector<TR::AbsValue*, TR::Region&> _container;
   };

}
#endif
