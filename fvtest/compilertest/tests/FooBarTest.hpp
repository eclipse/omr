/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "TestDriver.hpp"

namespace TestCompiler
{

typedef int32_t (FooMethodType)(int32_t);
typedef int32_t (BarMethodType)(int32_t);

class FooBarTest : public TestDriver
   {
   public:
   static int32_t          dataArraySize() { return _dataArraySize; }
   static int32_t        * dataArray()     { return _dataArray; }
   static TR::ResolvedMethod * barMethod() { return _barCompilee; }

   protected:
   virtual void compileTestMethods();
   virtual void invokeTests();

   private:
   const static int32_t _dataArraySize = 100;
   static int32_t _dataArray[100];

   static FooMethodType *_foo;
   static BarMethodType *_bar;

   static TR::ResolvedMethod *_barCompilee;

   // straight C(++) implementations of foo and bar for initial test validation purposes
   static int32_t foo(int32_t);
   static int32_t bar(int32_t);
   };

} // namespace TestCompiler
