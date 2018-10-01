/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#include "ilgen/JitBuilderRecorder.hpp"

#include <iostream>
#include <fstream>

#include <stdint.h>
#include "infra/BitVector.hpp"
#include "compile/Compilation.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/IlInjector.hpp"
#include "ilgen/IlBuilder.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/BytecodeBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/VirtualMachineState.hpp"
#include "ilgen/JitBuilderRecorderBinaryFile.hpp"
#include "ilgen/JitBuilderRecorderTextFile.hpp"

// MethodBuilderRecorder introduces recording support for the MethodBuilder
// class. It is designed as a superclass for MethodBuilder because
// MethodBuilder should be able to call out to its Recorder to enable
// diagnostic scenarios (e.g. record how a method was compiled), but the
// Recorder class should not be able to directly refer to MethodBuilder
// to enable the separation of JitBuilder API calls (via
// MethodBuilderRecorder) from the process of generating IL (via
// MethodBuilder).

// A MethodBuilderRecorder derives from IlBuilder so that
// MethodBuilderRecorder and MethodBuilder will both have access to the
// full IlBuilder API as well as introducing new MethodBuilder specifi
// API.

OMR::MethodBuilderRecorder::MethodBuilderRecorder(TR::TypeDictionary *types, TR::VirtualMachineState *vmState, TR::JitBuilderRecorder *recorder)
   : TR::IlBuilder(static_cast<TR::MethodBuilder *>(this), types),
   _nextValueID(0),
   _useBytecodeBuilders(false),
   _vmState(vmState),
   _bytecodeWorklist(NULL),
   _bytecodeHasBeenInWorklist(NULL),
   _recorder(recorder)
   {
   if (_recorder != NULL)
      _recorder->setMethodBuilderRecorder(asMethodBuilder());

   TR::JitBuilderRecorder *rec = _recorder;
   if (rec)
      {
      rec->StoreID(this);
      rec->BeginStatement(StatementName::STATEMENT_NEWMETHODBUILDER);
      rec->Builder(asMethodBuilder());
      rec->EndStatement();
      }
   }

TR::MethodBuilder *
OMR::MethodBuilderRecorder::asMethodBuilder()
   {
   return static_cast<TR::MethodBuilder *>(this);
   }

void
OMR::MethodBuilderRecorder::AllLocalsHaveBeenDefined()
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asMethodBuilder(), StatementName::STATEMENT_ALLLOCALSHAVEBEENDEFINED);
      rec->EndStatement();
      }
   }

void
OMR::MethodBuilderRecorder::AppendBuilder(TR::BytecodeBuilder *bb)
   {
   this->OMR::IlBuilder::AppendBuilder(bb);
   if (_vmState)
      bb->propagateVMState(_vmState);
   addBytecodeBuilderToWorklist(bb);
   }

void
OMR::MethodBuilderRecorder::DefineFile(const char *name)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asMethodBuilder(), StatementName::STATEMENT_DEFINEFILE);
      rec->String(name);
      rec->EndStatement();
      }
   }

void
OMR::MethodBuilderRecorder::DefineLine(const char *line)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asMethodBuilder(), StatementName::STATEMENT_DEFINELINESTRING);
      rec->String(line);
      rec->EndStatement();
      }
   }

void
OMR::MethodBuilderRecorder::DefineLine(int32_t line)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asMethodBuilder(), StatementName::STATEMENT_DEFINELINENUMBER);
      rec->Number(line);
      rec->EndStatement();
      }
   }

void
OMR::MethodBuilderRecorder::DefineName(const char *name)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asMethodBuilder(), StatementName::STATEMENT_DEFINENAME);
      rec->String(name);
      rec->EndStatement();
      }
   }

void
OMR::MethodBuilderRecorder::DefineLocal(const char *name, TR::IlType *dt)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      dt->RecordFirstTime(rec);

      rec->BeginStatement(asMethodBuilder(), StatementName::STATEMENT_DEFINELOCAL);
      rec->Type(dt);
      rec->String(name);
      rec->EndStatement();
      }
   }

void
OMR::MethodBuilderRecorder::DefineMemory(const char *name, TR::IlType *dt, void *location)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      dt->RecordFirstTime(rec);

      rec->BeginStatement(asMethodBuilder(), StatementName::STATEMENT_DEFINEMEMORY);
      rec->String(name);
      rec->Type(dt);
      rec->Location(location);
      rec->EndStatement();
      }
   }

void
OMR::MethodBuilderRecorder::DefineParameter(const char *name, TR::IlType *dt)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      dt->RecordFirstTime(rec);

      rec->BeginStatement(asMethodBuilder(), StatementName::STATEMENT_DEFINEPARAMETER);
      rec->Type(dt);
      rec->String(name);
      rec->EndStatement();
      }
   }

void
OMR::MethodBuilderRecorder::DefineArrayParameter(const char *name, TR::IlType *elementType)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      elementType->RecordFirstTime(rec);

      rec->BeginStatement(asMethodBuilder(), StatementName::STATEMENT_DEFINEARRAYPARAMETER);
      rec->Type(elementType);
      rec->String(name);
      rec->EndStatement();
      }
   }

void
OMR::MethodBuilderRecorder::DefineReturnType(TR::IlType *dt)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      dt->RecordFirstTime(rec);

      rec->BeginStatement(asMethodBuilder(), StatementName::STATEMENT_DEFINERETURNTYPE);
      rec->Type(dt);
      rec->EndStatement();
      }
   }

void
OMR::MethodBuilderRecorder::DefineFunction(const char* const name,
                                           const char* const fileName,
                                           const char* const lineNumber,
                                           void           * entryPoint,
                                           TR::IlType     * returnType,
                                           int32_t          numParms,
                                           ...)
   {
   // if in compilation, should really allocate from jit persistent memory
   TR::IlType **parmTypes = (TR::IlType **) new (_types->trMemory()->trHeapMemory()) TR::IlType*[numParms];
   va_list parms;
   va_start(parms, numParms);
   for (int32_t p=0;p < numParms;p++)
      {
      parmTypes[p] = (TR::IlType *) va_arg(parms, TR::IlType *);
      }
   va_end(parms);

   DefineFunction(name, fileName, lineNumber, entryPoint, returnType, numParms, parmTypes);
   }

void
OMR::MethodBuilderRecorder::DefineFunction(const char* const name,
                                           const char* const fileName,
                                           const char* const lineNumber,
                                           void            * entryPoint,
                                           TR::IlType      * returnType,
                                           int32_t           numParms,
                                           TR::IlType     ** parmTypes)
        {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      returnType->RecordFirstTime(rec);
      for (int32_t p=0;p < numParms;p++)
         parmTypes[p]->RecordFirstTime(rec);

      rec->BeginStatement(asMethodBuilder(), StatementName::STATEMENT_DEFINEFUNCTION);
      rec->String(name);
      rec->String(fileName);
      rec->String(lineNumber);
      rec->Location(entryPoint);
      rec->Type(returnType);
      rec->Number(numParms);

      for (int32_t p=0;p < numParms;p++)
         rec->Type(parmTypes[p]);

      rec->EndStatement();
      }
   }

void
OMR::MethodBuilderRecorder::addBytecodeBuilderToWorklist(TR::BytecodeBuilder *builder)
   {
   if (_bytecodeWorklist == NULL)
      {
      _bytecodeWorklist = new (comp()->trHeapMemory()) TR_BitVector(32, comp()->trMemory());
      _bytecodeHasBeenInWorklist = new (comp()->trHeapMemory()) TR_BitVector(32, comp()->trMemory());
      }

   int32_t b_bci = builder->bcIndex();
   if (!_bytecodeHasBeenInWorklist->get(b_bci))
      {
      _bytecodeWorklist->set(b_bci);
      _bytecodeHasBeenInWorklist->set(b_bci);
      }
   }

int32_t
OMR::MethodBuilderRecorder::GetNextBytecodeFromWorklist()
   {
   if (_bytecodeWorklist == NULL || _bytecodeWorklist->isEmpty())
      return -1;

   TR_BitVectorIterator it(*_bytecodeWorklist);
   int32_t bci=it.getFirstElement();
   if (bci > -1)
      _bytecodeWorklist->reset(bci);
   return bci;
   }
