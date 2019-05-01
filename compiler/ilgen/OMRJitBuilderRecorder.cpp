/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#include "infra/Assert.hpp"
#include "ilgen/JitBuilderRecorder.hpp"

namespace TR { class MethodBuilder; }

OMR::JitBuilderRecorder::JitBuilderRecorder(const TR::MethodBuilder *mb, const char *fileName)
: _mb(mb), _nextID(0), _idSize(8),  _file(fileName, std::fstream::out | std::fstream::trunc)
   {
   start(); // initializes IDs 0 and 1 (reserved)
   }

OMR::JitBuilderRecorder::~JitBuilderRecorder()
   {
   }

void
OMR::JitBuilderRecorder::start()
   {
   // special reserved value, must do it first !
   StoreID(0);

   // another special reserved value, so do it first
   StoreID((const void *)1);

   String(StatementName::RECORDER_SIGNATURE);
   Number(StatementName::VERSION_MAJOR);
   Number(StatementName::VERSION_MINOR);
   Number(StatementName::VERSION_PATCH);
   EndStatement();
   }

void 
OMR::JitBuilderRecorder::Close()                                       
   { 
   end();
   EndStatement();
   }

OMR::JitBuilderRecorder::TypeID
OMR::JitBuilderRecorder::getNewID()
   {
   // support for variable sized ID encoding
   //  to avoid any synchronization issues in how decoders/encoders count IDs, use a statement to signal change
   // check if we're close to 8-bit / 16-bit boundary
   // need to leave space for one ID because these statements will not have been defined yet
   //  and defining the statement needs an ID!
   if (_nextID == (1 << 8) - 2)
      {
      _idSize = 16;
      Statement(StatementName::STATEMENT_ID16BIT);
      }
   else if (_nextID == (1 << 16) - 2)
      {
      _idSize = 32;
      Statement(StatementName::STATEMENT_ID32BIT);
      }

   return _nextID++;
   }

OMR::JitBuilderRecorder::TypeID
OMR::JitBuilderRecorder::myID()
   {
   return lookupID(_mb);
   }

bool
OMR::JitBuilderRecorder::knownID(const void *ptr)
   {
   TypeMapID::iterator it = _idMap.find(ptr);
   return (it != _idMap.end());
   }

OMR::JitBuilderRecorder::TypeID
OMR::JitBuilderRecorder::lookupID(const void *ptr)
   {
   TypeMapID::iterator it = _idMap.find(ptr);
   if (it == _idMap.end())
      TR_ASSERT_FATAL(0, "Attempt to lookup a builder object that has not yet been created");

   TypeID id = it->second;
   return id;
   }

void
OMR::JitBuilderRecorder::ensureStatementDefined(const char *s)
   {
   if (knownID(s))
      return;

   StoreID(s);
   Builder(0);
   Statement(s);
   String(s);
   EndStatement();
   }

void
OMR::JitBuilderRecorder::end()
   {
   ID(1); // reserved ID to indicate end of file
   String(StatementName::JBIL_COMPLETE);
   }

void
OMR::JitBuilderRecorder::BeginStatement(const char *s)
   {
   BeginStatement(_mb, s);
   }

void
OMR::JitBuilderRecorder::BeginStatement(const TR::MethodBuilder *b, const char *s)
   {
   ensureStatementDefined(s);
   Builder(b);
   Statement(s);
   }

void
OMR::JitBuilderRecorder::StoreID(const void *ptr)
   {
   TypeMapID::iterator it = _idMap.find(ptr);
   if (it != _idMap.end())
      TR_ASSERT_FATAL(0, "Unexpected ID already defined for address %p", ptr);

   _idMap.insert(std::make_pair(ptr, getNewID()));
   }

bool
OMR::JitBuilderRecorder::EnsureAvailableID(const void *ptr)
   {
   if (knownID(ptr)) // already available, so just return
      return true;

   StoreID(ptr);
   return false; // ID was not available, but is now
   }
