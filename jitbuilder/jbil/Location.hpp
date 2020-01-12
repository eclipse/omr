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

#ifndef LOCATION_INCL
#define LOCATION_INCL

#include "stdint.h"

namespace OMR
{

namespace JitBuilder
{

class FunctionBuilder;

class Location : Object
   {
public:
   static Location * create(FunctionBuilder * fb)
      {
      return new Location(fb);
      }
   static Location * create(FunctionBuilder * fb, std::string lineNumber)
      {
      return new Location(fb, lineNumber);
      }
   static Location * create(FunctionBuilder * fb, std::string lineNumber, int32_t bcIndex)
      {
      return new Location(fb, lineNumber, bcIndex);
      }

   virtual size_t size()          { return sizeof(Location); }
   uint64_t id() const            { return _id; }
   int32_t bcIndex() const        { return _bcIndex; }
   std::string fileName() const   { return _fileName; }
   std::string lineNumber() const { return _lineNumber; }

protected:
   Location(FunctionBuilder * fb);
   Location(FunctionBuilder * fb, std::string lineNumber);
   Location(FunctionBuilder * fb, std::string lineNumber, int32_t bcIndex);

   int64_t           _id;
   FunctionBuilder * _fb;
   int32_t           _bcIndex;
   std::string       _fileName;
   std::string       _lineNumber;

   static int64_t globalIndex;
   };

} // namespace JitBuilder

} // namespace OMR

#endif // defined(LOCATION_INCL)
