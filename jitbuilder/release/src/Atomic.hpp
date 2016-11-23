/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2016, 2016
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
 ******************************************************************************/


#ifndef SIMPLE_INCL
#define SIMPLE_IMPL

#include "ilgen/MethodBuilder.hpp"

class AtomicMethod : public TR::MethodBuilder
   {
   private:
   TR::IlType *pInt32;

   public:
   AtomicMethod(TR::TypeDictionary *);
   virtual bool buildIL();
   };

#endif // !defined(POINTER_INCL)
