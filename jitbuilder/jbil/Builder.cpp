/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#include <string>
#include <vector>
#include "Builder.hpp"
#include "FunctionBuilder.hpp"
#include "TypeDictionary.hpp"
#include "Value.hpp"

using namespace OMR::JitBuilder;

Builder::Builder(Builder * parent, FunctionBuilder * fb, TypeDictionary *types)
   : BuilderBase(parent, fb, types)

   // New initialization (must be duplicated for all constructors)
   // BEGIN {
   //

   //
   // } END
   // New initialization

   {
   // New constructor (must be duplicated for all constructors)
   // BEGIN {
   //

   //
   // } END
   // New constructor code
   }

Builder::Builder(Builder * parent, TypeDictionary *types)
   : BuilderBase(parent, types)

   // New initialization (must be duplicated for all constructors)
   // BEGIN {
   //

   //
   // } END
   // New initialization

   {
   // New constructor (must be duplicated for all constructors)
   // BEGIN {
   //

   //
   // } END
   // New constructor code
   }

Builder *
Builder::OrphanBuilder()
   {
   Builder * orphan = new Builder(_fb, _fb->dict());
   _fb->registerObject(orphan);
   orphan->setLocation(location());
   return orphan;
   }

// Add new Builder class code below
// BEGIN {
//

//
// } END
// Add new Builder class code below
