/*******************************************************************************
* Copyright (c) 2017, 2019 IBM Corp. and others
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

#ifndef OMRRECOGNIZEDCALLTRANSFORMER_INCL
#define OMRRECOGNIZEDCALLTRANSFORMER_INCL

#include <stdint.h>
#include "il/Node.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"

namespace OMR
{

class RecognizedCallTransformer : public TR::Optimization
   {
   public:

   RecognizedCallTransformer(TR::OptimizationManager* manager)
      : TR::Optimization(manager)
      {}
   static TR::Optimization* create(TR::OptimizationManager* manager);
   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   protected:

   /** \brief
    *     Check if a treetop is inlineable. The treetop must have one sole child TR::call
    *
    *  \param treetop
    *     The treetop which anchors the call node.
    *
    */
   virtual bool isInlineable(TR::TreeTop* treetop);

   /** \brief
    *     Perform transformation on the treetop. The treetop must have one sole child TR::call
    *
    *  \param treetop
    *     The treetop which anchors the call node.
    *
    */
   virtual void transform(TR::TreeTop* treetop);
   };

}
#endif
