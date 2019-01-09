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

 #include <stdint.h>
 #include <iostream>
 #include <fstream>

 #include "infra/Assert.hpp"
 #include "ilgen/JitBuilderReplayTextFile.hpp"
 #include "ilgen/MethodBuilder.hpp"
 #include "ilgen/JitBuilderReplay.hpp"
 #include "ilgen/TypeDictionary.hpp"
 #include "ilgen/IlBuilder.hpp"

 OMR::JitBuilderReplayTextFile::JitBuilderReplayTextFile(const char *fileName)
    : TR::JitBuilderReplay(fileName)
    {
    }

void
OMR::JitBuilderReplayTextFile::ConsumeStart()
   {
   // Expected Form: "strLen [RecordSignature]" VERSION_MAJOR VERSION_MINOR VERSION_PATCH
   // e.g. "4 [JBIL]" 0 0 0 

   const char * const recordSignature = ConsumeString(); // Consumes first 2 tokens 

   int16_t version_major;
   int16_t version_minor;
   int16_t version_patch;

   if (!(_file >> version_major));
      TR_ASSERT_FATAL(0, "Unable to read file (version_major) at consumeStart.");
   if (!(_file >> version_minor));
      TR_ASSERT_FATAL(0, "Unable to read file (version_minor) at consumeStart.");
   if (!(_file >> version_patch));
      TR_ASSERT_FATAL(0, "Unable to read file (version_patch) at consumeStart.");

   }

int8_t 
OMR::JitBuilderReplayTextFile::Consume8bitNumber()
   {
   // Expected Form: ##
   // e.g. 71

   int32_t num; // Recorder records as int32_t
   if (!(_file >> num));
      TR_ASSERT_FATAL(0, "Unable to read file at consume8bitNumber.");
  
   return (int8_t)num;
   }

int16_t 
OMR::JitBuilderReplayTextFile::Consume16bitNumber()
   {
   // Expected Form: ###
   // e.g. 591

   int16_t num;
   if (!(_file >> num));
      TR_ASSERT_FATAL(0, "Unable to read file at consume16bitNumber.");
  
   return num;
   }

int32_t 
OMR::JitBuilderReplayTextFile::Consume32bitNumber()
   {
   // Expected Form: ####
   // e.g. 539

   int32_t num;
   if (!(_file >> num));
      TR_ASSERT_FATAL(0, "Unable to read file at consume32bitNumber.");
  
   return num;
   }

int64_t 
OMR::JitBuilderReplayTextFile::Consume64bitNumber()
   {
   // Expected Form: #####
   // e.g. 6234

   int64_t num;
   if (!(_file >> num));
      TR_ASSERT_FATAL(0, "Unable to read file at consume64bitNumber.");
  
   return num;
   }

float 
OMR::JitBuilderReplayTextFile::ConsumeFloatNumber()
   {
   // Expected Form: ##.##
   // e.g. 34.2

   float num;
   if (!(_file >> num));
      TR_ASSERT_FATAL(0, "Unable to read file at consumeFloatNumber.");
  
   return num;
   }

double 
OMR::JitBuilderReplayTextFile::ConsumeDoubleNumber()
   {
   // Expected Form: ##.##
   // e.g. 34.2

   double num;
   if (!(_file >> num));
      TR_ASSERT_FATAL(0, "Unable to read file at consumeDoubleNumber.");
  
   return num;
   }

const char * 
OMR::JitBuilderReplayTextFile::ConsumeStatement()
   {
   // Expected Form: S###
   // e.g. S36

   std::string tempStr; 
   if (!(_file >> tempStr));
      TR_ASSERT_FATAL(0, "Unable to read file at consumeStatement.");
   TypeID typeID = stoi(tempStr.substr(1));

   TypePointer statementPointer = lookupPointer(typeID);
   return static_cast<const char *>(statementPointer);
   }

TR::IlType * 
OMR::JitBuilderReplayTextFile::ConsumeType()
   {
   // Expected Form: T###
   // e.g. T12

   std::string tempStr; 
   if (!(_file >> tempStr));
      TR_ASSERT_FATAL(0, "Unable to read file at consumeType.");
   TypeID typeID = stoi(tempStr.substr(1));

   TypePointer typePointer = lookupPointer(typeID);
   return static_cast<TR::IlType *>(typePointer);
   }

TR::IlValue * 
OMR::JitBuilderReplayTextFile::ConsumeValue()
   {
   // Expected Form: V###
   // e.g. V31

   std::string tempStr; 
   if (!(_file >> tempStr));
      TR_ASSERT_FATAL(0, "Unable to read file at consumeValue.");
   TypeID typeID = stoi(tempStr.substr(1));

   TypePointer valPointer = lookupPointer(typeID);
   return static_cast<TR::IlValue *>(valPointer);
   }

const char * const 
OMR::JitBuilderReplayTextFile::ConsumeString()
   {
   // Expected Form: "strLen [StringName]"
   // e.g. "16 [DefineReturnType]"

   std::string firstPart;
   std::string secondPart;
   if (!(_file >> firstPart));
      TR_ASSERT_FATAL(0, "Unable to read file (firstPart) at consumeString.");
   uint32_t strLen = stoi(firstPart.substr(1));

   if (!(_file >> secondPart));
      TR_ASSERT_FATAL(0, "Unable to read file (secondPart) at consumeString.");
   const char * const serviceString = strdup(secondPart.substr(1, strLen).c_str());
   return serviceString;
   }

const uint32_t
OMR::JitBuilderReplayTextFile::ConsumeID()
   {
   // Expected Form: ID##
   // e.g. ID8

   std::string tempStr; 
   if (!(_file >> tempStr));
      TR_ASSERT_FATAL(0, "Unable to read file at consumeID.");
   TypeID typeID = stoi(tempStr.substr(2));

   return typeID;
   }

TR::IlBuilder * 
OMR::JitBuilderReplayTextFile::ConsumeBuilder()
   {
   // Expected Form: B### or Def
   // e.g. B31, Def ...

   std::string tempStr; 
   if (!(_file >> tempStr));
      TR_ASSERT_FATAL(0, "Unable to read file at consumeBuilder.");
   
   if(tempStr.at(0) != 'B') { // It is "Def"
      TR_ASSERT_FATAL(0, "Try to lookup non existing Builder, found Def.");
      // TODO: Insert ID pointer pair in map. Use "registerMapping" here or just return NULL?
   }

   TypeID typeID = stoi(tempStr.substr(1));

   TypePointer builderPointer = lookupPointer(typeID);
   return static_cast<TR::IlBuilder *>(builderPointer);
   }

const void * 
OMR::JitBuilderReplayTextFile::ConsumeLocation()
   {
   // Expected Form: {#location}
   // e.g. {0x02F3DA922}

   std::string tempStr; 
   if (!(_file >> tempStr));
      TR_ASSERT_FATAL(0, "Unable to read file at consumeLocation.");
   const char * locationPtr = tempStr.substr(1, tempStr.length() - 2).c_str();

   return static_cast<const void *>(locationPtr);
   }