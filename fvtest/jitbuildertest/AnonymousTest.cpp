/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2017, 2017
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

#include "gtest/gtest.h" 
#include "Jit.hpp"
#include "JBTestUtil.hpp"

DECLARE_BUILDER(Anonymous);
DEFINE_BUILDER_CTOR(Anonymous)
   {
   // This builder intentionally has no name, line, nor file
   // as we are testing the creation of 'anonymous' builders. 
   DefineReturnType(types->toIlType<uint32_t>());
   }

DEFINE_BUILDIL(Anonymous)
   {
   Return(ConstInt32(1024u)); 
   return true;
   }

typedef uint32_t (*AnonFunc)();

class AnonymousTest : public JitBuilderTest {};

TEST_F(AnonymousTest, AnonymousTest) 
   {
   AnonFunc func; 
   ASSERT_COMPILE(TR::TypeDictionary, Anonymous, func); 

   ASSERT_EQ( func(), 1024u); 
   }


