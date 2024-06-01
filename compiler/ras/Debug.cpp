/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#include "ras/Debug.hpp"

#include <algorithm>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenPhase.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "env/KnownObjectTable.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterIterator.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/RegisterRematerializationInfo.hpp"
#include "codegen/Snippet.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "cs2/hashtab.h"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/RawAllocator.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/List.hpp"
#include "infra/SimpleRegex.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/PreExistence.hpp"
#include "optimizer/Structure.hpp"
#include "ras/CallStackIterator.hpp"
#include "ras/DebugCounter.hpp"
#include "runtime/Runtime.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "env/CHTable.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#endif

#ifdef AIXPPC
#include <sys/debug.h>
#endif

#ifdef TR_TARGET_ARM
#include "arm/codegen/ARMInstruction.hpp"
#endif

#ifdef LINUX
#include <arpa/inet.h>
#include <dlfcn.h>
#include <netdb.h>
#endif

#if defined(J9ZOS390) || defined(AIXPPC) || defined(LINUX)
#include <unistd.h>
#endif

#ifdef J9OS_I5
#include "Xj9I5OSJitDebug.H"
#endif

// -----------------------------------------------------------------------------

// TODO: the following would look much better as instance fields in TR_Debug
int32_t addressWidth = 0;


extern const char *pIlOpNames[];  // from Tree.cpp

const char * jitdCurrentMethodSignature(TR::Compilation *comp)
   {
   return comp->getCurrentMethod()->signature(comp->trMemory(), heapAlloc);
   }


void * TR_Debug::operator new (size_t s, TR_HeapMemory m)
   {
   return m.allocate(s, TR_Memory::Debug);
   }

void * TR_Debug::operator new(size_t s, TR::PersistentAllocator &allocator)
   {
   return allocator.allocate(s);
   }


TR_Debug * createDebugObject(TR::Compilation * comp)
   {
   TR_Debug *dbg = NULL;
   if (comp)
      dbg = new (comp->trHeapMemory()) TR_Debug(comp);
   else
      dbg = new (TR::Compiler->regionAllocator) TR_Debug(comp);
   return dbg;
   }



#if defined(AIXPPC) || defined(LINUX) || defined(J9ZOS390) || defined(OMR_OS_WINDOWS)
static void stopOnCreate()
   {
   static int first = 1;
   // only print the following message once
   if (first)
      {
      printf("stopOnCreate is a dummy routine.\n");
      first = 0;
      }
   }
#endif /* defined(AIXPPC) || defined(LINUX) || defined(J9ZOS390) || defined(OMR_OS_WINDOWS) */


void
TR_Debug::breakOn()
   {
#if defined(TR_HOST_X86)
   TR::Compiler->debug.breakPoint();
#else
   static int first = 1;

   // only print the following message once
   if (first)
      {
      printf("\n");
      printf("Break point encountered.\n");
      printf("Start the program with a debuger, Or\n");
      printf("use <debugOnCreate> option to auto attach the debugger.\n");
      printf("\n");
      first = 0;
      }
   TR::Compiler->debug.breakPoint();
#endif
   }


void
TR_Debug::debugOnCreate()
   {
#if defined(TR_HOST_X86)
   TR::Compiler->debug.breakPoint();
#elif defined(AIXPPC)
   setupDebugger((void *) *((long*)&(stopOnCreate)));
   stopOnCreate();
#elif defined(LINUX) || defined(J9ZOS390) || (defined(OMR_OS_WINDOWS))
   setupDebugger((void *) &stopOnCreate,(void *) &stopOnCreate,true);
   stopOnCreate();
#endif /* defined(TR_HOST_X86) */
   }


TR_Debug::TR_Debug( TR::Compilation * c)
   : _comp(c),
   _compilationFilters(0), _relocationFilters(0), _inlineFilters(0),
   _lastFrequency(-1), _isCold(false), _currentParent(NULL),
   _currentChildIndex(0),
   _mainEntrySeen(false)
   {
   if (c)
      {
      _cg = c->cg();
      _fe = c->fe();
      _file = c->getOutFile();
      resetDebugData();

      _nodeChecklist.init(0, c->trMemory(), heapAlloc, growable);
      _structureChecklist.init(0, c->trMemory(), heapAlloc, growable);
      }
   else // still need to initialize other fields that are being initialized inside resetDebugData()
      {
      _nextLabelNumber         = 1;

      _nextRegisterNumber      = 1;
      _nextNodeNumber          = 1;
      _nextSymbolNumber        = 1;
      _nextInstructionNumber   = 1;
      _nextStructureNumber     = 1;
      _nextVariableSizeSymbolNumber = 1;
      _usesSingleAllocMetaData = 0;
      }

   // Figure out how many bytes are consumed by POINTER_PRINTF_FORMAT
   if (1) // force block scope
      {
      char buffer[30];
      addressWidth = sprintf(buffer, POINTER_PRINTF_FORMAT, this);
      }

   _registerAssignmentTraceFlags = 0;
   }


void
TR_Debug::resetDebugData()
   {
   _nextLabelNumber         = 1;
   _nextRegisterNumber      = 1;
   _nextNodeNumber          = 1;
   _nextSymbolNumber        = 1;
   _nextInstructionNumber   = 1;
   _nextStructureNumber     = 1;
   _nextVariableSizeSymbolNumber = 1;
   _usesSingleAllocMetaData = 0;
   }

inline void roundToBoundary(uint32_t &value, uint32_t boundary)
   {
   uint32_t vplus = value + boundary - 1;
   value = vplus - (vplus % boundary);
   }

void
TR_Debug::roundAddressEnumerationCounters(uint32_t boundary)
   {
   roundToBoundary(_nextLabelNumber,        boundary);
   roundToBoundary(_nextRegisterNumber,     boundary);
   roundToBoundary(_nextSymbolNumber,       boundary);
   roundToBoundary(_nextInstructionNumber,  boundary);
   roundToBoundary(_nextStructureNumber,    boundary);

   // Note: this currently has no effect because we use node->getGlobalIndex()
   // instead of _nextNodeNumber.  We may want to reconsider using
   // _nextNodeNumber to reduce spurious node name differences between runs.
   //
   roundToBoundary(_nextNodeNumber, boundary);
   }

void
TR_Debug::breakOrDebugOnCreate(char *artifactName)
   {
   TR::SimpleRegex *regex = _comp->getOptions()->getBreakOnCreate();
   if (regex && TR::SimpleRegex::match(regex, artifactName, false))
      breakOn();

   regex = _comp->getOptions()->getDebugOnCreate();
   if (regex && TR::SimpleRegex::match(regex, artifactName, false))
      debugOnCreate();
   }

void
TR_Debug::newNode(TR::Node *node)
   {
   if (_comp->getOptions()->getBreakOnCreate() ||
       _comp->getOptions()->getDebugOnCreate())
      {
      char buf[20];
      sprintf(buf, "ND_%04x", node->getGlobalIndex());
      breakOrDebugOnCreate(buf);
      }
   }

void
TR_Debug::newLabelSymbol(TR::LabelSymbol *labelSymbol)
   {
   _comp->getToNumberMap().Add((void *)labelSymbol, (intptr_t)_nextLabelNumber);

   if (_comp->getOptions()->getBreakOnCreate() ||
       _comp->getOptions()->getDebugOnCreate())
      {
      char buf[20];
      sprintf(buf, "L%04x", _nextLabelNumber);
      breakOrDebugOnCreate(buf);
      }

   _nextLabelNumber++;
   }

void
TR_Debug::newInstruction(TR::Instruction *instr)
   {
   if (_comp->getAddressEnumerationOption(TR_EnumerateInstruction) ||
      _comp->getOptions()->getBreakOnCreate() ||
      _comp->getOptions()->getDebugOnCreate())
      {
      char buf[20];
      _comp->getToNumberMap().Add((void *)instr, (intptr_t)_nextInstructionNumber);
      sprintf(buf, "IN_%04x", _nextInstructionNumber);
      breakOrDebugOnCreate(buf);
      }

   _nextInstructionNumber++;
   }

void
TR_Debug::newRegister(TR::Register *reg)
   {
   if (_comp->getAddressEnumerationOption(TR_EnumerateRegister))
      _comp->getToNumberMap().Add((void *)reg, (intptr_t)_nextRegisterNumber);

   if (_comp->getOptions()->getBreakOnCreate() ||
       _comp->getOptions()->getDebugOnCreate())
      {
      char buf[20];
      sprintf(buf, "GPR_%04x", _nextRegisterNumber );
      breakOrDebugOnCreate(buf);
      }

   _nextRegisterNumber++;
   }

void
TR_Debug::newVariableSizeSymbol(TR::AutomaticSymbol *sym)
   {
   int32_t strLength = static_cast<int32_t>(strlen(TR_VSS_NAME)) + TR::getMaxSignedPrecision<TR::Int32>() + 7;
   char *buf = (char *)_comp->trMemory()->allocateHeapMemory(strLength);
   sprintf(buf, "%s_%d",  TR_VSS_NAME, _nextVariableSizeSymbolNumber);

   _comp->getToStringMap().Add((void *)sym, buf);

   if (_comp->getOptions()->getBreakOnCreate() ||
       _comp->getOptions()->getDebugOnCreate())
      {
      breakOrDebugOnCreate(buf);
      }

   _nextVariableSizeSymbolNumber++;
   }

void
TR_Debug::addInstructionComment(TR::Instruction *instr, char * comment, ...)
   {
   TR_ASSERT(_comp, "Required compilation object is NULL.\n");

   if (comment == NULL || _comp->getOutFile() == NULL )
      return;

   CS2::HashIndex hashIndex;
   if (_comp->getToCommentMap().Locate(instr, hashIndex))
      {
      List<char> *comments = _comp->getToCommentMap().DataAt(hashIndex);
      comments->add(comment);
      }
   else
      {
      List<char> *comments = new (_comp->trHeapMemory()) List<char>(_comp->trMemory());
      comments->add(comment);
      _comp->getToCommentMap().Add(instr, comments);
      }
   }

const char *
TR_Debug::getDiagnosticFormat(const char *format, char *buffer, int32_t length)
   {
   if (!_comp->getOption(TR_MaskAddresses))
      return format;

   bool allowedToWrite = true;
   bool sawPercentP = false;

   int32_t j = 0;
   const char *c = format;
   for (; *c; c++, j++)
      {
      if (j >= length)
         allowedToWrite = false;
      if (allowedToWrite)
         buffer[j] = *c;

      if (*c == '%')
         {
         ++c; ++j;
         const char *base = c;
         while (*c == '*' || (*c >= '0' && *c <= '9'))
            c++;

         if (*c == 'p')
            {
            char s[] = ".0s*Masked*";
            int32_t slen = sizeof(s)/sizeof(char);
            if (j+slen >= length)
               allowedToWrite = false;
            if (allowedToWrite)
               memcpy(&buffer[j], s, slen);
            j+=slen-2;
            sawPercentP = true;
            }
         else
            {
            if (j+c-base+1 >= length)
               allowedToWrite = false;
            if (allowedToWrite)
               memcpy(&buffer[j], base, c-base+1);
            j+=static_cast<int32_t>(c-base);
            }
         }
      }

   if (j >= length)
      allowedToWrite = false;
   if (allowedToWrite)
      buffer[j] = 0; // null terminate
   j++;


   if (!sawPercentP)
      return format;
   if (allowedToWrite)
      return buffer;

   // allocate a jitMalloc buffer of the correct size
   buffer = (char *) _comp->trMemory()->allocateHeapMemory(j);
   return getDiagnosticFormat(format, buffer, j);
   }

bool
TR_Debug::performTransformationImpl(bool canOmitTransformation, const char * format, ...)
   {
   int32_t optIndex = _comp->getOptIndex();
   int32_t firstOptIndex = _comp->getOptions()->getFirstOptIndex();
   int32_t lastOptIndex = _comp->getOptions()->getLastOptIndex();
   // An opt that is marked as MustBeDone will always run.
   // However, we need to omit optional transformations when the opt is supposed
   // to be prohibited by firstOptIndex and lastOptIndex
   //
   if (canOmitTransformation && (optIndex < firstOptIndex || optIndex > lastOptIndex))
      return false;

   if (_comp->getOptimizer())
      _comp->getOptimizer()->incOptMessageIndex();

   const char *string = format; // without formattedString, we do our best with the format string itself
   bool alreadyFormatted = false;
   char messageBuffer[300];

   // We try to avoid doing the work of formatting the string if we don't need to.
   //
   if (  (_comp->getOption(TR_CountOptTransformations) && _comp->getOptions()->getVerboseOptTransformationsRegex())
      || (_file != NULL && canOmitTransformation && _comp->getOptions()->getDisabledOptTransformations()))
      {
      va_list args;
      va_start(args,format);
      string = formattedString(messageBuffer, sizeof(messageBuffer), format, args);
      va_end(args);
      alreadyFormatted = true;
      }

   if (_comp->getOption(TR_CountOptTransformations))
      {
      TR::SimpleRegex * regex = _comp->getOptions()->getVerboseOptTransformationsRegex();
      if (regex && TR::SimpleRegex::match(regex, string))
         {
         _comp->incVerboseOptTransformationCount();
         }
      }

   static int32_t curTransformationIndex=0;

   if (canOmitTransformation)
      {
      ++curTransformationIndex;
      _comp->incOptSubIndex();

      TR::SimpleRegex * regex = _comp->getOptions()->getDisabledOptTransformations();
      if (regex && (TR::SimpleRegex::match(regex, curTransformationIndex) || TR::SimpleRegex::match(regex, string)))
         return false;

      if (curTransformationIndex < _comp->getOptions()->getFirstOptTransformationIndex() ||
          curTransformationIndex > _comp->getOptions()->getLastOptTransformationIndex())
         return false;

      if (optIndex == lastOptIndex
         && _comp->getOptSubIndex() > _comp->getOptions()->getLastOptSubIndex())
         return false;

      //
      // Ok, we're doing this transformation
      //
      if (optIndex == comp()->getLastBegunOptIndex())
         comp()->recordPerformedOptTransformation();
      }

   // No need to do the printing logic below if there's no log file
   //
   if (_file == NULL)
      return true;

   if (canOmitTransformation)
      {
      if (_registerAssignmentTraceFlags & TRACERA_IN_PROGRESS)
         trfprintf(_file, "\n"); // traceRA convention is a newline at the start rather than the end

      trfprintf(_file, "[%6d] ", curTransformationIndex);
      if (optIndex == lastOptIndex)
         {
         trfprintf(_file, "%3d.%-4d ", optIndex, _comp->getOptSubIndex());
         }

      if ((format[0] == '%' && format[1] == 's') || (format[0] == 'O' && format[1] == '^' && format[2] == 'O'))
         {
         ;
         }
      else
         {
         // prefix the string with O^O so that tooling can easily count transformations
         // this should already be in most transformations but it could get accidentally dropped
         trfprintf(_file, "O^O (Unknown Transformation):");
         }
      }
   else if (_registerAssignmentTraceFlags & TRACERA_IN_PROGRESS)
      trfprintf(_file, "\n         ");
   else
      trfprintf(_file, "         ");


   char buffer[200];

   va_list args;
   va_start(args,format);

   TR::IO::vfprintf(_file, getDiagnosticFormat(format, buffer, sizeof(buffer)/sizeof(char)), args);
   va_end(args);
   trfflush(_file);

   roundAddressEnumerationCounters();

   return true;
   }

void
TR_Debug::trace(const char * format, ...)
   {
   if (_file != NULL)
      {
      va_list args;
      va_start(args,format);
      vtrace(format, args);
      va_end(args);
      }
   }

void
TR_Debug::vtrace(const char * format, va_list args)
   {
   if (_file != NULL)
      {
      char buffer[256];
      if ((0 != TR::Options::_traceFileLength) &&
          (static_cast<int64_t>(TR::IO::ftell(_file)) > (static_cast<int64_t>(TR::Options::_traceFileLength) << 20)))
         {
         TR::IO::fseek(_file, 0, SEEK_SET); // rewind the trace file
         TR::IO::fprintf(_file, "Rewind trace file ...\n\n\n");
         }
      TR::IO::vfprintf(_file, getDiagnosticFormat(format, buffer, sizeof(buffer)), args);
      trfflush(_file);
      }
   }

void
TR_Debug::traceLnFromLogTracer(const char *preFormatted)
   {
   if (_file != NULL)
      {
      trfprintf(_file,preFormatted);
      trfprintf(_file,"\n");
      trfflush(_file);
      }
   }


void
TR_Debug::printInstruction(TR::Instruction *instr)
   {
   if (_file != NULL)
      {
      print(_file, instr);
      trfflush(_file);
      }
   }



void
TR_Debug::printHeader()
   {
   if (_file == NULL)
      return;

   trfprintf(_file, "\n=======>%s\n", signature(_comp->getMethodSymbol()));
   }

void
TR_Debug::printMethodHotness()
   {
   if (_file == NULL)
      return;

   trfprintf(_file, "\nThis method is %s", _comp->getHotnessName(_comp->getMethodHotness()));
   if (_comp->getRecompilationInfo() &&
       _comp->getRecompilationInfo()->isProfilingCompilation())
      trfprintf(_file, " and will be profiled");

   trfprintf(_file, "\n");
   }


void
TR_Debug::printInstrDumpHeader(const char * title)
   {
   if (_file == NULL)
      return;

   int addressFieldWidth   = TR::Compiler->debug.hexAddressFieldWidthInChars();
   int codeByteColumnWidth = TR::Compiler->debug.codeByteColumnWidth();
   int prefixWidth         = addressFieldWidth * 2 + codeByteColumnWidth + 12; // 8 bytes of offsets, 2 spaces, and opening and closing brackets
   const char *lineNumber = "";

   if (strcmp(title, "Post Instruction Selection Instructions") == 0 || strcmp(title, "Post Register Assignment Instructions") == 0)
      {
      trfprintf(_file, "\n%*s+--------------------------------------- instruction address", addressFieldWidth-2, " ");
      trfprintf(_file, "\n%*s|       +------------------------------------------ %s", addressFieldWidth-2, " ",lineNumber);
      trfprintf(_file, "\n%*s|       |       +----------------------------------------- instruction", addressFieldWidth-2, " ");
      trfprintf(_file, "\n%*s|       |       |", addressFieldWidth-2, " ");
      trfprintf(_file, "\n%*sV       V       V", addressFieldWidth-2, " ");
      }
    else
      {
      trfprintf(_file, "\n%*s+--------------------------------------- instruction address", addressFieldWidth - 1, " ");
      trfprintf(_file, "\n%*s|        +----------------------------------------- instruction offset from start of method", addressFieldWidth - 1, " ");
      trfprintf(_file, "\n%*s|        | %*s+------------------------------------------ corresponding TR::Instruction instance", addressFieldWidth - 1, " ", addressFieldWidth, " ");
      trfprintf(_file, "\n%*s|        | %*s|  +-------------------------------------------------- code bytes", addressFieldWidth - 1, " ", addressFieldWidth, " ");
      trfprintf(_file, "\n%*s|        | %*s|  |%*s+-------------------------------------- %sopcode and operands", addressFieldWidth - 1, " ", addressFieldWidth, " ", codeByteColumnWidth - 2, " ", lineNumber);
      trfprintf(_file, "\n%*s|        | %*s|  |%*s|\t\t\t\t+----------- additional information", addressFieldWidth - 1, " ", addressFieldWidth, " ", codeByteColumnWidth - 2, " ");
      trfprintf(_file, "\n%*s|        | %*s|  |%*s|\t\t\t\t|", addressFieldWidth - 1, " ", addressFieldWidth, " ", codeByteColumnWidth - 2, " ");
      trfprintf(_file, "\n%*sV        V %*sV  V%*sV\t\t\t\tV", addressFieldWidth - 1, " ", addressFieldWidth, " ", codeByteColumnWidth - 2, " ");
      }
   }


#define MAX_PREFIX_WIDTH 80


// Prints the prefix of a dumped instruction, which includes its address in the
// code cache, its offset from the beginning of the method, the code bytes, and
// the address of the corresponding TR_Instruction object.
//
// If binary encoding has not been performend, only the last item is printed.
// The instruction address, offset and code bytes columns are compressed.
//
// If no TR_Instruction object is given (instr == NULL), the column is left blank.
//
// If it is currently the assembly generation phase, no prefix is printed.
//
// Returns the address of the last printed code byte.
//
uint8_t *
TR_Debug::printPrefix(TR::FILE *pOutFile, TR::Instruction *instr, uint8_t *cursor, uint8_t size)
   {
   if (cursor != NULL)
      {
      uint32_t offset = static_cast<uint32_t>(cursor - _comp->cg()->getCodeStart());

      char prefix[MAX_PREFIX_WIDTH + 1];

      int addressFieldWidth   = TR::Compiler->debug.hexAddressFieldWidthInChars();
      int codeByteColumnWidth = TR::Compiler->debug.codeByteColumnWidth();
      int prefixWidth         = addressFieldWidth * 2 + codeByteColumnWidth + 12; // 8 bytes of offsets, 2 spaces, and opening and closing brackets

      if (_comp->getOption(TR_MaskAddresses))
         {
         if (instr)
            sprintf(prefix, "%*s %08x [%s]", addressFieldWidth, "*Masked*", offset, getName(instr));
         else
            sprintf(prefix, "%*s %08x %*s", addressFieldWidth, "*Masked*", offset, addressFieldWidth + 2, " ");
         }
      else
         {
         if (instr)
            sprintf(prefix, POINTER_PRINTF_FORMAT " %08x [%s]", cursor, offset, getName(instr));
         else
            sprintf(prefix, POINTER_PRINTF_FORMAT " %08x %*s", cursor, offset, addressFieldWidth + 2, " ");
         }

      char *p0 = prefix;
      char *p1 = prefix + strlen(prefix);

      // Print machine code in bytes on X86, in words on PPC,ARM,ARM64
      // Stop if we try to run over the buffer.
      if (_comp->target().cpu.isX86())
         {
         for (int i = 0; i < size && p1 - p0 + 3 < prefixWidth; i++, p1 += 3)
            sprintf(p1, " %02x", *cursor++);
         }
      else if (_comp->target().cpu.isPower() || _comp->target().cpu.isARM() || _comp->target().cpu.isARM64())
         {
         for (int i = 0; i < size && p1 - p0 + 9 < prefixWidth; i += 4, p1 += 9, cursor += 4)
            sprintf(p1, " %08x", *((uint32_t *)cursor));
         }
      else // FIXME: Need a better general form
         {
         for (int i = 0; i < size && p1 - p0 + 3 < prefixWidth; i++, p1 += 3)
            sprintf(p1, " %02x", *cursor++);
         }

      int32_t leftOver = static_cast<int32_t>(p0 + prefixWidth - p1);
      if (leftOver >= 1)
         {
         memset(p1, ' ', leftOver);
         p1[leftOver] = '\0';
         }

      trfprintf(pOutFile, "\n%s", prefix);
      }
   else if (_registerAssignmentTraceFlags & TRACERA_IN_PROGRESS)
      {
      const char *spaces = "                                        ";
      int16_t padding = 30 - _registerAssignmentTraceCursor;

      if (padding < 0)
         trfprintf(_file, "\n%.*s", 30, spaces);
      else if (padding > 0)
         trfprintf(_file, "%.*s", padding, spaces);

      _registerAssignmentTraceCursor = 30;

      if (_registerAssignmentTraceFlags & TRACERA_INSTRUCTION_INSERTED)
         trfprintf(pOutFile, " <%s>\t", getName(instr));
      else
         trfprintf(pOutFile, " [%s]\t", getName(instr));
      }
   else
      {
      trfprintf(pOutFile, "\n [%s]\t", getName(instr));
      }

   return cursor;
   }

void
TR_Debug::printSnippetLabel(TR::FILE *pOutFile, TR::LabelSymbol *label, uint8_t *cursor, const char *comment1, const char *comment2)
   {
   int addressFieldWidth   = TR::Compiler->debug.hexAddressFieldWidthInChars();
   int codeByteColumnWidth = TR::Compiler->debug.codeByteColumnWidth();
   int prefixWidth         = addressFieldWidth * 2 + codeByteColumnWidth + 12; // 8 bytes of offsets, 2 spaces, and opening and closing brackets

   uint32_t offset = static_cast<uint32_t>(cursor - _comp->cg()->getCodeStart());

   if (_comp->getOption(TR_MaskAddresses))
      {
      trfprintf(pOutFile, "\n\n%*s %08x %*s", TR::Compiler->debug.hexAddressFieldWidthInChars(), "*Masked*", offset, addressFieldWidth + codeByteColumnWidth + 2, " ");
      }
   else
      {
      trfprintf(pOutFile, "\n\n" POINTER_PRINTF_FORMAT " %08x %*s", cursor, offset, addressFieldWidth + codeByteColumnWidth + 2, " ");
      }
   print(pOutFile, label);
   trfprintf(pOutFile, ":");
   if (comment1)
      {
      trfprintf(pOutFile, "\t\t%c %s", (_comp->target().cpu.isX86() && _comp->target().isLinux()) ? '#' : ';', comment1);
      if (comment2)
         trfprintf(pOutFile, " (%s)", comment2);
      }
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR_BitVector * bv)
   {
   if (pOutFile == NULL) return;

   trfprintf(pOutFile,"{");
   bool firstOne = true;
   TR_BitVectorIterator bvi(*bv);
   int32_t num = 0;
   while (bvi.hasMoreElements())
      {
      if (!firstOne)
         trfprintf(pOutFile,", ");
      else
         firstOne = false;
      trfprintf(pOutFile,"%d",bvi.getNextElement());

      if (num > 30)
         {
         trfprintf(pOutFile,"\n");
         num = 0;
         }
      num++;

      }
   trfprintf(pOutFile,"}");
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR_SingleBitContainer *sbc)
   {
   if (pOutFile == NULL) return;

   if (sbc->isEmpty())
      trfprintf(pOutFile,"{}");
   else
      trfprintf(pOutFile,"{0}");
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::BitVector * bv)
   {
   if (pOutFile == NULL) return;

   trfprintf(pOutFile,"{");
   bool firstOne = true;
   int32_t  num      = 0;
   TR::BitVector::Cursor bi(*bv);
   for (bi.SetToFirstOne(); bi.Valid(); bi.SetToNextOne())
      {
      if (!firstOne)
         trfprintf(pOutFile, ", ");
      else
         firstOne = false;
      trfprintf(pOutFile, "%d", (uint32_t) bi);

      if (num > 30)
         {
         trfprintf(pOutFile,"\n");
         num = 0;
         }
      num++;

      }
   trfprintf(pOutFile, "}");
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::SparseBitVector * sparse)
   {
   if (pOutFile == NULL) return;

   trfprintf(pOutFile,"{");
   bool firstOne = true;
   int32_t  num      = 0;
   TR::SparseBitVector::Cursor bi(*sparse);
   for (bi.SetToFirstOne(); bi.Valid(); bi.SetToNextOne())
      {
      if (!firstOne)
         trfprintf(pOutFile, ", ");
      else
         firstOne = false;
      trfprintf(pOutFile, "%d", (uint32_t) bi);

      if (num > 30)
         {
         trfprintf(pOutFile,"\n");
         num = 0;
         }
      num++;

      }
   trfprintf(pOutFile, "}");
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::SymbolReference * symRef)
   {
   if (pOutFile == NULL) return;

   TR_PrettyPrinterString  output(this);
   print(symRef, output);
   trfprintf(pOutFile, "%s", output.getStr());
   trfflush(pOutFile);
   }


const char *
TR_Debug::signature(TR::ResolvedMethodSymbol *s)
   {
#ifdef J9_PROJECT_SPECIFIC
   if (s->getRecognizedMethod() == TR::java_lang_Class_newInstancePrototype)
      return s->getResolvedMethod()->newInstancePrototypeSignature(_comp->trMemory());
#endif
   return s->signature(_comp->trMemory());
   }


TR_OpaqueClassBlock *
TR_Debug::containingClass(TR::SymbolReference *symRef)
   {
   TR::Method *method = symRef->getSymbol()->castToMethodSymbol()->getMethod();

   if (method)
      {
      char *className = method->classNameChars();
      uint16_t classNameLen = method->classNameLength();
      return fe()->getClassFromSignature(className, classNameLen, symRef->getOwningMethod(comp()));
      }

   return NULL;
   }

void
TR_Debug::nodePrintAllFlags(TR::Node *node, TR_PrettyPrinterString &output)
   {
   // This guard info is not strictly speaking in the node flags anymore, but
   // with the node flags is a good place to show it.
   TR_VirtualGuard *guard = node->virtualGuardInfo();
   if (guard != NULL)
      {
      const char *kind = getVirtualGuardKindName(guard->getKind());
      const char *testType = getVirtualGuardTestTypeName(guard->getTestType());
      output.appendf("%s/%s", kind, testType);

      if (guard->mergedWithHCRGuard())
         output.appends("+HCRGuard");

      if (guard->mergedWithOSRGuard())
         output.appends("+OSRGuard");

      if (!guard->getInnerAssumptions().isEmpty())
         output.appends("+inner");

      output.appends(" ");
      }

#define FLAG_IF(cond, text) do { if (cond) output.appends(text " "); } while (false)
#define FLAG(query, text) FLAG_IF(node->query(), text)

   FLAG(chkFoldedImplicitNULLCHK, "foldedImplicitNULLCHK");
   FLAG(chkHighWordZero, "highWordZero");
   FLAG(chkUnsigned, "Unsigned");
   FLAG(chkClassPointerConstant, "classPointerConstant");
   FLAG(chkMethodPointerConstant, "methodPointerConstant");
   FLAG(chkSafeToSkipTableBoundCheck, "safeToSkipTblBndChk");
   FLAG(isProfilingCode, "profilingCode");
   FLAG(isZero, "X==0");
   FLAG(isNonZero, "X!=0");
   FLAG(isNonNegative, "X>=0");
   FLAG(isNonPositive, "X<=0");
   FLAG(chkPointsToNull, "*X==null");
   FLAG(nodeRequiresConditionCodes, "requiresConditionCodes");
   FLAG(isUnneededConversion, "unneededConv");
   FLAG(parentSupportsLazyClobber, "lazyClobber");
   FLAG(isFPStrictCompliant, "FPPrecise");
   FLAG(chkCannotOverflow, "cannotOverflow");
   FLAG(chkPointsToNonNull, "nodePointsToNonNull");

   FLAG(isInvalid8BitGlobalRegister, "invalid8BitGlobalRegister");
   FLAG(isDirectMemoryUpdate, "directMemoryUpdate");
   FLAG(chkTheVirtualCallNodeForAGuardedInlinedCall, "virtualCallNodeForAGuardedInlinedCall");
   FLAG(chkDontTransformArrayCopyCall, "dontTransformArrayCopyCall");
   FLAG(chkNodeRecognizedArrayCopyCall, "nodeRecognizedArrayCopyCall");
   FLAG(chkDesynchronizeCall, "desynchronizeCall");
   FLAG(chkCompressionSequence, "compressionSequence");
   FLAG(isInternalPointer, "internalPtr");
   FLAG(isMaxLoopIterationGuard, "maxLoopIternGuard");
   FLAG_IF(guard != NULL && node->vftEntryIsInBounds(), "vftEntryIsInBounds");
   FLAG(childrenWereSwapped, "swappedChildren");
   FLAG(isVersionableIfWithMaxExpr, "versionableIfWithMaxExpr");
   FLAG(isVersionableIfWithMinExpr, "versionableIfWithMinExpr");
   FLAG(chkByteToByteTranslate, "byte2byteXlate");
   FLAG(chkByteToCharTranslate, "byte2charXlate");
   FLAG(chkCharToByteTranslate, "char2byteXlate");
   FLAG(chkCharToCharTranslate, "char2charXlate");
   FLAG(chkSourceByteArrayTranslate, "sourceIsByteArrayTranslate");
   FLAG(chkTargetByteArrayTranslate, "byteArrayXlate");
   FLAG(chkTermCharNodeIsHint, "termCharNodeIsHint");
   FLAG(chkTableBackedByRawStorage, "tableBackedByRawStorage");
   FLAG(chkForwardArrayCopy, "forwardArrayCopy");
   FLAG(chkBackwardArrayCopy, "backwardArrayCopy");
   FLAG(chkRarePathForwardArrayCopy, "rarePathFwdArrayCopy");
   FLAG(chkNoArrayStoreCheckArrayCopy, "noArrayStoreCheckArrayCopy");
   FLAG(chkReferenceArrayCopy, "referenceArrayCopy");
   FLAG(chkHalfWordElementArrayCopy, "halfWordElementArrayCopy");
   FLAG(chkWordElementArrayCopy, "wordElementArrayCopy");
   FLAG(chkHeapObjectWrtBar, "heapObjectWrtBar");
   FLAG(chkNonHeapObjectWrtBar, "nonHeapObjectWrtBar");
   FLAG(chkSkipWrtBar, "skipWrtBar");
   FLAG(chkArrayChkPrimitiveArray1, "arrayChkPrimitiveArray1");
   FLAG(chkArrayChkReferenceArray1, "arrayChkReferenceArray1");
   FLAG(chkArrayChkPrimitiveArray2, "arrayChkPrimitiveArray2");
   FLAG(chkArrayChkReferenceArray2, "arrayChkReferenceArray2");
   FLAG(isSignExtendedTo32BitAtSource, "signExtendedTo32BitAtSource");
   FLAG(isSignExtendedTo64BitAtSource, "signExtendedTo64BitAtSource");
   FLAG(isZeroExtendedTo32BitAtSource, "zeroExtendTo32BitAtSource");
   FLAG(isZeroExtendedTo64BitAtSource, "zeroExtendTo64BitAtSource");
   FLAG(needsSignExtension, "NeedsSignExt");
   FLAG(skipSignExtension, "SkipSignExt");
   FLAG(useSignExtensionMode, "SignExtMode");
   FLAG(chkSeenRealReference, "SeenRealReference");
   FLAG(chkNormalizeNanValues, "mustNormalizeNanValues");
   FLAG(chkCannotTrackLocalUses, "cannotTrackLocalUses");
   FLAG(chkSkipSync, "skipSync");
   FLAG(chkReadMonitor, "readMonitor");
   FLAG(chkLocalObjectMonitor, "localObjectMonitor");
   FLAG(chkPrimitiveLockedRegion, "primitiveLockedRegion");
   FLAG(chkSyncMethodMonitor, "syncMethodMonitor");
   FLAG(chkStaticMonitor, "staticMonitor");
   FLAG(chkNormalizedShift, "normalizedShift");
   FLAG(chkSimpleDivCheck, "simpleDivCheck");
   FLAG(chkOmitSync, "omitSync");
   FLAG(chkNOPLongStore, "NOPLongStore");
   FLAG(chkStoredValueIsIrrelevant, "StoredValueIsIrrelevant");
   FLAG(chkThrowInsertedByOSR, "ThrowInsertedByOSR");
   FLAG(chkSkipZeroInitialization, "skipZeroInit");
   FLAG(chkDontMoveUnderBranch, "dontMoveUnderBranch");
   FLAG(chkIsPrivatizedInlinerArg, "privatizedInlinerArg");
   FLAG(chkArrayCmpSign, "arrayCmpSign");
   FLAG(chkXorBitOpMem, "SubOp=XOR");
   FLAG(chkOrBitOpMem, "SubOp=OR");
   FLAG(chkAndBitOpMem, "SubOp=AND");
#ifdef J9_PROJECT_SPECIFIC
   FLAG(chkSkipCopyOnStore, "skipCopyOnStore");
   FLAG(chkSkipCopyOnLoad, "skipCopyOnLoad");
   FLAG(chkSkipPadByteClearing, "skipPadByteClearing");
   FLAG(chkUseStoreAsAnAccumulator, "useStoreAsAnAccumulator");
   FLAG(chkCleanSignInPDStoreEvaluator, "cleanSignInPDStoreEvaluator");
#endif
   FLAG(chkUseCallForFloatToFixedConversion, "useCallForFloatToFixedConv");
#ifdef J9_PROJECT_SPECIFIC
   FLAG(chkCleanSignDuringPackedLeftShift, "cleanSignDuringPackedLeftShift");
   FLAG(chkIsInMemoryCopyProp, "IsInMemoryCopyProp");
#endif
   FLAG(chkAllocationCanBeRemoved, "allocationCanBeRemoved");
   FLAG(chkArrayTRT, "arrayTRT");
   FLAG(chkCannotTrackLocalStringUses, "cannotTrackLocalStringUses");
   FLAG(chkCharArrayTRT, "charArrayTRT");
   FLAG(chkEscapesInColdBlock, "escapesInColdBlock");
#ifdef J9_PROJECT_SPECIFIC
   FLAG(chkDontInlineUnsafePutOrderedCall, "dontInlineUnsafePutOrderedCall");
#endif
   FLAG(chkHeapificationStore, "HeapificationStore");
   FLAG(chkHeapificationAlloc, "HeapificationAlloc");
   FLAG(chkIdentityless, "Identityless");
   FLAG(chkLiveMonitorInitStore, "liveMonitorInitStore");
   FLAG(chkReturnIsDummy, "returnIsDummy");
#ifdef J9_PROJECT_SPECIFIC
   FLAG_IF(node->getType().isAddress() && node->chkSharedMemory(), "sharedMemory");
#endif
   FLAG(chkSourceCellIsTermChar, "sourceCellIsTermChar");
#ifdef J9_PROJECT_SPECIFIC
   FLAG(chkSpineCheckWithArrayElementChild, "spineCHKWithArrayElementChild");
#endif
   FLAG(chkStoreAlreadyEvaluated, "storeAlreadyEvaluated");
   FLAG(isCopyToNewVirtualRegister, "copyToNewVirtualRegister");
   FLAG(chkNodeCreatedByPRE, "createdByPRE");
   FLAG(chkIsReferenceNonNull, "referenceIsNonNull");

#undef FLAG
#undef FLAG_IF
   }


void
TR_Debug::print(TR::SymbolReference * symRef, TR_PrettyPrinterString& output, bool hideHelperMethodInfo, bool verbose)
   {
   int32_t displacement = 0;
   uint32_t numSpaces;
   TR_PrettyPrinterString symRefNum(this),
                          symRefOffset(this),
                          symRefAddress(this),
                          symRefName(this),
                          symRefKind(this),
                          otherInfo(this),
                          symRefObjIndex(this),
                          labelSymbol(this);

   TR::Symbol * sym = symRef->getSymbol();

   symRefAddress.appendf("%s", getName(sym));


   if (sym)
      {
      if (_comp->cg()->getMappingAutomatics() &&
          sym->isRegisterMappedSymbol() &&
          sym->getRegisterMappedSymbol()->getOffset() != 0)
         {
         displacement = sym->getRegisterMappedSymbol()->getOffset();
         }
      }

   if (symRef->getOffset() + displacement)
      {
      symRefOffset.appendf( "%+d", displacement + symRef->getOffset());
      }

   if (symRef->getKnownObjectIndex() != TR::KnownObjectTable::UNKNOWN)
      symRefObjIndex.appendf( " (obj%d)", (int)symRef->getKnownObjectIndex());
   else if (sym && sym->isFixedObjectRef() && comp()->getKnownObjectTable() && !symRef->isUnresolved())
      {
      TR::KnownObjectTable::Index i = comp()->getKnownObjectTable()->getExistingIndexAt((uintptr_t*)sym->castToStaticSymbol()->getStaticAddress());
      if (i != TR::KnownObjectTable::UNKNOWN)
         symRefObjIndex.appendf( " (==obj%d)", (int)i);
      }

   if (sym)
      {
      if (symRef->isUnresolved())
         symRefKind.appends(" unresolved");
      switch (symRef->hasBeenAccessedAtRuntime())
         {
         case TR_yes: symRefKind.appends( " accessed");    break;
         case TR_no:  symRefKind.appends( " notAccessed"); break;
         default: break;
         }
      if (symRef->getSymbol()->isFinal())
         symRefKind.appends(" final");
      if (symRef->getSymbol()->isVolatile())
         symRefKind.appends(" volatile");
      switch (sym->getKind())
         {
         case TR::Symbol::IsAutomatic:
            symRefName.appendf(" %s", getName(symRef));
            if (sym->getAutoSymbol()->getName() == NULL)
               symRefKind.appends(" Auto");
            else
               symRefKind.appendf(" %s", sym->getAutoSymbol()->getName());
            break;
         case TR::Symbol::IsParameter:
            symRefKind.appends(" Parm");
            symRefName.appendf(" %s", getName(symRef));
            break;
         case TR::Symbol::IsStatic:
            if(symRef->isFromLiteralPool())
             {
               symRefKind.appends(" DLP-Static");
               symRefName.appendf(" %s", getName(symRef));
             }
            else
               {
               symRefKind.appends(" Static");
               if (sym->isNamed())
                  {
                  symRefName.appendf(" \"%s\"", ((TR::StaticSymbol*)sym)->getName());
                  }
               symRefName.appendf(" %s", getName(symRef));
               }
            break;

         case TR::Symbol::IsResolvedMethod:
         case TR::Symbol::IsMethod:
            {
            TR::MethodSymbol *methodSym = sym->castToMethodSymbol();
            if (methodSym->isNative())
               symRefKind.appends(" native");
            switch (methodSym->getMethodKind())
               {
               case TR::MethodSymbol::Virtual:
                  symRefKind.appends(" virtual");
                  break;
               case TR::MethodSymbol::Interface:
                  symRefKind.appends(" interface");
                  break;
               case TR::MethodSymbol::Static:
                  symRefKind.appends(" static");
                  break;
               case TR::MethodSymbol::Special:
                  symRefKind.appends(" special");
                  break;
               case TR::MethodSymbol::Helper:
                  symRefKind.appends(" helper");
                  break;
               case TR::MethodSymbol::ComputedStatic:
                  symRefKind.appends(" computed-static");
                  break;
               case TR::MethodSymbol::ComputedVirtual:
                  symRefKind.appends(" computed-virtual");
                  break;
               default:
                     symRefKind.appends(" UNKNOWN");
                  break;
               }

            symRefKind.appends(" Method");
            symRefName.appendf(" %s", getName(symRef));
            TR_OpaqueClassBlock *clazz = containingClass(symRef);
            if (clazz)
               {
               if (TR::Compiler->cls.isInterfaceClass(_comp, clazz))
                  otherInfo.appends(" (Interface class)");
               else if (TR::Compiler->cls.isAbstractClass(_comp, clazz))
                  otherInfo.appends(" (Abstract class)");
               }
            }
            break;

         case TR::Symbol::IsShadow:
            if (sym->isNamedShadowSymbol() && sym->getNamedShadowSymbol()->getName() != NULL)
               {
               symRefKind.appendf(" Named Shadow");
               symRefName.appendf(" %s", getName(symRef));
               }
            else
               {
               symRefKind.appends(" Shadow");
               symRefName.appendf(" %s", getName(symRef));
               }
            break;
         case TR::Symbol::IsMethodMetaData:
            symRefKind.appends(" MethodMeta");
            symRefName.appendf(" %s", symRef->getSymbol()->getMethodMetaDataSymbol()->getName());
            break;
         case TR::Symbol::IsLabel:
            print(sym->castToLabelSymbol(), labelSymbol);
            if (!labelSymbol.isEmpty())
               labelSymbol.appends(" ");
            break;
         default:
            TR_ASSERT(0, "unexpected symbol kind");
         }
         otherInfo.appendf( " [flags 0x%x 0x%x ]", sym->getFlags(),sym->getFlags2());
      }

   numSpaces = getNumSpacesAfterIndex( symRef->getReferenceNumber(), getIntLength(_comp->getSymRefTab()->baseArray.size()) );

   symRefNum.appendf("#%d", symRef->getReferenceNumber());

   if (verbose)
      {
       output.appendf("%s:%*s", symRefNum.getStr(), numSpaces, "");

       if (hideHelperMethodInfo)
          output.appendf(" %s[%s]%s", labelSymbol.getStr(), symRefOffset.getStr(), symRefObjIndex.getStr());
       else
          output.appendf(" %s%s[%s%s%s]%s%s", symRefName.getStr(), labelSymbol.getStr(), symRefKind.getStr(), symRefOffset.isEmpty() ? "" : " ",
                symRefOffset.getStr(), symRefObjIndex.getStr(), otherInfo.getStr());

       output.appendf(" [%s]", symRefAddress.getStr());

       if(sym)
          {
          output.appendf(" (%s",TR::DataType::getName(sym->getDataType()));

          TR_OpaqueClassBlock *klass = sym->getDeclaredClass();
          if (klass != NULL)
             {
             int32_t len = 0;
             const char *className =
                TR::Compiler->cls.classNameChars(_comp, klass, len);

             output.appendf(": %p %.*s", klass, len, className);
             }

          output.appendf(")");

          if (sym->isVolatile())
             {
             output.appends(" [volatile]");
             }
          }
      }
   else
      {
      if (hideHelperMethodInfo)
         output.appendf(" %s[%s%s%s]%s", labelSymbol.getStr(), symRefNum.getStr(), symRefOffset.isEmpty() ? "" : " ", symRefOffset.getStr(),
               symRefObjIndex.getStr());
      else
         output.appendf(" %s%s[%s%s%s%s%s]%s%s", symRefName.getStr(), labelSymbol.getStr(), symRefNum.getStr(), symRefKind.isEmpty() ? "" : " ",
               symRefKind.getStr(), symRefOffset.isEmpty() ? "" : " ", symRefOffset.getStr(), symRefObjIndex.getStr(), otherInfo.getStr());
      }

   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::LabelSymbol * labelSymbol)
   {
   if (pOutFile == NULL)  return;
   TR_PrettyPrinterString output(this);
   print(labelSymbol, output);
   trfprintf(pOutFile, "%s", output.getStr());
   }
void
TR_Debug::print(TR::LabelSymbol * labelSymbol, TR_PrettyPrinterString& output)
   {
   output.appendf( "%s", getName(labelSymbol));
   }

const char *
TR_Debug::getName(void * address, const char * prefix, uint32_t nextNumber, bool enumerate)
   {
   TR_ASSERT(_comp, "Required compilation object is NULL.\n");
   // Common getName facility

   if (enumerate)
      {
      if (!address)
         {
         char *buf = (char *)_comp->trMemory()->allocateHeapMemory(20 + TR::Compiler->debug.pointerPrintfMaxLenInChars());
         sprintf(buf, "%0*d", TR::Compiler->debug.hexAddressWidthInChars(), 0);
         return buf;
         }

      CS2::HashIndex hashIndex;
      if (_comp->getToStringMap().Locate((void *)address, hashIndex))
         {
         return (const char*)_comp->getToStringMap().DataAt(hashIndex);
         }
      else
         {
         char *buf = (char *)_comp->trMemory()->allocateHeapMemory(20 + TR::Compiler->debug.pointerPrintfMaxLenInChars());

         uint8_t indentation = TR::Compiler->debug.hexAddressFieldWidthInChars() - 4;
         sprintf(buf, "%*s%04x", indentation, prefix, nextNumber);
         _comp->getToStringMap().Add((void *)address, buf);
         return buf;
         }
      }

   char *buf = (char *)_comp->trMemory()->allocateHeapMemory(20 + TR::Compiler->debug.pointerPrintfMaxLenInChars());
   if (_comp->getOption(TR_MaskAddresses))
      {
      sprintf(buf, "%*s", TR::Compiler->debug.hexAddressFieldWidthInChars(), "*Masked*");
      return buf;
      }
   else
      {
      if (address)
         sprintf(buf, POINTER_PRINTF_FORMAT, address);
      else
         sprintf(buf, "%0*d", TR::Compiler->debug.hexAddressWidthInChars(), 0);
      return buf;
      }
   }

const char *
TR_Debug::getVSSName(TR::AutomaticSymbol *sym)
   {
   TR_ASSERT(_comp, "Required compilation object is NULL.\n");
   CS2::HashIndex hashIndex;
   if (_comp->getToStringMap().Locate((void *)sym, hashIndex))
      {
      return (const char *)_comp->getToStringMap().DataAt(hashIndex);
      }

   TR_ASSERT( false, "could not find variable size symbol in the _comp->getToStringMap()");
   return getName((void *) sym, TR_VSS_NAME, 0, false);
   }

const char *
TR_Debug::getName( TR::Symbol * sym)
   {
   if (sym == NULL)
      return "(null)";

   if (sym->isVariableSizeSymbol())
      return getVSSName(sym->castToVariableSizeSymbol());
   // TODO: Rewrite this more like the other getName functions.  Currently it
   // burns a lot of nextSymbolNumbers.
   //
   return getName((void *) sym, "SYM_", _nextSymbolNumber++, _comp->getAddressEnumerationOption(TR_EnumerateSymbol));
   }

const char *
TR_Debug::getName(TR::Instruction * instr)
   {
   TR_ASSERT(_comp, "Required compilation object is NULL.\n");
   uint32_t InstructionNumber = 0;

   if (!_comp->getAddressEnumerationOption(TR_EnumerateInstruction))
      return getName((void *) instr, "IN_", 0, false);

   CS2::HashIndex hashIndex;
   if (_comp->getToNumberMap().Locate((void *)instr, hashIndex))
      {
      InstructionNumber = (uint32_t)_comp->getToNumberMap().DataAt(hashIndex);
      return getName((void *) instr, "IN_", InstructionNumber, _comp->getAddressEnumerationOption(TR_EnumerateInstruction));
      }

   TR_ASSERT(0, "each instruction should be associated with a unique number");
   return getName((void *) instr, "IN1_", 0, _comp->getAddressEnumerationOption(TR_EnumerateInstruction));

   }

const char *
TR_Debug::getName(TR_Structure * structure)
   {
   // TODO: Rewrite this more like the other getName functions.  Currently it
   // burns a lot of nextStructureNumbers.
   //
   return getName((void *) structure, "ST_", _nextStructureNumber++, _comp->getAddressEnumerationOption(TR_EnumerateStructure));
   }

const char *
TR_Debug::getName(TR::Node * node)
   {
   if (!node)
      return "(null)";
   else
      return getName((void *) node, "ND_", node->getGlobalIndex(), _comp->getAddressEnumerationOption(TR_EnumerateNode));
   }

const char *
TR_Debug::getName(TR::CFGNode * node)
   {
   char *buf = (char *)_comp->trMemory()->allocateHeapMemory(25);
   if (_comp->getAddressEnumerationOption(TR_EnumerateBlock))
      {
      sprintf(buf, "block_%d", node->getNumber());
      }
   else if (_comp->getOption(TR_MaskAddresses))
      {
      sprintf(buf, "%*s", TR::Compiler->debug.hexAddressWidthInChars(), "*Masked*");
      }
   else
      {
      sprintf(buf, POINTER_PRINTF_FORMAT, node);
      }

   return buf;
   }

const char *
TR_Debug::getName(TR::LabelSymbol *labelSymbol)
   {
   TR_ASSERT(_comp, "Required compilation object is NULL.\n");
   CS2::HashIndex hashIndex;
   uint32_t labelNumber = 0;

   if (labelSymbol->isRelativeLabel())
      {
      return labelSymbol->getName();
      }
   if (_comp->getToStringMap().Locate((void *)labelSymbol, hashIndex))
      {
      return (const char *)_comp->getToStringMap().DataAt(hashIndex);
      }
   else if (_comp->getToNumberMap().Locate((void *)labelSymbol, hashIndex))
      {
      char *buf = NULL;
      labelNumber = (uint32_t)_comp->getToNumberMap().DataAt(hashIndex);

      if (labelSymbol->getSnippet())
         {
         buf = (char *)_comp->trMemory()->allocateHeapMemory(25);
         sprintf(buf, "Snippet Label L%04d", labelNumber);
         }
      else if (labelSymbol->isStartOfColdInstructionStream())
         {
         buf = (char *)_comp->trMemory()->allocateHeapMemory(25);
         sprintf(buf, "Outlined Label L%04d", labelNumber);
         }
      else
         {
         buf = (char *)_comp->trMemory()->allocateHeapMemory(25);
         sprintf(buf, "Label L%04d", labelNumber);
         }
      _comp->getToStringMap().Add((void *)labelSymbol, buf);
      return buf;
      }
   else
      {
      char *buf = (char *)_comp->trMemory()->allocateHeapMemory(20+TR::Compiler->debug.pointerPrintfMaxLenInChars());

      if (labelSymbol->getSnippet())
         if (_comp->getOption(TR_MaskAddresses))
            sprintf(buf, "Snippet Label [*Masked*]");
         else
            sprintf(buf, "Snippet Label [" POINTER_PRINTF_FORMAT "]", labelSymbol);
      else
         if (_comp->getOption(TR_MaskAddresses))
            sprintf(buf, "Label [*Masked*]");
         else
            sprintf(buf, "Label [" POINTER_PRINTF_FORMAT "]", labelSymbol);

      _comp->getToStringMap().Add((void *)labelSymbol, buf);
      return buf;
      }
   }

const char *
TR_Debug::getPerCodeCacheHelperName(TR_CCPreLoadedCode helper)
   {
#if defined(TR_TARGET_POWER)
   switch (helper)
      {
      case TR_AllocPrefetch: return "Alloc Prefetch";
      case TR_ObjAlloc: return "Object Alloc Helper";
      case TR_VariableLenArrayAlloc: return "Variable Length Array Alloc Helper";
      case TR_ConstLenArrayAlloc: return "Constant Length Array Alloc Helper";
      case TR_writeBarrier: return "Write Barrier Helper";
      case TR_writeBarrierAndCardMark: return "Write Barrier and Card Mark Helper";
      case TR_cardMark: return "Card Mark Helper";
      case TR_arrayStoreCHK: return "Array Store Check";
      default: break;
      }
#endif // TR_TARGET_POWER
   return "Unknown Helper";
   }

const char *
TR_Debug::getName(TR::SymbolReference * symRef)
   {
   int32_t index = symRef->getReferenceNumber();
   int32_t nonhelperIndex, numHelperSymbols;

   nonhelperIndex = comp()->getSymRefTab()->getNonhelperIndex(comp()->getSymRefTab()->getLastCommonNonhelperSymbol());
   numHelperSymbols = comp()->getSymRefTab()->getNumHelperSymbols();

   if (index < numHelperSymbols)
      {
      return getRuntimeHelperName(index);
      }

   if (index < nonhelperIndex)
      {
      if (index >= numHelperSymbols + TR::SymbolReferenceTable::firstArrayShadowSymbol &&
          index < numHelperSymbols + TR::SymbolReferenceTable::firstArrayShadowSymbol + TR::NumAllTypes)
         return "<array-shadow>";
      if (index >= numHelperSymbols + TR::SymbolReferenceTable::firstPerCodeCacheHelperSymbol &&
          index <= numHelperSymbols + TR::SymbolReferenceTable::lastPerCodeCacheHelperSymbol)
         return getPerCodeCacheHelperName((TR_CCPreLoadedCode)(index - numHelperSymbols - TR::SymbolReferenceTable::firstPerCodeCacheHelperSymbol));
      switch (index - numHelperSymbols)
         {
         case TR::SymbolReferenceTable::contiguousArraySizeSymbol:
            return "<contiguous-array-size>";
         case TR::SymbolReferenceTable::discontiguousArraySizeSymbol:
            return "<discontiguous-array-size>";
         case TR::SymbolReferenceTable::arrayClassRomPtrSymbol:
            return "<array-class-rom-ptr>";
         case TR::SymbolReferenceTable::vftSymbol:
            return "<vft-symbol>";
         case TR::SymbolReferenceTable::currentThreadSymbol:
            return "<current-thread>";
         case TR::SymbolReferenceTable::thisRangeExtensionSymbol:
            return "<this-range-extension>";
         case TR::SymbolReferenceTable::recompilationCounterSymbol:
            return "<recompilation-counter>";
         case TR::SymbolReferenceTable::counterAddressSymbol:
            return "<recompilation-counter-address>";
         case TR::SymbolReferenceTable::countForRecompileSymbol:
            return "<count-for-recompile>";
         case TR::SymbolReferenceTable::gcrPatchPointSymbol:
            return "<gcr-patch-point>";
         case TR::SymbolReferenceTable::startPCSymbol:
            return "<start-PC>";
         case TR::SymbolReferenceTable::compiledMethodSymbol:
            return "<J9Method>";
         case TR::SymbolReferenceTable::excpSymbol:
            return "<exception-symbol>";
         case TR::SymbolReferenceTable::indexableSizeSymbol:
            return "<indexable-size>";
         case TR::SymbolReferenceTable::resolveCheckSymbol:
            return "<resolve check>";
         case TR::SymbolReferenceTable::arraySetSymbol:
            return "<arrayset>";
         case TR::SymbolReferenceTable::arrayCopySymbol:
            return "<arraycopy>";
         case TR::SymbolReferenceTable::prefetchSymbol:
            return "<prefetch>";
         case TR::SymbolReferenceTable::arrayTranslateSymbol:
            return "<arraytranslate>";
         case TR::SymbolReferenceTable::arrayTranslateAndTestSymbol:
            return "<arraytranslateandtest>";
         case TR::SymbolReferenceTable::long2StringSymbol:
            return "<long2String>";
         case TR::SymbolReferenceTable::bitOpMemSymbol:
            return "<bitOpMem>";
         case TR::SymbolReferenceTable::reverseLoadSymbol:
            return "<reverse-load>";
         case TR::SymbolReferenceTable::reverseStoreSymbol:
            return "<reverse-store>";
         case TR::SymbolReferenceTable::arrayCmpSymbol:
            return "<arraycmp>";
         case TR::SymbolReferenceTable::arrayCmpLenSymbol:
            return "<arraycmplen>";
         case TR::SymbolReferenceTable::currentTimeMaxPrecisionSymbol:
            return "<currentTimeMaxPrecision>";
         case TR::SymbolReferenceTable::encodeASCIISymbol:
            return "<encodeASCII>";
         case TR::SymbolReferenceTable::singlePrecisionSQRTSymbol:
            return "<fsqrt>";
         case TR::SymbolReferenceTable::killsAllMethodSymbol:
            return "<killsAllMethod>";
         case TR::SymbolReferenceTable::usesAllMethodSymbol:
            return "<usesAllMethod>";
         case TR::SymbolReferenceTable::synchronizedFieldLoadSymbol:
            return "<synchronizedFieldLoad>";
         case TR::SymbolReferenceTable::atomicAddSymbol:
             return "<atomicAdd>";
         case TR::SymbolReferenceTable::atomicFetchAndAddSymbol:
             return "<atomicFetchAndAdd>";
         case TR::SymbolReferenceTable::atomicFetchAndAdd32BitSymbol:
             return "<atomicFetchAndAdd32Bit>";
         case TR::SymbolReferenceTable::atomicFetchAndAdd64BitSymbol:
             return "<atomicFetchAndAdd64Bit>";
         case TR::SymbolReferenceTable::atomicSwapSymbol:
             return "<atomicSwap>";
         case TR::SymbolReferenceTable::atomicSwap32BitSymbol:
             return "<atomicSwap32Bit>";
         case TR::SymbolReferenceTable::atomicSwap64BitSymbol:
             return "<atomicSwap64Bit>";
         case TR::SymbolReferenceTable::atomicCompareAndSwapReturnStatusSymbol:
             return "<atomicCompareAndSwapReturnStatus>";
         case TR::SymbolReferenceTable::atomicCompareAndSwapReturnValueSymbol:
             return "<atomicCompareAndSwapReturnValue>";
         case TR::SymbolReferenceTable::potentialOSRPointHelperSymbol:
             return "<potentialOSRPointHelper>";
         case TR::SymbolReferenceTable::osrFearPointHelperSymbol:
             return "<osrFearPointHelper>";
         case TR::SymbolReferenceTable::eaEscapeHelperSymbol:
             return "<eaEscapeHelper>";
         case TR::SymbolReferenceTable::j9VMThreadTempSlotFieldSymbol:
             return "<j9VMThreadTempSlotFieldSymbol>";
         case TR::SymbolReferenceTable::computedStaticCallSymbol:
             return "<computedStaticCallSymbol>";
         case TR::SymbolReferenceTable::j9VMThreadFloatTemp1Symbol:
             return "<j9VMThreadFloatTemp1Symbol>";
         case TR::SymbolReferenceTable::objectEqualityComparisonSymbol:
             return "<objectEqualityComparison>";
         case TR::SymbolReferenceTable::objectInequalityComparisonSymbol:
             return "<objectInequalityComparison>";
         case TR::SymbolReferenceTable::nonNullableArrayNullStoreCheckSymbol:
             return "<nonNullableArrayNullStoreCheck>";
         case TR::SymbolReferenceTable::loadFlattenableArrayElementNonHelperSymbol:
             return "<loadFlattenableArrayElementNonHelper>";
         case TR::SymbolReferenceTable::storeFlattenableArrayElementNonHelperSymbol:
             return "<storeFlattenableArrayElementNonHelper>";
         case TR::SymbolReferenceTable::J9JNIMethodIDvTableIndexFieldSymbol:
             return "<J9JNIMethodIDvTableIndexFieldSymbol>";
         case TR::SymbolReferenceTable::contiguousArrayDataAddrFieldSymbol:
             return "<contiguousArrayDataAddrFieldSymbol>";
         case TR::SymbolReferenceTable::defaultValueSymbol:
             return "<defaultValue>";
         case TR::SymbolReferenceTable::jitDispatchJ9MethodSymbol:
             return "<jitDispatchJ9Method>";
         }
      }

    TR::Symbol *sym = symRef->getSymbol();
   switch (sym->getKind())
      {
      case TR::Symbol::IsAutomatic:
         return getAutoName(symRef);
      case TR::Symbol::IsParameter:
         return getParmName(symRef);
      case TR::Symbol::IsStatic:
         return getStaticName(symRef);
      case TR::Symbol::IsResolvedMethod:
      case TR::Symbol::IsMethod:
         return getMethodName(symRef);
      case TR::Symbol::IsShadow:
         return getShadowName(symRef);
      case TR::Symbol::IsMethodMetaData:
         return getMetaDataName(symRef);
      case TR::Symbol::IsLabel:
         return getName((TR::LabelSymbol *)sym);
      default:
         TR_ASSERT(0, "unexpected symbol kind");
         return "unknown name";
      }
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::SymbolReferenceTable * symRefTab)
   {
   if (pOutFile != NULL && symRefTab->baseArray.size() > 0 && _comp->getOption(TR_TraceAliases))
      {
      trfprintf(pOutFile, "Symbol Reference Map for this method:\n");
      for (auto i = 0U; i<symRefTab->baseArray.size(); i++)
         if (symRefTab->getSymRef(static_cast<int32_t>(i)))
            trfprintf(pOutFile,"  %d[" POINTER_PRINTF_FORMAT "]\n", i, symRefTab->getSymRef(i));
      }
   }

void
TR_Debug::printAliasInfo(TR::FILE *pOutFile, TR::SymbolReferenceTable * symRefTab)
   {
   if (pOutFile == NULL) return;

   trfprintf(pOutFile, "\nSymbol References with Aliases:\n\n");
   for (int32_t i = symRefTab->getIndexOfFirstSymRef(); i < symRefTab->getNumSymRefs(); ++i)
      if (symRefTab->getSymRef(i))
         printAliasInfo(pOutFile, symRefTab->getSymRef(i));
   }

void
TR_Debug::printAliasInfo(TR::FILE *pOutFile, TR::SymbolReference * symRef)
   {
   if (pOutFile == NULL) return;

   TR_BitVector * useDefAliases = symRef->getUseDefAliasesBV();
   TR_BitVector * useOnlyAliases = symRef->getUseonlyAliasesBV(_comp->getSymRefTab());
   if (useDefAliases || useOnlyAliases)
      {
      trfprintf(pOutFile, "Symref #%d %s \n", symRef->getReferenceNumber(), getName(symRef));
      if (useOnlyAliases)
         {
         trfprintf(pOutFile, "   Use Aliases: %p   ", useOnlyAliases);
         print(pOutFile, useOnlyAliases);
         }
      else
         trfprintf(pOutFile, "   Use Aliases: NULL ");
      trfprintf(pOutFile, "\n");

      if (useDefAliases)
         {
         trfprintf(pOutFile, "   Usedef Aliases: %p ", useDefAliases);
         print(pOutFile, useDefAliases);
         }
      else
         trfprintf(pOutFile, "   Usedef Aliases: NULL ");
      trfprintf(pOutFile, "\n");
      }
   else
      {
      trfprintf(pOutFile, "Symref #%d %s has no aliases\n", symRef->getReferenceNumber(), getName(symRef));
      }
   }

TR::ResolvedMethodSymbol *
TR_Debug::getOwningMethodSymbol(TR::SymbolReference * symRef)
   {
   return _comp->getOwningMethodSymbol(symRef->getOwningMethodIndex());
   }

TR_ResolvedMethod *
TR_Debug::getOwningMethod(TR::SymbolReference * symRef)
   {
   return getOwningMethodSymbol(symRef)->getResolvedMethod();
   }



const char *
TR_Debug::getAutoName(TR::SymbolReference * symRef)
   {
   int32_t slot = symRef->getCPIndex();
   char * name = (char *)_comp->trMemory()->allocateHeapMemory(50+TR::Compiler->debug.pointerPrintfMaxLenInChars());

   name[0]=0;  //initialize to empty string

   if (symRef->getSymbol()->isSpillTempAuto())
      {
      char * symName = (char *)_comp->trMemory()->allocateHeapMemory(20);
      if (symRef->getSymbol()->getDataType() == TR::Float || symRef->getSymbol()->getDataType() == TR::Double)
         sprintf(symName, "#FPSPILL%zu_%d", symRef->getSymbol()->getSize(), symRef->getReferenceNumber());
      else
         sprintf(symName, "#SPILL%zu_%d", symRef->getSymbol()->getSize(), symRef->getReferenceNumber());

      if (_comp->getOption(TR_MaskAddresses))
         sprintf(name, "<%s *Masked*>", symName);
      else
         sprintf(name, "<%s " POINTER_PRINTF_FORMAT ">", symName, symRef->getSymbol());
      }
   else if (symRef->isTempVariableSizeSymRef())
      {
      TR_ASSERT(symRef->getSymbol()->isVariableSizeSymbol(),"symRef #%d must contain a variable size symbol\n",symRef->getReferenceNumber());
      TR::AutomaticSymbol *sym = symRef->getSymbol()->getVariableSizeSymbol();
      sprintf(name, "<%s rc=%d>",getVSSName(sym),sym->getReferenceCount());
      }
   else if (symRef->getSymbol()->isPendingPush())
      {
      sprintf(name, "<pending push temp %d>", -slot - 1);
      }
   else if (slot < getOwningMethodSymbol(symRef)->getFirstJitTempIndex())
      {
      int debugNameLen;
      const char *debugName = getOwningMethod(symRef)->localName(slot, 0, debugNameLen, comp()->trMemory()); // TODO: Proper bcIndex somehow; TODO: proper length
      if (!debugName)
         {
         debugName = "";
         debugNameLen = 0;
         }
      debugNameLen = std::min(debugNameLen, 15); // Don't overrun the buffer
      if (symRef->getSymbol()->castToAutoSymbol()->isPinningArrayPointer())
         sprintf(name, "%.*s<pinning array auto slot %d>", debugNameLen, debugName, slot);
      else if (symRef->getSymbol()->castToAutoSymbol()->holdsMonitoredObject())
         {
         if (symRef->holdsMonitoredObjectForSyncMethod())
            sprintf(name, "%.*s<auto slot %d holds monitoredObject syncMethod>", debugNameLen, debugName, slot);
         else
            sprintf(name, "%.*s<auto slot %d holds monitoredObject>", debugNameLen, debugName, slot);
         }
      else
         sprintf(name, "%.*s<auto slot %d>", debugNameLen, debugName, slot);
      }
   else
      {
      if (symRef->getSymbol()->castToAutoSymbol()->isInternalPointer())
         sprintf(name, "<internal pointer temp slot %d>", slot);
      else
         {
         if (symRef->getSymbol()->castToAutoSymbol()->isPinningArrayPointer())
            sprintf(name, "<pinning array temp slot %d>", slot);
         else if (symRef->getSymbol()->castToAutoSymbol()->holdsMonitoredObject())
            {
            if (symRef->holdsMonitoredObjectForSyncMethod())
               sprintf(name, "<temp slot %d holds monitoredObject syncMethod>", slot);
            else
               sprintf(name, "<temp slot %d holds monitoredObject>", slot);
            }
         else
            sprintf(name, "<temp slot %d>", slot);
         }
      }
   return name;
   }

const char *
TR_Debug::getParmName(TR::SymbolReference * symRef)
   {
   int32_t debugNameLen, signatureLen;
   int32_t slot = symRef->getCPIndex();
   const char * s = symRef->getSymbol()->castToParmSymbol()->getTypeSignature(signatureLen);
   const char *debugName = getOwningMethod(symRef)->localName(slot, 0, debugNameLen, comp()->trMemory()); // TODO: Proper bcIndex somehow; TODO: proper length
   char *buf;

   if (!debugName)
      {
      debugName = "";
      debugNameLen = 0;
      }
   if (slot == 0 && !getOwningMethodSymbol(symRef)->isStatic())
      {
      buf = (char *)_comp->trMemory()->allocateHeapMemory(debugNameLen + signatureLen + 17);
      sprintf(buf, "%.*s<'this' parm %.*s>", debugNameLen, debugName, signatureLen, s);
      }
   else
      {
      buf = (char *)_comp->trMemory()->allocateHeapMemory(debugNameLen + signatureLen + 15);
      sprintf(buf, "%.*s<parm %d %.*s>", debugNameLen, debugName, symRef->getCPIndex(), signatureLen, s);
      }
   return buf;
   }



const char *
TR_Debug::getMethodName(TR::SymbolReference * symRef)
   {
   TR::Method *method = symRef->getSymbol()->castToMethodSymbol()->getMethod();

   if (method==NULL)
      {
      return "unknown";
      }

   return method->signature(comp()->trMemory(), stackAlloc);
   }



const char *
TR_Debug::getStaticName_ForListing(TR::SymbolReference * symRef)
   {
   TR::StaticSymbol * sym = symRef->getSymbol()->castToStaticSymbol();


   if (_comp->getSymRefTab()->isConstantAreaSymbol(sym) && sym->isStatic() && sym->isNamed())
      {
      return sym->getName();
      }

   return NULL;
   }


const char *
TR_Debug::getStaticName(TR::SymbolReference * symRef)
   {
   TR::StaticSymbol * sym = symRef->getSymbol()->castToStaticSymbol();
   void * staticAddress;
   staticAddress = sym->getStaticAddress();

   if (sym->isClassObject())
      {
      if (!sym->addressIsCPIndexOfStatic() && staticAddress)
         {
         int32_t len;
         const char *name = TR::Compiler->cls.classNameChars(comp(), symRef, len);
         if (name)
            {
            char * s = (char *)_comp->trMemory()->allocateHeapMemory(len+1);
            sprintf(s, "%.*s", len, name);
            return s;
            }
         }
      return "unknown class object";
      }
   else if (sym->isConstantPoolAddress())
      return "<constant pool address>";
   else if (symRef->getCPIndex() >= 0)
      {
      if (sym->isAddressOfClassObject())
         return "<address of class object>";

      if (sym->isConstString())
         {
         TR::StackMemoryRegion stackMemoryRegion(*comp()->trMemory());
         char *contents = NULL;
         intptr_t length = 0, prefixLength = 0, suffixOffset = 0;
         const char *etc = "";
         const intptr_t LENGTH_LIMIT=80;
         const intptr_t PIECE_LIMIT=20;

#ifdef J9_PROJECT_SPECIFIC
         TR::VMAccessCriticalSection getStaticNameCriticalSection(comp(),
                                                                   TR::VMAccessCriticalSection::tryToAcquireVMAccess);
         if (!symRef->isUnresolved() && getStaticNameCriticalSection.acquiredVMAccess())
            {
            uintptr_t stringLocation = (uintptr_t)sym->castToStaticSymbol()->getStaticAddress();
            if (stringLocation)
               {
               uintptr_t string = comp()->fej9()->getStaticReferenceFieldAtAddress(stringLocation);
               length = comp()->fej9()->getStringUTF8Length(string);
               contents = (char*)comp()->trMemory()->allocateMemory(length+1, stackAlloc, TR_MemoryBase::UnknownType);
               comp()->fej9()->getStringUTF8(string, contents, length+1);

               //
               // We don't want to mess up the logs too much.  Make sure the
               // strings aren't too ugly.
               //

               // Use ellipsis if the string is too long
               //
               if (length <= LENGTH_LIMIT)
                  {
                  prefixLength = suffixOffset = length;
                  }
               else
                  {
                  prefixLength = PIECE_LIMIT;
                  suffixOffset = length - PIECE_LIMIT;
                  etc = "\"...\"";
                  }

               // Stop before any non-printable characters (like newlines or UTF8 weirdness)
               //
               intptr_t i;
               for (i=0; i < prefixLength; i++)
                  if (!isprint(contents[i]))
                     {
                     prefixLength = i;
                     etc = "\"...\"";
                     break;
                     }
               for (i = length-1; i > suffixOffset; i--)
                  if (!isprint(contents[i]))
                     {
                     suffixOffset = i;
                     etc = "\"...\"";
                     break;
                     }
               }
            char *result = (char*)_comp->trMemory()->allocateHeapMemory(length+20);
            sprintf(result, "<string \"%.*s%s%s\">", (int)prefixLength, contents, etc, contents+suffixOffset);
            return result;
            }
#endif
         return "<string>";
         }

      if (sym->isConstMethodType())
         return "<method type>"; // TODO: Print the signature

      if (sym->isConstMethodHandle())
         return "<method handle>"; // TODO: Print some kind of identification

      if (sym->isConstObjectRef())
         return "<constant object ref>";

      if (sym->isConst())
         return "<constant>";

      // Value Type default value instance slot address
      if (sym->isStaticDefaultValueInstance() && staticAddress)
         {
         if (_comp->getOption(TR_MaskAddresses))
            return "*Masked*";

         const uint8_t EXTRA_SPACE = 5;
         char * name = (char *)_comp->trMemory()->allocateHeapMemory(TR::Compiler->debug.pointerPrintfMaxLenInChars()+EXTRA_SPACE);
         sprintf(name, POINTER_PRINTF_FORMAT, staticAddress);
         return name;
         }

      return getOwningMethod(symRef)->staticName(symRef->getCPIndex(), comp()->trMemory());
      }

   if (_comp->getSymRefTab()->isVtableEntrySymbolRef(symRef))
      return "<class_loader>";

#ifdef J9_PROJECT_SPECIFIC
   if (sym->isCallSiteTableEntry())
      {
      char * s = (char *)_comp->trMemory()->allocateHeapMemory(60);
      sprintf(s, "<callSite entry @%d " POINTER_PRINTF_FORMAT ">", sym->castToCallSiteTableEntrySymbol()->getCallSiteIndex(), staticAddress);
      return s;
      }

   if (sym->isMethodTypeTableEntry())
      {
      char * s = (char *)_comp->trMemory()->allocateHeapMemory(62);
      sprintf(s, "<methodType entry @%d " POINTER_PRINTF_FORMAT ">", sym->castToMethodTypeTableEntrySymbol()->getMethodTypeIndex(), staticAddress);
      return s;
      }
#endif

   if (_comp->getSymRefTab()->isConstantAreaSymbol(sym) && sym->isStatic() && sym->isNamed())
      {
      return sym->getName();
      }

   if (staticAddress)
      {
      char * name = (char *)_comp->trMemory()->allocateHeapMemory(TR::Compiler->debug.pointerPrintfMaxLenInChars()+5);
      if (_comp->getOption(TR_MaskAddresses))
         sprintf(name, "*Masked*");
      else
         sprintf(name, POINTER_PRINTF_FORMAT, staticAddress);
      return name;
      }

   return "unknown static";
   }

// Note: This array needs to match up with what is in compile/SymbolReferenceTable.hpp
static const char *commonNonhelperSymbolNames[] =
   {
   "<contiguousArraySize>",
   "<discontiguousArraySize>",
   "<arrayClassRomPtr>",
   "<classRomPtr>",
   "<javaLangClassFromClass>",
   "<classFromJavaLangClass>",
   "<addressOfClassOfMethod>",
   "<ramStaticsFromClass>",
   "<componentClass>",
   "<componentClassAsPrimitive>",
   "<isArray>",
   "<isClassDepthAndFlags>",
   "<initializeStatusFromClass>",
   "<isClassFlags>",
   "<vft>",
   "<currentThread>",
   "<recompilationCounter>",
   "<excp>",
   "<indexableSize>",
   "<resolveCheck>",
   "<arrayTranslate>",
   "<arrayTranslateAndTest>",
   "<long2String>",
   "<bitOpMem>",
   "<reverseLoad>",
   "<reverseStore>",
   "<currentTimeMaxPrecision>",
   "<encodeASCII>",
   "<headerFlags>",
   "<singlePrecisionSQRT>",
   "<threadPrivateFlags>",
   "<arrayletSpineFirstElement>",
   "<dltBlock>",
   "<countForRecompile>",
   "<gcrPatchPoint>",
   "<counterAddress>",
   "<startPC>",
   "<compiledMethod>",
   "<thisRangeExtension>",
   "<profilingBufferCursor>",
   "<profilingBufferEnd>",
   "<profilingBuffer>",
   "<osrBuffer>",
   "<osrScratchBuffer>",
   "<osrFrameIndex>",
   "<osrReturnAddress>",
   "<contiguousArrayDataAddrField>",
   "<potentialOSRPointHelper>",
   "<osrFearPointHelper>",
   "<eaEscapeHelper>",
   "<lowTenureAddress>",
   "<highTenureAddress>",
   "<fragmentParent>",
   "<globalFragment>",
   "<instanceShape>",
   "<instanceDescription>",
   "<descriptionWordFromPtr>",
   "<classFromJavaLangClassAsPrimitive>",
   "<javaVM>",
   "<heapBase>",
   "<heapTop>",
   "<j9methodExtraField>",
   "<j9methodConstantPoolField>",
   "<startPCLinkageInfo>",
   "<instanceShapeFromROMClass>",
   "<objectEqualityComparison>",
   "<objectInequalityComparison>",
   "<nonNullableArrayNullStoreCheck>",
   "<loadFlattenableArrayElementNonHelper>",
   "<storeFlattenableArrayElementNonHelper>",
   "<synchronizedFieldLoad>",
   "<atomicAdd>",
   "<atomicFetchAndAdd>",
   "<atomicFetchAndAdd32Bit>",
   "<atomicFetchAndAdd64Bit>",
   "<atomicSwap>",
   "<atomicSwap32Bit>",
   "<atomicSwap64Bit>",
   "<atomicCompareAndSwapReturnStatus>",
   "<atomicCompareAndSwapReturnValue>",
   "<jProfileValueSymbol>",
   "<jProfileValueWithNullCHKSymbol>",
   "<j9VMThreadTempSlotField>",
   "<computedStaticCallSymbol>",
   "<j9VMThreadFloatTemp1>",
   "<J9JNIMethodIDvTableIndexFieldSymbol>",
   "<defaultValue>",
   "<jitDispatchJ9Method>"
   };

const char *
TR_Debug::getShadowName(TR::SymbolReference * symRef)
   {

   if (symRef->getCPIndex() >= 0 && !symRef->getSymbol()->isArrayShadowSymbol())
      return getOwningMethod(symRef)->fieldName(symRef->getCPIndex(), comp()->trMemory());

   if (symRef->getSymbol() == _comp->getSymRefTab()->findGenericIntShadowSymbol())
      {
      if (symRef->reallySharesSymbol(_comp))
         {
         return "<generic int shadow>";
         }
      else
         {
         return "<immutable generic int shadow>";
         }
      }

   if (_comp->getSymRefTab()->isVtableEntrySymbolRef(symRef))
      return "<vtable-entry-symbol>";

   if (symRef->getSymbol()->isUnsafeShadowSymbol())
      return "<unsafe shadow sym>";

   if (symRef == _comp->getSymRefTab()->findHeaderFlagsSymbolRef())
      return "<object header flag word>";

   if (symRef->getSymbol())
      {
      if (comp()->getSymRefTab()->isRefinedArrayShadow(symRef))
         return "<refined-array-shadow>";

      if (comp()->getSymRefTab()->isImmutableArrayShadow(symRef))
         return "<immutable-array-shadow>";

      if(symRef->getSymbol()->isArrayletShadowSymbol())
         return "<arraylet-shadow>";

      if(symRef->getSymbol()->isGlobalFragmentShadowSymbol())
         return "<global-fragmnet>";

      if(symRef->getSymbol()->isMemoryTypeShadowSymbol())
         return "<memory-type>";

      if (symRef->getSymbol()->isNamedShadowSymbol())
         {
         return symRef->getSymbol()->getNamedShadowSymbol()->getName();
         }
      }

   const int32_t numCommonNonhelperSymbols = TR::SymbolReferenceTable::lastCommonNonhelperSymbol - TR::SymbolReferenceTable::firstCommonNonhelperNonArrayShadowSymbol - TR_numCCPreLoadedCode;
   static_assert (sizeof(commonNonhelperSymbolNames)/sizeof(commonNonhelperSymbolNames[0]) == numCommonNonhelperSymbols,
      "commonNonhelperSymbolNames array must match CommonNonhelperSymbol enumeration");

   for (int32_t i = TR::SymbolReferenceTable::firstCommonNonhelperNonArrayShadowSymbol; i < _comp->getSymRefTab()->getLastCommonNonhelperSymbol(); i++)
      {
      TR::SymbolReference *other = _comp->getSymRefTab()->element((TR::SymbolReferenceTable::CommonNonhelperSymbol)i);
      if (other && other->getSymbol() == symRef->getSymbol())
         return commonNonhelperSymbolNames[i - TR::SymbolReferenceTable::firstCommonNonhelperNonArrayShadowSymbol];
      }

   return "unknown field";
   }

const char *
TR_Debug::getMetaDataName(TR::SymbolReference * symRef)
   {
   const char *name = symRef->getSymbol()->getMethodMetaDataSymbol()->getName();
   return name ? name : "method meta data";
   }

void
TR_Debug::printBlockInfo(TR::FILE *pOutFile, TR::Node *node)
   {
   if (node)
      {
      if (node->getOpCodeValue() == TR::BBStart)
         {
         trfprintf(pOutFile, " BBStart");

         TR::Block *block = node->getBlock();
         if (block->getNumber() >= 0)
            trfprintf(pOutFile, " <block_%d>",block->getNumber());
         if (block->getFrequency()>=0)
            trfprintf(pOutFile, " (frequency %d)",block->getFrequency());
         if (block->isExtensionOfPreviousBlock())
            trfprintf(pOutFile, " (extension of previous block)");
         if (block->isCatchBlock())
            {
            int32_t length;
            const char *classNameChars = block->getExceptionClassNameChars();
            if (classNameChars)
               {
               length = block->getExceptionClassNameLength();
               trfprintf(pOutFile, " (catches %.*s)", length, getName(classNameChars, length));
               }
            else
               {
               classNameChars = "...";
               length = 3;
               trfprintf(pOutFile, " (catches ...)");
               }
            }
         if (block->isSuperCold())
            trfprintf(pOutFile, " (super cold)");
         else if (block->isCold())
            trfprintf(pOutFile, " (cold)");

         if (block->isLoopInvariantBlock())
            trfprintf(pOutFile, " (loop pre-header)");
         TR_BlockStructure *blockStructure = block->getStructureOf();
         if (_comp->getFlowGraph()->getStructure() && blockStructure)
            {
            TR_Structure *parent = blockStructure->getParent();
            while (parent)
               {
               TR_RegionStructure *region = parent->asRegion();
               if (region->isNaturalLoop() ||
                   region->containsInternalCycles())
                  {
                  trfprintf(pOutFile, " (in loop %d)", region->getNumber());
                  break;
                  }
               parent = parent->getParent();
               }
            TR_BlockStructure *dupBlock = blockStructure->getDuplicatedBlock();
            if (dupBlock)
               trfprintf(pOutFile, " (dup of block_%d)", dupBlock->getNumber());
            }
         }
      else if (node && node->getOpCodeValue() == TR::BBEnd)
         {
         trfprintf(pOutFile, " BBEnd");

         TR::Block *block = node->getBlock();

         if (block->getNumber() >= 0)
            trfprintf(pOutFile, " </block_%d>", block->getNumber());
         }
      }
   }

void
TR_Debug::clearNodeChecklist()
   {
   _nodeChecklist.empty();
   }

void
TR_Debug::saveNodeChecklist(TR_BitVector &saveArea)
   {
   saveArea = _nodeChecklist;
   }

void
TR_Debug::restoreNodeChecklist(TR_BitVector &saveArea)
   {
   _nodeChecklist = saveArea;
   }

int32_t
TR_Debug::dumpLiveRegisters()
   {
   TR::FILE *pOutFile = _comp->getOutFile();
   int i = 0;

   if (pOutFile == NULL)
      return 0;

   trfprintf(pOutFile, "; Live regs:");

   // Dump the number of each kind
   //
   for (i = 0; i < NumRegisterKinds; i++)
      {
      TR_RegisterKinds kind = (TR_RegisterKinds)i;
      TR_LiveRegisters *liveRegisters = _comp->cg()->getLiveRegisters(kind);
      if (liveRegisters)
         trfprintf(pOutFile, " %s=%d", getRegisterKindName(kind), liveRegisters->getNumberOfLiveRegisters());
      }

   trfprintf(pOutFile, " {");

   // Dump all the registers of each kind
   //
   const char *separator = "";
   for (i = 0; i < NumRegisterKinds; i++)
      {
      TR_RegisterKinds kind = (TR_RegisterKinds)i;
      TR_LiveRegisters *liveRegisters = _comp->cg()->getLiveRegisters(kind);
      if (liveRegisters)
         {
         for (TR_LiveRegisterInfo *info = liveRegisters->getFirstLiveRegister(); info; info = info->getNext())
            {
            trfprintf(pOutFile, "%s%s", separator, getName(info->getRegister()));
            separator = ", ";
            }
         }
      }
   trfprintf(pOutFile, "}");
   return 0;
   }

int32_t
TR_Debug::dumpLiveRegisters(TR::FILE *pOutFile, TR_RegisterKinds rk)
   {
   if (pOutFile == NULL)
      return 0;

   int32_t n = 0;

   TR::RegisterPair *regPair;
   TR_LiveRegisters *liveReg = _comp->cg()->getLiveRegisters(rk);

   if (!liveReg)
      return 0;

   trfprintf(pOutFile, "Live %s registers:\n", getRegisterKindName(rk));
   for (TR_LiveRegisterInfo *p = liveReg->getFirstLiveRegister(); p; p = p->getNext())
      {
      regPair = p->getRegister()->getRegisterPair();

      ++n;
      if (regPair)
         trfprintf(pOutFile, "\t[" POINTER_PRINTF_FORMAT "] %d:  " POINTER_PRINTF_FORMAT " pair (" POINTER_PRINTF_FORMAT ", " POINTER_PRINTF_FORMAT ")  ",
                 p, n, regPair, regPair->getLowOrder(), regPair->getHighOrder());
      else
         {
         TR::Register *reg = p->getRegister();
         trfprintf(pOutFile, "\t[" POINTER_PRINTF_FORMAT "] %d:  " POINTER_PRINTF_FORMAT "  ", p, n, reg);
         }

      trfprintf(pOutFile, "\n");
      }

   if (n == 0)
      trfprintf(pOutFile, "\tNo live %s.\n", getRegisterKindName(rk));

   return n;
   }

void
TR_Debug::dumpLiveRealRegisters(TR::FILE *pOutFile, TR_RegisterKinds rk)
   {
   if (pOutFile == NULL)
      return;

   TR_RegisterMask  mask = _comp->cg()->getLiveRealRegisters(rk);
   trfprintf(pOutFile, "Live real %s registers:\n\t", getRegisterKindName(rk));
   if (mask)
      printRegisterMask(pOutFile, mask, rk);
   else
      trfprintf(pOutFile, "None");

   trfprintf(pOutFile, "\n");
   }

void
TR_Debug::setupToDumpTreesAndInstructions(const char *title)
   {
   TR::FILE *pOutFile = _comp->getOutFile();
   if (pOutFile == NULL)
      return;

   trfprintf(pOutFile, "\n%s:\n", title);
   _nodeChecklist.empty();
   trfprintf(pOutFile,"\n\n============================================================\n");
   }

void
TR_Debug::dumpSingleTreeWithInstrs(TR::TreeTop     *treeTop,
                                   TR::Instruction *instr,
                                   bool              dumpTrees,
                                   bool              dumpInstrs,
                                   bool              dumpRefCounts,
                                   bool              dumpLiveRegs)
   {
   TR::FILE *pOutFile = _comp->getOutFile();
   if (pOutFile == NULL)
      return;

   if (dumpLiveRegs)
      {
      dumpLiveRegisters();
      trfprintf(pOutFile, "\n------------------------------\n");
      }

   if (dumpTrees)
      printWithFixedPrefix(pOutFile, treeTop->getNode(), 1, true, dumpRefCounts, " ");

   if (dumpInstrs)
      {
      trfprintf(pOutFile, "\n------------------------------\n");
      if (treeTop->getLastInstruction())
         {
         TR::Instruction *cursor = instr;
         while (cursor)
            {
            print(pOutFile, cursor);
            if (cursor == treeTop->getLastInstruction())
               break;
            cursor = cursor->getNext();
            }

#if defined(TR_TARGET_POWER) || defined(TR_TARGET_S390)
         // Loop a second time and dump outlined code
         TR::Instruction *lastOutlinedTarget = NULL;
         cursor = instr;
         while (cursor)
            {
#if defined(TR_TARGET_POWER)
            TR::Instruction *outlinedCursor = getOutlinedTargetIfAny(cursor);
#elif defined(TR_TARGET_S390)
            TR::Instruction *outlinedCursor = getOutlinedTargetIfAny(cursor);
#endif

            if (outlinedCursor)
               {
               // Separator between main line code and outlined code
               if (!lastOutlinedTarget)
                  trfprintf(pOutFile, "\n\n------------------------------\n");
               if (outlinedCursor != lastOutlinedTarget)
                  {
                  // Avoiding dumping the same outlined code sequence more than once by keeping track of the last sequence we dumped
                  // Not fool-proof, but cheap
                  lastOutlinedTarget = outlinedCursor;
                  while (outlinedCursor)
                     {
                     print(pOutFile, outlinedCursor);
                     outlinedCursor = outlinedCursor->getNext();
                     }
                  }
               }
            if (cursor == treeTop->getLastInstruction())
               break;
            cursor = cursor->getNext();
            }
#endif
         }
      trfprintf(pOutFile, "\n\n============================================================\n");
      }
   }

void
TR_Debug::dumpMethodInstrs(TR::FILE *pOutFile, const char *title, bool dumpTrees, bool header)
   {
   const char * methodName = NULL;
   int32_t bfLineNo = -1;
   int32_t efLineNo = -1;
   int16_t sourceFileIndex = -1; // WCode related (for now)
   int16_t lastSourceFileIndex = -1; // ditto
   int32_t inlinedIndex = -1; // ditto
   int16_t lastInlinedIndex = -1; // ditto
   bool printLinenos = false;
   bool printBIandEI = false;

   if (pOutFile == NULL)
      return;

   const char *hotnessString = _comp->getHotnessName(_comp->getMethodHotness());

   trfprintf(pOutFile,"\n<instructions\n"
                 "\ttitle=\"%s\"\n"
                 "\tmethod=\"%s\"\n"
                 "\thotness=\"%s\">\n",
                 title, signature(_comp->getMethodSymbol()), hotnessString);

   if (header)
      printInstrDumpHeader(title);

   TR::Instruction *instr = _comp->cg()->getFirstInstruction();
   if (dumpTrees)
      {
      _nodeChecklist.empty();
      trfprintf(pOutFile,"\n\n============================================================\n");
      for ( TR::TreeTop *treeTop = _comp->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
         {
         printWithFixedPrefix(_comp->getOutFile(), treeTop->getNode(), 1, true, false, " ");
         if (treeTop->getLastInstruction())
            {
            trfprintf(pOutFile,"\n------------------------------\n");
            while (instr)
               {
               print(pOutFile, instr);
               if (instr == treeTop->getLastInstruction())
                  break;
               instr = instr->getNext();
               }
            instr = instr->getNext();
            trfprintf(pOutFile,"\n\n============================================================\n");
            }
         else
            trfprintf(pOutFile,"\n");
         }
      }


   int lastLineNoPrinted = -1;

   while (instr)
      {
      // printLinenos is true for PPC, WCode, and -qlinedebug
      if (printLinenos){
        int32_t crtLineNo = -1;
        TR::Node *node = instr->getNode();
        if (!node)
           {
           // Current instruction has no node - so we have no location.
           // To handle, search forward in stream to find a node and
           // use that node's location for this instruction.  This works around
           // the problem where the platform generates instructions with no
           // node information. Having the platform reduce instructions with no
           // nodes could provide better location information.
           TR::Instruction *searchInstr = instr->getNext();
           for (; searchInstr && !node; searchInstr = searchInstr->getNext())
              node = searchInstr->getNode();
           }
        // Ignore BB_End location info and any instructions with null nodes
        if (node && (node->getOpCodeValue() != TR::BBEnd))
           {
           TR_ByteCodeInfo &byteCodeInfo = node->getByteCodeInfo();
           }
        else
           {
           crtLineNo = lastLineNoPrinted;
           }

        bool printEveryLine = _comp->target().cpu.isZ() && _comp->target().isLinux();
        // Emit .line directive if there was change in line number, or source file or
        // inlined method state since last time
        if ((crtLineNo != -1) &&
              ( (crtLineNo != lastLineNoPrinted) ||
              (inlinedIndex != lastInlinedIndex) ||
              (sourceFileIndex != lastSourceFileIndex) ||
              printEveryLine)) {
           // Following is somewhat PPC specific - line numbers for "primary" source are relative to
           // the first line number of the method. Line numbrers for includes are absoluted .
           // Since we treat Inlined methods (like include files - see previous), such line numbers
           // are absolute as well.

           int32_t lno;

           if (_comp->target().cpu.isZ() && _comp->target().isLinux())
              {
              lno = crtLineNo - 1;
              }
           else
              {
              lno = ((sourceFileIndex == 0) && (inlinedIndex == -1)) ? (crtLineNo - bfLineNo + 1) : crtLineNo;
              }

           if (lno > 0) // this check is workaround for front end problem with line numbers
              {
              trfprintf(pOutFile, "\n\t.line \t %d", lno);
              trfflush(_comp->getOutFile());
              lastLineNoPrinted = crtLineNo;
              }
           lastSourceFileIndex = sourceFileIndex;
           lastInlinedIndex = inlinedIndex;
        }
      }

      print(pOutFile, instr, title);


      instr = instr->getNext();
      }

      // Dump OOL instruction sequences.
      //
#if defined(TR_TARGET_POWER) || defined(TR_TARGET_S390) || defined(TR_TARGET_ARM64)
      // After RA outlined code is linked to the regular instruction stream and will automatically be printed
      if (_comp->cg()->getCodeGeneratorPhase() < TR::CodeGenPhase::RegisterAssigningPhase)
#if defined(TR_TARGET_POWER)
         printPPCOOLSequences(pOutFile);
#elif defined(TR_TARGET_ARM64)
         printARM64OOLSequences(pOutFile);
#else
         printS390OOLSequences(pOutFile);
#endif
#elif defined(TR_TARGET_X86)
      if (_comp->target().cpu.isX86())
         printX86OOLSequences(pOutFile);
#endif

   trfprintf(pOutFile,"\n</instructions>\n");

   }

void
TR_Debug::dumpMixedModeDisassembly()
   {
   TR::FILE *pOutFile = _comp->getOutFile();
   if (pOutFile == NULL)
      return;

   const char * title = "Mixed Mode Disassembly";

   trfprintf(pOutFile,"<instructions\n"
                 "\ttitle=\"%s\"\n"
                 "\tmethod=\"%s\">\n",
                 title, signature(_comp->getMethodSymbol()));

   TR::Node * n = 0;
   TR::Instruction * inst = _comp->cg()->getFirstInstruction();
   for (; inst; inst = inst->getNext())
      {
      if (inst->getNode()!=NULL &&
          (!n ||
           ((inst->getNode()->getByteCodeInfo().getCallerIndex() != n->getByteCodeInfo().getCallerIndex() || inst->getNode()->getByteCodeInfo().getByteCodeIndex() != n->getByteCodeInfo().getByteCodeIndex()) &&
            inst->getBinaryLength() > 0)))
         {
         n = inst->getNode();

         trfprintf(pOutFile, "\n\n");
         char * indent = (char *)_comp->trMemory()->allocateHeapMemory(6 + 3*(_comp->getMaxInlineDepth()+1));
         printByteCodeStack(n->getInlinedSiteIndex(), n->getByteCodeIndex(), indent);
         }

      print(pOutFile, inst);
      }
   trfprintf(pOutFile,"\n</instructions>\n");

   trfprintf(pOutFile,"<snippets>");
   print(pOutFile, _comp->cg()->getSnippetList());
   trfprintf(pOutFile,"\n</snippets>\n");
   }


void
TR_Debug::dumpInstructionComments(TR::FILE *pOutFile, TR::Instruction *instr, bool needsStartComment)
   {
   TR_ASSERT(_comp, "Required compilation object is NULL.\n");

   CS2::HashIndex hashIndex;
   if (_comp->getToCommentMap().Locate((void *)instr, hashIndex))
      {

      char* data;
      List<char> *comments = (List<char>*)_comp->getToCommentMap().DataAt(hashIndex);
      ListIterator<char> itr(comments);
      // If this is the first annotation, need to add space and semicolon
      if (itr.getFirst() && needsStartComment)
         {
         trfprintf(pOutFile, " ;");
         needsStartComment = false;
         }

      // Dump out all comments added to the instruction _comp->getToCommentMap() hashtable
      for(data=itr.getFirst(); data!=NULL; data= itr.getNext()) trfprintf(pOutFile, " %s", data);

      }
   }

#if !defined(TR_TARGET_POWER) && !defined(TR_TARGET_ARM) && !defined(TR_TARGET_ARM64) && !defined(TR_TARGET_RISCV)
void
TR_Debug::print(TR::FILE *pOutFile, TR::Instruction * inst)
   {
      print (pOutFile, inst, NULL);
   }
#endif

void
TR_Debug::print(TR::FILE *pOutFile, TR::Instruction * inst, const char *title)
   {
   if (pOutFile == NULL)
      return;

#if defined(TR_TARGET_X86)
   if (_comp->target().cpu.isX86())
      {
      printx(pOutFile, inst);
      return;
      }
#endif

#if defined(TR_TARGET_POWER)
   if (_comp->target().cpu.isPower())
      {
      print(pOutFile, inst);
      return;
      }
#endif

#if defined(TR_TARGET_ARM)
   if (_comp->target().cpu.isARM())
      {
      print(pOutFile, inst);
      return;
      }
#endif

#if defined(TR_TARGET_ARM64)
   if (_comp->target().cpu.isARM64())
      {
      print(pOutFile, inst);
      return;
      }
#endif

#if defined(TR_TARGET_RISCV)
   if (TR::Compiler->target.cpu.isRISCV())
      {
      print(pOutFile, inst);
      return;
      }
#endif

#if defined(TR_TARGET_S390)
   if (_comp->target().cpu.isZ())
      {
      printz(pOutFile, inst, title);
      return;
      }
#endif

   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::GCRegisterMap * map)
   {
   if (pOutFile == NULL)
      return;

#if defined(TR_TARGET_X86)
   if (_comp->target().cpu.isX86())
      {
      printX86GCRegisterMap(pOutFile, map);
      return;
      }
#endif

#if defined(TR_TARGET_POWER)
   if (_comp->target().cpu.isPower())
      {
      printPPCGCRegisterMap(pOutFile, map);
      return;
      }
#endif

#if defined(TR_TARGET_ARM)
   if (_comp->target().cpu.isARM())
      {
      printARMGCRegisterMap(pOutFile, map);
      return;
      }
#endif

#if defined(TR_TARGET_S390)
   if (_comp->target().cpu.isZ())
      {
      printS390GCRegisterMap(pOutFile, map);
      return;
      }
#endif

#if defined(TR_TARGET_ARM64)
   if (_comp->target().cpu.isARM64())
      {
      printARM64GCRegisterMap(pOutFile, map);
      return;
      }
#endif

#if defined(TR_TARGET_RISCV)
   if (TR::Compiler->target.cpu.isRISCV())
      {
      printRVGCRegisterMap(pOutFile, map);
      return;
      }
#endif

   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::list<TR::Snippet*> & snippetList)
   {
   if (pOutFile == NULL)
      return;

   for (auto snippets = snippetList.begin(); snippets != snippetList.end(); ++snippets)
      {
      print(pOutFile, *snippets);
      }

   if (_comp->cg()->hasDataSnippets())
      _comp->cg()->dumpDataSnippets(pOutFile);
   }


void
TR_Debug::print(TR::FILE *pOutFile, List<TR::Snippet> & snippetList)
   {
   if (pOutFile == NULL)
      return;

   ListIterator<TR::Snippet> snippets(&snippetList);
   for (TR::Snippet * snippet = snippets.getFirst(); snippet; snippet = snippets.getNext())
      {
      print(pOutFile, snippet);
      }

   if (_comp->cg()->hasDataSnippets())
      _comp->cg()->dumpDataSnippets(pOutFile);

   trfprintf(pOutFile, "\n");
   }

const char *
TR_Debug::getName(TR::Snippet *snippet)
   {
#if defined(TR_TARGET_X86)
   if (_comp->target().cpu.isX86())
      return getNamex(snippet);
#endif
#if defined(TR_TARGET_ARM)
   if (_comp->target().cpu.isARM())
      return getNamea(snippet);
#endif
#if defined(TR_TARGET_ARM64)
   if (_comp->target().cpu.isARM64())
      return getNamea64(snippet);
#endif
   return "<unknown snippet>"; // TODO: Return a more informative name
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::Snippet * snippet)
   {
#if defined(TR_TARGET_X86)
   if (_comp->target().cpu.isX86())
      {
      printx(pOutFile, snippet);
      return;
      }
#endif
#if defined(TR_TARGET_POWER)
   if (_comp->target().cpu.isPower())
      {
      printp(pOutFile, snippet);
      return;
      }
#endif
#if defined(TR_TARGET_ARM)
   if (_comp->target().cpu.isARM())
      {
      printa(pOutFile, snippet);
      return;
      }
#endif
#if defined(TR_TARGET_S390)
   if (_comp->target().cpu.isZ())
      {
      printz(pOutFile, (TR::Snippet *)snippet);
      return;
      }
#endif
#if defined(TR_TARGET_ARM64)
   if (_comp->target().cpu.isARM64())
      {
      printa64(pOutFile, snippet);
      return;
      }
#endif

   }

const char *
TR_Debug::getRealRegisterName(uint32_t regNum)
   {
//need to add 1 to regNum because the enum has 0 = NoReg, and we want 0 to = GPR0
#if defined(TR_TARGET_X86)
   return getName(regNum+1);
#endif
#if defined(TR_TARGET_POWER)
   //based on formula: 31 - regNum + FirstGPR, where FirstGPR=gp0=1
   return getPPCRegisterName(31-regNum+1);
#endif
#if defined(TR_TARGET_ARM)
   return getName(regNum+1);
#endif
#if defined(TR_TARGET_ARM64)
   return getARM64RegisterName(regNum+1);
#endif
#if defined(TR_TARGET_S390)
   return getS390RegisterName(regNum+1);
#endif
   return "???";
   }

const char *
TR_Debug::getName(TR::Register *reg, TR_RegisterSizes size)
   {
   TR_ASSERT(_comp, "Required compilation object is NULL.\n");
   if (!reg)
      return "(null)";

   if (reg->getRealRegister())
      {
#if defined(TR_TARGET_X86)
      if (_comp->target().cpu.isX86())
         return getName((TR::RealRegister *)reg, size);
#endif
#if defined(TR_TARGET_POWER)
      if (_comp->target().cpu.isPower())
         return getName((TR::RealRegister *)reg, size);
#endif
#if defined(TR_TARGET_ARM)
      if (_comp->target().cpu.isARM())
         return getName((TR::RealRegister *)reg, size);
#endif
#if defined(TR_TARGET_S390)
      if (_comp->target().cpu.isZ())
         return getName(toRealRegister(reg), size);
#endif
#if defined(TR_TARGET_ARM64)
      if (_comp->target().cpu.isARM64())
         return getName((TR::RealRegister *)reg, size);
#endif
#if defined(TR_TARGET_RISCV)
      if (TR::Compiler->target.cpu.isRISCV())
         return getName((TR::RealRegister *)reg, size);
#endif

      TR_ASSERT(0, "TR_Debug::getName() ==> unknown target platform for given real register\n");
      }

   if (_comp->getAddressEnumerationOption(TR_EnumerateRegister) && reg == _comp->cg()->getVMThreadRegister())
      return "GPR_0000";

   const int32_t maxPrefixSize = 4;
   char prefix[maxPrefixSize + 1];
   if (reg->getRegisterPair())
      {
      // Prefix checking for register pairs is a bit complex, and likely irrelevant since they
      // can't contain collected refs or internal pointers anyway.  Let's not worry about it.
      prefix[0] = '\0';
      }
   else
      {
      sprintf(prefix, "%s%s%s",
         reg->containsCollectedReference() ? "&" : "",
         reg->containsInternalPointer()    ? "*" : "",
         reg->isPlaceholderReg()           ? "D_" : "");
      }

   CS2::HashIndex hashIndex;
   if (_comp->getToStringMap().Locate((void *)reg, hashIndex))
      {
      const char *name = (const char *)_comp->getToStringMap().DataAt(hashIndex);
      if (!strncmp(name, prefix, strlen(prefix)))
         return name;
      //
      // Prefix mismatch means some important flags have changed since the name
      // string was generated.  Ignore the cached string and generate a new one.
      }

   if (reg->getRegisterPair())
      {
      const char *high = getName(reg->getHighOrder());
      const char *low  = getName(reg->getLowOrder());
      char *buf = (char *)_comp->trMemory()->allocateHeapMemory(strlen(high) + strlen(low) + 2);
      sprintf(buf, "%s:%s", high, low);
      _comp->getToStringMap().Add((void *)reg, buf);
      return buf;
      }
   else if (_comp->getAddressEnumerationOption(TR_EnumerateRegister) && _comp->getToNumberMap().Locate((void *)reg, hashIndex))
      {
      char *buf = (char *)_comp->trMemory()->allocateHeapMemory(maxPrefixSize + 6 + 11); // max register kind name size plus underscore plus 10-digit reg num plus null terminator
      uint32_t regNum = (uint32_t)(intptr_t)_comp->getToNumberMap().DataAt(hashIndex);
      sprintf(buf, "%s%s_%04d", prefix, getRegisterKindName(reg->getKind()), regNum);
      _comp->getToStringMap().Add((void *)reg, buf);
      return buf;
      }
   else
      {
      char *buf = (char *)_comp->trMemory()->allocateHeapMemory(maxPrefixSize + 6 + TR::Compiler->debug.pointerPrintfMaxLenInChars() + 1);
      if (_comp->getOption(TR_MaskAddresses))
         sprintf(buf, "%s%s_*Masked*", prefix, getRegisterKindName(reg->getKind()));
      else
         sprintf(buf, "%s%s_" POINTER_PRINTF_FORMAT, prefix, getRegisterKindName(reg->getKind()), reg);
      _comp->getToStringMap().Add((void *)reg, buf);
      return buf;
      }
   }

const char *TR_Debug::getGlobalRegisterName(TR_GlobalRegisterNumber regNum, TR_RegisterSizes size)
   {
   uint32_t realRegNum = _comp->cg()->getGlobalRegister(regNum);
#if defined(TR_TARGET_X86)
   if (_comp->target().cpu.isX86())
      return getName(realRegNum, size);
#endif
#if defined(TR_TARGET_POWER)
   if (_comp->target().cpu.isPower())
      return getPPCRegisterName(realRegNum);
#endif
#if defined(TR_TARGET_S390)
   if (_comp->target().cpu.isZ())
      return getS390RegisterName(realRegNum);
#endif
#if defined(TR_TARGET_ARM)
   if (_comp->target().cpu.isARM())
      return getName(realRegNum, size);
#endif
#if defined(TR_TARGET_ARM64)
   if (_comp->target().cpu.isARM64())
      return getARM64RegisterName(realRegNum, size);
#endif
   return "???";
   }

const char *
TR_Debug::getRegisterKindName(TR_RegisterKinds rk)
   {
   switch (rk)
      {
      case TR_GPR:   return "GPR";
      case TR_FPR:   return "FPR";
      case TR_VMR:   return "VMR";
      case TR_CCR:   return "CCR";
      case TR_X87:   return "X87";
      case TR_VRF:   return "VRF";
      case TR_VSX_SCALAR:   return "VSX_SCALAR";
      case TR_VSX_VECTOR:   return "VSX_VECTOR";
      case TR_SSR:   return "SSR";
      default:       return "??R";
      }
   }

void
TR_Debug::printRegisterMask(TR::FILE *pOutFile, TR_RegisterMask mask, TR_RegisterKinds rk)
   {
   if (pOutFile == NULL)
      return;

   mask = mask & (TR::RealRegister::getAvailableRegistersMask(rk));
   int32_t n = populationCount(mask);

   TR::RealRegister *reg;
   if (mask)
      {
      TR_RegisterMask slider = 1;
      while (slider)
         {
         if (mask & slider)
            {
            reg = TR::RealRegister::regMaskToRealRegister(slider, rk, _comp->cg());
            trfprintf(pOutFile, "%s", getName(reg));
            if (--n == 0)
               break;
            else
               trfprintf(pOutFile, " ");
            }

         slider <<= 1;
         }
      }
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::Register * reg, TR_RegisterSizes size)
   {
   if (pOutFile == NULL)
      return;
#if defined(TR_TARGET_S390)
   if (reg == NULL && _comp->target().cpu.isZ()) // zero based ptr
      {
      trfprintf(pOutFile, "%s", "GPR0");
      return;
      }
#endif

   if (reg->getRealRegister())
      {
#if defined(TR_TARGET_X86)
      if (_comp->target().cpu.isX86())
         {
         print(pOutFile, (TR::RealRegister *)reg, size);
         return;
         }
#endif
#if defined(TR_TARGET_POWER)
      if (_comp->target().cpu.isPower())
         {
         print(pOutFile, (TR::RealRegister *)reg, size);
         return;
         }
#endif
#if defined(TR_TARGET_ARM)
      if (_comp->target().cpu.isARM())
         {
         print(pOutFile, (TR::RealRegister *)reg, size);
         return;
         }
#endif
#if defined(TR_TARGET_S390)
      if (_comp->target().cpu.isZ())
         {
         print(pOutFile, toRealRegister(reg), size);
         return;
         }
#endif
#if defined(TR_TARGET_ARM64)
      if (_comp->target().cpu.isARM64())
         {
         print(pOutFile, (TR::RealRegister *)reg, size);
         return;
         }
#endif
#if defined(TR_TARGET_RISCV)
      if (TR::Compiler->target.cpu.isRISCV())
         {
         print(pOutFile, (TR::RealRegister *)reg, size);
         return;
         }
#endif
      }
   else
      {
      trfprintf(pOutFile, getName(reg));
      if (reg->getRegisterPair())
         {
         trfprintf(pOutFile, "(");
         print(pOutFile, reg->getHighOrder());
         trfprintf(pOutFile, ":");
         print(pOutFile, reg->getLowOrder());
         trfprintf(pOutFile, ")");
         }
      }
   }

#if 0
/* TODO: Work in progress, column based, side-by-side traceRA={*} format that looks like this (note order will change, columns will widen etc):
   Initially for Java on z, then sTR on z, then Java on other platforms.
   S = State; W = Weight; V = Virtual Register; RC = Ref Count)
   +---------------------------------+------------------------------+------------------------------+
   | Name   S   W  Flag     V    RC  | Name   S   W Flag   V    RC  | Name   S  W Flag    V    RC  |
   +---------------------------------+------------------------------+------------------------------+
   | GPR0       80 Free              | VRF16     80 Free            | FPR0     80 Free FPR_34  4/5 |
   | GPR1       80 Free              | VRF17     80 Free            | FPR1     80 Free             |
   ..
*/
void TR_Debug::printFullRegInfoNewFormat(TR::FILE *pOutFile, TR::Register * rr)
   {
   if (pOutFile == NULL) return;

   static const char * stateNames[5] = { "Free", "Unlatched", "Assigned", "Blocked", "Locked" };

   TR::Register * vr = rr->getAssignedRegister();

   //                      | Name |Asgnd?| Weight|Flag   |Virtual | Ref Count  |mr|
   trfprintf(pOutFile,"|%-7-%s|%-3-%c|%-6-%4x|%-11-%s|%-18-%s-|%-13-%5d/%5d|%d|",
         getName(reg),
         rr->getRealRegister()->getAssignedRegister()? 'A':' ',
         rr->getRealRegister()->getWeight(),
         stateNames[rr->getRealRegister()->getState()],
         rr->getAssignedRegister() ? getName(rr->getAssignedRegister()) : " ",
         vr->getTotalUseCount(), vr->getFutureUseCount(),
         vr->isUsedInMemRef());
   }
#endif

void
TR_Debug::printFullRegInfo(TR::FILE *pOutFile, TR::Register * reg)
   {
   if (pOutFile == NULL)
      return;

   static const char * ignoreFreeRegs = feGetEnv("TR_ignoreFreeRegsDuringTraceRA");
   static const char * ignoreFreeAndLockedRegs = feGetEnv("TR_ignoreFreeAndLockedRegsDuringTraceRA");

   if (reg->getRealRegister())
      {
      if (ignoreFreeRegs && reg->getRealRegister()->getState() == TR::RealRegister::Free)
         return;

      if (ignoreFreeAndLockedRegs &&
            (reg->getRealRegister()->getState() == TR::RealRegister::Locked ||
             reg->getRealRegister()->getState() == TR::RealRegister::Free))
         return;

      static const char * stateNames[5] = { "Free", "Unlatched", "Assigned", "Blocked", "Locked" };


      trfprintf(pOutFile, "[ %-4s ]", getName(reg));
      trfprintf(pOutFile, "[%c]", reg->getAssignedRegister() ? 'A':' ');
      trfprintf(pOutFile, "[%4x]", reg->getRealRegister()->getWeight());
      if (reg->getRealRegister()->getState() == TR::RealRegister::Assigned)
         {
         TR::Register *virtReg = reg->getAssignedRegister();
         TR_ASSERT( virtReg, "real register is assigned to an unknown virtual register");
         if (virtReg)
            {
            TR_ASSERT(reg->getRealRegister()->getHasBeenAssignedInMethod(),
                      "Register state is assigned but register has never been assigned in method");
            }
         trfprintf(pOutFile, "[ %-10s ]", getName(virtReg));
         trfprintf(pOutFile, "[%5d/%5d]",
                       virtReg->getFutureUseCount(),
                       virtReg->getTotalUseCount());
#ifdef TR_TARGET_S390
         trfprintf(pOutFile, "[%d]\n", virtReg->isUsedInMemRef());
#else
         trfprintf(pOutFile, "\n");
#endif
         }
      else
         {
         trfprintf(pOutFile, "[ %-10s ]", stateNames[reg->getRealRegister()->getState()]);
         // track restricted regs usage
         if (reg->getRealRegister()->getState() == TR::RealRegister::Locked &&
             reg->getAssignedRegister()!=NULL &&
             reg->getAssignedRegister()!=reg)
            {
            TR::Register *virtReg = reg->getAssignedRegister();
            trfprintf(pOutFile, "[%5d/%5d]",
                          virtReg->getFutureUseCount(),
                          virtReg->getTotalUseCount());


            trfprintf(pOutFile, "[ %-10s ]", getName(virtReg));
            }
         trfprintf(pOutFile, "\n");
         }
      }
   else
      {
      trfprintf(pOutFile, "[ %-12s ][ ", getName(reg));
      if (reg->getAssignedRegister())
         trfprintf(pOutFile, "Assigned  ");
      else if (reg->getFutureUseCount() != 0 && reg->getTotalUseCount() != reg->getFutureUseCount())
         trfprintf(pOutFile, "Spilled   ");
      else
         trfprintf(pOutFile, "Unassigned");

      trfprintf(pOutFile, " ][ ");

      trfprintf(pOutFile, "%-12s", reg->getAssignedRegister() ? getName(reg->getAssignedRegister()) : " ");

      trfprintf(pOutFile, " ][%5d][%5d]\n", reg->getTotalUseCount(), reg->getFutureUseCount());
      }
   }

void
TR_Debug::printGPRegisterStatus(TR::FILE *pOutFile, OMR::MachineConnector *machine)
   {
   if (pOutFile == NULL)
      return;
#if defined(TR_TARGET_S390)
   if (_comp->target().cpu.isZ())
      {
      printGPRegisterStatus(pOutFile, (TR::Machine *)machine);
      return;
      }
#endif
   }
void
TR_Debug::printFPRegisterStatus(TR::FILE *pOutFile, OMR::MachineConnector *machine)
   {
   if (pOutFile == NULL)
      return;
#if defined(TR_TARGET_S390)
   if (_comp->target().cpu.isZ())
      {
      printFPRegisterStatus(pOutFile, (TR::Machine *)machine);
      return;
      }
#endif
   }

const char *
TR_Debug::toString(TR_RematerializationInfo * info)
   {
   if (info->isRematerializableFromConstant())
      return "constant load";

    TR::Symbol *sym = info->getSymbolReference()->getSymbol();

   if (info->isRematerializableFromMemory())
      {
      if (info->isIndirect())
         return info->isStore() ?  "indirect memory store" : "indirect memory load";

      if (sym->isStatic())
         return info->isStore() ? "static memory store" : "static memory load";
      if (sym->isAutoOrParm())
         return info->isStore() ? "local memory store": "local memory load";
      return info->isStore() ? "memory store": "memory load";
      }
   if (info->isRematerializableFromAddress())
      return sym->isStatic() ? "static address load" : "local address load";
   return "unknown";
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::RegisterMappedSymbol *localCursor, bool isSpillTemp)
   {
   trfprintf(pOutFile, "  Local [%s] (GC map index : %3d, Offset : %3d, Size : %d) is an ", getName(localCursor), localCursor->getGCMapIndex(), localCursor->getOffset(), localCursor->getSize());
   if (!localCursor->isInitializedReference())
      trfprintf(pOutFile, "uninitialized ");
   else
      trfprintf(pOutFile, "initialized ");

   if (localCursor->isCollectedReference())
      trfprintf(pOutFile, "collected ");
   else if (!localCursor->isInternalPointer() &&
            !localCursor->isPinningArrayPointer())
      trfprintf(pOutFile, "uncollected ");

   if (localCursor->isInternalPointer())
      trfprintf(pOutFile, "internal pointer ");
   else if (localCursor->isPinningArrayPointer())
      trfprintf(pOutFile, "pinning array pointer ");

   if (isSpillTemp)
      trfprintf(pOutFile, "spill ");

   if (localCursor->isLocalObject())
      trfprintf(pOutFile, "local object ");

   if (localCursor->isParm())
      trfprintf(pOutFile, "parm ");
   else
      trfprintf(pOutFile, "auto ");

   trfprintf(pOutFile, "\n");
   }

void
TR_Debug::print(TR::FILE *pOutFile, uint8_t* instrStart, uint8_t* instrEnd)
   {
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::GCStackAtlas * atlas)
   {
   if (pOutFile == NULL)
      return;
   trfprintf(pOutFile,"\n<atlas>\n");
   trfprintf(pOutFile,"\nInternal stack atlas:\n");
   trfprintf(pOutFile,"  numberOfMaps=%d\n", atlas->getNumberOfMaps());
   trfprintf(pOutFile,"  numberOfSlotsMapped=%d\n", atlas->getNumberOfSlotsMapped());
   trfprintf(pOutFile,"  numberOfParmSlots=%d\n", atlas->getNumberOfParmSlotsMapped());
   trfprintf(pOutFile,"  parmBaseOffset=%d\n", atlas->getParmBaseOffset());
   trfprintf(pOutFile,"  localBaseOffset=%d\n", atlas->getLocalBaseOffset());
   trfprintf(pOutFile,"\n  Locals information : \n");

   TR::ResolvedMethodSymbol * methodSymbol = _comp->getMethodSymbol();

   ListIterator<TR::ParameterSymbol> parameterIterator(&methodSymbol->getParameterList());
   TR::ParameterSymbol *parmCursor;
   for (parmCursor = parameterIterator.getFirst(); parmCursor; parmCursor = parameterIterator.getNext())
      print(pOutFile, parmCursor, false);

   ListIterator<TR::AutomaticSymbol> automaticIterator(&methodSymbol->getAutomaticList());
   TR::AutomaticSymbol *localCursor;
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      print(pOutFile, localCursor, false);

   // Print the map for spill temps
   //
   for(auto temps = _comp->cg()->getCollectedSpillList().begin(); temps != _comp->cg()->getCollectedSpillList().end(); ++temps)
      {
      TR::AutomaticSymbol *s = (*temps)->getSymbolReference()->getSymbol()->getAutoSymbol();
      print(pOutFile, s, true);
      }

   TR_InternalPointerMap *internalPtrMap = atlas->getInternalPointerMap();
   if (internalPtrMap)
      {
      trfprintf(pOutFile,"\n  Internal pointer autos information:\n");
      int32_t indexOfFirstInternalPtr = atlas->getIndexOfFirstInternalPointer();
      ListElement<TR_InternalPointerPair> *currElement = internalPtrMap->getInternalPointerPairs().getListHead();
      for (;currElement; currElement = currElement->getNextElement())
         {
         int a = -1;
         if (currElement->getData()->getPinningArrayPointer())
            {
            a = currElement->getData()->getPinningArrayPointer()->getGCMapIndex();
            }
         int b = -1;
         if (currElement->getData()->getInternalPointerAuto())
            {
            b = currElement->getData()->getInternalPointerAuto()->getGCMapIndex();
            }
         trfprintf(pOutFile,"    Base array index : %d Internal pointer index : %d\n", a, b);
         }
      }

   if (!atlas->getPinningArrayPtrsForInternalPtrRegs().isEmpty())
      {
      int32_t indexOfFirstInternalPtr = atlas->getIndexOfFirstInternalPointer();
      ListElement<TR::AutomaticSymbol> *currElement = atlas->getPinningArrayPtrsForInternalPtrRegs().getListHead();
      for (;currElement; currElement = currElement->getNextElement())
         trfprintf(pOutFile,"    Base array index : %d pins internal pointers only in regs\n", currElement->getData()->getGCMapIndex());
      }
   else if (!internalPtrMap)
      trfprintf(pOutFile,"\n  No internal pointers in this method\n");

   trfprintf(pOutFile,"\n");

   if (atlas->getStackAllocMap())
      {
      trfprintf(pOutFile, "Stack alloc map size : %d ", atlas->getStackAllocMap()->getMapSizeInBytes());
      trfprintf(pOutFile, "\n  Stack slots containing local objects --> {");
      TR_GCStackAllocMap *stackAllocMap = atlas->getStackAllocMap();

      int mapBytes = (stackAllocMap->_numberOfSlotsMapped + 7) >> 3;
      uint32_t bits = 0;
      bool firstBitOn = true;
      for (int i = 0; i < mapBytes; ++i)
         {
         uint8_t mapByte = stackAllocMap->_mapBits[i];
         for (int j = 0; j < 8; ++j)
            if (bits < stackAllocMap->_numberOfSlotsMapped)
                {
                if (mapByte & 1)
                   {
                   if (!firstBitOn)
                      trfprintf(pOutFile,",%d", bits);
                   else
                      trfprintf(pOutFile,"%d", bits);
                   firstBitOn = false;
                   }
                 //trfprintf(pOutFile,"%d", mapByte & 1);
                 mapByte >>= 1;
                 bits++;
                 }
         }

       trfprintf(pOutFile,"}\n\n");
       }


   int32_t num = 1;
   ListIterator<TR_GCStackMap> maps(&atlas->getStackMapList());
   for (TR_GCStackMap *map = maps.getFirst(); map; map = maps.getNext())
      {
      trfprintf(pOutFile,"  Map number : %d", num);
      print(pOutFile, map, atlas);
      trfprintf(pOutFile,"\n");
      num++;
     }

   trfprintf(pOutFile,"\n</atlas>\n");
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR_GCStackMap * map, TR::GCStackAtlas *atlas)
   {
   if (pOutFile == NULL)
      return;

   trfprintf(pOutFile,"\n  Code offset range starts at [%08x]", map->_lowestCodeOffset);
   trfprintf(pOutFile,"\n  GC stack map information : ");
   trfprintf(pOutFile,"\n    number of stack slots mapped = %d", map->_numberOfSlotsMapped);
   trfprintf(pOutFile,"\n    live stack slots containing addresses --> {");

   int mapBytes = (map->_numberOfSlotsMapped + 7) >> 3;
   uint32_t bits = 0;
   bool firstBitOn = true;
   for (int i = 0; i < mapBytes; ++i)
      {
      uint8_t mapByte = map->_mapBits[i];
      for (int j = 0; j < 8; ++j)
      if (bits < map->_numberOfSlotsMapped)
         {
         if (mapByte & 1)
            {
            if (!firstBitOn)
               trfprintf(pOutFile,",%d", bits);
            else
               trfprintf(pOutFile,"%d", bits);
            firstBitOn = false;
            }
         //trfprintf(pOutFile,"%d", mapByte & 1);
         mapByte >>= 1;
         bits++;
         }
      }

   trfprintf(pOutFile,"}\n");
   trfprintf(pOutFile,"  GC register map information : \n");
   TR_InternalPointerMap *internalPtrMap = map->getInternalPointerMap();
   if (internalPtrMap)
      {
      int32_t indexOfFirstInternalPtr = atlas->getIndexOfFirstInternalPointer();
      trfprintf(pOutFile,"    internal pointer regs information :\n");
      ListElement<TR_InternalPointerPair> *currElement = internalPtrMap->getInternalPointerPairs().getListHead();
      for (;currElement; currElement = currElement->getNextElement())
         trfprintf(pOutFile,"      pinning array GC stack map index = %d Internal pointer regnum = %d\n", currElement->getData()->getPinningArrayPointer()->getGCMapIndex(), currElement->getData()->getInternalPtrRegNum());
      }

   print(pOutFile, &map->_registerMap);
   }

#ifdef J9_PROJECT_SPECIFIC
void
TR_Debug::dump(TR::FILE *pOutFile, TR_CHTable * chTable)
   {
   if (pOutFile == NULL) return;
   const TR::Compilation::GuardSet &vguards = _comp->getVirtualGuards();

   if (!chTable->_preXMethods && !chTable->_classes &&
       vguards.empty()) return;

   trfprintf(pOutFile, "                       Class Hierarchy Assumption Table\n");
   trfprintf(pOutFile, "----------------------------------------------------------------------------------------\n");

   if (!vguards.empty())
      {
      uint8_t *startPC = _comp->cg()->getCodeStart();

      trfprintf(pOutFile, "Following virtual guards are NOPed:\n");
      uint32_t i = 0;
      for (auto info = vguards.begin(); info != vguards.end(); ++info, ++i)
         {
         char guardKindName[49];
         sprintf(guardKindName, "%s %s%s", getVirtualGuardKindName((*info)->getKind()),
            (*info)->mergedWithHCRGuard()?"+ HCRGuard ":"",
            (*info)->mergedWithOSRGuard()?"+ OSRGuard ":"");

         if (!(*info)->getSymbolReference())
            trfprintf(pOutFile, "[%4d] %-49s %s%s\n",
                  i, guardKindName ,(*info)->isInlineGuard()?"inlined ":"", guardKindName);
         else
            trfprintf(pOutFile, "[%4d] %-49s %scalleeSymbol=" POINTER_PRINTF_FORMAT "\n",
                  i, guardKindName ,(*info)->isInlineGuard()?"inlined ":"", (*info)->getSymbolReference()->getSymbol());
         ListIterator<TR_VirtualGuardSite> siteIt(&(*info)->getNOPSites());
         for (TR_VirtualGuardSite *site = siteIt.getFirst(); site; site = siteIt.getNext())
            {
            uint8_t * loc  = site->getLocation();
            uint8_t *&dest = site->getDestination();

            trfprintf(pOutFile, "\tSite: location=" POINTER_PRINTF_FORMAT " (e+%5x) branch-dest=" POINTER_PRINTF_FORMAT " (e+%5x)\n",
                loc, loc - startPC, dest, dest - startPC);
            }
         ListIterator<TR_InnerAssumption> innerIt(&(*info)->getInnerAssumptions());
         for (TR_InnerAssumption *inner = innerIt.getFirst(); inner; inner = innerIt.getNext())
            {
            trfprintf(pOutFile, "\tInner Assumption: calleeSymbol=" POINTER_PRINTF_FORMAT " for parm ordinal=%d\n",
                inner->_guard->getSymbolReference()->getSymbol(), inner->_ordinal);
            }
         }
      }

   if (chTable->_preXMethods)
      {
      trfprintf(pOutFile, "\nOverriding of the following methods will cause a recompilation:\n");

      for (int32_t i = chTable->_preXMethods->lastIndex(); i >= 0; --i)
         {
         TR_ResolvedMethod *method = chTable->_preXMethods->element(i);
         trfprintf(pOutFile, "[%s] %s\n", getName(method), method->signature(comp()->trMemory(), heapAlloc));
         }
      }

   if (chTable->_classes)
      {
      trfprintf(pOutFile, "\nExtension of the following classes will cause a recompilation:\n");

      char  buf[256];
      for (int32_t i = chTable->_classes->lastIndex(); i >= 0; --i)
         {
         TR_OpaqueClassBlock * clazz = chTable->_classes->element(i);
         int32_t len;

         const char *sig = TR::Compiler->cls.classNameChars(comp(), clazz, len);

         if (len>255) len = 255;
         strncpy(buf, sig, len);
         buf[len] = 0;

         trfprintf(pOutFile, "[%s] %s\n", getName(clazz), buf);
         }
      }

   trfprintf(pOutFile, "----------------------------------------------------------------------------------------\n");
   }
#endif

const char *
TR_Debug::getRuntimeHelperName(int32_t index)
   {

#ifdef J9_PROJECT_SPECIFIC
   if (index < TR_FSRH)
      {
      switch (index)
         {
         case TR_checkCast:                 return "jitCheckCast";
         case TR_checkCastForArrayStore:    return "jitCheckCastForArrayStore";
         case TR_instanceOf:                return "jitInstanceOf";
         case TR_checkAssignable:           return "jitCheckAssignable";
         case TR_induceOSRAtCurrentPC:      return "jitInduceOSRAtCurrentPC";
         case TR_induceOSRAtCurrentPCAndRecompile:    return "jitInduceOSRAtCurrentPCAndRecompile";
         case TR_monitorEntry:              return "jitMonitorEntry";
         case TR_methodMonitorEntry:        return "jitMethodMonitorEntry";
         case TR_monitorExit:               return "jitMonitorExit";
         case TR_methodMonitorExit:         return "jitMethodMonitorExit";
         case TR_transactionEntry:          return "transactionEntry";
         case TR_transactionAbort:          return "transactionAbort";
         case TR_transactionExit:           return "transactionExit";
         case TR_asyncCheck:                return "jitCheckAsyncMessages";

         case TR_estimateGPU:               return "estimateGPU";
         case TR_regionEntryGPU:            return "regionEntryGPU";
         case TR_regionExitGPU:             return "regionExitGPU";
         case TR_copyToGPU:                 return "copyToGPU";
         case TR_copyFromGPU:               return "copyFromGPU";
         case TR_allocateGPUKernelParms:    return "allocateGPUKernelParms";
         case TR_launchGPUKernel:           return "launchGPUKernel";
         case TR_invalidateGPU:             return "invalidateGPU";
         case TR_getStateGPU:               return "getStateGPU";
         case TR_flushGPU:                  return "flushGPU";
         case TR_callGPU:                   return "callGPU";

         case TR_newObject:                 return "jitNewObject";
         case TR_newObjectNoZeroInit:       return "jitNewObjectNoZeroInit";
         case TR_newValue:                  return "jitNewValue";
         case TR_newValueNoZeroInit:        return "jitNewValueNoZeroInit";
         case TR_newArray:                  return "jitNewArray";
         case TR_newArrayNoZeroInit:        return "jitNewArrayNoZeroInit";
         case TR_aNewArray:                 return "jitANewArray";
         case TR_aNewArrayNoZeroInit:       return "jitANewArrayNoZeroInit";

         case TR_getFlattenableField:       return "jitGetFlattenableField";
         case TR_withFlattenableField:      return "jitWithFlattenableField";
         case TR_putFlattenableField:       return "jitPutFlattenableField";
         case TR_getFlattenableStaticField: return "jitGetFlattenableStaticField";
         case TR_putFlattenableStaticField: return "jitPutFlattenableStaticField";
         case TR_ldFlattenableArrayElement: return "jitLoadFlattenableArrayElement";
         case TR_strFlattenableArrayElement: return "jitStoreFlattenableArrayElement";

         case TR_multiANewArray:            return "jitAMultiANewArray";
         case TR_aThrow:                    return "jitThrowException";
         case TR_methodTypeCheck:           return "jitThrowWrongMethodTypeException";
         case TR_incompatibleReceiver:      return "jitThrowIncompatibleReceiver";
         case TR_nullCheck:                 return "jitThrowNullPointerException";
         case TR_arrayBoundsCheck:          return "jitThrowArrayIndexOutOfBounds";
         case TR_divCheck:                  return "jitThrowArithmeticException";
         case TR_arrayStoreException:       return "jitThrowArrayStoreException";
         case TR_typeCheckArrayStore:       return "jitTypeCheckArrayStore";
         case TR_softwareReadBarrier:       return "jitSoftwareReadBarrier";
         case TR_writeBarrierStore:         return "jitWriteBarrierStore";
         case TR_writeBarrierStoreGenerational: return "jitWriteBarrierStoreGenerational";
         case TR_writeBarrierStoreGenerationalAndConcurrentMark: return "jitWriteBarrierStoreGenerationalAndConcurrentMark";
         case TR_writeBarrierStoreRealTimeGC:return "jitWriteBarrierStoreRealTimeGC";
         case TR_writeBarrierClassStoreRealTimeGC:return "jitWriteBarrierClassStoreRealTimeGC";
         case TR_writeBarrierBatchStore:    return "jitWriteBarrierBatchStore";
         case TR_stackOverflow:             return "jitStackOverflow";
         case TR_reportMethodEnter:         return "jitReportMethodEnter";
         case TR_reportStaticMethodEnter:   return "jitReportStaticMethodEnter";
         case TR_jitReportInstanceFieldRead: return "jitReportInstanceFieldRead";
         case TR_jitReportStaticFieldRead:  return "jitReportStaticFieldRead";
         case TR_jitReportInstanceFieldWrite:  return "jitReportInstanceFieldWrite";
         case TR_jitReportStaticFieldWrite:  return "jitReportStaticFieldWrite";
         case TR_jitResolveFieldDirect:      return "jitResolveFieldDirect";
         case TR_jitResolveFieldSetterDirect: return "jitResolveFieldSetterDirect";
         case TR_jitResolveStaticFieldDirect: return "jitResolveStaticFieldDirect";
         case TR_jitResolveStaticFieldSetterDirect:  return "jitResolveStaticFieldSetterDirect";
         case TR_reportMethodExit:          return "jitReportMethodExit";
         case TR_reportFinalFieldModified:  return "jitReportFinalFieldModified";
         case TR_acquireVMAccess:           return "jitAcquireVMAccess";
         case TR_jitCheckIfFinalizeObject:  return "jitCheckIfFinalizeObject";
         case TR_releaseVMAccess:           return "jitReleaseVMAccess";
         case TR_throwCurrentException:     return "jitThrowCurrentException";
         case TR_throwClassCastException:   return "jitThrowClassCastException";
         case TR_acmpeqHelper:              return "jitAcmpeqHelper";
         case TR_acmpneHelper:              return "jitAcmpneHelper";

         case TR_IncompatibleClassChangeError:return "jitThrowIncompatibleClassChangeError";
         case TR_newInstanceImplAccessCheck:return "jitNewInstanceImplAccessCheck";

         case TR_icallVMprJavaSendStatic0:                        return "icallVMprJavaSendStatic0";
         case TR_icallVMprJavaSendStatic1:                        return "icallVMprJavaSendStatic1";
         case TR_icallVMprJavaSendStaticJ:                        return "icallVMprJavaSendStaticJ";
         case TR_icallVMprJavaSendStaticF:                        return "icallVMprJavaSendStaticF";
         case TR_icallVMprJavaSendStaticD:                        return "icallVMprJavaSendStaticD";
         case TR_icallVMprJavaSendStaticSync0:                    return "icallVMprJavaSendStaticSync0";
         case TR_icallVMprJavaSendStaticSync1:                    return "icallVMprJavaSendStaticSync1";
         case TR_icallVMprJavaSendStaticSyncJ:                    return "icallVMprJavaSendStaticSyncJ";
         case TR_icallVMprJavaSendStaticSyncF:                    return "icallVMprJavaSendStaticSyncF";
         case TR_icallVMprJavaSendStaticSyncD:                    return "icallVMprJavaSendStaticSyncD";
         case TR_icallVMprJavaSendInvokeExact0:                   return "icallVMprJavaSendInvokeExact0";
         case TR_icallVMprJavaSendInvokeExact1:                   return "icallVMprJavaSendInvokeExact1";
         case TR_icallVMprJavaSendInvokeExactJ:                   return "icallVMprJavaSendInvokeExactJ";
         case TR_icallVMprJavaSendInvokeExactL:                   return "icallVMprJavaSendInvokeExactL";
         case TR_icallVMprJavaSendInvokeExactF:                   return "icallVMprJavaSendInvokeExactF";
         case TR_icallVMprJavaSendInvokeExactD:                   return "icallVMprJavaSendInvokeExactD";
         case TR_icallVMprJavaSendInvokeWithArguments:            return "icallVMprJavaSendInvokeWithArguments";
         case TR_icallVMprJavaSendNativeStatic:                   return "icallVMprJavaSendNativeStatic";
         case TR_j2iTransition:                                   return "j2iTransition";
         case TR_initialInvokeExactThunk_unwrapper:               return "initialInvokeExactThunk_unwrapper";
         case TR_methodHandleJ2I_unwrapper:                       return "methodHandleJ2I_unwrapper";
         case TR_interpreterUnresolvedMethodTypeGlue:             return "interpreterUnresolvedMethodTypeGlue";
         case TR_interpreterUnresolvedMethodHandleGlue:           return "interpreterUnresolvedMethodHandleGlue";
         case TR_interpreterUnresolvedCallSiteTableEntryGlue:     return "interpreterUnresolvedCallSiteTableEntryGlue";
         case TR_interpreterUnresolvedMethodTypeTableEntryGlue:   return "interpreterUnresolvedMethodTypeTableEntryGlue";

         case TR_jitProfileValue:           return "jitProfileValue";
         case TR_jitProfileLongValue:       return "jitProfileLongValue";
         case TR_jitProfileAddress:         return "jitProfileAddress";
         case TR_jitProfileWarmCompilePICAddress: return "jitProfileAddress for mainline code PIC's";
         case TR_jProfile32BitValue:        return "jProfile32BitValue";
         case TR_jProfile64BitValue:        return "jProfile64BitValue";
         case TR_prepareForOSR:             return "prepareForOSR";

         case TR_jitRetranslateCaller:      return "jitRetranslateCaller";
         case TR_jitRetranslateCallerWithPrep:      return "jitRetranslateCallerWithPreparation";

         case TR_volatileReadLong:          return "jitVolatileReadLong";
         case TR_volatileWriteLong:         return "jitVolatileWriteLong";
         case TR_volatileReadDouble:        return "jitVolatileReadDouble";
         case TR_volatileWriteDouble:       return "jitVolatileWriteDouble";
         case TR_referenceArrayCopy:        return "jitReferenceArrayCopy";
         case TR_jitLookupDynamicInterfaceMethod: return "jitLookupDynamicInterfaceMethod";
         case TR_jitLookupDynamicPublicInterfaceMethod: return "jitLookupDynamicPublicInterfaceMethod";
         }
      }
#ifdef TR_TARGET_X86
   else if (_comp->target().cpu.isI386() || _comp->target().cpu.isAMD64())
      {
      if (index < TR_LXRH)
         {
         switch (index)
            {
            case TR_X86resolveIPicClass:                              return "resolveIPicClass";
            case TR_X86populateIPicSlotClass:                         return "populateIPicSlotClass";
            case TR_X86populateIPicSlotCall:                          return "populateIPicSlotCall";
            case TR_X86dispatchInterpretedFromIPicSlot:               return "dispatchInterpretedFromIPicSlot";
            case TR_X86IPicLookupDispatch:                            return "IPicLookupDispatch";
            case TR_X86resolveVPicClass:                              return "resolveVPicClass";
            case TR_X86populateVPicSlotClass:                         return "populateVPicSlotClass";
            case TR_X86populateVPicSlotCall:                          return "populateVPicSlotCall";
            case TR_X86dispatchInterpretedFromVPicSlot:               return "dispatchInterpretedFromVPicSlot";
            case TR_X86populateVPicVTableDispatch:                    return "populateVPicVTableDispatch";
            case TR_X86interpreterUnresolvedStaticGlue:               return "interpreterUnresolvedStaticGlue";
            case TR_X86interpreterUnresolvedSpecialGlue:              return "interpreterUnresolvedSpecialGlue";
            case TR_X86interpreterUnresolvedClassGlue:                return "interpreterUnresolvedClassGlue";
            case TR_X86interpreterUnresolvedClassFromStaticFieldGlue: return "interpreterUnresolvedClassFromStaticFieldGlue";
            case TR_X86interpreterUnresolvedStringGlue:               return "interpreterUnresolvedStringGlue";
            case TR_X86interpreterUnresolvedStaticFieldGlue:          return "interpreterUnresolvedStaticFieldGlue";
            case TR_X86interpreterUnresolvedConstantDynamicGlue:      return "interpreterUnresolvedConstantDynamicGlue";
            case TR_X86interpreterUnresolvedStaticFieldSetterGlue:    return "interpreterUnresolvedStaticFieldSetterGlue";
            case TR_X86interpreterUnresolvedFieldGlue:                return "interpreterUnresolvedFieldGlue";
            case TR_X86interpreterUnresolvedFieldSetterGlue:          return "interpreterUnresolvedFieldSetterGlue";
            case TR_X86CodeCachePrefetchHelper:                       return "per code cache TLH prefetch helper";
            }
         }
      else if (_comp->target().cpu.isI386())
         {
         switch (index)
            {
            case TR_IA32longDivide:                                   return "__longDivide";
            case TR_IA32longRemainder:                                return "__longRemainder";
            case TR_IA32floatRemainder:                               return "__floatRemainder";
            case TR_IA32floatRemainderSSE:                            return "__SSEfloatRemainderIA32Thunk";
            case TR_IA32doubleRemainder:                              return "__doubleRemainder";
            case TR_IA32doubleRemainderSSE:                           return "__SSEdoubleRemainderIA32Thunk";
            case TR_IA32doubleToLong:                                 return "__doubleToLong";
            case TR_IA32doubleToInt:                                  return "__doubleToInt";
            case TR_IA32floatToLong:                                  return "__floatToLong";
            case TR_IA32floatToInt:                                   return "__floatToInt";
            case TR_IA32double2LongSSE:                               return "__SSEdouble2LongIA32";
            case TR_IA32jitCollapseJNIReferenceFrame:                 return "_jitCollapseJNIReferenceFrame";

            case TR_IA32compressString:                               return "_compressString";
            case TR_IA32compressStringNoCheck:                        return "_compressStringNoCheck";
            case TR_IA32compressStringJ:                              return "_compressStringJ";
            case TR_IA32compressStringNoCheckJ:                       return "_compressStringNoCheckJ";
            case TR_IA32andORString:                                  return "_andORString";

            case TR_IA32samplingRecompileMethod:                      return "__samplingRecompileMethod";
            case TR_IA32countingRecompileMethod:                      return "__countingRecompileMethod";
            case TR_IA32samplingPatchCallSite:                        return "__samplingPatchCallSite";
            case TR_IA32countingPatchCallSite:                        return "__countingPatchCallSite";
            case TR_IA32induceRecompilation:                          return "__induceRecompilation";
            }
         }
      else
         {
         switch(index)
            {
            case TR_AMD64floatRemainder:                              return "__SSEfloatRemainder";
            case TR_AMD64doubleRemainder:                             return "__SSEdoubleRemainder";
            case TR_AMD64icallVMprJavaSendVirtual0:                   return "_icallVMprJavaSendVirtual0";
            case TR_AMD64icallVMprJavaSendVirtual1:                   return "_icallVMprJavaSendVirtual1";
            case TR_AMD64icallVMprJavaSendVirtualJ:                   return "_icallVMprJavaSendVirtualJ";
            case TR_AMD64icallVMprJavaSendVirtualL:                   return "_icallVMprJavaSendVirtualL";
            case TR_AMD64icallVMprJavaSendVirtualF:                   return "_icallVMprJavaSendVirtualF";
            case TR_AMD64icallVMprJavaSendVirtualD:                   return "_icallVMprJavaSendVirtualD";
            case TR_AMD64jitCollapseJNIReferenceFrame:                return "_jitCollapseJNIReferenceFrame";

            case TR_AMD64compressString:                               return "_compressString";
            case TR_AMD64compressStringNoCheck:                        return "_compressStringNoCheck";
            case TR_AMD64compressStringJ:                              return "_compressStringJ";
            case TR_AMD64compressStringNoCheckJ:                       return "_compressStringNoCheckJ";
            case TR_AMD64andORString:                                  return "_andORString";

            case TR_AMD64samplingRecompileMethod:                     return "__samplingRecompileMethod";
            case TR_AMD64countingRecompileMethod:                     return "__countingRecompileMethod";
            case TR_AMD64samplingPatchCallSite:                       return "__samplingPatchCallSite";
            case TR_AMD64countingPatchCallSite:                       return "__countingPatchCallSite";
            case TR_AMD64induceRecompilation:                         return "__induceRecompilation";
            case TR_AMD64doAESENCDecrypt:                             return "doAESDecrypt";
            case TR_AMD64doAESENCEncrypt:                             return "doAESEncrypt";
            }
         }
      }
#elif defined (TR_TARGET_POWER)
   else if (_comp->target().cpu.isPower())
      {
      switch (index)
         {
         case TR_PPCdouble2Long:                                   return "__double2Long";
         case TR_PPCdoubleRemainder:                               return "__doubleRemainder";
         case TR_PPCinteger2Double:                                return "__integer2Double";
         case TR_PPClong2Double:                                   return "__long2Double";
         case TR_PPClong2Float:                                    return "__long2Float";
         case TR_PPClong2Float_mv:                                 return "__long2Float_mv";
         case TR_PPClongMultiply:                                  return "<null>";
         case TR_PPClongDivide:                                    return "__longDivide";
         case TR_PPClongDivideEP:                                  return "__longDivideEP";
         case TR_PPCunsignedLongDivide:                            return "__unsignedLongDivide";
         case TR_PPCunsignedLongDivideEP:                          return "__unsignedLongDivideEP";
         case TR_PPCinterpreterUnresolvedStaticGlue:               return "_interpreterUnresolvedStaticGlue";
         case TR_PPCinterpreterUnresolvedSpecialGlue:              return "_interpreterUnresolvedSpecialGlue";
         case TR_PPCinterpreterUnresolvedDirectVirtualGlue:        return "_interpreterUnresolvedDirectVirtualGlue";
         case TR_PPCinterpreterUnresolvedClassGlue:                return "_interpreterUnresolvedClassGlue";
         case TR_PPCinterpreterUnresolvedClassGlue2:               return "_interpreterUnresolvedClassGlue2";
         case TR_PPCinterpreterUnresolvedStringGlue:               return "_interpreterUnresolvedStringGlue";
         case TR_PPCinterpreterUnresolvedConstantDynamicGlue:      return "_interpreterUnresolvedConstantDynamicGlue";
         case TR_PPCinterpreterUnresolvedStaticDataGlue:           return "_interpreterUnresolvedStaticDataGlue";
         case TR_PPCinterpreterUnresolvedStaticDataStoreGlue:      return "_interpreterUnresolvedStaticDataStoreGlue";
         case TR_PPCinterpreterUnresolvedInstanceDataGlue:         return "_interpreterUnresolvedInstanceDataGlue";
         case TR_PPCinterpreterUnresolvedInstanceDataStoreGlue:    return "_interpreterUnresolvedInstanceDataStoreGlue";
         case TR_PPCvirtualUnresolvedHelper:                       return "_virtualUnresolvedHelper";
         case TR_PPCinterfaceCallHelper:                           return "_interfaceCallHelper";
         case TR_PPCrevertToInterpreterGlue:                       return "_revertToInterpreterGlue";
         case TR_PPCarrayTranslateTRTO:                            return "__arrayTranslateTRTO (ISO8859_1 & ASCII)";
         case TR_PPCarrayTranslateTRTO255:                         return "__arrayTranslateTRTO255 (ISO8859_1)";
         case TR_PPCarrayTranslateTROT255:                         return "__arrayTranslateTRTO255 (ISO8859_1)";
         case TR_PPCarrayTranslateTROT:                            return "__arrayTranslateTROT (ASCII)";

         case TR_PPCicallVMprJavaSendVirtual0:                     return "icallVMprJavaSendVirtual0";
         case TR_PPCicallVMprJavaSendVirtual1:                     return "icallVMprJavaSendVirtual1";
         case TR_PPCicallVMprJavaSendVirtualJ:                     return "icallVMprJavaSendVirtualJ";
         case TR_PPCicallVMprJavaSendVirtualF:                     return "icallVMprJavaSendVirtualF";
         case TR_PPCicallVMprJavaSendVirtualD:                     return "icallVMprJavaSendVirtualD";
         case TR_PPCinterpreterVoidStaticGlue:                     return "_interpreterVoidStaticGlue";
         case TR_PPCinterpreterSyncVoidStaticGlue:                 return "_interpreterSyncVoidStaticGlue";
         case TR_PPCinterpreterGPR3StaticGlue:                     return "_interpreterGPR3StaticGlue";
         case TR_PPCinterpreterSyncGPR3StaticGlue:                 return "_interpreterSyncGPR3StaticGlue";
         case TR_PPCinterpreterGPR3GPR4StaticGlue:                 return "_interpreterGPR3GPR4StaticGlue";
         case TR_PPCinterpreterSyncGPR3GPR4StaticGlue:             return "_interpreterSyncGPR3GPR4StaticGlue";
         case TR_PPCinterpreterFPR0FStaticGlue:                    return "_interpreterFPR0FStaticGlue";
         case TR_PPCinterpreterSyncFPR0FStaticGlue:                return "_interpreterSyncFPR0FStaticGlue";
         case TR_PPCinterpreterFPR0DStaticGlue:                    return "_interpreterFPR0DStaticGlue";
         case TR_PPCinterpreterSyncFPR0DStaticGlue:                return "_interpreterSyncFPR0DStaticGlue";
         case TR_PPCnativeStaticHelperForUnresolvedGlue:           return "_nativeStaticHelperForUnresolvedGlue";
         case TR_PPCnativeStaticHelper:                            return "_nativeStaticHelper";

         case TR_PPCinterfaceCompeteSlot2:                         return "_interfaceCompeteSlot2";
         case TR_PPCinterfaceSlotsUnavailable:                     return "_interfaceSlotsUnavailable";
         case TR_PPCcollapseJNIReferenceFrame:                     return "jitCollapseJNIReferenceFrame";
         case TR_PPCarrayCopy:                                     return "__arrayCopy";
         case TR_PPCwordArrayCopy:                                 return "__wordArrayCopy";
         case TR_PPChalfWordArrayCopy:                             return "__halfWordArrayCopy";
         case TR_PPCforwardArrayCopy:                              return "__forwardArrayCopy";
         case TR_PPCforwardWordArrayCopy:                          return "__forwardWordArrayCopy";
         case TR_PPCforwardHalfWordArrayCopy:                      return "__forwardHalfWordArrayCopy";

         case TR_PPCarrayCopy_dp:                                  return "__arrayCopy_dp";
         case TR_PPCwordArrayCopy_dp:                              return "__wordArrayCopy_dp";
         case TR_PPChalfWordArrayCopy_dp:                          return "__halfWordArrayCopy_dp";
         case TR_PPCforwardArrayCopy_dp:                           return "__forwardArrayCopy_dp";
         case TR_PPCforwardWordArrayCopy_dp:                       return "__forwardWordArrayCopy_dp";
         case TR_PPCforwardHalfWordArrayCopy_dp:                   return "__forwardHalfWordArrayCopy_dp";

         case TR_PPCquadWordArrayCopy_vsx:                         return "__quadWordArrayCopy_vsx";
         case TR_PPCforwardQuadWordArrayCopy_vsx:                  return "__forwardQuadWordArrayCopy_vsx";
         case TR_PPCpostP10ForwardCopy:                            return "__postP10ForwardCopy";
         case TR_PPCpostP10GenericCopy:                            return "__postP10GenericCopy";

         case TR_PPCP256Multiply:                                  return "ECP256MUL_PPC";
         case TR_PPCP256Mod:                                       return "ECP256MOD_PPC";
         case TR_PPCP256addNoMod:                                  return "ECP256addNoMod_PPC";
         case TR_PPCP256subNoMod:                                  return "ECP256subNoMod_PPC";

         case TR_PPCencodeUTF16Big:                                return "__encodeUTF16Big";
         case TR_PPCencodeUTF16Little:                             return "__encodeUTF16Little";

         case TR_PPCcompressString:                                 return "__compressString";
         case TR_PPCcompressStringNoCheck:                          return "__compressStringNoCheck";
         case TR_PPCcompressStringJ:                                return "__compressStringJ";
         case TR_PPCcompressStringNoCheckJ:                         return "__compressStringNoCheckJ";
         case TR_PPCandORString:                                    return "__andORString";

         case TR_PPCreferenceArrayCopy:                            return "__referenceArrayCopy";
         case TR_PPCgeneralArrayCopy:                              return "__generalArrayCopy";
         case TR_PPCsamplingRecompileMethod:                       return "__samplingRecompileMethod";
         case TR_PPCcountingRecompileMethod:                       return "__countingRecompileMethod";
         case TR_PPCsamplingPatchCallSite:                         return "__samplingPatchCallSite";
         case TR_PPCcountingPatchCallSite:                         return "__countingPatchCallSite";
         case TR_PPCinduceRecompilation:                           return "__induceRecompilation";
         case TR_PPCarrayXor:                                      return "_arrayxor";
         case TR_PPCarrayOr:                                       return "_arrayor";
         case TR_PPCarrayAnd:                                      return "_arrayand";
         case TR_PPCarrayCmp:                                      return "_arraycmp";
         case TR_PPCoverlapArrayCopy:                              return "overlapArrayCopy";

         case TR_PPCAESEncryptVMX:                                 return "PPCAESEncryptVMX";
         case TR_PPCAESDecryptVMX:                                 return "PPCAESDecryptVMX";
         case TR_PPCAESEncrypt:                                    return "PPCAESEncrypt";
         case TR_PPCAESDecrypt:                                    return "PPCAESDecrypt";
         case TR_PPCAESCBCDecrypt:                                 return "PPCAESCBCDecrypt";
         case TR_PPCAESCBCEncrypt:                                 return "PPCAESCBCEncrypt";
         case TR_PPCAESKeyExpansion:                               return "PPCAESKeyExpansion";
         case TR_PPCcrc32_vpmsum:                                  return "PPCcrc32_vpmsum";
         case TR_PPCcrc32_no_vpmsum:                               return "PPCcrc32_no_vpmsum";
         case TR_PPCcrc32_oneByte:                                 return "PPCcrc32_oneByte";
         }
      }
#elif defined (TR_TARGET_S390)
   else if (_comp->target().cpu.isZ())
      {
      switch (index)
         {
         case TR_S390double2Long:                                  return "__double2Long";
         case TR_S390doubleRemainder:                              return "__doubleRemainder";
         case TR_S390double2Integer:                               return "__double2Integer";
         case TR_S390integer2Double:                               return "__integer2Double";
         case TR_S390intDivide:                                    return "__intDivide";
         case TR_S390long2Double:                                  return "__long2Double";
         case TR_S390longMultiply:                                 return "__longMultiply";
         case TR_S390longDivide:                                   return "__longDivide";
         case TR_S390longRemainder:                                return "__longRemainder";
         case TR_S390longShiftLeft:                                return "__longShiftLeft";
         case TR_S390longShiftRight:                               return "__longShiftRight";
         case TR_S390longUShiftRight:                              return "__longUShiftRight";
         case TR_S390interpreterUnresolvedStaticGlue:              return "_interpreterUnresolvedStaticGlue";
         case TR_S390interpreterUnresolvedSpecialGlue:             return "_interpreterUnresolvedSpecialGlue";
         case TR_S390interpreterUnresolvedClassGlue:               return "_interpreterUnresolvedClassGlue";
         case TR_S390interpreterUnresolvedClassGlue2:              return "_interpreterUnresolvedClassGlue2";
         case TR_S390interpreterUnresolvedStringGlue:              return "_interpreterUnresolvedStringGlue";
         case TR_S390interpreterUnresolvedStaticDataGlue:          return "_interpreterUnresolvedStaticDataGlue";
         case TR_S390interpreterUnresolvedStaticDataStoreGlue:     return "_interpreterUnresolvedStaticDataStoreGlue";
         case TR_S390interpreterUnresolvedInstanceDataGlue:        return "_interpreterUnresolvedInstanceDataGlue";
         case TR_S390interpreterUnresolvedInstanceDataStoreGlue:   return "_interpreterUnresolvedInstanceDataStoreGlue";
         case TR_S390virtualUnresolvedHelper:                      return "_virtualUnresolvedHelper";
         case TR_S390interfaceCallHelper:                          return "_interfaceCallHelper";
         case TR_S390interfaceCallHelperSingleDynamicSlot:         return "_interfaceCallHelperSingleDynamicSlot";
         case TR_S390interfaceCallHelperMultiSlots:                return "_interfaceCallHelperMultiSlots";
         case TR_S390icallVMprJavaSendVirtual0:                    return "icallVMprJavaSendVirtual0";
         case TR_S390icallVMprJavaSendVirtual1:                    return "icallVMprJavaSendVirtual1";
         case TR_S390icallVMprJavaSendVirtualJ:                    return "icallVMprJavaSendVirtualJ";
         case TR_S390icallVMprJavaSendVirtualF:                    return "icallVMprJavaSendVirtualF";
         case TR_S390icallVMprJavaSendVirtualD:                    return "icallVMprJavaSendVirtualD";
         case TR_S390interpreterStaticSpecialCallGlue:             return "_interpreterStaticSpecialCallGlue";
         case TR_S390jitLookupInterfaceMethod:                     return "__jitLookupInterfaceMethod";
         case TR_S390jitMethodIsNative:                            return "__jitMethodIsNative";
         case TR_S390jitMethodIsSync:                              return "__jitMethodIsSync";
         case TR_S390jitResolveClass:                              return "__jitResolveClass";
         case TR_S390jitResolveField:                              return "__jitResolveField";
         case TR_S390jitResolveConstantDynamicGlue:                return "_jitResolveConstantDynamic";
         case TR_S390jitResolveFieldSetter:                        return "__jitResolveFieldSetter";
         case TR_S390jitResolveInterfaceMethod:                    return "__jitResolveInterfaceMethod";
         case TR_S390jitResolveStaticField:                        return "__jitResolveStaticField";
         case TR_S390jitResolveStaticFieldSetter:                  return "__jitResolveStaticFieldSetter";
         case TR_S390jitResolveString:                             return "__jitResolveString";
         case TR_S390jitResolveVirtualMethod:                      return "__jitResolveVirtualMethod";
         case TR_S390jitResolveSpecialMethod:                      return "__jitResolveSpecialMethod";
         case TR_S390jitResolveStaticMethod:                       return "__jitResolveStaticMethod";
         case TR_S390nativeStaticHelper:                           return "_nativeStaticHelper";
         case TR_S390referenceArrayCopyHelper:                     return "__referenceArrayCopyHelper";
         case TR_S390arrayCopyHelper:                              return "__arrayCopyHelper";
         case TR_S390arraySetZeroHelper:                           return "__arraySetZeroHelper";
         case TR_S390arraySetGeneralHelper:                        return "__arraySetGeneralHelper";
         case TR_S390arrayCmpHelper:                               return "__arrayCmpHelper";
         case TR_S390arrayTranslateAndTestHelper:                  return "__arrayTranslateAndTestHelper";
         case TR_S390long2StringHelper:                            return "__long2StringHelper";
         case TR_S390arrayXORHelper:                               return "__arrayXORHelper";
         case TR_S390arrayORHelper:                                return "__arrayORHelper";
         case TR_S390arrayANDHelper:                               return "__arrayANDHelper";
         case TR_S390collapseJNIReferenceFrame:                    return "_collapseJNIReferenceFrame";
         case TR_S390jitRetranslateMethod:                         return "_jitRetranslateMethod";
         case TR_S390countingRecompileMethod:                      return "__countingRecompileMethod";
         case TR_S390samplingRecompileMethod:                      return "__samplingRecompileMethod";
         case TR_S390countingPatchCallSite:                        return "__countingPatchCallSite";
         case TR_S390samplingPatchCallSite:                        return "__samplingPatchCallSite";
         case TR_S390jitProfileAddressC:                           return "_jitProfileAddressC";
         case TR_S390jitProfileValueC:                             return "_jitProfileValueC";
         case TR_S390jitProfileLongValueC:                         return "_jitProfileLongValueC";
         case TR_S390revertToInterpreter:                          return "__revertToInterpreter";
         case TR_S390CEnvironmentAddress:                          return "__CEnvironmentAddress";
         case TR_S390jitPreJNICallOffloadCheck:                    return "_jitPreJNICallOffloadCheck";
         case TR_S390jitPostJNICallOffloadCheck:                   return "_jitPostJNICallOffloadCheck";
         case TR_S390jitMathHelperDREM:                            return "__jitMathHelperDREM";
         case TR_S390floatRemainder:                               return "__floatRemainder";
         case TR_S390jitMathHelperFREM:                            return "__jitMathHelperFREM";
         case TR_S390jitCallCFunction:                             return "__jitCallCFunction";
         case TR_S390mcc_reservationAdjustment_unwrapper:          return "__mcc_reservationAdjustment_unwrapper";
         case TR_S390mcc_callPointPatching_unwrapper:              return "__mcc_callPointPatching_unwrapper";
         case TR_S390mcc_lookupHelperTrampoline_unwrapper:         return "__mcc_lookupHelperTrampoline_unwrapper";
         case TR_S390jitMathHelperConvertLongToFloat:              return "__jitMathHelperConvertLongToFloat";
         case TR_S390induceRecompilation:                          return "__induceRecompilation";
         case TR_S390induceRecompilation_unwrapper:                return "__induceRecompilation_unwrapper";
         case TR_S390OutlinedNew:                                  return "outlinedNew";
         }
      }
#elif defined (TR_TARGET_ARM)
   else if (_comp->target().cpu.isARM())
      {
      switch (index)
         {
         case TR_ARMdouble2Long:                                   return "__double2Long";
         case TR_ARMdoubleRemainder:                               return "__doubleRemainder";
         case TR_ARMdouble2Integer:                                return "__double2Integer";
         case TR_ARMfloat2Long:                                    return "__float2Long";
         case TR_ARMfloatRemainder:                                return "__floatRemainder";
         case TR_ARMinteger2Double:                                return "__integer2Double";
         case TR_ARMlong2Double:                                   return "__long2Double";
         case TR_ARMlong2Float:                                    return "__long2Float";
         case TR_ARMlongMultiply:                                  return "__multi64";
         case TR_ARMintDivide:                                     return "__intDivide";
         case TR_ARMintRemainder:                                  return "__intRemainder";
         case TR_ARMlongDivide:                                    return "__longDivide";
         case TR_ARMlongRemainder:                                 return "__longRemainder";
         case TR_ARMlongShiftRightArithmetic:                      return "__longShiftRightArithmetic";
         case TR_ARMlongShiftRightLogical:                         return "__longShiftRightLogical";
         case TR_ARMlongShiftLeftLogical:                          return "__longShiftLeftLogical";
         case TR_ARMarrayCopy:                                     return "__arrayCopy";
         case TR_ARMjitCollapseJNIReferenceFrame:                  return "jitCollapseJNIReferenceFrame";
         case TR_ARMinterpreterUnresolvedStaticGlue:               return "_interpreterUnresolvedStaticGlue";
         case TR_ARMinterpreterUnresolvedSpecialGlue:              return "_interpreterUnresolvedSpecialGlue";
         case TR_ARMinterpreterUnresolvedDirectVirtualGlue:        return "_interpreterUnresolvedDirectVirtualGlue";
         case TR_ARMinterpreterUnresolvedClassGlue:                return "_interpreterUnresolvedClassGlue";
         case TR_ARMinterpreterUnresolvedClassGlue2:               return "_interpreterUnresolvedClassGlue2";
         case TR_ARMinterpreterUnresolvedStringGlue:               return "_interpreterUnresolvedStringGlue";
         case TR_ARMinterpreterUnresolvedStaticDataGlue:           return "_interpreterUnresolvedStaticDataGlue";
         case TR_ARMinterpreterUnresolvedInstanceDataGlue:         return "_interpreterUnresolvedInstanceDataGlue";
         case TR_ARMinterpreterUnresolvedStaticDataStoreGlue:      return "_interpreterUnresolvedStaticDataStoreGlue";
         case TR_ARMinterpreterUnresolvedInstanceDataStoreGlue:    return "_interpreterUnresolvedInstanceDataStoreGlue";
         case TR_ARMvirtualUnresolvedHelper:                       return "_virtualUnresolvedHelper";
         case TR_ARMinterfaceCallHelper:                           return "_interfaceCallHelper";

         case TR_ARMicallVMprJavaSendVirtual0:                     return "icallVMprJavaSendVirtual0";
         case TR_ARMicallVMprJavaSendVirtual1:                     return "icallVMprJavaSendVirtual1";
         case TR_ARMicallVMprJavaSendVirtualJ:                     return "icallVMprJavaSendVirtualJ";
         case TR_ARMicallVMprJavaSendVirtualF:                     return "icallVMprJavaSendVirtualF";
         case TR_ARMicallVMprJavaSendVirtualD:                     return "icallVMprJavaSendVirtualD";
         case TR_ARMinterpreterVoidStaticGlue:                     return "_interpreterVoidStaticGlue";
         case TR_ARMinterpreterSyncVoidStaticGlue:                 return "_interpreterSyncVoidStaticGlue";
         case TR_ARMinterpreterGPR3StaticGlue:                     return "_interpreterGPR3StaticGlue";
         case TR_ARMinterpreterSyncGPR3StaticGlue:                 return "_interpreterSyncGPR3StaticGlue";
         case TR_ARMinterpreterGPR3GPR4StaticGlue:                 return "_interpreterGPR3GPR4StaticGlue";
         case TR_ARMinterpreterSyncGPR3GPR4StaticGlue:             return "_interpreterSyncGPR3GPR4StaticGlue";
         case TR_ARMinterpreterFPR0FStaticGlue:                    return "_interpreterFPR0FStaticGlue";
         case TR_ARMinterpreterSyncFPR0FStaticGlue:                return "_interpreterSyncFPR0FStaticGlue";
         case TR_ARMinterpreterFPR0DStaticGlue:                    return "_interpreterFPR0DStaticGlue";
         case TR_ARMinterpreterSyncFPR0DStaticGlue:                return "_interpreterSyncFPR0DStaticGlue";
         case TR_ARMnativeStaticHelper:                            return "_nativeStaticHelper";

         case TR_ARMinterfaceCompeteSlot2:                     return "_interfaceCompeteSlot2";
         case TR_ARMinterfaceSlotsUnavailable:                 return "_interfaceSlotsUnavailable";
#ifdef ZAURUS
         case TR_ARMjitMathHelperFloatAddFloat:                return "jitMathHelperFloatAddFloat";
         case TR_ARMjitMathHelperFloatSubtractFloat:           return "jitMathHelperFloatSubtractFloat";
         case TR_ARMjitMathHelperFloatMultiplyFloat:           return "jitMathHelperFloatMultiplyFloat";
         case TR_ARMjitMathHelperFloatDivideFloat:             return "jitMathHelperFloatDivideFloat";
         case TR_ARMjitMathHelperFloatRemainderFloat:          return "jitMathHelperFloatRemainderFloat";
         case TR_ARMjitMathHelperDoubleAddDouble:              return "jitMathHelperDoubleAddDouble";
         case TR_ARMjitMathHelperDoubleSubtractDouble:         return "jitMathHelperDoubleSubtractDouble";
         case TR_ARMjitMathHelperDoubleMultiplyDouble:         return "jitMathHelperDoubleMultiplyDouble";
         case TR_ARMjitMathHelperDoubleDivideDouble:           return "jitMathHelperDoubleDivideDouble";
         case TR_ARMjitMathHelperDoubleRemainderDouble:        return "jitMathHelperDoubleRemainderDouble";
         case TR_ARMjitMathHelperFloatNegate:                  return "jitMathHelperFloatNegate";
         case TR_ARMjitMathHelperDoubleNegate:                 return "jitMathHelperDoubleNegate";
         case TR_ARMjitMathHelperConvertFloatToInt:            return "helperCConvertFloatToInteger";
         case TR_ARMjitMathHelperConvertFloatToLong:           return "helperCConvertFloatToLong";
         case TR_ARMjitMathHelperConvertFloatToDouble:         return "jitMathHelperConvertFloatToDouble";
         case TR_ARMjitMathHelperConvertDoubleToInt:           return "helperCConvertDoubleToInteger";
         case TR_ARMjitMathHelperConvertDoubleToLong:          return "helperCConvertDoubleToLong";
         case TR_ARMjitMathHelperConvertDoubleToFloat:         return "jitMathHelperConvertDoubleToFloat";
         case TR_ARMjitMathHelperConvertIntToFloat:            return "jitMathHelperConvertIntToFloat";
         case TR_ARMjitMathHelperConvertIntToDouble:           return "jitMathHelperConvertIntToDouble";
         case TR_ARMjitMathHelperConvertLongToFloat:           return "jitMathHelperConvertLongToFloat";
         case TR_ARMjitMathHelperConvertLongToDouble:          return "jitMathHelperConvertLongToDouble";
#else
         case TR_ARMjitMathHelperFloatAddFloat:                return "helperCFloatPlusFloat";
         case TR_ARMjitMathHelperFloatSubtractFloat:           return "helperCFloatMinusFloat";
         case TR_ARMjitMathHelperFloatMultiplyFloat:           return "helperCFloatMultiplyFloat";
         case TR_ARMjitMathHelperFloatDivideFloat:             return "helperCFloatDivideFloat";
         case TR_ARMjitMathHelperFloatRemainderFloat:          return "helperCFloatRemainderFloat";
         case TR_ARMjitMathHelperDoubleAddDouble:              return "helperCDoublePlusDouble";
         case TR_ARMjitMathHelperDoubleSubtractDouble:         return "helperCDoubleMinusDouble";
         case TR_ARMjitMathHelperDoubleMultiplyDouble:         return "helperCDoubleMultiplyDouble";
         case TR_ARMjitMathHelperDoubleDivideDouble:           return "helperCDoubleDivideDouble";
         case TR_ARMjitMathHelperDoubleRemainderDouble:        return "helperCDoubleRemainderDouble";
         case TR_ARMjitMathHelperFloatNegate:                  return "jitMathHelperFloatNegate";
         case TR_ARMjitMathHelperDoubleNegate:                 return "jitMathHelperDoubleNegate";
         case TR_ARMjitMathHelperConvertFloatToInt:            return "helperCConvertFloatToInteger";
         case TR_ARMjitMathHelperConvertFloatToLong:           return "helperCConvertFloatToLong";
         case TR_ARMjitMathHelperConvertFloatToDouble:         return "helperCConvertFloatToDouble";
         case TR_ARMjitMathHelperConvertDoubleToInt:           return "helperCConvertDoubleToInteger";
         case TR_ARMjitMathHelperConvertDoubleToLong:          return "helperCConvertDoubleToLong";
         case TR_ARMjitMathHelperConvertDoubleToFloat:         return "helperCConvertDoubleToFloat";
         case TR_ARMjitMathHelperConvertIntToFloat:            return "helperCConvertIntegerToFloat";
         case TR_ARMjitMathHelperConvertIntToDouble:           return "helperCConvertIntegerToDouble";
         case TR_ARMjitMathHelperConvertLongToFloat:           return "helperCConvertLongToFloat";
         case TR_ARMjitMathHelperConvertLongToDouble:          return "helperCConvertLongToDouble";
#endif
         case TR_ARMjitMathHelperFloatCompareEQ:               return "jitMathHelperFloatCompareEQ";
         case TR_ARMjitMathHelperFloatCompareEQU:              return "jitMathHelperFloatCompareEQU";
         case TR_ARMjitMathHelperFloatCompareNE:               return "jitMathHelperFloatCompareNE";
         case TR_ARMjitMathHelperFloatCompareNEU:              return "jitMathHelperFloatCompareNEU";
         case TR_ARMjitMathHelperFloatCompareLT:               return "jitMathHelperFloatCompareLT";
         case TR_ARMjitMathHelperFloatCompareLTU:              return "jitMathHelperFloatoCmpareLTU";
         case TR_ARMjitMathHelperFloatCompareLE:               return "jitMathHelperFloatCompareLE";
         case TR_ARMjitMathHelperFloatCompareLEU:              return "jitMathHelperFloatCompareLEU";
         case TR_ARMjitMathHelperFloatCompareGT:               return "jitMathHelperFloatCompareGT";
         case TR_ARMjitMathHelperFloatCompareGTU:              return "jitMathHelperFloatoCmpareGTU";
         case TR_ARMjitMathHelperFloatCompareGE:               return "jitMathHelperFloatCompareGE";
         case TR_ARMjitMathHelperFloatCompareGEU:              return "jitMathHelperFloatCompareGEU";
         case TR_ARMjitMathHelperDoubleCompareEQ:              return "jitMathHelperDoubleCompareEQ";
         case TR_ARMjitMathHelperDoubleCompareEQU:             return "jitMathHelperDoubleCompareEQU";
         case TR_ARMjitMathHelperDoubleCompareNE:              return "jitMathHelperDoubleCompareNE";
         case TR_ARMjitMathHelperDoubleCompareNEU:             return "jitMathHelperDoubleCompareNEU";
         case TR_ARMjitMathHelperDoubleCompareLT:              return "jitMathHelperDoubleCompareLT";
         case TR_ARMjitMathHelperDoubleCompareLTU:             return "jitMathHelperDoubleCompareLTU";
         case TR_ARMjitMathHelperDoubleCompareLE:              return "jitMathHelperDoubleCompareLE";
         case TR_ARMjitMathHelperDoubleCompareLEU:             return "jitMathHelperDoubleCompareLEU";
         case TR_ARMjitMathHelperDoubleCompareGT:              return "jitMathHelperDoubleCompareGT";
         case TR_ARMjitMathHelperDoubleCompareGTU:             return "jitMathHelperDoubleCompareGTU";
         case TR_ARMjitMathHelperDoubleCompareGE:              return "jitMathHelperDoubleCompareGE";
         case TR_ARMjitMathHelperDoubleCompareGEU:             return "jitMathHelperDoubleCompareGEU";
         case TR_ARMsamplingRecompileMethod:                   return "_samplingRecompileMethod";
         case TR_ARMcountingRecompileMethod:                   return "_countingRecompileMethod";
         case TR_ARMsamplingPatchCallSite:                     return "_samplingPatchCallSite";
         case TR_ARMcountingPatchCallSite:                     return "_countingPatchCallSite";
         case TR_ARMinduceRecompilation:                       return "_induceRecompilation";
         case TR_ARMrevertToInterpreterGlue:                   return "_revertToInterpreterGlue";
         }
      }
#elif defined (TR_TARGET_ARM64)
   else if (_comp->target().cpu.isARM64())
      {
      switch (index)
         {
         case TR_ARM64interpreterUnresolvedStaticGlue:             return "_interpreterUnresolvedStaticGlue";
         case TR_ARM64interpreterUnresolvedSpecialGlue:            return "_interpreterUnresolvedSpecialGlue";
         case TR_ARM64interpreterUnresolvedDirectVirtualGlue:      return "_interpreterUnresolvedDirectVirtualGlue";
         case TR_ARM64interpreterUnresolvedClassGlue:              return "_interpreterUnresolvedClassGlue";
         case TR_ARM64interpreterUnresolvedClassGlue2:             return "_interpreterUnresolvedClassGlue2";
         case TR_ARM64interpreterUnresolvedStringGlue:             return "_interpreterUnresolvedStringGlue";
         case TR_ARM64interpreterUnresolvedConstantDynamicGlue:    return "_interpreterUnresolvedConstantDynamicGlue";
         case TR_ARM64interpreterUnresolvedStaticDataGlue:         return "_interpreterUnresolvedStaticDataGlue";
         case TR_ARM64interpreterUnresolvedInstanceDataGlue:       return "_interpreterUnresolvedInstanceDataGlue";
         case TR_ARM64interpreterUnresolvedStaticDataStoreGlue:    return "_interpreterUnresolvedStaticDataStoreGlue";
         case TR_ARM64interpreterUnresolvedInstanceDataStoreGlue:  return "_interpreterUnresolvedInstanceDataStoreGlue";
         case TR_ARM64virtualUnresolvedHelper:                     return "_virtualUnresolvedHelper";
         case TR_ARM64interfaceCallHelper:                         return "_interfaceCallHelper";
         case TR_ARM64icallVMprJavaSendVirtual0:                   return "icallVMprJavaSendVirtual0";
         case TR_ARM64icallVMprJavaSendVirtual1:                   return "icallVMprJavaSendVirtual1";
         case TR_ARM64icallVMprJavaSendVirtualJ:                   return "icallVMprJavaSendVirtualJ";
         case TR_ARM64icallVMprJavaSendVirtualF:                   return "icallVMprJavaSendVirtualF";
         case TR_ARM64icallVMprJavaSendVirtualD:                   return "icallVMprJavaSendVirtualD";
         case TR_ARM64interpreterVoidStaticGlue:                   return "_interpreterVoidStaticGlue";
         case TR_ARM64interpreterSyncVoidStaticGlue:               return "_interpreterSyncVoidStaticGlue";
         case TR_ARM64interpreterIntStaticGlue:                    return "_interpreterIntStaticGlue";
         case TR_ARM64interpreterSyncIntStaticGlue:                return "_interpreterSyncIntStaticGlue";
         case TR_ARM64interpreterLongStaticGlue:                   return "_interpreterLongStaticGlue";
         case TR_ARM64interpreterSyncLongStaticGlue:               return "_interpreterSyncLongStaticGlue";
         case TR_ARM64interpreterFloatStaticGlue:                  return "_interpreterFloatStaticGlue";
         case TR_ARM64interpreterSyncFloatStaticGlue:              return "_interpreterSyncFloatStaticGlue";
         case TR_ARM64interpreterDoubleStaticGlue:                 return "_interpreterDoubleStaticGlue";
         case TR_ARM64interpreterSyncDoubleStaticGlue:             return "_interpreterSyncDoubleStaticGlue";
         case TR_ARM64nativeStaticHelper:                          return "_nativeStaticHelper";
         case TR_ARM64interfaceDispatch:                           return "_interfaceDispatch";
         case TR_ARM64samplingRecompileMethod:                     return "_samplingRecompileMethod";
         case TR_ARM64countingRecompileMethod:                     return "_countingRecompileMethod";
         case TR_ARM64samplingPatchCallSite:                       return "_samplingPatchCallSite";
         case TR_ARM64countingPatchCallSite:                       return "_countingPatchCallSite";
         case TR_ARM64induceRecompilation:                         return "_induceRecompilation";
         case TR_ARM64revertToInterpreterGlue:                     return "_revertToInterpreterGlue";
         case TR_ARM64doubleRemainder:                             return "doubleRemainder";
         case TR_ARM64floatRemainder:                              return "floatRemainder";
         case TR_ARM64jitCollapseJNIReferenceFrame:                return "jitCollapseJNIReferenceFrame";
         case TR_ARM64arrayCopy:                                   return "__arrayCopy";
         case TR_ARM64forwardArrayCopy:                            return "__forwardArrayCopy";
         case TR_ARM64backwardArrayCopy:                           return "__backwardArrayCopy";
         case TR_ARM64interfaceCompleteSlot2:                      return "_interfaceCompleteSlot2";
         case TR_ARM64interfaceSlotsUnavailable:                   return "_interfaceSlotsUnavailable";
         case TR_ARM64PatchGCRHelper:                              return "_patchGCRHelper" ;
         }
      }
#endif
#endif // J9_PROJECT_SPECIFIC

   return "unknown helper";
   }

const char *
TR_Debug::getLinkageConventionName(uint8_t lc)
   {
   switch((TR_LinkageConventions)lc)
      {
      case TR_Private:
         return "Private";
      case TR_System:
         return "System";
      case TR_Helper:
         return "Helper";
      default:
         TR_ASSERT(0, "Shouldn't get here");
         return "(unknown linkage convention)";
      }
   }

const char *
TR_Debug::getVirtualGuardKindName(TR_VirtualGuardKind kind)
   {
   switch (kind)
      {
      case TR_NoGuard:
         return "NoGuard";
      case TR_ProfiledGuard:
         return "ProfiledGuard";
      case TR_InterfaceGuard:
         return "InterfaceGuard";
      case TR_AbstractGuard:
         return "AbstractGuard";
      case TR_HierarchyGuard:
         return "HierarchyGuard";
      case TR_NonoverriddenGuard:
         return "NonoverriddenGuard";
      case TR_SideEffectGuard:
         return "SideEffectGuard";
      case TR_DummyGuard:
         return "DummyGuard";
      case TR_HCRGuard:
         return "HCRGuard";
      case TR_MutableCallSiteTargetGuard:
         return "MutableCallSiteTargetGuard";
      case TR_MethodEnterExitGuard:
         return "MethodEnterExitGuard";
      case TR_DirectMethodGuard:
         return "DirectMethodGuard";
      case TR_InnerGuard:
         return "InnerGuard";
      case TR_ArrayStoreCheckGuard:
         return "ArrayStoreCheckGuard";
      case TR_OSRGuard:
         return "OSRGuard";
      case TR_BreakpointGuard:
         return "BreakpointGuard";
      default:
         break;
      }
   TR_ASSERT(0, "Unknown virtual guard kind");
   return "(unknown virtual guard kind)";
   }

const char *
TR_Debug::getVirtualGuardTestTypeName(TR_VirtualGuardTestType testType)
   {
   switch (testType)
      {
      case TR_DummyTest:
         return "DummyTest";
      case TR_VftTest:
         return "VftTest";
      case TR_MethodTest:
         return "MethodTest";
      case TR_NonoverriddenTest:
         return "NonoverriddenTest";
      case TR_FSDTest:
         return "FSDTest";
      }
   TR_ASSERT(0, "Unknown virtual guard test type");
   return "(unknown virtual guard test type)";
   }

const char *
TR_Debug::getWriteBarrierKindName(int32_t kind)
   {
   switch (kind)
      {
      case gc_modron_wrtbar_none:
         return "None";
      case gc_modron_wrtbar_always:
         return "Always";
      case gc_modron_wrtbar_oldcheck:
         return "OldCheck";
      case gc_modron_wrtbar_cardmark:
         return "CardMark";
      case gc_modron_wrtbar_cardmark_incremental:
         return "CardMarkIncremental";
      case gc_modron_wrtbar_cardmark_and_oldcheck:
         return "CardMarkAndOldCheck";
      case gc_modron_wrtbar_satb:
      case gc_modron_wrtbar_satb_and_oldcheck:
         return "RealTime";
      default:
         return "UNKNOWN";
      }
   }

const char *
TR_Debug::getSpillKindName(uint8_t kind)
   {
   switch((TR_SpillKinds)kind)
      {
      case TR_gprSpill:
         return "gpr";
      case TR_fprSpill:
         return "fpr";
      case TR_vrfSpill:
         return "vrf";
      case TR_volatileSpill:
         return "volatile";
      case TR_linkageSpill:
         return "linkage";
      case TR_vmThreadSpill:
         return "vmThread";
#if defined(TR_TARGET_X86)
      case TR_ecxSpill:
         return "ecx";
      case TR_eaxSpill:
         return "eax";
      case TR_edxSpill:
         return "edx";
#endif
#if defined(TR_TARGET_S390)
      case TR_litPoolSpill:
         return "litPool";
      case TR_staticBaseSpill:
         return "staticBase";
      case TR_gpr0Spill:
         return "gpr0";
      case TR_gpr1Spill:
         return "gpr1";
      case TR_gpr2Spill:
         return "gpr2";
      case TR_gpr3Spill:
         return "gpr3";
      case TR_gpr4Spill:
         return "gpr4";
      case TR_gpr5Spill:
         return "gpr5";
      case TR_gpr6Spill:
         return "gpr6";
      case TR_gpr7Spill:
         return "gpr7";
      case TR_gpr8Spill:
         return "gpr8";
      case TR_gpr9Spill:
         return "gpr9";
      case TR_gpr10Spill:
         return "gpr10";
      case TR_gpr11Spill:
         return "gpr11";
      case TR_gpr12Spill:
         return "gpr12";
      case TR_gpr13Spill:
         return "gpr13";
      case TR_gpr14Spill:
         return "gpr14";
      case TR_gpr15Spill:
         return "gpr15";
#endif
      default:
         TR_ASSERT(0, "Unknown spill kind: %d", kind);
         return "(unknown spill kind)";
      }
   }

void TR_Debug::dumpSimulatedNode(TR::Node *node, char tagChar)
   {
   trfprintf(_file, "\n               [%s]", getName(node));
   if (_comp->cg()->simulatedNodeState(node)._willBeRematerialized)
      {
      trfprintf(_file, " R/%-2d", node->getReferenceCount());
      }
   else if (node->getReferenceCount() >= 1)
      {
      trfprintf(_file, "%2d/%-2d", node->getLocalIndex(), node->getReferenceCount());
      }
   else
      {
      trfprintf(_file, "     ");
      }
   trfprintf(_file, " %c ", tagChar);

   // 16 chars is enough room for ArrayCopyBNDCHK which is reasonably common
   int32_t padding = 16 - static_cast<int32_t>(strlen(getName(node->getOpCode())));
   if (node->getOpCode().hasSymbolReference())
      {
      // Yes, big methods can have 4-digit symref numbers
      trfprintf(_file, "%s #%-4d", getName(node->getOpCode()), node->getSymbolReference()->getReferenceNumber());
      padding -= 6;
      }
   else if (node->getOpCode().isBranch())
      {
      int32_t destBlockNum = node->getBranchDestination()->getNode()->getBlock()->getNumber();
      trfprintf(_file, "%s %-4d", getName(node->getOpCode()), destBlockNum);
      padding -= 5;
      }
   else if (node->getOpCodeValue() == TR::BBStart || node->getOpCodeValue() == TR::BBEnd)
      {
      trfprintf(_file, "%s %-4d", getName(node->getOpCode()), node->getBlock()->getNumber());
      padding -= 5;
      }
   else if (node->getOpCode().isLoadConst())
      {
      const int32_t limit = 99999999;
      const char * const opCodeOnlyFormatString = "%s (big)   ";
      const char * const signedFormatString     = "%s %-8d";
      const char * const unsignedFormatString   = "%s %-8u";
      if (node->getType().isIntegral())
         {
         padding -= 9;
//          if (node->getOpCode().isUnsigned())
//             {
//             uint64_t value;
//             switch (node->getDataType())
//                {
//                case TR_UInt8:
//                   value = node->getUnsignedByte();
//                   break;
//                case TR_UInt16:
//                   value = node->getConst<uint16_t>();
//                   break;
//                case TR_UInt32:
//                   value = node->getUnsignedInt();
//                   break;
//                case TR_UInt64:
//                   value = node->getUnsignedLongInt();
//                   break;
//                default:
//                   TR_ASSERT(0, "Unsigned integral type should be one of the above");
//                   value = limit+1;
//                   break;
//                }
//             if (value <= limit)
//                {
//                trfprintf(_file, unsignedFormatString, getName(node->getOpCode()), value);
//                }
//             else
//                {
//                trfprintf(_file, opCodeOnlyFormatString, getName(node->getOpCode()));
//                }
//             }
//          else
//             {
            int64_t value;
            switch (node->getDataType())
               {
               case TR::Int8:
                  value = node->getByte();
                  break;
               case TR::Int16:
                  value = node->getShortInt();
                  break;
               case TR::Int32:
                  value = node->getInt();
                  break;
               case TR::Int64:
                  value = node->getLongInt();
                  break;
               default:
                  TR_ASSERT(0, "Signed integral type should be one of the above");
                  value = limit+1;
                  break;
               }
            if (value >= -limit && value <= limit)
               {
               trfprintf(_file, signedFormatString, getName(node->getOpCode()), value);
               }
            else
               {
               trfprintf(_file, opCodeOnlyFormatString, getName(node->getOpCode()));
               }
//            }
         }
      else if (node->getDataType() == TR::Float)
         {
         trfprintf(_file, "%s %-8g", getName(node->getOpCode()), node->getFloat());
         padding -= 9;
         }
      else if (node->getDataType() == TR::Double)
         {
         trfprintf(_file, "%s %-8g", getName(node->getOpCode()), node->getDouble());
         padding -= 9;
         }
      else if (node->getDataType() == TR::Address && node->getAddress() == 0)
         {
         trfprintf(_file, "%s NULL", getName(node->getOpCode()));
         padding -= 5;
         }
      else
         {
         trfprintf(_file, "%s", getName(node->getOpCode()));
         }
      }
   else
      {
      trfprintf(_file, "%s", getName(node->getOpCode()));
      }

   trfprintf(_file, " %*s", padding, "");
   }

void
TR_Debug::dumpGlobalRegisterTable()
   {
   trfprintf(_file, "Global regs:\n");
   for (int32_t i = 0; i < _comp->cg()->getNumberOfGlobalRegisters(); i++)
      {
      trfprintf(_file, "   %d: %s\n", i, getGlobalRegisterName((TR_GlobalRegisterNumber)i));
      }
   }

void
TR_Debug::startTracingRegisterAssignment(const char *direction, TR_RegisterKinds kindsToAssign)
   {
   if (_file == NULL)
      return;
   if (!_comp->getOption(TR_TraceRA))
      return;
   trfprintf(_file, "\n\n<regassign direction=\"%s\" method=\"%s\">\n", direction, jitdCurrentMethodSignature(_comp));
   trfprintf(_file, "<legend>\n"
                        "  V(F/T)   virtual register V with future use count F and total use count T\n"
                        "  V=R      V assigned to real register R\n"
                        "  V:R      V assigned to R by association\n"
                        "  V#R      V assigned to R by graph colouring\n"
                        "  V=$R     another virtual register in R now spilled\n"
                        "  $V=R     spilled V now reloaded into R\n"
                        "  !V=R     coercion due to a pre-dependency\n"
                        "  V=R!     coercion due to a post-dependency\n"
                        "  (V=R)    coercion due to another assignment/coercion\n"
                        "  {V#R}    coercion due to colouring\n"
                        "  V~R      V evicted from R (spill, death, etc.)\n"
                        "  R[N]?    considering R with weight N\n"
                        "  V{I,D}?  considering V with association index I and interference distance D\n"
                        "</legend>\n");
   trfflush(_file);
   _registerAssignmentTraceFlags |= TRACERA_IN_PROGRESS;
   _registerAssignmentTraceCursor = 0;
   _registerKindsToAssign = kindsToAssign;
   }

void
TR_Debug::pauseTracingRegisterAssignment()
  {
  _pausedRegisterAssignmentTraceFlags = _registerAssignmentTraceFlags;
  _registerAssignmentTraceFlags = 0;
  }


void
TR_Debug::resumeTracingRegisterAssignment()
  {
  _registerAssignmentTraceFlags = _pausedRegisterAssignmentTraceFlags;
  }

void
TR_Debug::stopTracingRegisterAssignment()
   {
   if (_file == NULL)
      return;
   if (!_comp->getOption(TR_TraceRA))
      return;
   if (_registerAssignmentTraceCursor)
      trfprintf(_file, "\n");
   trfprintf(_file, "</regassign>\n");
   trfflush(_file);
   _registerAssignmentTraceFlags &= ~TRACERA_IN_PROGRESS;
   }

void
TR_Debug::traceRegisterAssignment(const char *format, va_list args)
   {
   if (_file == NULL)
      return;
   if (!_comp->getOption(TR_TraceRA))
      return;
   if (_registerAssignmentTraceCursor)
      {
      trfprintf(_file, "\n");
      _registerAssignmentTraceCursor = 0;
      }

   // indent the message.
   trfprintf(_file, "details:                      ");

   int32_t  j = 0;
   int32_t  length = static_cast<int32_t>(strlen(format)) + 40;
   char    *buffer = (char *)_comp->trMemory()->allocateHeapMemory(length + 1);
   bool     sawPercentR = false;

   for (const char *c = format; *c; c++, j++)
      {
      if (*c == '%' && *(c + 1) == 'R')
         {
         ++c;
         const char *regName = getName(va_arg(args, TR::Register *));
         int32_t slen  = static_cast<int32_t>(strlen(regName));

         if (j + slen >= length) // re-allocate buffer if too small
            {
            length += 40;
            char *newBuffer = (char *)_comp->trMemory()->allocateHeapMemory(length + 1);
            memcpy(newBuffer, buffer, j);
            buffer = newBuffer;
            }
         memcpy(&buffer[j], regName, slen);
         j += slen - 1;
         sawPercentR = true;
         }
      else
         {
         if (j >= length) // re-allocate buffer if too small
            {
            length += 40;
            char *newBuffer = (char *)_comp->trMemory()->allocateHeapMemory(length + 1);
            memcpy(newBuffer, buffer, j);
            buffer = newBuffer;
            }
         buffer[j] = *c;
         }
      }

   buffer[j] = 0;

   // print the rest of the arguments using the modified format
   if (sawPercentR)
      TR::IO::vfprintf(_file, buffer, args);
   else
      TR::IO::vfprintf(_file, format, args);

   va_end(args);
   trfprintf(_file, "\n");
   trfflush(_file);
   }

void
TR_Debug::traceRegisterAssignment(TR::Instruction *instr, bool insertedByRA, bool postRA)
   {
   TR_ASSERT( instr, "cannot trace assignment of NULL instructions\n");
   if (_file == NULL)
      return;
   if (!_comp->getOption(TR_TraceRA))
      return;
   if (insertedByRA)
      _registerAssignmentTraceFlags |=  TRACERA_INSTRUCTION_INSERTED;
   else if (postRA)
      _registerAssignmentTraceFlags &= ~TRACERA_INSTRUCTION_INSERTED;
   print(_file, instr);
   if (_registerAssignmentTraceCursor)
      {
      trfprintf(_file, "\n");
      _registerAssignmentTraceCursor = 0;
      if (postRA)
         {
         if (_comp->getOption(TR_TraceRA))
            {
            trfprintf(_file, "<regstates>\n");
#if 0
/* Work in progress side-by side traceRA={*} */
            const bool isGPR = _registerKindsToAssign & TR_GPR_Mask;
            const bool isVRF = _registerKindsToAssign & TR_VRF_Mask;
            const bool isFPR = _registerKindsToAssign & TR_FPR_Mask;
            const bool isX87 = _registerKindsToAssign & TR_X87_Mask;

            TR::RegisterIterator *gprIter = _comp->cg()->getGPRegisterIterator();
            TR::RegisterIterator *hprIter = _comp->cg()->getGPRegisterIterator();
            TR::RegisterIterator *vrfIter = _comp->cg()->getGPRegisterIterator();
            TR::RegisterIterator *fprIter = _comp->cg()->getGPRegisterIterator();
            TR::RegisterIterator *arIter  = _comp->cg()->getGPRegisterIterator();
            TR::RegisterIterator *x87Iter = _comp->cg()->getGPRegisterIterator();

            // Print header

            // Print regs
            for (int row = 0; row < 16; row++)
               {
               // Call printFullRegInfoNewFormat(_file,...)
               trfprintf("\n");
               }

            // Print footer
#endif
            if (_registerKindsToAssign & TR_GPR_Mask)
               {
               trfprintf(_file, "<gprs>\n");
               TR::RegisterIterator *iter = _comp->cg()->getGPRegisterIterator();
               for (TR::Register *gpr = iter->getFirst(); gpr; gpr = iter->getNext())
                  {
                  printFullRegInfo(_file, gpr);
                  }
               trfprintf(_file, "</gprs>\n");
               }
#if defined(TR_TARGET_S390)
            if (_registerKindsToAssign & TR_VRF_Mask && _cg->getSupportsVectorRegisters())
               {
               trfprintf(_file, "<vrfs>\n");
               TR::RegisterIterator *iter = _cg->getVRFRegisterIterator();
               for (TR::Register *vrf = iter->getFirst(); vrf; vrf = iter->getNext())
                  {
                  printFullRegInfo(_file, vrf);
                  }
               trfprintf(_file, "</vrfs>\n");
               }
#endif
            if (_registerKindsToAssign & TR_FPR_Mask)
               {
               trfprintf(_file, "<fprs>\n");
               TR::RegisterIterator *iter = _comp->cg()->getFPRegisterIterator();
               for (TR::Register *fpr = iter->getFirst(); fpr; fpr = iter->getNext())
                  {
                  printFullRegInfo(_file, fpr);
                  }
               trfprintf(_file, "</fprs>\n");
               }

            if (_registerKindsToAssign & TR_VMR_Mask)
               {
               trfprintf(_file, "<vmrs>\n");
               TR::RegisterIterator *iter = _comp->cg()->getVMRegisterIterator();
               for (TR::Register *vmr = iter->getFirst(); vmr; vmr = iter->getNext())
                  {
                  printFullRegInfo(_file, vmr);
                  }
               trfprintf(_file, "</vmrs>\n");
               }

            trfprintf(_file, "</regstates>\n");
            }
         trfprintf(_file, "\n");
         }
      }
   }

void
TR_Debug::traceRegisterAssigned(TR_RegisterAssignmentFlags flags, TR::Register *virtReg, TR::Register *realReg)
   {
   TR_ASSERT(realReg, "Cannot trace assignment of NULL Real Register\n");
   TR_ASSERT(virtReg, "Cannot trace assignment of NULL Virtual Register\n");

   if (_file == NULL ||
       !_comp->getOption(TR_TraceRA))
      return;
   // Avoid superfluous traces, unless the user asks for them.
   if (virtReg->isPlaceholderReg() &&
       !_comp->getOption(TR_TraceRA))
      return;
   char buf[30];
   const char *preCoercionSymbol = flags.testAny(TR_PreDependencyCoercion) ? "!" : "";
   const char *postCoercionSymbol = flags.testAny(TR_PostDependencyCoercion) ? "!" : "";
   const char *regSpillSymbol = flags.testAny(TR_RegisterSpilled) ? "$" : "";
   const char *regReloadSymbol = flags.testAny(TR_RegisterReloaded) ? "$" : "";
   const char *openParen = flags.testAny(TR_ColouringCoercion) ? "{" : (flags.testAny(TR_IndirectCoercion) ? "(" : "");
   const char *closeParen = flags.testAny(TR_ColouringCoercion) ? "}" : (flags.testAny(TR_IndirectCoercion) ? ")" : "");
   const char *equalSign = flags.testAny(TR_ByColouring) ? "#" : (flags.testAny(TR_ByAssociation) ? ":" : "=");
   sprintf(buf, "%s%s%s%s(%d/%d)%s%s%s%s%s ",
           preCoercionSymbol,
           openParen,
           regReloadSymbol,
           getName(virtReg),
           virtReg->getFutureUseCount(),
           virtReg->getTotalUseCount(),
           equalSign,
           regSpillSymbol,
           getName(realReg),
           closeParen,
           postCoercionSymbol);
   if ((_registerAssignmentTraceCursor += static_cast<int16_t>(strlen(buf))) > 80)
      {
      _registerAssignmentTraceCursor = static_cast<int16_t>(strlen(buf));
      trfprintf(_file, "\n%s", buf);
      }
   else
      {
      trfprintf(_file, buf);
      }
   trfflush(_file);
   }

void
TR_Debug::traceRegisterFreed(TR::Register *virtReg, TR::Register *realReg)
   {
   TR_ASSERT(realReg, "Cannot trace assignment of NULL Real Register\n");
   TR_ASSERT(virtReg, "Cannot trace assignment of NULL Virtual Register\n");

   if (_file == NULL ||
       !_comp->getOption(TR_TraceRA))
      return;
   // Avoid superfluous traces, unless the user asks for them.
   if (virtReg->isPlaceholderReg() &&
       !_comp->getOption(TR_TraceRA))
      return;
   char buf[30];
   sprintf(buf, "%s(%d/%d)~%s ",
           getName(virtReg),
           virtReg->getFutureUseCount(),
           virtReg->getTotalUseCount(),
           getName(realReg));
   if ((_registerAssignmentTraceCursor += static_cast<int16_t>(strlen(buf))) > 80)
      {
      _registerAssignmentTraceCursor = static_cast<int16_t>(strlen(buf));
      trfprintf(_file, "\n%s", buf);
      }
   else
      {
      trfprintf(_file, buf);
      }
   trfflush(_file);
   }

void
TR_Debug::traceRegisterInterference(TR::Register *virtReg, TR::Register *interferingVirtual, int32_t distance)
   {
   TR_ASSERT( virtReg && interferingVirtual, "cannot trace assignment of NULL registers\n");
   if (_file == NULL)
      return;
   if (!_comp->getOption(TR_TraceRA))
      return;
   char buf[40];
   sprintf(buf, "%s{%d,%d}? ", getName(interferingVirtual), interferingVirtual->getAssociation(), distance);
   if ((_registerAssignmentTraceCursor += static_cast<int16_t>(strlen(buf))) > 80)
      {
      _registerAssignmentTraceCursor = static_cast<int16_t>(strlen(buf));
      trfprintf(_file, "\n%s", buf);
      }
   else
      {
      trfprintf(_file, buf);
      }
   trfflush(_file);
   }

void
TR_Debug::traceRegisterWeight(TR::Register *realReg, uint32_t weight)
   {
   TR_ASSERT( realReg, "cannot trace weight of NULL register\n");
   if (_file == NULL)
      return;
   if (!_comp->getOption(TR_TraceRA))
      return;
   char buf[30];
   sprintf(buf, "%s[0x%x]? ", getName(realReg), weight);
   if ((_registerAssignmentTraceCursor += static_cast<int16_t>(strlen(buf))) > 80)
      {
      _registerAssignmentTraceCursor = static_cast<int16_t>(strlen(buf));
      trfprintf(_file, "\n%s", buf);
      }
   else
      {
      trfprintf(_file, buf);
      }
   trfflush(_file);
   }

#if defined(AIXPPC)
#if !defined(J9OS_I5)
#include <unistd.h>
extern "C" void yield(void);
void TR_Debug::setupDebugger(void *addr)
   {
   static bool started = false;

   // Only need to start the debugger once.
   if (!started)
      {
      pid_t ppid = getpid();
      pid_t p;

      if ((p = fork()) == 0)
         {
         char cfname[20];
         FILE *cf;
         char pp[20];
         char *Argv[20];

         yield();
         sprintf(cfname, "_%" OMR_PRId64 "_", (int64_t)getpid());
         sprintf(pp, "%" OMR_PRId64, (int64_t)ppid);
         Argv[1] = (char *)"-a";
         Argv[2] = pp;
         Argv[3] = NULL;

         if ((Argv[0] = ::feGetEnv("TR_DEBUGGER")) == NULL)
            {
            Argv[0] = (char *)"/usr/bin/dbx";
            if ((cf = fopen(cfname, "wb+")) == 0)
              cfname[0] = '\0';
            else
               {
               fprintf(cf, "stopi at 0x%p\n", addr);
               fprintf(cf, "sh rm %s\n", cfname);
               fprintf(cf, "set $ignorenonbptrap=1\n");
               fprintf(cf, "set $repeat=1\n");
               fprintf(cf, "cont SIGCONT\n");
               fclose(cf);

               Argv[3] = (char *)"-c";
               Argv[4] = cfname;
               Argv[5] = NULL;
              }
            }
         execvp(Argv[0], Argv);
         }
      else
         sleep(60);

      started = true;
      }
   }
#else
// J9OS_I5
void TR_Debug::setupDebugger(void *addr)
   {
   static bool started = false;
   // Only need to start the debugger once.
   if (!started)
      {
      Xj9JitBreakPoint();
      started = true;
      }
   }

#endif

#elif defined(LINUX)
void TR_Debug::setupDebugger(void *startaddr, void *endaddr, bool before)
   {
   static bool started = false;
   char cfname[256];
   FILE *cf;
   char pp[20];
   char * Argv[4];

   /* Only need to start the debugger once. */
   if (!started)
      {
      pid_t p;
      pid_t ppid = getpid();
      if ((p = fork()) == 0)
         {
         sprintf(cfname, "/tmp/__TRJIT_%d_", getpid());
         sprintf(pp,"%d", ppid);

         if ((Argv[0] =  ::feGetEnv("TR_DEBUGGER")) == NULL)
            Argv[0] = "/usr/bin/gdb";

         if ((cf = fopen(cfname, "wb+")) == 0)
            {
            cfname[0] = '\0';
            printf("ERROR: Couldn't open file %s", cfname);
            }
         else
            {
            fprintf(cf, "file /proc/%s/exe\n", pp);
            fprintf(cf, "attach %s\n",pp);
            fprintf(cf, "i sh\n");

            if ( before )
               fprintf(cf, "break *%p\n",startaddr);
            else
               {
               printf("\n methodStartAddress = %p",startaddr);
               printf("\n methodEndAddress = %p\n",endaddr);
               fprintf(cf, "break *%p\n",startaddr);
               fprintf(cf, "disassemble %p %p\n",startaddr,endaddr);
               }

            fprintf(cf, "finish\n");
            fprintf(cf, "shell rm %s\n",cfname);
            fclose(cf);

            Argv[1] = "-x";
            Argv[2] = cfname;
            Argv[3] = NULL;
            }
         execvp(Argv[0], Argv);
         }
      else
         {
          sleep(2);
         }
      }

   started = true;
   }

#elif defined(J9ZOS390)
#include <spawn.h>
#include <unistd.h>
void TR_Debug::setupDebugger(void *startaddr, void *endaddr, bool before)
   {
   #define BUFFER_SIZE 40
   #define MAX_NO_ARGUMENTS 6
   static bool started = false;
   /* Only need to start the debugger once. */
   if (!started)
      {
      char cfname[BUFFER_SIZE];
      FILE *cf;
      char pp[BUFFER_SIZE];
      char * Argv[MAX_NO_ARGUMENTS];
      pid_t ppid = getpid();
      volatile int size=sprintf(cfname, "/tmp/_%d_", ppid);
      volatile int x;
      TR_ASSERT(size<BUFFER_SIZE, "assertion failure");
      size=sprintf(pp,"%d", ppid);
      TR_ASSERT(size<BUFFER_SIZE, "assertion failure");
      // open new file for writing in the case of break_before
      // in the case of the break_after commands file was already created in final.c
      if ((Argv[0] = ::feGetEnv("TR_DEBUGGER")) == NULL)
         Argv[0] = "/tr/zos-tools/bin/dbxattach";
         // for break_before, although command file is always the same,
         // we generate it dynamically for consistency with other platforms
         if (cf = fopen(cfname, "wb+"))
            {
            fprintf(cf, "set $unsafebps\n");
            fprintf(cf, "set $unsafegoto\n");
            fprintf(cf, "set $unsafecall\n");
            fprintf(cf, "set $unsafebounds\n");
            fprintf(cf, "set $unsafeassign\n");
            fprintf(cf, "set $hexints\n");
            fprintf(cf, "set $asciistrings\n");
            fprintf(cf, "set $repeat\n");
            fprintf(cf, "set $listwindow=25\n");
            fprintf(cf, "set $historywindow=50\n");
            if ( before )
               {
               fprintf(cf, "stopi at 0x%p\n" ,startaddr);
               fprintf(cf, "stop in stopBeforeCompile\n" );
               }
            else
               {
               printf("\n methodStartAddress = %p",startaddr);
               printf("\n methodEndAddress = %p\n",endaddr);
               fprintf(cf, "stopi at 0x%p\n" ,startaddr);
               }

            fprintf(cf, "status\n");
            // instrumentation to break out of the infinite loop
            // defined in setup_debugger(..) in debug JIT
            fprintf(cf, "assign 0x%p=1\n",&x);
            fclose(cf);
            }
         // check for dbx that cfname file could be read (dbx does not do this check)
         if (cf = fopen(cfname, "r"))
            {
            struct inheritance inh = { 0 }; /* use all the default inheritance stuff */
            int fdCount = 0; /* inherit all file descriptors from parent */
            int* fd = 0;
            char  ***__Envn();
            #define  environ  (*(__Envn()))
            const char** env = (const char **) environ; // pass TR environemtn to debugger
            fclose(cf);
            Argv[1] = "-c";
            Argv[2] = cfname;
            Argv[3] = "-a";
            Argv[4] = pp;
            Argv[5] = NULL;
            spawnp(Argv[0], fdCount, NULL, &inh, (const char**) Argv, env);
            printf("Looping on pid:%d while waiting for dbx\n", ppid);
            printf("Be patient, this may take a minute or two...\n");
            printf("If spawn has failed to execute the above command,\n");
            printf(" Execute the following command manually on command line \n");
            printf("%s %s %s %s %s\n", Argv[0], Argv[1], Argv[2], Argv[3], Argv[4]);
            for(x=0;x==0;);
            if (remove(cfname))
               printf("Could not delete %s .\n",cfname);
            started = true;
            }
         else printf("Could not open %s, skipping break !\n",cfname);
         }
   }
#elif defined(OMR_OS_WINDOWS)
#ifndef WINDOWS_API_INCLUDED
extern "C"
   {
   long _stdcall  GetCurrentProcessId(); // msf - including Windows.h causes problems with UDATA redefinition
   long _stdcall  CreateProcessA(const char * appName, const char * cmdLine, void * processAttributes, void * threadAttributes,
                            int inheritsHandles, long creationFlags, void * envP, const char * currentDirectory,
                            void * startupInfo, void * processInfo);
   void _stdcall  Sleep(long time);
   }
#endif
void TR_Debug::setupDebugger(void *startaddr, void *endaddr, bool before)
   {
   #define BUFFER_SIZE 1024
   static bool started = false;
   /* Only need to start the debugger once. */
   if (!started)
      {
      char cmdBuffer[BUFFER_SIZE];
      char appBuffer[BUFFER_SIZE];
      uint32_t ppid = GetCurrentProcessId();
      char* srcDir = ::feGetEnv("DBGPATH");
      if (!srcDir)
         {
         srcDir = ::feGetEnv("%TRHOME%\\jre\\bin\\codegen.dev");
         if (!srcDir)
            {
            srcDir = ".";
            }
         }
      int* startupInfo = (int*)calloc(20, 4);   // msf - yuck. should get system headers and include here
      *startupInfo = 17*4;                      // ditto...
      void* processInfo = calloc(4, 4);         // ditto...
      volatile int x;
      int size;

      // instrumentation to break out of the infinite loop
      size = sprintf(cmdBuffer, "?? *((int*)0x%p)=1;", &x);
      if ( before )
         {
         size += sprintf(&cmdBuffer[size], "bp 0x%p;" ,startaddr);
         size += sprintf(&cmdBuffer[size], "bm stopBeforeCompile;g;" );
         }
      else
         {
         size += sprintf(&cmdBuffer[size], "bp 0x%p;g;" ,startaddr);
         }

      // check for windbg that cfname file could be read
      size = sprintf(appBuffer, "\"C:\\Program Files\\Debugging Tools For Windows\\windbg\" -p %d -srcpath %s -c \"%s\"", ppid, srcDir, cmdBuffer);
      TR_ASSERT(size<BUFFER_SIZE, "assertion failure");

#ifdef WINDOWS_API_INCLUDED
      CreateProcessA(NULL, appBuffer, NULL, NULL, 0, 0, NULL, NULL, (LPSTARTUPINFOA) startupInfo, (LPPROCESS_INFORMATION) processInfo);
#else
      CreateProcessA(NULL, appBuffer, NULL, NULL, 0, 0, NULL, NULL, startupInfo, processInfo);
#endif

      for(x=0;x==0;)
         {
         Sleep(10);
         }
      started = true;
      }
   }
#endif /* defined(AIXPPC) */

void TR_Debug::setSingleAllocMetaData(bool usesSingleAllocMetaData)
   {
   _usesSingleAllocMetaData = usesSingleAllocMetaData;
   }




void TR_Debug::printStackBacktrace()
   {
   TR_CallStackIteratorImpl cs;
   cs.printStackBacktrace(NULL);
   }

void TR_Debug::printStackBacktraceToTraceLog(TR::Compilation *comp)
   {
   TR_ASSERT(comp, "ERROR: Must provide a compliation object in order to print to trace");
   TR_CallStackIteratorImpl cs;
   cs.printStackBacktrace(comp);
   }

static void printDenominators(TR::DebugCounter *c, int64_t rawCount, FILE *counterFile)
   {
   if (!c)
      return;

   printDenominators(c->getDenominator(), rawCount, counterFile);

   if (c->getCount() == 0)
      {
      fprintf(counterFile, "     ---   |");
      return;
      }

   double quotient = ((double)rawCount) / ((double)c->getCount());
   if (-1.1 < quotient && quotient < 1.1) // print as percentage
      {
      fprintf(counterFile, " %8.2f%% |", 100.0 * quotient);
      }
   else // print as fraction
      {
      fprintf(counterFile, " %8.2f  |", quotient);
      }

   }

static int counterCompare(const char *left, const char *right)
   {
   // Like strcmp but it looks for NUMERIC_SEPARATOR and does numeric ordering where indicated
   // Note: '.' doesn't work like an actual decimal point, ".12" > ".2" because 12 > 2

   // Works by looking at the names as numeric and non-numeric sections, and comparing one section at a time

   static char numericStart[] = {TR::DebugCounterGroup::NUMERIC_SEPARATOR, 0};
   static char numericEnd[] = {TR::DebugCounterGroup::FRACTION_SEPARATOR, TR::DebugCounterGroup::RATIO_SEPARATOR, '.', 0};

   bool numericComparisonMode = false;

   while (*left != 0 && *right != 0)
      {
      // The terminator indicates a change in numeric/string comparison mode
      char *terminator = numericComparisonMode ? numericEnd : numericStart;
      auto leftSectionLength = strcspn(left, terminator);
      auto rightSectionLength = strcspn(right, terminator);
      if (leftSectionLength != rightSectionLength)
         {
         return numericComparisonMode
               ? static_cast<int32_t>(leftSectionLength - rightSectionLength) // Assume that numbers are not 0-padded, so any longer number must be bigger
               : strcmp(left, right);
         }
      // strncmp also works for comparing numbers with an equal number of digits
      int result = strncmp(left, right, leftSectionLength);
      if (result != 0)
         {
         return result;
         }
      // Move on to the next section
      left += leftSectionLength;
      right += rightSectionLength;
      // Compare the terminators (usually the same)
      if (left[0] != right[0])
         return left[0] - right[0];
      if (left[0] == '\0')
         return 0;
      // Except for '.' in numeric mode, a terminator changes the numeric/non-numeric mode
      bool switchMode = (left[0] != '.');
      // Terminators are the same, so skip to the next character, which starts the next section
      left++;
      right++;

      if (switchMode)
         numericComparisonMode = !numericComparisonMode;
      }
   return left[0] - right[0];
   }

static FILE *openDebugCounterFile(char *counterFileName)
   {
   FILE *result = NULL;
   if (counterFileName)
      result = fopen(counterFileName, "a");
   if (!result)
      result = stderr;
   return result;
   }

static int compareDebugCounter (const void * a, const void * b)
{
   return counterCompare(  (*((TR::DebugCounter**)a))->getName(),
                           (*((TR::DebugCounter**)b))->getName());
}

static void swap(void* elemA, void* elemB, int32_t elemSize) {
   if ((elemSize % 4) == 0) {
      for (int i = 0; i < elemSize / 4; i++) {
         int32_t t = ((int32_t*)elemA)[i];
         ((int32_t*)elemA)[i] = ((int32_t*)elemB)[i];
         ((int32_t*)elemB)[i] = t;
      }
   }
   else {
      for (int i = 0; i < elemSize; i++) {
         char t = ((char*)elemA)[i];
         ((char*)elemA)[i] = ((char*)elemB)[i];
         ((char*)elemB)[i] = t;
      }
   }
}

static int32_t partition(void* array, int32_t first, int32_t last, int32_t elemSize, int (*cmpFunc)(void*,void*))
   {
   int32_t i = first;
   int32_t j = last;
   while (1)
      {
         while (cmpFunc((char*)array + i*elemSize, (char*) array + first*elemSize) < 0)
         i++;
         while (cmpFunc((char*)array + j*elemSize, (char*) array + first*elemSize) > 0)
         j--;
      if (i >= j)
         return j;
      swap((char*)array + i*elemSize, (char*) array + j*elemSize, elemSize);
      i++;
      j--;
      }
   }

static void quicksort(void *array, int32_t first, int32_t last, int32_t elemSize, int (*cmpFunc)(void*,void*)) {
  int32_t mid;
  if (first < last)
     {
     mid = partition(array, first, last, elemSize, cmpFunc);
     quicksort(array, first, mid, elemSize, cmpFunc);
     quicksort(array, mid+1, last, elemSize, cmpFunc);
     }
   }

static void qsort(void *array, int32_t nitems, int32_t elemSize, int (*cmpFunc)(void*,void*))
   {
   quicksort(array, 0, nitems-1, elemSize, cmpFunc);
   }


void TR_Debug::printDebugCounters(TR::DebugCounterGroup *counterGroup, const char *name)
   {
   ListBase<TR::DebugCounter> &counters = counterGroup->_counters;
   if (counters.isEmpty())
      return;

   TR::DebugCounter **counterArray = (TR::DebugCounter**)(TR::Compiler->regionAllocator.allocate(counters.getSize() * sizeof(TR::DebugCounter*)));
   ListIterator<TR::DebugCounter> li(&counters);
   TR::DebugCounter *c; int32_t count=0, i;
   int32_t longestName = 0;

   static FILE *counterFile = openDebugCounterFile(::feGetEnv("TR_DebugCounterFileName"));
   fprintf(counterFile, "\n== %s ==\n", name);

   // We want them printed in order of name.  Given the naming convention with
   // prefixes indicating denominators, sorting makes for a much much more
   // readable report.
   //
   for (c = li.getCurrent(); c; c = li.getNext())
      {
      counterArray[count++] = c;
      if (c->getCount() != 0)
         {
         int32_t nameLength = static_cast<int32_t>(strlen(c->getName()));
         longestName = std::max(longestName, nameLength);
         }
      }
   qsort(counterArray, count, sizeof(TR::DebugCounter*), compareDebugCounter);

   // Avoid displaying redundant counters.  If a nonzero counter C has a denominator D,
   // no other nonzero counter has denominator D, and C's count equals D's count, then D
   // is redundant.  This is actually quite a common occurrence.
   //
   // It would be nice to do this in O(n) time... this loop is O(n^2).
   //
   TR::DebugCounter **breadthFirst = (TR::DebugCounter**)(TR::Compiler->regionAllocator.allocate(counters.getSize() * sizeof(TR::DebugCounter*)));
   for (i = 0; i < count; i++)
      {
      TR::DebugCounter *d = counterArray[i];
      if (d->getCount() == 0)
         continue;
      int32_t numNumerators = 0;
      bool keep = false;
      for (int32_t j = i; j < count && !keep; j++)
         {
         c = counterArray[j];
         if (c->getDenominator() == d && c->getCount() != 0)
            {
            if (++numNumerators >= 2)
               keep = true;
            else if (c->getCount() != d->getCount())
               keep = true;
            }
         }
      if (numNumerators == 0)
         keep = true;
      if (!keep)
         counterArray[i] = NULL;
      }

   for (i = 0; i < count; i++)
      {
      c = counterArray[i];
      if (c && c->getCount() != 0)
         {
         fprintf(counterFile, "%3d: %-*s | %12.0f | ", i, longestName, c->getName(), (double)(c->getCount()));
         printDenominators(c->getDenominator(), c->getCount(), counterFile);
         fprintf(counterFile, "  __ %3d __\n", i);
         }
      }

   }
