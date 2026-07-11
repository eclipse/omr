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
#include "infra/CfgNode.hpp"
#include "infra/List.hpp"
#include "infra/SimpleRegex.hpp"
#include "infra/String.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/PreExistence.hpp"
#include "optimizer/Structure.hpp"
#include "ras/CallStackIterator.hpp"
#include "ras/DebugCounter.hpp"
#include "ras/Logger.hpp"
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

extern const char *pIlOpNames[]; // from Tree.cpp

const char *jitdCurrentMethodSignature(TR::Compilation *comp)
{
    return comp->getCurrentMethod()->signature(comp->trMemory(), heapAlloc);
}

void *TR_Debug::operator new(size_t s, TR_HeapMemory m) { return m.allocate(s, TR_Memory::Debug); }

void *TR_Debug::operator new(size_t s, TR::PersistentAllocator &allocator) { return allocator.allocate(s); }

TR_Debug *createDebugObject(TR::Compilation *comp)
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
    if (first) {
        printf("stopOnCreate is a dummy routine.\n");
        first = 0;
    }
}
#endif /* defined(AIXPPC) || defined(LINUX) || defined(J9ZOS390) || defined(OMR_OS_WINDOWS) */

void TR_Debug::breakOn()
{
#if defined(TR_HOST_X86)
    TR::Compiler->debug.breakPoint();
#else
    static int first = 1;

    // only print the following message once
    if (first) {
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

void TR_Debug::debugOnCreate()
{
#if defined(TR_HOST_X86)
    TR::Compiler->debug.breakPoint();
#elif defined(AIXPPC)
    setupDebugger((void *)*((long *)&(stopOnCreate)));
    stopOnCreate();
#elif defined(LINUX) || defined(J9ZOS390) || (defined(OMR_OS_WINDOWS))
    setupDebugger((void *)&stopOnCreate, (void *)&stopOnCreate, true);
    stopOnCreate();
#endif /* defined(TR_HOST_X86) */
}

TR_Debug::TR_Debug(TR::Compilation *c)
    : _comp(c)
    , _compilationFilters(0)
    , _relocationFilters(0)
    , _inlineFilters(0)
    , _lastFrequency(-1)
    , _isCold(false)
    , _currentParent(NULL)
    , _currentChildIndex(0)
    , _mainEntrySeen(false)
{
    if (c) {
        _cg = c->cg();
        _fe = c->fe();
        _logger = c->log();
        resetDebugData();

        _nodeChecklist.init(0, c->trMemory(), heapAlloc, growable);
        _structureChecklist.init(0, c->trMemory(), heapAlloc, growable);
    } else // still need to initialize other fields that are being initialized inside resetDebugData()
    {
        _nextLabelNumber = 1;

        _nextRegisterNumber = 1;
        _nextNodeNumber = 1;
        _nextSymbolNumber = 1;
        _nextInstructionNumber = 1;
        _nextStructureNumber = 1;
        _nextVariableSizeSymbolNumber = 1;
        _usesSingleAllocMetaData = 0;
    }

    // Figure out how many bytes are consumed by POINTER_PRINTF_FORMAT
    if (1) // force block scope
    {
        const size_t bufferSize = 30;
        char buffer[bufferSize];
        addressWidth = snprintf(buffer, bufferSize, POINTER_PRINTF_FORMAT, this);
    }

    _registerAssignmentTraceFlags = 0;
}

void TR_Debug::resetDebugData()
{
    _nextLabelNumber = 1;
    _nextRegisterNumber = 1;
    _nextNodeNumber = 1;
    _nextSymbolNumber = 1;
    _nextInstructionNumber = 1;
    _nextStructureNumber = 1;
    _nextVariableSizeSymbolNumber = 1;
    _usesSingleAllocMetaData = 0;
}

inline void roundToBoundary(uint32_t &value, uint32_t boundary)
{
    uint32_t vplus = value + boundary - 1;
    value = vplus - (vplus % boundary);
}

void TR_Debug::roundAddressEnumerationCounters(uint32_t boundary)
{
    roundToBoundary(_nextLabelNumber, boundary);
    roundToBoundary(_nextRegisterNumber, boundary);
    roundToBoundary(_nextSymbolNumber, boundary);
    roundToBoundary(_nextInstructionNumber, boundary);
    roundToBoundary(_nextStructureNumber, boundary);

    // Note: this currently has no effect because we use node->getGlobalIndex()
    // instead of _nextNodeNumber.  We may want to reconsider using
    // _nextNodeNumber to reduce spurious node name differences between runs.
    //
    roundToBoundary(_nextNodeNumber, boundary);
}

void TR_Debug::breakOrDebugOnCreate(char *artifactName)
{
    TR::SimpleRegex *regex = _comp->getOptions()->getBreakOnCreate();
    if (regex && TR::SimpleRegex::match(regex, artifactName, false))
        breakOn();

    regex = _comp->getOptions()->getDebugOnCreate();
    if (regex && TR::SimpleRegex::match(regex, artifactName, false))
        debugOnCreate();
}

void TR_Debug::newNode(TR::Node *node)
{
    if (_comp->getOptions()->getBreakOnCreate() || _comp->getOptions()->getDebugOnCreate()) {
        const size_t bufSize = 20;
        char buf[bufSize];
        snprintf(buf, bufSize, "ND_%04x", node->getGlobalIndex());
        breakOrDebugOnCreate(buf);
    }
}

void TR_Debug::newLabelSymbol(TR::LabelSymbol *labelSymbol)
{
    _comp->getToNumberMap().Add((void *)labelSymbol, (intptr_t)_nextLabelNumber);

    if (_comp->getOptions()->getBreakOnCreate() || _comp->getOptions()->getDebugOnCreate()) {
        const size_t bufSize = 20;
        char buf[bufSize];
        snprintf(buf, bufSize, "L%04x", _nextLabelNumber);
        breakOrDebugOnCreate(buf);
    }

    _nextLabelNumber++;
}

void TR_Debug::newInstruction(TR::Instruction *instr)
{
    if (_comp->getAddressEnumerationOption(TR_EnumerateInstruction) || _comp->getOptions()->getBreakOnCreate()
        || _comp->getOptions()->getDebugOnCreate()) {
        const size_t bufSize = 20;
        char buf[bufSize];
        _comp->getToNumberMap().Add((void *)instr, (intptr_t)_nextInstructionNumber);
        snprintf(buf, bufSize, "IN_%04x", _nextInstructionNumber);
        breakOrDebugOnCreate(buf);
    }

    _nextInstructionNumber++;
}

void TR_Debug::newRegister(TR::Register *reg)
{
    if (_comp->getAddressEnumerationOption(TR_EnumerateRegister))
        _comp->getToNumberMap().Add((void *)reg, (intptr_t)_nextRegisterNumber);

    if (_comp->getOptions()->getBreakOnCreate() || _comp->getOptions()->getDebugOnCreate()) {
        const size_t bufSize = 20;
        char buf[bufSize];
        snprintf(buf, bufSize, "GPR_%04x", _nextRegisterNumber);
        breakOrDebugOnCreate(buf);
    }

    _nextRegisterNumber++;
}

void TR_Debug::newVariableSizeSymbol(TR::AutomaticSymbol *sym)
{
    const size_t bufSize = static_cast<int32_t>(strlen(TR_VSS_NAME)) + TR::getMaxSignedPrecision<TR::Int32>() + 7;
    char *buf = (char *)_comp->trMemory()->allocateHeapMemory(bufSize);
    snprintf(buf, bufSize, "%s_%d", TR_VSS_NAME, _nextVariableSizeSymbolNumber);

    _comp->getToStringMap().Add((void *)sym, buf);

    if (_comp->getOptions()->getBreakOnCreate() || _comp->getOptions()->getDebugOnCreate()) {
        breakOrDebugOnCreate(buf);
    }

    _nextVariableSizeSymbolNumber++;
}

void TR_Debug::addInstructionComment(TR::Instruction *instr, char *comment, ...)
{
    TR_ASSERT(_comp, "Required compilation object is NULL.\n");

    static char *disableInstructionComments = feGetEnv("TR_DisableInstructionComments");

    if (disableInstructionComments || comment == NULL || !_comp->log()->isEnabled_DEPRECATED())
        return;

    CS2::HashIndex hashIndex;
    if (_comp->getToCommentMap().Locate(instr, hashIndex)) {
        List<char> *comments = _comp->getToCommentMap().DataAt(hashIndex);
        comments->add(comment);
    } else {
        List<char> *comments = new (_comp->trHeapMemory()) List<char>(_comp->trMemory());
        comments->add(comment);
        _comp->getToCommentMap().Add(instr, comments);
    }
}

bool TR_Debug::performTransformationImpl(bool canOmitTransformation, const char *format, ...)
{
    OMR::Logger *log = _comp->log();
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

    static char *disablePerformTransformationLogging = feGetEnv("TR_DisablePerformTransformationLogging");

    // We try to avoid doing the work of formatting the string if we don't need to.
    //
    if ((_comp->getOption(TR_CountOptTransformations) && _comp->getOptions()->getVerboseOptTransformationsRegex())
        || (!disablePerformTransformationLogging && log->isEnabled_DEPRECATED() && canOmitTransformation
            && _comp->getOptions()->getDisabledOptTransformations())) {
        va_list args;
        va_start(args, format);
        string = formattedString(messageBuffer, sizeof(messageBuffer), format, args);
        va_end(args);
        alreadyFormatted = true;
    }

    if (_comp->getOption(TR_CountOptTransformations)) {
        TR::SimpleRegex *regex = _comp->getOptions()->getVerboseOptTransformationsRegex();
        if (regex && TR::SimpleRegex::match(regex, string)) {
            _comp->incVerboseOptTransformationCount();
        }
    }

    static int32_t curTransformationIndex = 0;

    if (canOmitTransformation) {
        ++curTransformationIndex;
        _comp->incOptSubIndex();

        TR::SimpleRegex *regex = _comp->getOptions()->getDisabledOptTransformations();
        if (regex && (TR::SimpleRegex::match(regex, curTransformationIndex) || TR::SimpleRegex::match(regex, string)))
            return false;

        if (curTransformationIndex < _comp->getOptions()->getFirstOptTransformationIndex()
            || curTransformationIndex > _comp->getOptions()->getLastOptTransformationIndex())
            return false;

        if (optIndex == lastOptIndex && _comp->getOptSubIndex() > _comp->getOptions()->getLastOptSubIndex())
            return false;

        //
        // Ok, we're doing this transformation
        //
        if (optIndex == comp()->getLastBegunOptIndex())
            comp()->recordPerformedOptTransformation();
    }

    // No need to do the printing logic below if logging is disabled
    //
    if (disablePerformTransformationLogging || !log->isEnabled_DEPRECATED())
        return true;

    if (canOmitTransformation) {
        if (_registerAssignmentTraceFlags & TRACERA_IN_PROGRESS)
            log->println(); // traceRA convention is a newline at the start rather than the end

        log->printf("[%6d] ", curTransformationIndex);
        if (optIndex == lastOptIndex) {
            log->printf("%3d.%-4d ", optIndex, _comp->getOptSubIndex());
        }

        if ((format[0] == '%' && format[1] == 's') || (format[0] == 'O' && format[1] == '^' && format[2] == 'O')) {
            ;
        } else {
            // prefix the string with O^O so that tooling can easily count transformations
            // this should already be in most transformations but it could get accidentally dropped
            log->prints("O^O (Unknown Transformation):");
        }
    } else if (_registerAssignmentTraceFlags & TRACERA_IN_PROGRESS)
        log->prints("\n         ");
    else
        log->prints("         ");

    va_list args;
    va_start(args, format);
    log->vprintf(format, args);
    va_end(args);

    log->flush();

    roundAddressEnumerationCounters();

    return true;
}

void TR_Debug::traceLnFromLogTracer(const char *preFormatted)
{
    OMR::Logger *log = getLogger();
    if (log->isEnabled_DEPRECATED()) {
        log->prints(preFormatted);
        log->println();
        log->flush();
    }
}

void TR_Debug::printHeader(OMR::Logger *log) { log->printf("\n=======>%s\n", signature(_comp->getMethodSymbol())); }

void TR_Debug::printMethodHotness(OMR::Logger *log)
{
    log->printf("\nThis method is %s", _comp->getHotnessName(_comp->getMethodHotness()));
    if (_comp->getRecompilationInfo() && _comp->getRecompilationInfo()->isProfilingCompilation()) {
        log->prints(" and will be profiled\n");
    } else {
        log->println();
    }
}

void TR_Debug::printInstrDumpHeader(OMR::Logger *log, const char *title)
{
    int addressFieldWidth = TR::Compiler->debug.hexAddressFieldWidthInChars();
    int codeByteColumnWidth = TR::Compiler->debug.codeByteColumnWidth();
    int prefixWidth = addressFieldWidth * 2 + codeByteColumnWidth
        + 12; // 8 bytes of offsets, 2 spaces, and opening and closing brackets
    const char *lineNumber = "";

    if (strcmp(title, "Post Instruction Selection Instructions") == 0
        || strcmp(title, "Post Register Assignment Instructions") == 0) {
        log->printf("\n%*s+--------------------------------------- instruction address", addressFieldWidth - 2, " ");
        log->printf("\n%*s|       +------------------------------------------ %s", addressFieldWidth - 2, " ",
            lineNumber);
        log->printf("\n%*s|       |       +----------------------------------------- instruction",
            addressFieldWidth - 2, " ");
        log->printf("\n%*s|       |       |", addressFieldWidth - 2, " ");
        log->printf("\n%*sV       V       V", addressFieldWidth - 2, " ");
    } else {
        log->printf("\n%*s+--------------------------------------- instruction address", addressFieldWidth - 1, " ");
        log->printf("\n%*s|        +----------------------------------------- instruction offset from start of method",
            addressFieldWidth - 1, " ");
        log->printf(
            "\n%*s|        | %*s+------------------------------------------ corresponding TR::Instruction instance",
            addressFieldWidth - 1, " ", addressFieldWidth, " ");
        log->printf("\n%*s|        | %*s|  +-------------------------------------------------- code bytes",
            addressFieldWidth - 1, " ", addressFieldWidth, " ");
        log->printf("\n%*s|        | %*s|  |%*s+-------------------------------------- %sopcode and operands",
            addressFieldWidth - 1, " ", addressFieldWidth, " ", codeByteColumnWidth - 2, " ", lineNumber);
        log->printf("\n%*s|        | %*s|  |%*s|\t\t\t\t+----------- additional information", addressFieldWidth - 1,
            " ", addressFieldWidth, " ", codeByteColumnWidth - 2, " ");
        log->printf("\n%*s|        | %*s|  |%*s|\t\t\t\t|", addressFieldWidth - 1, " ", addressFieldWidth, " ",
            codeByteColumnWidth - 2, " ");
        log->printf("\n%*sV        V %*sV  V%*sV\t\t\t\tV", addressFieldWidth - 1, " ", addressFieldWidth, " ",
            codeByteColumnWidth - 2, " ");
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
uint8_t *TR_Debug::printPrefix(OMR::Logger *log, TR::Instruction *instr, uint8_t *cursor, uint8_t size)
{
    if (cursor != NULL) {
        uint32_t offset = static_cast<uint32_t>(cursor - _comp->cg()->getCodeStart());

        const size_t prefixSize = MAX_PREFIX_WIDTH + 1;
        char prefix[prefixSize];

        int addressFieldWidth = TR::Compiler->debug.hexAddressFieldWidthInChars();
        int codeByteColumnWidth = TR::Compiler->debug.codeByteColumnWidth();
        int prefixWidth = addressFieldWidth * 2 + codeByteColumnWidth
            + 12; // 8 bytes of offsets, 2 spaces, and opening and closing brackets

        if (instr)
            snprintf(prefix, prefixSize, POINTER_PRINTF_FORMAT " %08x [%s]", cursor, offset, getName(instr));
        else
            snprintf(prefix, prefixSize, POINTER_PRINTF_FORMAT " %08x %*s", cursor, offset, addressFieldWidth + 2, " ");

        char *p0 = prefix;
        char *p1 = prefix + strlen(prefix);

        // Print machine code in bytes on X86, in words on PPC,ARM,ARM64
        // Stop if we try to run over the buffer. The remaining size must be
        // recomputed on every iteration because p1 advances through prefix.
        if (_comp->target().cpu.isX86()) {
            for (int i = 0; i < size && p1 - p0 + 3 < prefixWidth; i++, p1 += 3)
                snprintf(p1, prefixSize - (p1 - p0), " %02x", *cursor++);
        } else if (_comp->target().cpu.isPower() || _comp->target().cpu.isARM() || _comp->target().cpu.isARM64()) {
            for (int i = 0; i < size && p1 - p0 + 9 < prefixWidth; i += 4, p1 += 9, cursor += 4)
                snprintf(p1, prefixSize - (p1 - p0), " %08x", *((uint32_t *)cursor));
        } else // FIXME: Need a better general form
        {
            for (int i = 0; i < size && p1 - p0 + 3 < prefixWidth; i++, p1 += 3)
                snprintf(p1, prefixSize - (p1 - p0), " %02x", *cursor++);
        }

        int32_t leftOver = static_cast<int32_t>(p0 + prefixWidth - p1);
        if (leftOver >= 1) {
            memset(p1, ' ', leftOver);
            p1[leftOver] = '\0';
        }

        log->printf("\n%s", prefix);
    } else if (_registerAssignmentTraceFlags & TRACERA_IN_PROGRESS) {
        const char *spaces = "                                        ";
        int16_t padding = 30 - _registerAssignmentTraceCursor;

        if (padding < 0)
            log->printf("\n%.*s", 30, spaces);
        else if (padding > 0)
            log->printf("%.*s", padding, spaces);

        _registerAssignmentTraceCursor = 30;

        const char *formatStr = (_registerAssignmentTraceFlags & TRACERA_INSTRUCTION_INSERTED) ? " <%s>\t" : " [%s]\t";
        log->printf(formatStr, getName(instr));
    } else {
        log->printf("\n [%s]\t", getName(instr));
    }

    return cursor;
}

void TR_Debug::printSnippetLabel(OMR::Logger *log, TR::LabelSymbol *label, uint8_t *cursor)
{
    int32_t addressFieldWidth = TR::Compiler->debug.hexAddressFieldWidthInChars();
    int32_t codeByteColumnWidth = TR::Compiler->debug.codeByteColumnWidth();

    uint32_t offset = static_cast<uint32_t>(cursor - _comp->cg()->getCodeStart());

    log->printf("\n\n" POINTER_PRINTF_FORMAT " %08x %*s", cursor, offset, addressFieldWidth + codeByteColumnWidth + 2,
        " ");

    print(log, label);
}

void TR_Debug::printSnippetLabel(OMR::Logger *log, TR::Snippet *snippet, uint8_t *cursor)
{
    printSnippetLabel(log, snippet->getSnippetLabel(), cursor);

    log->printc(':');
    log->printf("\t\t%c ", (_comp->target().cpu.isX86() && _comp->target().isLinux()) ? '#' : ';');
    snippet->printName(log);
}

void TR_Debug::printSnippetLabel(OMR::Logger *log, TR::LabelSymbol *label, uint8_t *cursor, const char *comment1,
    const char *comment2)
{
    printSnippetLabel(log, label, cursor);

    log->printc(':');
    if (comment1) {
        log->printf("\t\t%c %s", (_comp->target().cpu.isX86() && _comp->target().isLinux()) ? '#' : ';', comment1);
        if (comment2) {
            log->printf(" (%s)", comment2);
        }
    }
}

void TR_Debug::print(OMR::Logger *log, TR_BitVector *bv)
{
    log->printc('{');
    bool firstOne = true;
    TR_BitVectorIterator bvi(*bv);
    int32_t num = 0;
    while (bvi.hasMoreElements()) {
        char *formatStr;
        if (firstOne) {
            formatStr = "%d";
            firstOne = false;
        } else {
            formatStr = ", %d";
        }

        log->printf(formatStr, bvi.getNextElement());

        if (num > 30) {
            log->println();
            num = 0;
        }
        num++;
    }
    log->printc('}');
}

void TR_Debug::print(OMR::Logger *log, TR_SingleBitContainer *sbc) { log->prints(sbc->isEmpty() ? "{}" : "{0}"); }

void TR_Debug::print(OMR::Logger *log, TR::BitVector *bv)
{
    log->printc('{');
    bool firstOne = true;
    int32_t num = 0;
    TR::BitVector::Cursor bi(*bv);
    for (bi.SetToFirstOne(); bi.Valid(); bi.SetToNextOne()) {
        char *formatStr;
        if (firstOne) {
            formatStr = "%d";
            firstOne = false;
        } else {
            formatStr = ", %d";
        }

        log->printf(formatStr, static_cast<uint32_t>(bi));

        if (num > 30) {
            log->println();
            num = 0;
        }
        num++;
    }
    log->printc('}');
}

void TR_Debug::print(OMR::Logger *log, TR::SparseBitVector *sparse)
{
    log->printc('{');
    bool firstOne = true;
    int32_t num = 0;
    TR::SparseBitVector::Cursor bi(*sparse);
    for (bi.SetToFirstOne(); bi.Valid(); bi.SetToNextOne()) {
        char *formatStr;
        if (firstOne) {
            formatStr = "%d";
            firstOne = false;
        } else {
            formatStr = ", %d";
        }

        log->printf(formatStr, static_cast<uint32_t>(bi));

        if (num > 30) {
            log->println();
            num = 0;
        }
        num++;
    }
    log->printc('}');
}

const char *TR_Debug::signature(TR::ResolvedMethodSymbol *s)
{
#ifdef J9_PROJECT_SPECIFIC
    if (s->getRecognizedMethod() == TR::java_lang_Class_newInstancePrototype)
        return s->getResolvedMethod()->newInstancePrototypeSignature(_comp->trMemory());
#endif
    return s->signature(_comp->trMemory());
}

TR_OpaqueClassBlock *TR_Debug::containingClass(TR::SymbolReference *symRef)
{
    TR::Method *method = symRef->getSymbol()->castToMethodSymbol()->getMethod();

    if (method) {
        char *className = method->classNameChars();
        uint16_t classNameLen = method->classNameLength();
        return fe()->getClassFromSignature(className, classNameLen, symRef->getOwningMethod(comp()));
    }

    return NULL;
}

int32_t TR_Debug::nodePrintAllFlags(OMR::Logger *log, TR::Node *node)
{
    int32_t len = 0;

    // This guard info is not strictly speaking in the node flags anymore, but
    // with the node flags is a good place to show it.
    TR_VirtualGuard *guard = node->virtualGuardInfo();
    if (guard != NULL) {
        const char *kind = getVirtualGuardKindName(guard->getKind());
        const char *testType = getVirtualGuardTestTypeName(guard->getTestType());
        len += log->printf("%s/%s", kind, testType);

        if (guard->mergedWithHCRGuard())
            len_logprints_literal(len, log, "+HCRGuard");

        if (guard->mergedWithOSRGuard())
            len_logprints_literal(len, log, "+OSRGuard");

        if (!guard->getInnerAssumptions().isEmpty())
            len_logprints_literal(len, log, "+inner");

        len_logprintc(len, log, ' ');
    }

#define FLAG_IF(cond, text)                            \
    do {                                               \
        if (cond)                                      \
            len_logprints_literal(len, log, text " "); \
    } while (false)
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
    FLAG(isCallThatWasRefinedFromKnownObject, "wasRefinedFromKnownObject");
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
    FLAG(isNodeRegSignExtendedTo64Bit, "nodeRegSignExtendedTo64Bit");
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
    FLAG(isPreparedForDirectJNI, "preparedForDirectJNI");
    FLAG(chkSafeForCGToFastPathUnsafeCall, "safeForCGToFastPathUnsafeCall");
    FLAG(chkSafeForCGToInlineStringIntrinsic, "safeForCGToInlineStringIntrinsic");
    FLAG(chkSkippedInRecognizedCallTransformation, "skipRecognizedCallTransformation");

#undef FLAG
#undef FLAG_IF

    return len;
}

// Prints the SymRef offset if total displacement is non-zero
int32_t TR_Debug::printSymRefOffset(OMR::Logger *log, TR::SymbolReference *symRef)
{
    int32_t len = 0;
    int32_t displacement = 0;
    TR::Symbol *sym = symRef->getSymbol();

    if (comp()->cg()->getMappingAutomatics() && sym && sym->isRegisterMappedSymbol()) {
        displacement = sym->getRegisterMappedSymbol()->getOffset();
    }

    if (symRef->getOffset() + displacement) {
        len += log->printf("%+d", displacement + symRef->getOffset());
    }

    return len;
}

int32_t TR_Debug::printSymRefObjIndex(OMR::Logger *log, TR::SymbolReference *symRef)
{
    int32_t len = 0;
    TR::Symbol *sym = symRef->getSymbol();

    if (symRef->getKnownObjectIndex() != TR::KnownObjectTable::UNKNOWN) {
        len += log->printf("(obj%d)", (int32_t)symRef->getKnownObjectIndex());
    } else if (sym && sym->isFixedObjectRef() && comp()->getKnownObjectTable() && !symRef->isUnresolved()) {
        len_logprints_literal(len, log, "(fixed obj ref missing known object index)");

        TR::KnownObjectTable::Index i = comp()->getKnownObjectTable()->getExistingIndexAt(
            (uintptr_t *)sym->castToStaticSymbol()->getStaticAddress());

        if (i != TR::KnownObjectTable::UNKNOWN)
            len += log->printf(" (==obj%d)", (int32_t)i);
    }

    return len;
}

int32_t TR_Debug::printSymRefKind(OMR::Logger *log, TR::SymbolReference *symRef)
{
    int32_t len = 0;
    TR::Symbol *sym = symRef->getSymbol();

    if (symRef->isUnresolved())
        len_logprints_literal(len, log, " unresolved");

    switch (symRef->hasBeenAccessedAtRuntime()) {
        case TR_yes:
            len_logprints_literal(len, log, " accessed");
            break;
        case TR_no:
            len_logprints_literal(len, log, " notAccessed");
            break;
        default:
            break;
    }

    if (symRef->getSymbol()->isFinal())
        len_logprints_literal(len, log, " final");

    if (!symRef->getSymbol()->isTransparent())
        len += log->printf(" %s", TR::Symbol::getMemoryOrderingName(symRef->getSymbol()->getMemoryOrdering()));

    switch (sym->getKind()) {
        case TR::Symbol::IsAutomatic:
            if (sym->getAutoSymbol()->getName() == NULL)
                len_logprints_literal(len, log, " Auto");
            else
                len += log->printf(" %s", sym->getAutoSymbol()->getName());
            break;
        case TR::Symbol::IsParameter:
            len_logprints_literal(len, log, " Parm");
            break;
        case TR::Symbol::IsStatic:
            if (symRef->isFromLiteralPool()) {
                len_logprints_literal(len, log, " DLP-Static");
            } else {
                len_logprints_literal(len, log, " Static");
            }
            break;

        case TR::Symbol::IsResolvedMethod:
        case TR::Symbol::IsMethod: {
            TR::MethodSymbol *methodSym = sym->castToMethodSymbol();
            if (methodSym->isNative())
                len_logprints_literal(len, log, " native");
            switch (methodSym->getMethodKind()) {
                case TR::MethodSymbol::Virtual:
                    len_logprints_literal(len, log, " virtual");
                    break;
                case TR::MethodSymbol::Interface:
                    len_logprints_literal(len, log, " interface");
                    break;
                case TR::MethodSymbol::Static:
                    len_logprints_literal(len, log, " static");
                    break;
                case TR::MethodSymbol::Special:
                    len_logprints_literal(len, log, " special");
                    break;
                case TR::MethodSymbol::Helper:
                    len_logprints_literal(len, log, " helper");
                    break;
                case TR::MethodSymbol::ComputedStatic:
                    len_logprints_literal(len, log, " computed-static");
                    break;
                case TR::MethodSymbol::ComputedVirtual:
                    len_logprints_literal(len, log, " computed-virtual");
                    break;
                default:
                    len_logprints_literal(len, log, " UNKNOWN");
                    break;
            }

            len_logprints_literal(len, log, " Method");
        } break;

        case TR::Symbol::IsShadow:
            if (sym->isNamedShadowSymbol() && sym->getNamedShadowSymbol()->getName() != NULL) {
                len_logprints_literal(len, log, " Named Shadow");
            } else {
                len_logprints_literal(len, log, " Shadow");
            }
            break;
        case TR::Symbol::IsMethodMetaData:
            len_logprints_literal(len, log, " MethodMeta");
            break;
        case TR::Symbol::IsLabel:
            break;
        default:
            TR_ASSERT_FATAL(false, "unexpected symbol kind");
    }

    return len;
}

int32_t TR_Debug::printSymRefName(OMR::Logger *log, TR::SymbolReference *symRef)
{
    int32_t len = 0;
    TR::Symbol *sym = symRef->getSymbol();

    switch (sym->getKind()) {
        case TR::Symbol::IsStatic:
            if (!symRef->isFromLiteralPool() && sym->isNamed()) {
                len += log->printf("\"%s\" ", ((TR::StaticSymbol *)sym)->getName());
            }
            // deliberate fall through

        case TR::Symbol::IsAutomatic:
        case TR::Symbol::IsParameter:
        case TR::Symbol::IsResolvedMethod:
        case TR::Symbol::IsMethod:
        case TR::Symbol::IsShadow:
            len += log->printf("%s", getName(symRef));
            break;

        case TR::Symbol::IsMethodMetaData:
            len += log->printf("%s", sym->getMethodMetaDataSymbol()->getName());
            break;

        case TR::Symbol::IsLabel:
            break;

        default:
            TR_ASSERT_FATAL(0, "unexpected symbol kind");
    }

    return len;
}

int32_t TR_Debug::printSymRefOtherInfo(OMR::Logger *log, TR::SymbolReference *symRef)
{
    int32_t len = 0;
    TR::Symbol *sym = symRef->getSymbol();

    switch (sym->getKind()) {
        case TR::Symbol::IsAutomatic:
        case TR::Symbol::IsParameter:
        case TR::Symbol::IsStatic:
        case TR::Symbol::IsShadow:
        case TR::Symbol::IsMethodMetaData:
        case TR::Symbol::IsLabel:
            break;

        case TR::Symbol::IsResolvedMethod:
        case TR::Symbol::IsMethod: {
            TR_OpaqueClassBlock *clazz = containingClass(symRef);
            if (clazz) {
                if (TR::Compiler->cls.isInterfaceClass(_comp, clazz))
                    len_logprints_literal(len, log, " (Interface class)");
                else if (TR::Compiler->cls.isAbstractClass(_comp, clazz))
                    len_logprints_literal(len, log, " (Abstract class)");
            }
        } break;

        default:
            TR_ASSERT_FATAL(false, "unexpected symbol kind");
    }

    len += log->printf(" [flags 0x%x 0x%x ]", sym->getFlags(), sym->getFlags2());

    return len;
}

int32_t TR_Debug::print(OMR::Logger *log, TR::SymbolReference *symRef, bool hideHelperMethodInfo, bool verbose)
{
    int32_t len = 0;
    TR::Symbol *sym = symRef->getSymbol();

    if (verbose) {
        uint32_t numSpaces = getNumSpacesAfterIndex(symRef->getReferenceNumber(),
            getIntLength(comp()->getSymRefTab()->baseArray.size()));
        len += log->printf("#%d:%*s ", symRef->getReferenceNumber(), numSpaces, "");

        // Short circuit for NULL symbols.  These can appear, for example, from
        // some code generator memory references.
        //
        if (!sym) {
            len_logprintc(len, log, '[');
            len += printSymRefOffset(log, symRef);
            len_logprints_literal(len, log, "] ");
            len += printSymRefObjIndex(log, symRef);
            len_logprints_literal(len, log, " [(null)]");
            return len;
        }

        if (!hideHelperMethodInfo) {
            len += printSymRefName(log, symRef);
        }

        if (sym->getKind() == TR::Symbol::IsLabel) {
            len += log->prints_len(getName(sym->castToLabelSymbol()));
            len_logprintc(len, log, ' ');
        }

        len_logprintc(len, log, '[');

        if (!hideHelperMethodInfo) {
            len += printSymRefKind(log, symRef);
            len_logprintc(len, log, ' ');
        }

        len += printSymRefOffset(log, symRef);
        len_logprints_literal(len, log, "] ");
        len += printSymRefObjIndex(log, symRef);

        if (!hideHelperMethodInfo) {
            len += printSymRefOtherInfo(log, symRef);
        }

        len += log->printf(" [%s] (%s", getName(sym), TR::DataType::getName(sym->getDataType()));

        TR_OpaqueClassBlock *klass = sym->getDeclaredClass();
        if (klass != NULL) {
            int32_t klassLen = 0;
            const char *className = TR::Compiler->cls.classNameChars(_comp, klass, klassLen);
            len += log->printf(": %p %.*s", klass, klassLen, className);
        }

        len_logprintc(len, log, ')');

        if (!sym->isTransparent()) {
            len += log->printf(" [%s]", TR::Symbol::getMemoryOrderingName(sym->getMemoryOrdering()));
        }
    } else {
        len_logprintc(len, log, ' ');

        // Short circuit for NULL symbols.  These can appear, for example, from
        // some code generator memory references.
        //
        if (!sym) {
            len += log->printf("[#%d ", symRef->getReferenceNumber());
            len += printSymRefOffset(log, symRef);
            len_logprintc(len, log, ']');
            len += printSymRefObjIndex(log, symRef);
            return len;
        }

        if (!hideHelperMethodInfo) {
            len += printSymRefName(log, symRef);
        }

        if (sym->getKind() == TR::Symbol::IsLabel) {
            len += log->printf("%s ", getName(sym->castToLabelSymbol()));
        }

        len += log->printf("[#%d ", symRef->getReferenceNumber());

        if (!hideHelperMethodInfo) {
            len += printSymRefKind(log, symRef);
            len_logprintc(len, log, ' ');
        }

        len += printSymRefOffset(log, symRef);
        len_logprintc(len, log, ']');
        len += printSymRefObjIndex(log, symRef);

        if (!hideHelperMethodInfo) {
            len += printSymRefOtherInfo(log, symRef);
        }
    }

    return len;
}

void TR_Debug::print(OMR::Logger *log, TR::LabelSymbol *labelSymbol) { log->prints(getName(labelSymbol)); }

const char *TR_Debug::getName(TR_YesNoMaybe value)
{
    return (value == TR_yes) ? "TR_yes" : ((value == TR_no) ? "TR_no" : "TR_maybe");
}

const char *TR_Debug::getName(void *address, const char *prefix, uint32_t nextNumber, bool enumerate)
{
    TR_ASSERT(_comp, "Required compilation object is NULL.\n");
    // Common getName facility

    if (enumerate) {
        if (!address) {
            const size_t bufSize = 20 + TR::Compiler->debug.pointerPrintfMaxLenInChars();
            char *buf = (char *)_comp->trMemory()->allocateHeapMemory(bufSize);
            snprintf(buf, bufSize, "%0*d", TR::Compiler->debug.hexAddressWidthInChars(), 0);
            return buf;
        }

        CS2::HashIndex hashIndex;
        if (_comp->getToStringMap().Locate((void *)address, hashIndex)) {
            return (const char *)_comp->getToStringMap().DataAt(hashIndex);
        } else {
            const size_t bufSize = 20 + TR::Compiler->debug.pointerPrintfMaxLenInChars();
            char *buf = (char *)_comp->trMemory()->allocateHeapMemory(bufSize);

            uint8_t indentation = TR::Compiler->debug.hexAddressFieldWidthInChars() - 4;
            snprintf(buf, bufSize, "%*s%04x", indentation, prefix, nextNumber);
            _comp->getToStringMap().Add((void *)address, buf);
            return buf;
        }
    }

    const size_t bufSize = 20 + TR::Compiler->debug.pointerPrintfMaxLenInChars();
    char *buf = (char *)_comp->trMemory()->allocateHeapMemory(bufSize);

    if (address)
        snprintf(buf, bufSize, POINTER_PRINTF_FORMAT, address);
    else
        snprintf(buf, bufSize, "%0*d", TR::Compiler->debug.hexAddressWidthInChars(), 0);
    return buf;
}

const char *TR_Debug::getVSSName(TR::AutomaticSymbol *sym)
{
    TR_ASSERT(_comp, "Required compilation object is NULL.\n");
    CS2::HashIndex hashIndex;
    if (_comp->getToStringMap().Locate((void *)sym, hashIndex)) {
        return (const char *)_comp->getToStringMap().DataAt(hashIndex);
    }

    TR_ASSERT(false, "could not find variable size symbol in the _comp->getToStringMap()");
    return getName((void *)sym, TR_VSS_NAME, 0, false);
}

const char *TR_Debug::getName(TR::Symbol *sym)
{
    if (sym == NULL)
        return "(null)";

    if (sym->isVariableSizeSymbol())
        return getVSSName(sym->castToVariableSizeSymbol());
    // TODO: Rewrite this more like the other getName functions.  Currently it
    // burns a lot of nextSymbolNumbers.
    //
    return getName((void *)sym, "SYM_", _nextSymbolNumber++, _comp->getAddressEnumerationOption(TR_EnumerateSymbol));
}

const char *TR_Debug::getName(TR::Instruction *instr)
{
    TR_ASSERT(_comp, "Required compilation object is NULL.\n");
    uint32_t InstructionNumber = 0;

    if (!_comp->getAddressEnumerationOption(TR_EnumerateInstruction))
        return getName((void *)instr, "IN_", 0, false);

    CS2::HashIndex hashIndex;
    if (_comp->getToNumberMap().Locate((void *)instr, hashIndex)) {
        InstructionNumber = (uint32_t)_comp->getToNumberMap().DataAt(hashIndex);
        return getName((void *)instr, "IN_", InstructionNumber,
            _comp->getAddressEnumerationOption(TR_EnumerateInstruction));
    }

    TR_ASSERT(0, "each instruction should be associated with a unique number");
    return getName((void *)instr, "IN1_", 0, _comp->getAddressEnumerationOption(TR_EnumerateInstruction));
}

const char *TR_Debug::getName(TR_Structure *structure)
{
    // TODO: Rewrite this more like the other getName functions.  Currently it
    // burns a lot of nextStructureNumbers.
    //
    return getName((void *)structure, "ST_", _nextStructureNumber++,
        _comp->getAddressEnumerationOption(TR_EnumerateStructure));
}

const char *TR_Debug::getName(TR::Node *node)
{
    if (!node)
        return "(null)";
    else
        return getName((void *)node, "ND_", node->getGlobalIndex(),
            _comp->getAddressEnumerationOption(TR_EnumerateNode));
}

const char *TR_Debug::getName(TR::CFGNode *node)
{
    const size_t bufSize = 25;
    char *buf = (char *)_comp->trMemory()->allocateHeapMemory(bufSize);
    if (_comp->getAddressEnumerationOption(TR_EnumerateBlock)) {
        snprintf(buf, bufSize, "block_%d", node->getNumber());
    } else {
        snprintf(buf, bufSize, POINTER_PRINTF_FORMAT, node);
    }

    return buf;
}

const char *TR_Debug::getName(TR::LabelSymbol *labelSymbol)
{
    TR_ASSERT(_comp, "Required compilation object is NULL.\n");
    CS2::HashIndex hashIndex;
    uint32_t labelNumber = 0;

    if (labelSymbol->isRelativeLabel()) {
        return labelSymbol->getName();
    }
    if (_comp->getToStringMap().Locate((void *)labelSymbol, hashIndex)) {
        return (const char *)_comp->getToStringMap().DataAt(hashIndex);
    } else if (_comp->getToNumberMap().Locate((void *)labelSymbol, hashIndex)) {
        const size_t bufSize = 25;
        char *buf = (char *)_comp->trMemory()->allocateHeapMemory(bufSize);
        labelNumber = (uint32_t)_comp->getToNumberMap().DataAt(hashIndex);
        if (labelSymbol->getSnippet())
            snprintf(buf, bufSize, "Snippet Label L%04d", labelNumber);
        else if (labelSymbol->isStartOfColdInstructionStream())
            snprintf(buf, bufSize, "Outlined Label L%04d", labelNumber);
        else
            snprintf(buf, bufSize, "Label L%04d", labelNumber);
        _comp->getToStringMap().Add((void *)labelSymbol, buf);
        return buf;
    } else {
        const size_t bufSize = 20 + TR::Compiler->debug.pointerPrintfMaxLenInChars();
        char *buf = (char *)_comp->trMemory()->allocateHeapMemory(bufSize);

        if (labelSymbol->getSnippet())
            snprintf(buf, bufSize, "Snippet Label [" POINTER_PRINTF_FORMAT "]", labelSymbol);
        else
            snprintf(buf, bufSize, "Label [" POINTER_PRINTF_FORMAT "]", labelSymbol);

        _comp->getToStringMap().Add((void *)labelSymbol, buf);
        return buf;
    }
}

const char *TR_Debug::getPerCodeCacheHelperName(TR_CCPreLoadedCode helper)
{
#if defined(TR_TARGET_POWER)
    switch (helper) {
        case TR_AllocPrefetch:
            return "Alloc Prefetch";
        case TR_ObjAlloc:
            return "Object Alloc Helper";
        case TR_VariableLenArrayAlloc:
            return "Variable Length Array Alloc Helper";
        case TR_ConstLenArrayAlloc:
            return "Constant Length Array Alloc Helper";
        case TR_writeBarrier:
            return "Write Barrier Helper";
        case TR_writeBarrierAndCardMark:
            return "Write Barrier and Card Mark Helper";
        case TR_cardMark:
            return "Card Mark Helper";
        case TR_arrayStoreCHK:
            return "Array Store Check";
        default:
            break;
    }
#endif // TR_TARGET_POWER
    return "Unknown Helper";
}

const char *TR_Debug::getName(TR::SymbolReference *symRef)
{
    int32_t index = symRef->getReferenceNumber();
    int32_t nonhelperIndex, numHelperSymbols;

    nonhelperIndex = comp()->getSymRefTab()->getNonhelperIndex(comp()->getSymRefTab()->getLastCommonNonhelperSymbol());
    numHelperSymbols = comp()->getSymRefTab()->getNumHelperSymbols();

    if (index < numHelperSymbols) {
        return getRuntimeHelperName(index);
    }

    if (index < nonhelperIndex) {
        if (index >= numHelperSymbols + TR::SymbolReferenceTable::firstArrayShadowSymbol
            && index < numHelperSymbols + TR::SymbolReferenceTable::firstArrayShadowSymbol + TR::NumAllTypes)
            return "<array-shadow>";
        if (index >= numHelperSymbols + TR::SymbolReferenceTable::firstPerCodeCacheHelperSymbol
            && index <= numHelperSymbols + TR::SymbolReferenceTable::lastPerCodeCacheHelperSymbol)
            return getPerCodeCacheHelperName((TR_CCPreLoadedCode)(index - numHelperSymbols
                - TR::SymbolReferenceTable::firstPerCodeCacheHelperSymbol));
        switch (index - numHelperSymbols) {
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
            case TR::SymbolReferenceTable::coldPathRecompTriggerCounterSymbol:
                return "<cold-path-recomp-trigger-counter>";
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
                return "<computedStaticCall>";
            case TR::SymbolReferenceTable::j9VMThreadFloatTemp1Symbol:
                return "<j9VMThreadFloatTemp1>";
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
            case TR::SymbolReferenceTable::isIdentityObjectNonHelperSymbol:
                return "<isIdentityObject>";
            case TR::SymbolReferenceTable::J9JNIMethodIDvTableIndexFieldSymbol:
                return "<J9JNIMethodIDvTableIndexField>";
            case TR::SymbolReferenceTable::contiguousArrayDataAddrFieldSymbol:
                return "<contiguousArrayDataAddrField>";
            case TR::SymbolReferenceTable::jitDispatchJ9MethodSymbol:
                return "<jitDispatchJ9Method>";
            case TR::SymbolReferenceTable::jProfileValueSymbol:
                return "<jProfileValue>";
            case TR::SymbolReferenceTable::jProfileValueWithNullCHKSymbol:
                return "<jProfileValueWithNullCHK>";
        }
    }

    TR::Symbol *sym = symRef->getSymbol();
    switch (sym->getKind()) {
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

void TR_Debug::print(OMR::Logger *log, TR::SymbolReferenceTable *symRefTab)
{
    if (symRefTab->baseArray.size() > 0 && _comp->getOption(TR_TraceAliases)) {
        log->prints("Symbol Reference Map for this method:\n");
        for (auto i = 0U; i < symRefTab->baseArray.size(); i++)
            if (symRefTab->getSymRef(static_cast<int32_t>(i)))
                log->printf("  %d[" POINTER_PRINTF_FORMAT "]\n", i, symRefTab->getSymRef(i));
    }
}

void TR_Debug::printAliasInfo(OMR::Logger *log, TR::SymbolReferenceTable *symRefTab)
{
    log->prints("\nSymbol References with Aliases:\n\n");
    for (int32_t i = symRefTab->getIndexOfFirstSymRef(); i < symRefTab->getNumSymRefs(); ++i)
        if (symRefTab->getSymRef(i))
            printAliasInfo(log, symRefTab->getSymRef(i));
}

void TR_Debug::printAliasInfo(OMR::Logger *log, TR::SymbolReference *symRef)
{
    TR_BitVector *useDefAliases = symRef->getUseDefAliasesBV();
    TR_BitVector *useOnlyAliases = symRef->getUseonlyAliasesBV(_comp->getSymRefTab());
    if (useDefAliases || useOnlyAliases) {
        log->printf("Symref #%d %s \n", symRef->getReferenceNumber(), getName(symRef));
        if (useOnlyAliases) {
            log->printf("   Use Aliases: %p   ", useOnlyAliases);
            print(log, useOnlyAliases);
            log->println();
        } else
            log->prints("   Use Aliases: NULL \n");

        if (useDefAliases) {
            log->printf("   Usedef Aliases: %p ", useDefAliases);
            print(log, useDefAliases);
            log->println();
        } else
            log->prints("   Usedef Aliases: NULL \n");
    } else {
        log->printf("Symref #%d %s has no aliases\n", symRef->getReferenceNumber(), getName(symRef));
    }
}

TR::ResolvedMethodSymbol *TR_Debug::getOwningMethodSymbol(TR::SymbolReference *symRef)
{
    return _comp->getOwningMethodSymbol(symRef->getOwningMethodIndex());
}

TR_ResolvedMethod *TR_Debug::getOwningMethod(TR::SymbolReference *symRef)
{
    return getOwningMethodSymbol(symRef)->getResolvedMethod();
}

const char *TR_Debug::getAutoName(TR::SymbolReference *symRef)
{
    int32_t slot = symRef->getCPIndex();
    const size_t nameSize = 50 + TR::Compiler->debug.pointerPrintfMaxLenInChars();
    char *name = (char *)_comp->trMemory()->allocateHeapMemory(nameSize);

    name[0] = 0; // initialize to empty string

    if (symRef->getSymbol()->isSpillTempAuto()) {
        const size_t symNameSize = 20;
        char *symName = (char *)_comp->trMemory()->allocateHeapMemory(symNameSize);
        if (symRef->getSymbol()->getDataType() == TR::Float || symRef->getSymbol()->getDataType() == TR::Double)
            snprintf(symName, symNameSize, "#FPSPILL%zu_%d", symRef->getSymbol()->getSize(),
                symRef->getReferenceNumber());
        else
            snprintf(symName, symNameSize, "#SPILL%zu_%d", symRef->getSymbol()->getSize(),
                symRef->getReferenceNumber());

        snprintf(name, nameSize, "<%s " POINTER_PRINTF_FORMAT ">", symName, symRef->getSymbol());
    } else if (symRef->isTempVariableSizeSymRef()) {
        TR_ASSERT(symRef->getSymbol()->isVariableSizeSymbol(), "symRef #%d must contain a variable size symbol\n",
            symRef->getReferenceNumber());
        TR::AutomaticSymbol *sym = symRef->getSymbol()->getVariableSizeSymbol();
        snprintf(name, nameSize, "<%s rc=%d>", getVSSName(sym), sym->getReferenceCount());
    } else if (symRef->getSymbol()->isPendingPush()) {
        snprintf(name, nameSize, "<pending push temp %d>", -slot - 1);
    } else if (slot < getOwningMethodSymbol(symRef)->getFirstJitTempIndex()) {
        int debugNameLen;
        const char *debugName = getOwningMethod(symRef)->localName(slot, 0, debugNameLen,
            comp()->trMemory()); // TODO: Proper bcIndex somehow; TODO: proper length
        if (!debugName) {
            debugName = "";
            debugNameLen = 0;
        }
        debugNameLen = std::min(debugNameLen, 15); // Don't overrun the buffer
        if (symRef->getSymbol()->castToAutoSymbol()->isPinningArrayPointer())
            snprintf(name, nameSize, "%.*s<pinning array auto slot %d>", debugNameLen, debugName, slot);
        else if (symRef->getSymbol()->castToAutoSymbol()->holdsMonitoredObject()) {
            if (symRef->holdsMonitoredObjectForSyncMethod())
                snprintf(name, nameSize, "%.*s<auto slot %d holds monitoredObject syncMethod>", debugNameLen, debugName,
                    slot);
            else
                snprintf(name, nameSize, "%.*s<auto slot %d holds monitoredObject>", debugNameLen, debugName, slot);
        } else
            snprintf(name, nameSize, "%.*s<auto slot %d>", debugNameLen, debugName, slot);
    } else {
        if (symRef->getSymbol()->castToAutoSymbol()->isInternalPointer())
            snprintf(name, nameSize, "<internal pointer temp slot %d>", slot);
        else {
            if (symRef->getSymbol()->castToAutoSymbol()->isPinningArrayPointer())
                snprintf(name, nameSize, "<pinning array temp slot %d>", slot);
            else if (symRef->getSymbol()->castToAutoSymbol()->holdsMonitoredObject()) {
                if (symRef->holdsMonitoredObjectForSyncMethod())
                    snprintf(name, nameSize, "<temp slot %d holds monitoredObject syncMethod>", slot);
                else
                    snprintf(name, nameSize, "<temp slot %d holds monitoredObject>", slot);
            } else
                snprintf(name, nameSize, "<temp slot %d>", slot);
        }
    }
    return name;
}

const char *TR_Debug::getParmName(TR::SymbolReference *symRef)
{
    int32_t debugNameLen, signatureLen;
    int32_t slot = symRef->getCPIndex();
    const char *s = symRef->getSymbol()->castToParmSymbol()->getTypeSignature(signatureLen);
    const char *debugName = getOwningMethod(symRef)->localName(slot, 0, debugNameLen,
        comp()->trMemory()); // TODO: Proper bcIndex somehow; TODO: proper length
    char *buf;

    if (!debugName) {
        debugName = "";
        debugNameLen = 0;
    }
    if (slot == 0 && !getOwningMethodSymbol(symRef)->isStatic()) {
        const size_t bufSize = debugNameLen + signatureLen + 17;
        buf = (char *)_comp->trMemory()->allocateHeapMemory(bufSize);
        snprintf(buf, bufSize, "%.*s<'this' parm %.*s>", debugNameLen, debugName, signatureLen, s);
    } else {
        const size_t bufSize = debugNameLen + signatureLen + 15;
        buf = (char *)_comp->trMemory()->allocateHeapMemory(bufSize);
        snprintf(buf, bufSize, "%.*s<parm %d %.*s>", debugNameLen, debugName, symRef->getCPIndex(), signatureLen, s);
    }
    return buf;
}

const char *TR_Debug::getMethodName(TR::SymbolReference *symRef)
{
    TR::Method *method = symRef->getSymbol()->castToMethodSymbol()->getMethod();

    if (method == NULL) {
        return "unknown";
    }

    return method->signature(comp()->trMemory(), stackAlloc);
}

const char *TR_Debug::getStaticName_ForListing(TR::SymbolReference *symRef)
{
    TR::StaticSymbol *sym = symRef->getSymbol()->castToStaticSymbol();

    if (_comp->getSymRefTab()->isConstantAreaSymbol(sym) && sym->isStatic() && sym->isNamed()) {
        return sym->getName();
    }

    return NULL;
}

const char *TR_Debug::getStaticName(TR::SymbolReference *symRef)
{
    TR::StaticSymbol *sym = symRef->getSymbol()->castToStaticSymbol();
    void *staticAddress;
    staticAddress = sym->getStaticAddress();

    if (sym->isClassObject()) {
        if (!sym->addressIsCPIndexOfStatic() && staticAddress) {
            int32_t len;
            const char *name = TR::Compiler->cls.classNameChars(comp(), symRef, len);
            if (name) {
                char *s = (char *)_comp->trMemory()->allocateHeapMemory(len + 1);
                snprintf(s, len + 1, "%.*s", len, name);
                return s;
            }
        }
        return "unknown class object";
    } else if (sym->isConstantPoolAddress())
        return "<constant pool address>";

    if (sym->isAddressOfClassObject())
        return "<address of class object>";

    if (sym->isConstString()) {
#ifdef J9_PROJECT_SPECIFIC
        TR::VMAccessCriticalSection getStaticNameCriticalSection(comp(),
            TR::VMAccessCriticalSection::tryToAcquireVMAccess);
        if (!symRef->isUnresolved() && getStaticNameCriticalSection.acquiredVMAccess()) {
            uintptr_t stringLocation = (uintptr_t)sym->castToStaticSymbol()->getStaticAddress();
            if (stringLocation) {
                uintptr_t string = comp()->fej9()->getStaticReferenceFieldAtAddress(stringLocation);
                uint64_t length64 = comp()->fej9()->getStringUTF8UnabbreviatedLength(string);
                if (length64 >= 1024) {
                    // Don't bother converting very long strings to UTF8 just to
                    // trace the first few and last few characters.
                    return "<string (long text omitted)>";
                }

                TR::StackMemoryRegion stackMemoryRegion(*comp()->trMemory());
                size_t length = (size_t)length64;
                char *contents
                    = (char *)comp()->trMemory()->allocateMemory(length + 1, stackAlloc, TR_MemoryBase::UnknownType);

                comp()->fej9()->getStringUTF8(string, contents, length + 1);

                //
                // We don't want to mess up the logs too much.  Make sure the
                // strings aren't too ugly.
                //

                // Use ellipsis if the string is too long
                const size_t LENGTH_LIMIT = 80;
                size_t prefixLength = length;
                size_t suffixLength = 0;
                const char *etc = "";
                size_t etcLength = 0;
                if (length > LENGTH_LIMIT) {
                    etc = "\"...\"";
                    etcLength = 5;
                    suffixLength = (LENGTH_LIMIT - etcLength) / 2;
                    prefixLength = LENGTH_LIMIT - etcLength - suffixLength;
                }

                // Replace unprintable characters with ?
                for (size_t i = 0; i < prefixLength; i++) {
                    if (!isprint(contents[i])) {
                        contents[i] = '?';
                    }
                }

                for (size_t i = length - suffixLength; i < length; i++) {
                    if (!isprint(contents[i])) {
                        contents[i] = '?';
                    }
                }

                // 11 for '<string "">', 1 for terminating NUL.
                size_t resultSize = prefixLength + etcLength + suffixLength + 11 + 1;
                char *result = (char *)_comp->trMemory()->allocateHeapMemory(resultSize);
                TR::snprintfNoTrunc(result, resultSize, "<string \"%.*s%s%s\">", (int)prefixLength, contents, etc,
                    contents + (length - suffixLength));

                return result;
            }
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

    if (symRef->getCPIndex() >= 0)
        return getOwningMethod(symRef)->staticName(symRef->getCPIndex(), comp()->trMemory());

    if (_comp->getSymRefTab()->isVtableEntrySymbolRef(symRef))
        return "<class_loader>";

#ifdef J9_PROJECT_SPECIFIC
    if (sym->isCallSiteTableEntry()) {
        const size_t sSize = 60;
        char *s = (char *)_comp->trMemory()->allocateHeapMemory(sSize);
        snprintf(s, sSize, "<callSite entry @%d " POINTER_PRINTF_FORMAT ">",
            sym->castToCallSiteTableEntrySymbol()->getCallSiteIndex(), staticAddress);
        return s;
    }

    if (sym->isMethodTypeTableEntry()) {
        const size_t sSize = 62;
        char *s = (char *)_comp->trMemory()->allocateHeapMemory(sSize);
        snprintf(s, sSize, "<methodType entry @%d " POINTER_PRINTF_FORMAT ">",
            sym->castToMethodTypeTableEntrySymbol()->getMethodTypeIndex(), staticAddress);
        return s;
    }
#endif

    if (_comp->getSymRefTab()->isConstantAreaSymbol(sym) && sym->isStatic() && sym->isNamed()) {
        return sym->getName();
    }

    if (staticAddress) {
        const size_t nameSize = TR::Compiler->debug.pointerPrintfMaxLenInChars() + 5;
        char *name = (char *)_comp->trMemory()->allocateHeapMemory(nameSize);
        snprintf(name, nameSize, POINTER_PRINTF_FORMAT, staticAddress);
        return name;
    }

    return "unknown static";
}

// Note: This array needs to match up with what is in compile/SymbolReferenceTable.hpp
static const char *commonNonhelperSymbolNames[] = { "<contiguousArraySize>", "<discontiguousArraySize>",
    "<arrayClassRomPtr>", "<classRomPtr>", "<javaLangClassFromClass>", "<classFromJavaLangClass>",
    "<addressOfClassOfMethod>", "<ramStaticsFromClass>", "<componentClass>", "<componentClassAsPrimitive>", "<isArray>",
    "<isClassDepthAndFlags>", "<initializeStatusFromClass>", "<isClassFlags>", "<vft>", "<currentThread>",
    "<recompilationCounter>", "<excp>", "<indexableSize>", "<resolveCheck>", "<arrayTranslate>",
    "<arrayTranslateAndTest>", "<long2String>", "<bitOpMem>", "<reverseLoad>", "<reverseStore>",
    "<currentTimeMaxPrecision>", "<encodeASCII>", "<headerFlags>", "<singlePrecisionSQRT>", "<threadPrivateFlags>",
    "<arrayletSpineFirstElement>", "<dltBlock>", "<countForRecompile>", "<gcrPatchPoint>", "<counterAddress>",
    "<startPC>", "<compiledMethod>", "<thisRangeExtension>", "<profilingBufferCursor>", "<profilingBufferEnd>",
    "<profilingBuffer>", "<osrBuffer>", "<osrScratchBuffer>", "<osrFrameIndex>", "<osrReturnAddress>",
    "<contiguousArrayDataAddrField>", "<coldPathRecompTriggerCounterSymbol>", "<potentialOSRPointHelper>",
    "<osrFearPointHelper>", "<eaEscapeHelper>", "<lowTenureAddress>", "<highTenureAddress>", "<fragmentParent>",
    "<globalFragment>", "<instanceShape>", "<instanceDescription>", "<descriptionWordFromPtr>",
    "<classFromJavaLangClassAsPrimitive>", "<javaVM>", "<heapBase>", "<heapTop>", "<j9methodExtraField>",
    "<j9methodConstantPoolField>", "<startPCLinkageInfo>", "<instanceShapeFromROMClass>", "<objectEqualityComparison>",
    "<objectInequalityComparison>", "<nonNullableArrayNullStoreCheck>", "<loadFlattenableArrayElementNonHelper>",
    "<storeFlattenableArrayElementNonHelper>", "<isIdentityObject>", "<synchronizedFieldLoad>", "<atomicAdd>",
    "<atomicFetchAndAdd>", "<atomicFetchAndAdd32Bit>", "<atomicFetchAndAdd64Bit>", "<atomicSwap>", "<atomicSwap32Bit>",
    "<atomicSwap64Bit>", "<atomicCompareAndSwapReturnStatus>", "<atomicCompareAndSwapReturnValue>",
    "<jProfileValueSymbol>", "<jProfileValueWithNullCHKSymbol>", "<j9VMThreadTempSlotField>",
    "<computedStaticCallSymbol>", "<j9VMThreadFloatTemp1>", "<J9JNIMethodIDvTableIndexFieldSymbol>",
    "<jitDispatchJ9Method>" };

const char *TR_Debug::getShadowName(TR::SymbolReference *symRef)
{
    if (symRef->getCPIndex() >= 0 && !symRef->getSymbol()->isArrayShadowSymbol())
        return getOwningMethod(symRef)->fieldName(symRef->getCPIndex(), comp()->trMemory());

    if (symRef->getSymbol() == _comp->getSymRefTab()->findGenericIntShadowSymbol()) {
        if (symRef->reallySharesSymbol(_comp)) {
            return "<generic int shadow>";
        } else {
            return "<immutable generic int shadow>";
        }
    }

    if (_comp->getSymRefTab()->isVtableEntrySymbolRef(symRef))
        return "<vtable-entry-symbol>";

    if (symRef->getSymbol()->isUnsafeShadowSymbol())
        return "<unsafe shadow sym>";

    if (symRef == _comp->getSymRefTab()->findHeaderFlagsSymbolRef())
        return "<object header flag word>";

    if (symRef->getSymbol()) {
        if (comp()->getSymRefTab()->isRefinedArrayShadow(symRef))
            return "<refined-array-shadow>";

        if (comp()->getSymRefTab()->isImmutableArrayShadow(symRef))
            return "<immutable-array-shadow>";

        if (symRef->getSymbol()->isArrayletShadowSymbol())
            return "<arraylet-shadow>";

        if (symRef->getSymbol()->isGlobalFragmentShadowSymbol())
            return "<global-fragmnet>";

        if (symRef->getSymbol()->isMemoryTypeShadowSymbol())
            return "<memory-type>";

        if (symRef->getSymbol()->isNamedShadowSymbol()) {
            return symRef->getSymbol()->getNamedShadowSymbol()->getName();
        }
    }

    const int32_t numCommonNonhelperSymbols = TR::SymbolReferenceTable::lastCommonNonhelperSymbol
        - TR::SymbolReferenceTable::firstCommonNonhelperNonArrayShadowSymbol - TR_numCCPreLoadedCode;
    static_assert(sizeof(commonNonhelperSymbolNames) / sizeof(commonNonhelperSymbolNames[0])
            == numCommonNonhelperSymbols,
        "commonNonhelperSymbolNames array must match CommonNonhelperSymbol enumeration");

    for (int32_t i = TR::SymbolReferenceTable::firstCommonNonhelperNonArrayShadowSymbol;
         i < _comp->getSymRefTab()->getLastCommonNonhelperSymbol(); i++) {
        TR::SymbolReference *other = _comp->getSymRefTab()->element((TR::SymbolReferenceTable::CommonNonhelperSymbol)i);
        if (other && other->getSymbol() == symRef->getSymbol())
            return commonNonhelperSymbolNames[i - TR::SymbolReferenceTable::firstCommonNonhelperNonArrayShadowSymbol];
    }

    return "unknown field";
}

const char *TR_Debug::getMetaDataName(TR::SymbolReference *symRef)
{
    const char *name = symRef->getSymbol()->getMethodMetaDataSymbol()->getName();
    return name ? name : "method meta data";
}

void TR_Debug::printBlockInfo(OMR::Logger *log, TR::Node *node)
{
    if (node) {
        if (node->getOpCodeValue() == TR::BBStart) {
            log->prints(" BBStart");

            TR::Block *block = node->getBlock();
            if (block->getNumber() >= 0)
                log->printf(" <block_%d>", block->getNumber());
            if (block->getFrequency() >= 0)
                log->printf(" (frequency %d)", block->getFrequency());
            if (block->isExtensionOfPreviousBlock())
                log->prints(" (extension of previous block)");
            if (block->isCatchBlock()) {
                int32_t length;
                const char *classNameChars = block->getExceptionClassNameChars();
                if (classNameChars) {
                    length = block->getExceptionClassNameLength();
                    log->printf(" (catches %.*s)", length, getName(classNameChars, length));
                } else {
                    classNameChars = "...";
                    length = 3;
                    log->prints(" (catches ...)");
                }
            }
            if (block->isSuperCold())
                log->prints(" (super cold)");
            else if (block->isCold())
                log->prints(" (cold)");

            if (block->isLoopInvariantBlock())
                log->prints(" (loop pre-header)");
            TR_BlockStructure *blockStructure = block->getStructureOf();
            if (_comp->getFlowGraph()->getStructure() && blockStructure) {
                TR_Structure *parent = blockStructure->getParent();
                while (parent) {
                    TR_RegionStructure *region = parent->asRegion();
                    if (region->isNaturalLoop() || region->containsInternalCycles()) {
                        log->printf(" (in loop %d)", region->getNumber());
                        break;
                    }
                    parent = parent->getParent();
                }
                TR_BlockStructure *dupBlock = blockStructure->getDuplicatedBlock();
                if (dupBlock)
                    log->printf(" (dup of block_%d)", dupBlock->getNumber());
            }
        } else if (node && node->getOpCodeValue() == TR::BBEnd) {
            log->prints(" BBEnd");

            TR::Block *block = node->getBlock();

            if (block->getNumber() >= 0)
                log->printf(" </block_%d>", block->getNumber());
        }
    }
}

void TR_Debug::clearNodeChecklist() { _nodeChecklist.empty(); }

void TR_Debug::saveNodeChecklist(TR_BitVector &saveArea) { saveArea = _nodeChecklist; }

void TR_Debug::restoreNodeChecklist(TR_BitVector &saveArea) { _nodeChecklist = saveArea; }

void TR_Debug::dumpLiveRegisters(OMR::Logger *log)
{
    int i = 0;

    log->prints("; Live regs: ");

    // Dump the number of each kind
    //
    for (i = 0; i < NumRegisterKinds; i++) {
        TR_RegisterKinds kind = (TR_RegisterKinds)i;
        TR_LiveRegisters *liveRegisters = _comp->cg()->getLiveRegisters(kind);
        if (liveRegisters)
            log->printf("%s=%d ", getRegisterKindName(kind), liveRegisters->getNumberOfLiveRegisters());
    }

    log->printc('{');

    // Dump all the registers of each kind
    //
    const char *separator = "";
    for (i = 0; i < NumRegisterKinds; i++) {
        TR_RegisterKinds kind = (TR_RegisterKinds)i;
        TR_LiveRegisters *liveRegisters = _comp->cg()->getLiveRegisters(kind);
        if (liveRegisters) {
            for (TR_LiveRegisterInfo *info = liveRegisters->getFirstLiveRegister(); info; info = info->getNext()) {
                log->printf("%s%s", separator, getName(info->getRegister()));
                separator = ", ";
            }
        }
    }

    log->printc('}');
}

void TR_Debug::setupToDumpTreesAndInstructions(const char *title)
{
    // REMOVE after downstream refactoring to use the Logger API
    setupToDumpTreesAndInstructions(getLogger(), title);
}

void TR_Debug::setupToDumpTreesAndInstructions(OMR::Logger *log, const char *title)
{
    log->printf("\n%s:\n", title);
    _nodeChecklist.empty();
    log->prints("\n\n============================================================\n");
}

void TR_Debug::dumpSingleTreeWithInstrs(TR::TreeTop *treeTop, TR::Instruction *instr, bool dumpTrees, bool dumpInstrs,
    bool dumpRefCounts, bool dumpLiveRegs)
{
    // REMOVE after downstream refactoring to use the Logger API
    dumpSingleTreeWithInstrs(getLogger(), treeTop, instr, dumpTrees, dumpInstrs, dumpRefCounts, dumpLiveRegs);
}

void TR_Debug::dumpSingleTreeWithInstrs(OMR::Logger *log, TR::TreeTop *treeTop, TR::Instruction *instr, bool dumpTrees,
    bool dumpInstrs, bool dumpRefCounts, bool dumpLiveRegs)
{
    if (dumpLiveRegs) {
        dumpLiveRegisters(log);
        log->prints("\n------------------------------\n");
    }

    if (dumpTrees)
        printWithFixedPrefix(log, treeTop->getNode(), 1, true, dumpRefCounts, " ");

    if (dumpInstrs) {
        log->prints("\n------------------------------\n");
        if (treeTop->getLastInstruction()) {
            TR::Instruction *cursor = instr;
            while (cursor) {
                print(log, cursor);
                if (cursor == treeTop->getLastInstruction())
                    break;
                cursor = cursor->getNext();
            }

#if defined(TR_TARGET_POWER) || defined(TR_TARGET_S390)
            // Loop a second time and dump outlined code
            TR::Instruction *lastOutlinedTarget = NULL;
            cursor = instr;
            while (cursor) {
                TR::Instruction *outlinedCursor = getOutlinedTargetIfAny(cursor);

                if (outlinedCursor) {
                    // Separator between main line code and outlined code
                    if (!lastOutlinedTarget)
                        log->prints("\n\n------------------------------\n");
                    if (outlinedCursor != lastOutlinedTarget) {
                        // Avoiding dumping the same outlined code sequence more than once by keeping track of the last
                        // sequence we dumped Not fool-proof, but cheap
                        lastOutlinedTarget = outlinedCursor;
                        while (outlinedCursor) {
                            print(log, outlinedCursor);
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
        log->prints("\n\n============================================================\n");
    }
}

void TR_Debug::dumpMethodInstrs(OMR::Logger *log, const char *title, bool dumpTrees, bool header)
{
    const char *hotnessString = _comp->getHotnessName(_comp->getMethodHotness());

    log->printf("\n<instructions\n"
                "\ttitle=\"%s\"\n"
                "\tmethod=\"%s\"\n"
                "\thotness=\"%s\">\n",
        title, signature(_comp->getMethodSymbol()), hotnessString);

    if (header)
        printInstrDumpHeader(log, title);

    TR::Instruction *instr = _comp->cg()->getFirstInstruction();
    if (dumpTrees) {
        _nodeChecklist.empty();
        log->prints("\n\n============================================================\n");
        for (TR::TreeTop *treeTop = _comp->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop()) {
            printWithFixedPrefix(log, treeTop->getNode(), 1, true, false, " ");
            if (treeTop->getLastInstruction()) {
                log->prints("\n------------------------------\n");
                while (instr) {
                    print(log, instr);
                    if (instr == treeTop->getLastInstruction())
                        break;
                    instr = instr->getNext();
                }
                instr = instr->getNext();
                log->prints("\n\n============================================================\n");
            } else
                log->println();
        }
    }

    while (instr) {
        print(log, instr, title);
        instr = instr->getNext();
    }

    // Dump OOL instruction sequences.
    //
#if defined(TR_TARGET_POWER) || defined(TR_TARGET_S390) || defined(TR_TARGET_ARM64)
    // After RA outlined code is linked to the regular instruction stream and will automatically be printed
    if (_comp->cg()->getCodeGeneratorPhase() < TR::CodeGenPhase::RegisterAssigningPhase)
#if defined(TR_TARGET_POWER)
        printPPCOOLSequences(log);
#elif defined(TR_TARGET_ARM64)
        printARM64OOLSequences(log);
#else
        printS390OOLSequences(log);
#endif
#elif defined(TR_TARGET_X86)
    if (_comp->target().cpu.isX86())
        printX86OOLSequences(log);
#endif

    log->prints("\n</instructions>\n");
}

void TR_Debug::dumpMixedModeDisassembly(OMR::Logger *log)
{
    const char *title = "Mixed Mode Disassembly";

    log->printf("<instructions\n"
                "\ttitle=\"%s\"\n"
                "\tmethod=\"%s\">\n",
        title, signature(_comp->getMethodSymbol()));

    TR::Node *n = 0;
    TR::Instruction *inst = _comp->cg()->getFirstInstruction();
    for (; inst; inst = inst->getNext()) {
        if (inst->getNode() != NULL
            && (!n
                || ((inst->getNode()->getByteCodeInfo().getCallerIndex() != n->getByteCodeInfo().getCallerIndex()
                        || inst->getNode()->getByteCodeInfo().getByteCodeIndex()
                            != n->getByteCodeInfo().getByteCodeIndex())
                    && inst->getBinaryLength() > 0))) {
            n = inst->getNode();

            log->prints("\n\n");
            size_t indentLen = 0;
            printByteCodeStack(log, n->getInlinedSiteIndex(), n->getByteCodeIndex(), &indentLen);
        }

        print(log, inst);
    }

    log->prints("\n</instructions>\n<snippets>");
    print(log, _comp->cg()->getSnippetList());
    log->prints("\n</snippets>\n");
}

void TR_Debug::dumpInstructionComments(OMR::Logger *log, TR::Instruction *instr, bool needsStartComment)
{
    CS2::HashIndex hashIndex;
    if (_comp->getToCommentMap().Locate((void *)instr, hashIndex)) {
        char *data;
        List<char> *comments = (List<char> *)_comp->getToCommentMap().DataAt(hashIndex);
        ListIterator<char> itr(comments);
        // If this is the first annotation, need to add space and semicolon
        if (itr.getFirst() && needsStartComment) {
            log->prints(" ;");
            needsStartComment = false;
        }

        // Dump out all comments added to the instruction _comp->getToCommentMap() hashtable
        for (data = itr.getFirst(); data != NULL; data = itr.getNext()) {
            log->printf(" %s", data);
        }
    }
}

#if !defined(TR_TARGET_POWER) && !defined(TR_TARGET_ARM) && !defined(TR_TARGET_ARM64) && !defined(TR_TARGET_RISCV)
void TR_Debug::print(OMR::Logger *log, TR::Instruction *inst) { print(log, inst, NULL); }
#endif

void TR_Debug::print(OMR::Logger *log, TR::Instruction *inst, const char *title)
{
#if defined(TR_TARGET_X86)
    if (_comp->target().cpu.isX86()) {
        printx(log, inst);
        return;
    }
#endif

#if defined(TR_TARGET_POWER)
    if (_comp->target().cpu.isPower()) {
        print(log, inst);
        return;
    }
#endif

#if defined(TR_TARGET_ARM)
    if (_comp->target().cpu.isARM()) {
        print(log, inst);
        return;
    }
#endif

#if defined(TR_TARGET_ARM64)
    if (_comp->target().cpu.isARM64()) {
        print(log, inst);
        return;
    }
#endif

#if defined(TR_TARGET_RISCV)
    if (TR::Compiler->target.cpu.isRISCV()) {
        print(log, inst);
        return;
    }
#endif

#if defined(TR_TARGET_S390)
    if (_comp->target().cpu.isZ()) {
        printz(log, inst, title);
        return;
    }
#endif
}

void TR_Debug::print(OMR::Logger *log, TR::GCRegisterMap *map)
{
#if defined(TR_TARGET_X86)
    if (_comp->target().cpu.isX86()) {
        printX86GCRegisterMap(log, map);
        return;
    }
#endif

#if defined(TR_TARGET_POWER)
    if (_comp->target().cpu.isPower()) {
        printPPCGCRegisterMap(log, map);
        return;
    }
#endif

#if defined(TR_TARGET_ARM)
    if (_comp->target().cpu.isARM()) {
        printARMGCRegisterMap(log, map);
        return;
    }
#endif

#if defined(TR_TARGET_S390)
    if (_comp->target().cpu.isZ()) {
        printS390GCRegisterMap(log, map);
        return;
    }
#endif

#if defined(TR_TARGET_ARM64)
    if (_comp->target().cpu.isARM64()) {
        printARM64GCRegisterMap(log, map);
        return;
    }
#endif

#if defined(TR_TARGET_RISCV)
    if (TR::Compiler->target.cpu.isRISCV()) {
        printRVGCRegisterMap(log, map);
        return;
    }
#endif
}

void TR_Debug::print(OMR::Logger *log, TR::list<TR::Snippet *> &snippetList)
{
    for (auto snippets = snippetList.begin(); snippets != snippetList.end(); ++snippets) {
        print(log, *snippets);
    }

    TR::KnownObjectTable *knot = _comp->getKnownObjectTable();
    if (knot != NULL && _comp->cg()->hasConstRefs()) {
        auto kois = _comp->cg()->getConstRefSortOrder();
        for (auto it = kois.begin(); it != kois.end(); it++) {
            TR::KnownObjectTable::Index koi = *it;
            TR::LabelSymbol *label = _comp->cg()->getConstRefLabel(koi);
            uint8_t *pos = label->getCodeLocation();
            printSnippetLabel(log, label, pos, "constant reference");
            printPrefix(log, NULL, pos, (uint8_t)sizeof(uintptr_t));
            log->printf("obj%d\n", koi);
        }
    }

    if (_comp->cg()->hasDataSnippets())
        _comp->cg()->dumpDataSnippets(log);
}

void TR_Debug::print(OMR::Logger *log, TR::Snippet *snippet)
{
#if defined(TR_TARGET_X86)
    if (_comp->target().cpu.isX86()) {
        printx(log, snippet);
        return;
    }
#endif
#if defined(TR_TARGET_POWER)
    if (_comp->target().cpu.isPower()) {
        printp(log, snippet);
        return;
    }
#endif
#if defined(TR_TARGET_ARM)
    if (_comp->target().cpu.isARM()) {
        printa(log, snippet);
        return;
    }
#endif
#if defined(TR_TARGET_S390)
    if (_comp->target().cpu.isZ()) {
        printz(log, (TR::Snippet *)snippet);
        return;
    }
#endif
#if defined(TR_TARGET_ARM64)
    if (_comp->target().cpu.isARM64()) {
        printa64(log, snippet);
        return;
    }
#endif
}

bool TR_Debug::refineRegisterSizeForDataType()
{
#if defined(TR_TARGET_X86)
    return true;
#elif defined(TR_TARGET_ARM)
    return true;
#elif defined(TR_TARGET_ARM64)
    return true;
#else
    return false;
#endif
}

const char *TR_Debug::getRealRegisterName(uint32_t regNum)
{
// need to add 1 to regNum because the enum has 0 = NoReg, and we want 0 to = GPR0
#if defined(TR_TARGET_X86)
    return getName(regNum + 1);
#endif
#if defined(TR_TARGET_POWER)
    // based on formula: 31 - regNum + FirstGPR, where FirstGPR=gp0=1
    return getPPCRegisterName(31 - regNum + 1);
#endif
#if defined(TR_TARGET_ARM)
    return getName(regNum + 1);
#endif
#if defined(TR_TARGET_ARM64)
    return getARM64RegisterName(regNum + 1);
#endif
#if defined(TR_TARGET_S390)
    return getS390RegisterName(regNum + 1);
#endif
    return "???";
}

const char *TR_Debug::getName(TR::Register *reg, TR_RegisterSizes size)
{
    if (!reg)
        return "(null)";

    if (reg->getRealRegister()) {
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
    if (reg->getRegisterPair()) {
        // Prefix checking for register pairs is a bit complex, and likely irrelevant since they
        // can't contain collected refs or internal pointers anyway.  Let's not worry about it.
        prefix[0] = '\0';
    } else {
        snprintf(prefix, maxPrefixSize + 1, "%s%s%s", reg->containsCollectedReference() ? "&" : "",
            reg->containsInternalPointer() ? "*" : "", reg->isPlaceholderReg() ? "D_" : "");
    }

    CS2::HashIndex hashIndex;
    if (_comp->getToStringMap().Locate((void *)reg, hashIndex)) {
        const char *name = (const char *)_comp->getToStringMap().DataAt(hashIndex);
        if (!strncmp(name, prefix, strlen(prefix)))
            return name;
        //
        // Prefix mismatch means some important flags have changed since the name
        // string was generated.  Ignore the cached string and generate a new one.
    }

    if (reg->getRegisterPair()) {
        const char *high = getName(reg->getHighOrder());
        const char *low = getName(reg->getLowOrder());
        const size_t bufSize = strlen(high) + strlen(low) + 2;
        char *buf = (char *)_comp->trMemory()->allocateHeapMemory(bufSize);
        snprintf(buf, bufSize, "%s:%s", high, low);
        _comp->getToStringMap().Add((void *)reg, buf);
        return buf;
    } else if (_comp->getAddressEnumerationOption(TR_EnumerateRegister)
        && _comp->getToNumberMap().Locate((void *)reg, hashIndex)) {
        // max register kind name size plus underscore plus 10-digit reg num plus null terminator
        const size_t bufSize = maxPrefixSize + 6 + 11;
        char *buf = (char *)_comp->trMemory()->allocateHeapMemory(bufSize);
        uint32_t regNum = (uint32_t)(intptr_t)_comp->getToNumberMap().DataAt(hashIndex);
        snprintf(buf, bufSize, "%s%s_%04d", prefix, getRegisterKindName(reg->getKind()), regNum);
        _comp->getToStringMap().Add((void *)reg, buf);
        return buf;
    } else {
        const size_t bufSize = maxPrefixSize + 6 + TR::Compiler->debug.pointerPrintfMaxLenInChars() + 1;
        char *buf = (char *)_comp->trMemory()->allocateHeapMemory(bufSize);
        snprintf(buf, bufSize, "%s%s_" POINTER_PRINTF_FORMAT, prefix, getRegisterKindName(reg->getKind()), reg);
        _comp->getToStringMap().Add((void *)reg, buf);
        return buf;
    }
}

const char *TR_Debug::getGlobalRegisterName(TR_GlobalRegisterNumber regNum, TR::DataType dt)
{
    // TR_WordReg is the historical default for register size
    //
    TR_RegisterSizes regSize = TR_WordReg;

    if (refineRegisterSizeForDataType()) {
        switch (dt) {
            case TR::Int8:
                regSize = TR_ByteReg;
                break;
            case TR::Int16:
                regSize = TR_HalfWordReg;
                break;
            case TR::Int32:
                regSize = TR_WordReg;
                break;
            case TR::Int64:
                regSize = TR_DoubleWordReg;
                break;
            case TR::Float:
                regSize = TR_FloatReg;
                break;
            case TR::Double:
                regSize = TR_DoubleReg;
                break;
            case TR::Address:
                regSize = _comp->target().is64Bit() ? TR_DoubleWordReg : TR_WordReg;
                break;
            default:
                if (dt.isVector() || dt.isMask()) {
                    switch (dt.getVectorLength()) {
                        case TR::VectorLength128:
                            regSize = TR_VectorReg128;
                            break;
                        case TR::VectorLength256:
                            regSize = TR_VectorReg256;
                            break;
                        case TR::VectorLength512:
                            regSize = TR_VectorReg512;
                            break;
                        default:
                            regSize = TR_UnknownSizeReg;
                            break;
                    }
                } else {
                    regSize = TR_UnknownSizeReg;
                }

                break;
        }
    }

    return getGlobalRegisterName(regNum, regSize);
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
        return getName(realRegNum, size);
#endif
    return "???";
}

const char *TR_Debug::getRegisterKindName(TR_RegisterKinds rk)
{
    switch (rk) {
        case TR_GPR:
            return "GPR";
        case TR_FPR:
            return "FPR";
        case TR_VMR:
            return "VMR";
        case TR_CCR:
            return "CCR";
        case TR_VRF:
            return "VRF";
        case TR_VSX_SCALAR:
            return "VSX_SCALAR";
        case TR_VSX_VECTOR:
            return "VSX_VECTOR";
        case TR_SSR:
            return "SSR";
        default:
            return "??R";
    }
}

void TR_Debug::printRegisterMask(OMR::Logger *log, TR_RegisterMask mask, TR_RegisterKinds rk)
{
    mask = mask & (TR::RealRegister::getAvailableRegistersMask(rk));
    int32_t n = populationCount(mask);

    TR::RealRegister *reg;
    if (mask) {
        TR_RegisterMask slider = 1;
        while (slider) {
            if (mask & slider) {
                reg = TR::RealRegister::regMaskToRealRegister(slider, rk, _comp->cg());
                log->prints(getName(reg));
                if (--n == 0)
                    break;
                else
                    log->printc(' ');
            }

            slider <<= 1;
        }
    }
}

void TR_Debug::print(OMR::Logger *log, TR::Register *reg, TR_RegisterSizes size)
{
#if defined(TR_TARGET_S390)
    if (reg == NULL && _comp->target().cpu.isZ()) // zero based ptr
    {
        log->prints("GPR0");
        return;
    }
#endif

    if (reg->getRealRegister()) {
#if defined(TR_TARGET_X86)
        if (_comp->target().cpu.isX86()) {
            print(log, (TR::RealRegister *)reg, size);
            return;
        }
#endif
#if defined(TR_TARGET_POWER)
        if (_comp->target().cpu.isPower()) {
            print(log, (TR::RealRegister *)reg, size);
            return;
        }
#endif
#if defined(TR_TARGET_ARM)
        if (_comp->target().cpu.isARM()) {
            print(log, (TR::RealRegister *)reg, size);
            return;
        }
#endif
#if defined(TR_TARGET_S390)
        if (_comp->target().cpu.isZ()) {
            print(log, toRealRegister(reg), size);
            return;
        }
#endif
#if defined(TR_TARGET_ARM64)
        if (_comp->target().cpu.isARM64()) {
            print(log, (TR::RealRegister *)reg, size);
            return;
        }
#endif
#if defined(TR_TARGET_RISCV)
        if (TR::Compiler->target.cpu.isRISCV()) {
            print(log, (TR::RealRegister *)reg, size);
            return;
        }
#endif
    } else {
        log->prints(getName(reg));
        if (reg->getRegisterPair()) {
            log->printc('(');
            print(log, reg->getHighOrder());
            log->printc(':');
            print(log, reg->getLowOrder());
            log->printc(')');
        }
    }
}

void TR_Debug::printFullRegInfo(OMR::Logger *log, TR::Register *reg)
{
    static const char *ignoreFreeRegs = feGetEnv("TR_ignoreFreeRegsDuringTraceRA");
    static const char *ignoreFreeAndLockedRegs = feGetEnv("TR_ignoreFreeAndLockedRegsDuringTraceRA");

    if (reg->getRealRegister()) {
        if (ignoreFreeRegs && reg->getRealRegister()->getState() == TR::RealRegister::Free)
            return;

        if (ignoreFreeAndLockedRegs
            && (reg->getRealRegister()->getState() == TR::RealRegister::Locked
                || reg->getRealRegister()->getState() == TR::RealRegister::Free))
            return;

        static const char *stateNames[5] = { "Free", "Unlatched", "Assigned", "Blocked", "Locked" };

        log->printf("[ %-4s ]", getName(reg));
        log->printf("[%c]", reg->getAssignedRegister() ? 'A' : ' ');
        log->printf("[%4x]", reg->getRealRegister()->getWeight());
        if (reg->getRealRegister()->getState() == TR::RealRegister::Assigned) {
            TR::Register *virtReg = reg->getAssignedRegister();
            TR_ASSERT(virtReg, "real register is assigned to an unknown virtual register");
            if (virtReg) {
                TR_ASSERT(reg->getRealRegister()->getHasBeenAssignedInMethod(),
                    "Register state is assigned but register has never been assigned in method");
            }
            log->printf("[ %-10s ]", getName(virtReg));
            log->printf("[%5d/%5d]", virtReg->getFutureUseCount(), virtReg->getTotalUseCount());
#ifdef TR_TARGET_S390
            log->printf("[%d]\n", virtReg->isUsedInMemRef());
#else
            log->println();
#endif
        } else {
            log->printf("[ %-10s ]", stateNames[reg->getRealRegister()->getState()]);
            // track restricted regs usage
            if (reg->getRealRegister()->getState() == TR::RealRegister::Locked && reg->getAssignedRegister() != NULL
                && reg->getAssignedRegister() != reg) {
                TR::Register *virtReg = reg->getAssignedRegister();
                log->printf("[%5d/%5d]", virtReg->getFutureUseCount(), virtReg->getTotalUseCount());

                log->printf("[ %-10s ]", getName(virtReg));
            }
            log->println();
        }
    } else {
        log->printf("[ %-12s ][ ", getName(reg));

        if (reg->getAssignedRegister())
            log->prints("Assigned  ");
        else if (reg->getFutureUseCount() != 0 && reg->getTotalUseCount() != reg->getFutureUseCount())
            log->prints("Spilled   ");
        else
            log->prints("Unassigned");

        log->printf(" ][ %-12s", reg->getAssignedRegister() ? getName(reg->getAssignedRegister()) : " ");

        log->printf(" ][%5d][%5d]\n", reg->getTotalUseCount(), reg->getFutureUseCount());
    }
}

void TR_Debug::printGPRegisterStatus(OMR::Logger *log, OMR::MachineConnector *machine)
{
#if defined(TR_TARGET_S390)
    if (_comp->target().cpu.isZ()) {
        printGPRegisterStatus(log, (TR::Machine *)machine);
        return;
    }
#endif
}

void TR_Debug::printFPRegisterStatus(OMR::Logger *log, OMR::MachineConnector *machine)
{
#if defined(TR_TARGET_S390)
    if (_comp->target().cpu.isZ()) {
        printFPRegisterStatus(log, (TR::Machine *)machine);
        return;
    }
#endif
}

const char *TR_Debug::toString(TR_RematerializationInfo *info)
{
    if (info->isRematerializableFromConstant())
        return "constant load";

    TR::Symbol *sym = info->getSymbolReference()->getSymbol();

    if (info->isRematerializableFromMemory()) {
        if (info->isIndirect())
            return info->isStore() ? "indirect memory store" : "indirect memory load";

        if (sym->isStatic())
            return info->isStore() ? "static memory store" : "static memory load";
        if (sym->isAutoOrParm())
            return info->isStore() ? "local memory store" : "local memory load";
        return info->isStore() ? "memory store" : "memory load";
    }
    if (info->isRematerializableFromAddress())
        return sym->isStatic() ? "static address load" : "local address load";
    return "unknown";
}

void TR_Debug::print(OMR::Logger *log, TR::RegisterMappedSymbol *localCursor, bool isSpillTemp)
{
    log->printf("  Local [%s] (GC map index : %3d, Offset : %3d, Size : %d) is an ", getName(localCursor),
        localCursor->getGCMapIndex(), localCursor->getOffset(), localCursor->getSize());
    if (!localCursor->isInitializedReference())
        log->prints("uninitialized ");
    else
        log->prints("initialized ");

    if (localCursor->isCollectedReference())
        log->prints("collected ");
    else if (!localCursor->isInternalPointer() && !localCursor->isPinningArrayPointer())
        log->prints("uncollected ");

    if (localCursor->isInternalPointer())
        log->prints("internal pointer ");
    else if (localCursor->isPinningArrayPointer())
        log->prints("pinning array pointer ");

    if (isSpillTemp)
        log->prints("spill ");

    if (localCursor->isLocalObject())
        log->prints("local object ");

    if (localCursor->isParm())
        log->prints("parm ");
    else
        log->prints("auto ");

    log->println();
}

void TR_Debug::print(OMR::Logger *log, uint8_t *instrStart, uint8_t *instrEnd) {}

void TR_Debug::print(OMR::Logger *log, TR::GCStackAtlas *atlas)
{
    log->prints("\n<atlas>\n\nInternal stack atlas:\n");
    log->printf("  numberOfMaps=%d\n", atlas->getNumberOfMaps());
    log->printf("  numberOfSlotsMapped=%d\n", atlas->getNumberOfSlotsMapped());
    log->printf("  numberOfParmSlots=%d\n", atlas->getNumberOfParmSlotsMapped());
    log->printf("  parmBaseOffset=%d\n", atlas->getParmBaseOffset());
    log->printf("  localBaseOffset=%d\n", atlas->getLocalBaseOffset());
    log->prints("\n  Locals information : \n");

    TR::ResolvedMethodSymbol *methodSymbol = _comp->getMethodSymbol();

    ListIterator<TR::ParameterSymbol> parameterIterator(&methodSymbol->getParameterList());
    TR::ParameterSymbol *parmCursor;
    for (parmCursor = parameterIterator.getFirst(); parmCursor; parmCursor = parameterIterator.getNext())
        print(log, parmCursor, false);

    ListIterator<TR::AutomaticSymbol> automaticIterator(&methodSymbol->getAutomaticList());
    TR::AutomaticSymbol *localCursor;
    for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
        print(log, localCursor, false);

    // Print the map for spill temps
    //
    for (auto temps = _comp->cg()->getCollectedSpillList().begin(); temps != _comp->cg()->getCollectedSpillList().end();
         ++temps) {
        TR::AutomaticSymbol *s = (*temps)->getSymbolReference()->getSymbol()->getAutoSymbol();
        print(log, s, true);
    }

    TR_InternalPointerMap *internalPtrMap = atlas->getInternalPointerMap();
    if (internalPtrMap) {
        log->prints("\n  Internal pointer autos information:\n");
        int32_t indexOfFirstInternalPtr = atlas->getIndexOfFirstInternalPointer();
        ListElement<TR_InternalPointerPair> *currElement = internalPtrMap->getInternalPointerPairs().getListHead();
        for (; currElement; currElement = currElement->getNextElement()) {
            int a = -1;
            if (currElement->getData()->getPinningArrayPointer()) {
                a = currElement->getData()->getPinningArrayPointer()->getGCMapIndex();
            }
            int b = -1;
            if (currElement->getData()->getInternalPointerAuto()) {
                b = currElement->getData()->getInternalPointerAuto()->getGCMapIndex();
            }
            log->printf("    Base array index : %d Internal pointer index : %d\n", a, b);
        }
    }

    if (!atlas->getPinningArrayPtrsForInternalPtrRegs().isEmpty()) {
        int32_t indexOfFirstInternalPtr = atlas->getIndexOfFirstInternalPointer();
        ListElement<TR::AutomaticSymbol> *currElement = atlas->getPinningArrayPtrsForInternalPtrRegs().getListHead();
        for (; currElement; currElement = currElement->getNextElement())
            log->printf("    Base array index : %d pins internal pointers only in regs\n",
                currElement->getData()->getGCMapIndex());
    } else if (!internalPtrMap)
        log->prints("\n  No internal pointers in this method\n");

    log->println();

    if (atlas->getStackAllocMap()) {
        log->printf("Stack alloc map size : %d ", atlas->getStackAllocMap()->getMapSizeInBytes());
        log->prints("\n  Stack slots containing local objects --> {");
        TR_GCStackAllocMap *stackAllocMap = atlas->getStackAllocMap();

        int mapBytes = (stackAllocMap->_numberOfSlotsMapped + 7) >> 3;
        uint32_t bits = 0;
        bool firstBitOn = true;
        for (int i = 0; i < mapBytes; ++i) {
            uint8_t mapByte = stackAllocMap->_mapBits[i];
            for (int j = 0; j < 8; ++j)
                if (bits < stackAllocMap->_numberOfSlotsMapped) {
                    if (mapByte & 1) {
                        if (!firstBitOn)
                            log->printf(",%d", bits);
                        else
                            log->printf("%d", bits);
                        firstBitOn = false;
                    }
                    mapByte >>= 1;
                    bits++;
                }
        }

        log->prints("}\n\n");
    }

    int32_t num = 1;
    ListIterator<TR_GCStackMap> maps(&atlas->getStackMapList());
    for (TR_GCStackMap *map = maps.getFirst(); map; map = maps.getNext()) {
        log->printf("  Map number : %d", num);
        print(log, map, atlas);
        log->println();
        num++;
    }

    log->prints("\n</atlas>\n");
}

void TR_Debug::print(OMR::Logger *log, TR_GCStackMap *map, TR::GCStackAtlas *atlas)
{
    log->printf("\n  Code offset range starts at [%08x]", map->_lowestCodeOffset);
    log->prints("\n  GC stack map information : ");
    log->printf("\n    number of stack slots mapped = %d", map->_numberOfSlotsMapped);
    log->prints("\n    live stack slots containing addresses --> {");

    int mapBytes = (map->_numberOfSlotsMapped + 7) >> 3;
    uint32_t bits = 0;
    bool firstBitOn = true;
    for (int i = 0; i < mapBytes; ++i) {
        uint8_t mapByte = map->_mapBits[i];
        for (int j = 0; j < 8; ++j)
            if (bits < map->_numberOfSlotsMapped) {
                if (mapByte & 1) {
                    if (!firstBitOn)
                        log->printf(",%d", bits);
                    else
                        log->printf("%d", bits);
                    firstBitOn = false;
                }
                mapByte >>= 1;
                bits++;
            }
    }

    log->prints("}\n  GC register map information : \n");
    TR_InternalPointerMap *internalPtrMap = map->getInternalPointerMap();
    if (internalPtrMap) {
        int32_t indexOfFirstInternalPtr = atlas->getIndexOfFirstInternalPointer();
        log->prints("    internal pointer regs information :\n");
        ListElement<TR_InternalPointerPair> *currElement = internalPtrMap->getInternalPointerPairs().getListHead();
        for (; currElement; currElement = currElement->getNextElement())
            log->printf("      pinning array GC stack map index = %d Internal pointer regnum = %d\n",
                currElement->getData()->getPinningArrayPointer()->getGCMapIndex(),
                currElement->getData()->getInternalPtrRegNum());
    }

    print(log, &map->_registerMap);
}

#ifdef J9_PROJECT_SPECIFIC
void TR_Debug::dump(OMR::Logger *log, TR_CHTable *chTable)
{
    const TR::Compilation::GuardSet &vguards = _comp->getVirtualGuards();

    if (!chTable->_preXMethods && !chTable->_classes && vguards.empty()) {
        return;
    }

    log->prints("                       Class Hierarchy Assumption Table\n");
    log->prints("----------------------------------------------------------------------------------------\n");

    if (!vguards.empty()) {
        uint8_t *startPC = _comp->cg()->getCodeStart();

        log->prints("Following virtual guards are NOPed:\n");
        uint32_t i = 0;
        for (auto info = vguards.begin(); info != vguards.end(); ++info, ++i) {
            const size_t nameSize = 49;
            char guardKindName[nameSize];
            snprintf(guardKindName, nameSize, "%s %s%s", getVirtualGuardKindName((*info)->getKind()),
                (*info)->mergedWithHCRGuard() ? "+ HCRGuard " : "", (*info)->mergedWithOSRGuard() ? "+ OSRGuard " : "");

            if (!(*info)->getSymbolReference())
                log->printf("[%4d] %-49s %s%s\n", i, guardKindName, (*info)->isInlineGuard() ? "inlined " : "",
                    guardKindName);
            else
                log->printf("[%4d] %-49s %scalleeSymbol=" POINTER_PRINTF_FORMAT "\n", i, guardKindName,
                    (*info)->isInlineGuard() ? "inlined " : "", (*info)->getSymbolReference()->getSymbol());
            ListIterator<TR_VirtualGuardSite> siteIt(&(*info)->getNOPSites());
            for (TR_VirtualGuardSite *site = siteIt.getFirst(); site; site = siteIt.getNext()) {
                uint8_t *loc = site->getLocation();
                uint8_t *&dest = site->getDestination();

                log->printf("\tSite: location=" POINTER_PRINTF_FORMAT " (e+%5x) branch-dest=" POINTER_PRINTF_FORMAT
                            " (e+%5x)\n",
                    loc, loc - startPC, dest, dest - startPC);
            }
            ListIterator<TR_InnerAssumption> innerIt(&(*info)->getInnerAssumptions());
            for (TR_InnerAssumption *inner = innerIt.getFirst(); inner; inner = innerIt.getNext()) {
                log->printf("\tInner Assumption: calleeSymbol=" POINTER_PRINTF_FORMAT " for parm ordinal=%d\n",
                    inner->_guard->getSymbolReference()->getSymbol(), inner->_ordinal);
            }
        }
    }

    if (chTable->_preXMethods) {
        log->prints("\nOverriding of the following methods will cause a recompilation:\n");

        for (int32_t i = chTable->_preXMethods->lastIndex(); i >= 0; --i) {
            TR_ResolvedMethod *method = chTable->_preXMethods->element(i);
            log->printf("[%s] %s\n", getName(method), method->signature(comp()->trMemory(), heapAlloc));
        }
    }

    if (chTable->_classes) {
        log->prints("\nExtension of the following classes will cause a recompilation:\n");

        char buf[256];
        for (int32_t i = chTable->_classes->lastIndex(); i >= 0; --i) {
            TR_OpaqueClassBlock *clazz = chTable->_classes->element(i);
            int32_t len;

            const char *sig = TR::Compiler->cls.classNameChars(comp(), clazz, len);

            if (len > 255)
                len = 255;
            strncpy(buf, sig, len);
            buf[len] = 0;

            log->printf("[%s] %s\n", getName(clazz), buf);
        }
    }

    log->prints("----------------------------------------------------------------------------------------\n");
}
#endif

const char *TR_Debug::getRuntimeHelperName(int32_t index)
{
#ifdef J9_PROJECT_SPECIFIC
    if (index < TR_FSRH) {
        switch (index) {
            case TR_checkCast:
                return "jitCheckCast";
            case TR_checkCastForArrayStore:
                return "jitCheckCastForArrayStore";
            case TR_instanceOf:
                return "jitInstanceOf";
            case TR_checkAssignable:
                return "jitCheckAssignable";
            case TR_induceOSRAtCurrentPC:
                return "jitInduceOSRAtCurrentPC";
            case TR_induceOSRAtCurrentPCAndRecompile:
                return "jitInduceOSRAtCurrentPCAndRecompile";
            case TR_monitorEntry:
                return "jitMonitorEntry";
            case TR_methodMonitorEntry:
                return "jitMethodMonitorEntry";
            case TR_monitorExit:
                return "jitMonitorExit";
            case TR_methodMonitorExit:
                return "jitMethodMonitorExit";
            case TR_transactionEntry:
                return "transactionEntry";
            case TR_transactionAbort:
                return "transactionAbort";
            case TR_transactionExit:
                return "transactionExit";
            case TR_asyncCheck:
                return "jitCheckAsyncMessages";

            case TR_estimateGPU:
                return "estimateGPU";
            case TR_regionEntryGPU:
                return "regionEntryGPU";
            case TR_regionExitGPU:
                return "regionExitGPU";
            case TR_copyToGPU:
                return "copyToGPU";
            case TR_copyFromGPU:
                return "copyFromGPU";
            case TR_allocateGPUKernelParms:
                return "allocateGPUKernelParms";
            case TR_launchGPUKernel:
                return "launchGPUKernel";
            case TR_invalidateGPU:
                return "invalidateGPU";
            case TR_getStateGPU:
                return "getStateGPU";
            case TR_flushGPU:
                return "flushGPU";
            case TR_callGPU:
                return "callGPU";

            case TR_newObject:
                return "jitNewObject";
            case TR_newObjectNoZeroInit:
                return "jitNewObjectNoZeroInit";
            case TR_newValue:
                return "jitNewValue";
            case TR_newValueNoZeroInit:
                return "jitNewValueNoZeroInit";
            case TR_newArray:
                return "jitNewArray";
            case TR_newArrayNoZeroInit:
                return "jitNewArrayNoZeroInit";
            case TR_aNewArray:
                return "jitANewArray";
            case TR_aNewArrayNoZeroInit:
                return "jitANewArrayNoZeroInit";

            case TR_getFlattenableField:
                return "jitGetFlattenableField";
            case TR_withFlattenableField:
                return "jitWithFlattenableField";
            case TR_putFlattenableField:
                return "jitPutFlattenableField";
            case TR_getFlattenableStaticField:
                return "jitGetFlattenableStaticField";
            case TR_putFlattenableStaticField:
                return "jitPutFlattenableStaticField";
            case TR_resolveFlattenableField:
                return "jitResolveFlattenableField";
            case TR_ldFlattenableArrayElement:
                return "jitLoadFlattenableArrayElement";
            case TR_strFlattenableArrayElement:
                return "jitStoreFlattenableArrayElement";

            case TR_multiANewArray:
                return "jitAMultiANewArray";
            case TR_aThrow:
                return "jitThrowException";
            case TR_methodTypeCheck:
                return "jitThrowWrongMethodTypeException";
            case TR_incompatibleReceiver:
                return "jitThrowIncompatibleReceiver";
            case TR_nullCheck:
                return "jitThrowNullPointerException";
            case TR_arrayBoundsCheck:
                return "jitThrowArrayIndexOutOfBounds";
            case TR_divCheck:
                return "jitThrowArithmeticException";
            case TR_arrayStoreException:
                return "jitThrowArrayStoreException";
            case TR_identityException:
                return "jitThrowIdentityException";
            case TR_typeCheckArrayStore:
                return "jitTypeCheckArrayStore";
            case TR_softwareReadBarrier:
                return "jitSoftwareReadBarrier";
            case TR_writeBarrierStore:
                return "jitWriteBarrierStore";
            case TR_writeBarrierStoreGenerational:
                return "jitWriteBarrierStoreGenerational";
            case TR_writeBarrierStoreGenerationalAndConcurrentMark:
                return "jitWriteBarrierStoreGenerationalAndConcurrentMark";
            case TR_writeBarrierStoreRealTimeGC:
                return "jitWriteBarrierStoreRealTimeGC";
            case TR_writeBarrierClassStoreRealTimeGC:
                return "jitWriteBarrierClassStoreRealTimeGC";
            case TR_writeBarrierBatchStore:
                return "jitWriteBarrierBatchStore";
            case TR_stackOverflow:
                return "jitStackOverflow";
            case TR_reportMethodEnter:
                return "jitReportMethodEnter";
            case TR_reportStaticMethodEnter:
                return "jitReportStaticMethodEnter";
            case TR_jitReportInstanceFieldRead:
                return "jitReportInstanceFieldRead";
            case TR_jitReportStaticFieldRead:
                return "jitReportStaticFieldRead";
            case TR_jitReportInstanceFieldWrite:
                return "jitReportInstanceFieldWrite";
            case TR_jitReportStaticFieldWrite:
                return "jitReportStaticFieldWrite";
            case TR_jitResolveFieldDirect:
                return "jitResolveFieldDirect";
            case TR_jitResolveFieldSetterDirect:
                return "jitResolveFieldSetterDirect";
            case TR_jitResolveStaticFieldDirect:
                return "jitResolveStaticFieldDirect";
            case TR_jitResolveStaticFieldSetterDirect:
                return "jitResolveStaticFieldSetterDirect";
            case TR_reportMethodExit:
                return "jitReportMethodExit";
            case TR_reportFinalFieldModified:
                return "jitReportFinalFieldModified";
            case TR_acquireVMAccess:
                return "jitAcquireVMAccess";
            case TR_jitCheckIfFinalizeObject:
                return "jitCheckIfFinalizeObject";
            case TR_releaseVMAccess:
                return "jitReleaseVMAccess";
            case TR_throwCurrentException:
                return "jitThrowCurrentException";
            case TR_throwClassCastException:
                return "jitThrowClassCastException";
            case TR_acmpeqHelper:
                return "jitAcmpeqHelper";
            case TR_acmpneHelper:
                return "jitAcmpneHelper";

            case TR_IncompatibleClassChangeError:
                return "jitThrowIncompatibleClassChangeError";
            case TR_newInstanceImplAccessCheck:
                return "jitNewInstanceImplAccessCheck";

            case TR_icallVMprJavaSendStatic0:
                return "icallVMprJavaSendStatic0";
            case TR_icallVMprJavaSendStatic1:
                return "icallVMprJavaSendStatic1";
            case TR_icallVMprJavaSendStaticJ:
                return "icallVMprJavaSendStaticJ";
            case TR_icallVMprJavaSendStaticF:
                return "icallVMprJavaSendStaticF";
            case TR_icallVMprJavaSendStaticD:
                return "icallVMprJavaSendStaticD";
            case TR_icallVMprJavaSendStaticSync0:
                return "icallVMprJavaSendStaticSync0";
            case TR_icallVMprJavaSendStaticSync1:
                return "icallVMprJavaSendStaticSync1";
            case TR_icallVMprJavaSendStaticSyncJ:
                return "icallVMprJavaSendStaticSyncJ";
            case TR_icallVMprJavaSendStaticSyncF:
                return "icallVMprJavaSendStaticSyncF";
            case TR_icallVMprJavaSendStaticSyncD:
                return "icallVMprJavaSendStaticSyncD";
            case TR_icallVMprJavaSendInvokeExact0:
                return "icallVMprJavaSendInvokeExact0";
            case TR_icallVMprJavaSendInvokeExact1:
                return "icallVMprJavaSendInvokeExact1";
            case TR_icallVMprJavaSendInvokeExactJ:
                return "icallVMprJavaSendInvokeExactJ";
            case TR_icallVMprJavaSendInvokeExactL:
                return "icallVMprJavaSendInvokeExactL";
            case TR_icallVMprJavaSendInvokeExactF:
                return "icallVMprJavaSendInvokeExactF";
            case TR_icallVMprJavaSendInvokeExactD:
                return "icallVMprJavaSendInvokeExactD";
            case TR_icallVMprJavaSendInvokeWithArguments:
                return "icallVMprJavaSendInvokeWithArguments";
            case TR_icallVMprJavaSendNativeStatic:
                return "icallVMprJavaSendNativeStatic";
            case TR_j2iTransition:
                return "j2iTransition";
            case TR_initialInvokeExactThunk_unwrapper:
                return "initialInvokeExactThunk_unwrapper";
            case TR_methodHandleJ2I_unwrapper:
                return "methodHandleJ2I_unwrapper";
            case TR_interpreterUnresolvedMethodTypeGlue:
                return "interpreterUnresolvedMethodTypeGlue";
            case TR_interpreterUnresolvedMethodHandleGlue:
                return "interpreterUnresolvedMethodHandleGlue";
            case TR_interpreterUnresolvedCallSiteTableEntryGlue:
                return "interpreterUnresolvedCallSiteTableEntryGlue";
            case TR_interpreterUnresolvedMethodTypeTableEntryGlue:
                return "interpreterUnresolvedMethodTypeTableEntryGlue";

            case TR_jitProfileValue:
                return "jitProfileValue";
            case TR_jitProfileLongValue:
                return "jitProfileLongValue";
            case TR_jitProfileAddress:
                return "jitProfileAddress";
            case TR_jitProfileWarmCompilePICAddress:
                return "jitProfileAddress for mainline code PIC's";
            case TR_jProfile32BitValue:
                return "jProfile32BitValue";
            case TR_jProfile64BitValue:
                return "jProfile64BitValue";
            case TR_prepareForOSR:
                return "prepareForOSR";

            case TR_jitRetranslateCaller:
                return "jitRetranslateCaller";
            case TR_jitRetranslateCallerWithPrep:
                return "jitRetranslateCallerWithPreparation";

            case TR_volatileReadLong:
                return "jitVolatileReadLong";
            case TR_volatileWriteLong:
                return "jitVolatileWriteLong";
            case TR_volatileReadDouble:
                return "jitVolatileReadDouble";
            case TR_volatileWriteDouble:
                return "jitVolatileWriteDouble";
            case TR_referenceArrayCopy:
                return "jitReferenceArrayCopy";
            case TR_jitLookupDynamicInterfaceMethod:
                return "jitLookupDynamicInterfaceMethod";
            case TR_jitLookupDynamicPublicInterfaceMethod:
                return "jitLookupDynamicPublicInterfaceMethod";
        }
    }
#ifdef TR_TARGET_X86
    else if (_comp->target().cpu.isI386() || _comp->target().cpu.isAMD64()) {
        if (index < TR_LXRH) {
            switch (index) {
                case TR_X86resolveIPicClass:
                    return "resolveIPicClass";
                case TR_X86populateIPicSlotClass:
                    return "populateIPicSlotClass";
                case TR_X86populateIPicSlotCall:
                    return "populateIPicSlotCall";
                case TR_X86dispatchInterpretedFromIPicSlot:
                    return "dispatchInterpretedFromIPicSlot";
                case TR_X86IPicLookupDispatch:
                    return "IPicLookupDispatch";
                case TR_X86resolveVPicClass:
                    return "resolveVPicClass";
                case TR_X86populateVPicSlotClass:
                    return "populateVPicSlotClass";
                case TR_X86populateVPicSlotCall:
                    return "populateVPicSlotCall";
                case TR_X86dispatchInterpretedFromVPicSlot:
                    return "dispatchInterpretedFromVPicSlot";
                case TR_X86populateVPicVTableDispatch:
                    return "populateVPicVTableDispatch";
                case TR_X86interpreterUnresolvedStaticGlue:
                    return "interpreterUnresolvedStaticGlue";
                case TR_X86interpreterUnresolvedSpecialGlue:
                    return "interpreterUnresolvedSpecialGlue";
                case TR_X86interpreterUnresolvedClassGlue:
                    return "interpreterUnresolvedClassGlue";
                case TR_X86interpreterUnresolvedClassFromStaticFieldGlue:
                    return "interpreterUnresolvedClassFromStaticFieldGlue";
                case TR_X86interpreterUnresolvedStringGlue:
                    return "interpreterUnresolvedStringGlue";
                case TR_X86interpreterUnresolvedStaticFieldGlue:
                    return "interpreterUnresolvedStaticFieldGlue";
                case TR_X86interpreterUnresolvedConstantDynamicGlue:
                    return "interpreterUnresolvedConstantDynamicGlue";
                case TR_X86interpreterUnresolvedStaticFieldSetterGlue:
                    return "interpreterUnresolvedStaticFieldSetterGlue";
                case TR_X86interpreterUnresolvedFieldGlue:
                    return "interpreterUnresolvedFieldGlue";
                case TR_X86interpreterUnresolvedFieldSetterGlue:
                    return "interpreterUnresolvedFieldSetterGlue";
                case TR_X86CodeCachePrefetchHelper:
                    return "per code cache TLH prefetch helper";
            }
        } else if (_comp->target().cpu.isI386()) {
            switch (index) {
                case TR_IA32longDivide:
                    return "__longDivide";
                case TR_IA32longRemainder:
                    return "__longRemainder";
                case TR_IA32floatRemainder:
                    return "__floatRemainder";
                case TR_IA32floatRemainderSSE:
                    return "__SSEfloatRemainderIA32Thunk";
                case TR_IA32doubleRemainder:
                    return "__doubleRemainder";
                case TR_IA32doubleRemainderSSE:
                    return "__SSEdoubleRemainderIA32Thunk";
                case TR_IA32doubleToLong:
                    return "__doubleToLong";
                case TR_IA32doubleToInt:
                    return "__doubleToInt";
                case TR_IA32floatToLong:
                    return "__floatToLong";
                case TR_IA32floatToInt:
                    return "__floatToInt";
                case TR_IA32double2LongSSE:
                    return "__SSEdouble2LongIA32";
                case TR_IA32jitCollapseJNIReferenceFrame:
                    return "_jitCollapseJNIReferenceFrame";

                case TR_IA32samplingRecompileMethod:
                    return "__samplingRecompileMethod";
                case TR_IA32countingRecompileMethod:
                    return "__countingRecompileMethod";
                case TR_IA32samplingPatchCallSite:
                    return "__samplingPatchCallSite";
                case TR_IA32countingPatchCallSite:
                    return "__countingPatchCallSite";
                case TR_IA32induceRecompilation:
                    return "__induceRecompilation";
            }
        } else {
            switch (index) {
                case TR_AMD64floatRemainder:
                    return "__SSEfloatRemainder";
                case TR_AMD64doubleRemainder:
                    return "__SSEdoubleRemainder";
                case TR_AMD64icallVMprJavaSendVirtual0:
                    return "_icallVMprJavaSendVirtual0";
                case TR_AMD64icallVMprJavaSendVirtual1:
                    return "_icallVMprJavaSendVirtual1";
                case TR_AMD64icallVMprJavaSendVirtualJ:
                    return "_icallVMprJavaSendVirtualJ";
                case TR_AMD64icallVMprJavaSendVirtualL:
                    return "_icallVMprJavaSendVirtualL";
                case TR_AMD64icallVMprJavaSendVirtualF:
                    return "_icallVMprJavaSendVirtualF";
                case TR_AMD64icallVMprJavaSendVirtualD:
                    return "_icallVMprJavaSendVirtualD";
                case TR_AMD64jitCollapseJNIReferenceFrame:
                    return "_jitCollapseJNIReferenceFrame";

                case TR_AMD64samplingRecompileMethod:
                    return "__samplingRecompileMethod";
                case TR_AMD64countingRecompileMethod:
                    return "__countingRecompileMethod";
                case TR_AMD64samplingPatchCallSite:
                    return "__samplingPatchCallSite";
                case TR_AMD64countingPatchCallSite:
                    return "__countingPatchCallSite";
                case TR_AMD64induceRecompilation:
                    return "__induceRecompilation";
                case TR_AMD64doAESENCDecrypt:
                    return "doAESDecrypt";
                case TR_AMD64doAESENCEncrypt:
                    return "doAESEncrypt";
            }
        }
    }
#elif defined(TR_TARGET_POWER)
    else if (_comp->target().cpu.isPower()) {
        switch (index) {
            case TR_PPCdouble2Long:
                return "__double2Long";
            case TR_PPCdoubleRemainder:
                return "__doubleRemainder";
            case TR_PPCinteger2Double:
                return "__integer2Double";
            case TR_PPClong2Double:
                return "__long2Double";
            case TR_PPClong2Float:
                return "__long2Float";
            case TR_PPClong2Float_mv:
                return "__long2Float_mv";
            case TR_PPClongMultiply:
                return "<null>";
            case TR_PPClongDivide:
                return "__longDivide";
            case TR_PPClongDivideEP:
                return "__longDivideEP";
            case TR_PPCunsignedLongDivide:
                return "__unsignedLongDivide";
            case TR_PPCunsignedLongDivideEP:
                return "__unsignedLongDivideEP";
            case TR_PPCinterpreterUnresolvedStaticGlue:
                return "_interpreterUnresolvedStaticGlue";
            case TR_PPCinterpreterUnresolvedSpecialGlue:
                return "_interpreterUnresolvedSpecialGlue";
            case TR_PPCinterpreterUnresolvedDirectVirtualGlue:
                return "_interpreterUnresolvedDirectVirtualGlue";
            case TR_PPCinterpreterUnresolvedClassGlue:
                return "_interpreterUnresolvedClassGlue";
            case TR_PPCinterpreterUnresolvedClassGlue2:
                return "_interpreterUnresolvedClassGlue2";
            case TR_PPCinterpreterUnresolvedStringGlue:
                return "_interpreterUnresolvedStringGlue";
            case TR_PPCinterpreterUnresolvedConstantDynamicGlue:
                return "_interpreterUnresolvedConstantDynamicGlue";
            case TR_PPCinterpreterUnresolvedStaticDataGlue:
                return "_interpreterUnresolvedStaticDataGlue";
            case TR_PPCinterpreterUnresolvedStaticDataStoreGlue:
                return "_interpreterUnresolvedStaticDataStoreGlue";
            case TR_PPCinterpreterUnresolvedInstanceDataGlue:
                return "_interpreterUnresolvedInstanceDataGlue";
            case TR_PPCinterpreterUnresolvedInstanceDataStoreGlue:
                return "_interpreterUnresolvedInstanceDataStoreGlue";
            case TR_PPCvirtualUnresolvedHelper:
                return "_virtualUnresolvedHelper";
            case TR_PPCinterfaceCallHelper:
                return "_interfaceCallHelper";
            case TR_PPCrevertToInterpreterGlue:
                return "_revertToInterpreterGlue";
            case TR_PPCarrayTranslateTRTO:
                return "__arrayTranslateTRTO (ISO8859_1 & ASCII)";
            case TR_PPCarrayTranslateTRTO255:
                return "__arrayTranslateTRTO255 (ISO8859_1)";
            case TR_PPCarrayTranslateTROT255:
                return "__arrayTranslateTROT255 (ISO8859_1)";
            case TR_PPCarrayTranslateTROT:
                return "__arrayTranslateTROT (ASCII)";

            case TR_PPCicallVMprJavaSendVirtual0:
                return "icallVMprJavaSendVirtual0";
            case TR_PPCicallVMprJavaSendVirtual1:
                return "icallVMprJavaSendVirtual1";
            case TR_PPCicallVMprJavaSendVirtualJ:
                return "icallVMprJavaSendVirtualJ";
            case TR_PPCicallVMprJavaSendVirtualF:
                return "icallVMprJavaSendVirtualF";
            case TR_PPCicallVMprJavaSendVirtualD:
                return "icallVMprJavaSendVirtualD";
            case TR_PPCinterpreterVoidStaticGlue:
                return "_interpreterVoidStaticGlue";
            case TR_PPCinterpreterSyncVoidStaticGlue:
                return "_interpreterSyncVoidStaticGlue";
            case TR_PPCinterpreterGPR3StaticGlue:
                return "_interpreterGPR3StaticGlue";
            case TR_PPCinterpreterSyncGPR3StaticGlue:
                return "_interpreterSyncGPR3StaticGlue";
            case TR_PPCinterpreterGPR3GPR4StaticGlue:
                return "_interpreterGPR3GPR4StaticGlue";
            case TR_PPCinterpreterSyncGPR3GPR4StaticGlue:
                return "_interpreterSyncGPR3GPR4StaticGlue";
            case TR_PPCinterpreterFPR0FStaticGlue:
                return "_interpreterFPR0FStaticGlue";
            case TR_PPCinterpreterSyncFPR0FStaticGlue:
                return "_interpreterSyncFPR0FStaticGlue";
            case TR_PPCinterpreterFPR0DStaticGlue:
                return "_interpreterFPR0DStaticGlue";
            case TR_PPCinterpreterSyncFPR0DStaticGlue:
                return "_interpreterSyncFPR0DStaticGlue";
            case TR_PPCnativeStaticHelperForUnresolvedGlue:
                return "_nativeStaticHelperForUnresolvedGlue";
            case TR_PPCnativeStaticHelper:
                return "_nativeStaticHelper";

            case TR_PPCinterfaceCompeteSlot2:
                return "_interfaceCompeteSlot2";
            case TR_PPCinterfaceSlotsUnavailable:
                return "_interfaceSlotsUnavailable";
            case TR_PPCcollapseJNIReferenceFrame:
                return "jitCollapseJNIReferenceFrame";
            case TR_PPCarrayCopy:
                return "__arrayCopy";
            case TR_PPCwordArrayCopy:
                return "__wordArrayCopy";
            case TR_PPChalfWordArrayCopy:
                return "__halfWordArrayCopy";
            case TR_PPCforwardArrayCopy:
                return "__forwardArrayCopy";
            case TR_PPCforwardWordArrayCopy:
                return "__forwardWordArrayCopy";
            case TR_PPCforwardHalfWordArrayCopy:
                return "__forwardHalfWordArrayCopy";

            case TR_PPCarrayCopy_dp:
                return "__arrayCopy_dp";
            case TR_PPCwordArrayCopy_dp:
                return "__wordArrayCopy_dp";
            case TR_PPChalfWordArrayCopy_dp:
                return "__halfWordArrayCopy_dp";
            case TR_PPCforwardArrayCopy_dp:
                return "__forwardArrayCopy_dp";
            case TR_PPCforwardWordArrayCopy_dp:
                return "__forwardWordArrayCopy_dp";
            case TR_PPCforwardHalfWordArrayCopy_dp:
                return "__forwardHalfWordArrayCopy_dp";

            case TR_PPCquadWordArrayCopy_vsx:
                return "__quadWordArrayCopy_vsx";
            case TR_PPCforwardQuadWordArrayCopy_vsx:
                return "__forwardQuadWordArrayCopy_vsx";
            case TR_PPCpostP10ForwardCopy:
                return "__postP10ForwardCopy";
            case TR_PPCpostP10GenericCopy:
                return "__postP10GenericCopy";
            case TR_PPCpostPNextForwardCopy:
                return "__postPNextForwardCopy";
            case TR_PPCpostPNextGenericCopy:
                return "__postPNextGenericCopy";

            case TR_PPCP256Multiply:
                return "ECP256MUL_PPC";
            case TR_PPCP256Mod:
                return "ECP256MOD_PPC";
            case TR_PPCP256addNoMod:
                return "ECP256addNoMod_PPC";
            case TR_PPCP256subNoMod:
                return "ECP256subNoMod_PPC";

            case TR_PPCencodeUTF16Big:
                return "__encodeUTF16Big";
            case TR_PPCencodeUTF16Little:
                return "__encodeUTF16Little";

            case TR_PPCreferenceArrayCopy:
                return "__referenceArrayCopy";
            case TR_PPCgeneralArrayCopy:
                return "__generalArrayCopy";
            case TR_PPCsamplingRecompileMethod:
                return "__samplingRecompileMethod";
            case TR_PPCcountingRecompileMethod:
                return "__countingRecompileMethod";
            case TR_PPCsamplingPatchCallSite:
                return "__samplingPatchCallSite";
            case TR_PPCcountingPatchCallSite:
                return "__countingPatchCallSite";
            case TR_PPCinduceRecompilation:
                return "__induceRecompilation";
            case TR_PPCarrayXor:
                return "_arrayxor";
            case TR_PPCarrayOr:
                return "_arrayor";
            case TR_PPCarrayAnd:
                return "_arrayand";
            case TR_PPCarrayCmp:
                return "_arraycmp";
            case TR_PPCoverlapArrayCopy:
                return "overlapArrayCopy";

            case TR_PPCAESEncryptVMX:
                return "PPCAESEncryptVMX";
            case TR_PPCAESDecryptVMX:
                return "PPCAESDecryptVMX";
            case TR_PPCAESEncrypt:
                return "PPCAESEncrypt";
            case TR_PPCAESDecrypt:
                return "PPCAESDecrypt";
            case TR_PPCAESCBCDecrypt:
                return "PPCAESCBCDecrypt";
            case TR_PPCAESCBCEncrypt:
                return "PPCAESCBCEncrypt";
            case TR_PPCAESKeyExpansion:
                return "PPCAESKeyExpansion";
            case TR_PPCcrc32_vpmsum:
                return "PPCcrc32_vpmsum";
            case TR_PPCcrc32_no_vpmsum:
                return "PPCcrc32_no_vpmsum";
            case TR_PPCcrc32_oneByte:
                return "PPCcrc32_oneByte";
        }
    }
#elif defined(TR_TARGET_S390)
    else if (_comp->target().cpu.isZ()) {
        switch (index) {
            case TR_S390double2Long:
                return "__double2Long";
            case TR_S390doubleRemainder:
                return "__doubleRemainder";
            case TR_S390double2Integer:
                return "__double2Integer";
            case TR_S390integer2Double:
                return "__integer2Double";
            case TR_S390intDivide:
                return "__intDivide";
            case TR_S390long2Double:
                return "__long2Double";
            case TR_S390longMultiply:
                return "__longMultiply";
            case TR_S390longDivide:
                return "__longDivide";
            case TR_S390longRemainder:
                return "__longRemainder";
            case TR_S390longShiftLeft:
                return "__longShiftLeft";
            case TR_S390longShiftRight:
                return "__longShiftRight";
            case TR_S390longUShiftRight:
                return "__longUShiftRight";
            case TR_S390interpreterUnresolvedStaticGlue:
                return "_interpreterUnresolvedStaticGlue";
            case TR_S390interpreterUnresolvedSpecialGlue:
                return "_interpreterUnresolvedSpecialGlue";
            case TR_S390interpreterUnresolvedClassGlue:
                return "_interpreterUnresolvedClassGlue";
            case TR_S390interpreterUnresolvedClassGlue2:
                return "_interpreterUnresolvedClassGlue2";
            case TR_S390interpreterUnresolvedStringGlue:
                return "_interpreterUnresolvedStringGlue";
            case TR_S390interpreterUnresolvedStaticDataGlue:
                return "_interpreterUnresolvedStaticDataGlue";
            case TR_S390interpreterUnresolvedStaticDataStoreGlue:
                return "_interpreterUnresolvedStaticDataStoreGlue";
            case TR_S390interpreterUnresolvedInstanceDataGlue:
                return "_interpreterUnresolvedInstanceDataGlue";
            case TR_S390interpreterUnresolvedInstanceDataStoreGlue:
                return "_interpreterUnresolvedInstanceDataStoreGlue";
            case TR_S390virtualUnresolvedHelper:
                return "_virtualUnresolvedHelper";
            case TR_S390interfaceCallHelper:
                return "_interfaceCallHelper";
            case TR_S390interfaceCallHelperSingleDynamicSlot:
                return "_interfaceCallHelperSingleDynamicSlot";
            case TR_S390interfaceCallHelperMultiSlots:
                return "_interfaceCallHelperMultiSlots";
            case TR_S390icallVMprJavaSendVirtual0:
                return "icallVMprJavaSendVirtual0";
            case TR_S390icallVMprJavaSendVirtual1:
                return "icallVMprJavaSendVirtual1";
            case TR_S390icallVMprJavaSendVirtualJ:
                return "icallVMprJavaSendVirtualJ";
            case TR_S390icallVMprJavaSendVirtualF:
                return "icallVMprJavaSendVirtualF";
            case TR_S390icallVMprJavaSendVirtualD:
                return "icallVMprJavaSendVirtualD";
            case TR_S390interpreterStaticSpecialCallGlue:
                return "_interpreterStaticSpecialCallGlue";
            case TR_S390jitLookupInterfaceMethod:
                return "__jitLookupInterfaceMethod";
            case TR_S390jitMethodIsNative:
                return "__jitMethodIsNative";
            case TR_S390jitMethodIsSync:
                return "__jitMethodIsSync";
            case TR_S390jitResolveClass:
                return "__jitResolveClass";
            case TR_S390jitResolveField:
                return "__jitResolveField";
            case TR_S390jitResolveConstantDynamicGlue:
                return "_jitResolveConstantDynamic";
            case TR_S390jitResolveFieldSetter:
                return "__jitResolveFieldSetter";
            case TR_S390jitResolveInterfaceMethod:
                return "__jitResolveInterfaceMethod";
            case TR_S390jitResolveStaticField:
                return "__jitResolveStaticField";
            case TR_S390jitResolveStaticFieldSetter:
                return "__jitResolveStaticFieldSetter";
            case TR_S390jitResolveString:
                return "__jitResolveString";
            case TR_S390jitResolveVirtualMethod:
                return "__jitResolveVirtualMethod";
            case TR_S390jitResolveSpecialMethod:
                return "__jitResolveSpecialMethod";
            case TR_S390jitResolveStaticMethod:
                return "__jitResolveStaticMethod";
            case TR_S390nativeStaticHelper:
                return "_nativeStaticHelper";
            case TR_S390referenceArrayCopyHelper:
                return "__referenceArrayCopyHelper";
            case TR_S390arrayCopyHelper:
                return "__arrayCopyHelper";
            case TR_S390arraySetZeroHelper:
                return "__arraySetZeroHelper";
            case TR_S390arraySetGeneralHelper:
                return "__arraySetGeneralHelper";
            case TR_S390arrayCmpHelper:
                return "__arrayCmpHelper";
            case TR_S390arrayTranslateAndTestHelper:
                return "__arrayTranslateAndTestHelper";
            case TR_S390long2StringHelper:
                return "__long2StringHelper";
            case TR_S390arrayXORHelper:
                return "__arrayXORHelper";
            case TR_S390arrayORHelper:
                return "__arrayORHelper";
            case TR_S390arrayANDHelper:
                return "__arrayANDHelper";
            case TR_S390collapseJNIReferenceFrame:
                return "_collapseJNIReferenceFrame";
            case TR_S390jitRetranslateMethod:
                return "_jitRetranslateMethod";
            case TR_S390countingRecompileMethod:
                return "__countingRecompileMethod";
            case TR_S390samplingRecompileMethod:
                return "__samplingRecompileMethod";
            case TR_S390countingPatchCallSite:
                return "__countingPatchCallSite";
            case TR_S390samplingPatchCallSite:
                return "__samplingPatchCallSite";
            case TR_S390jitProfileAddressC:
                return "_jitProfileAddressC";
            case TR_S390jitProfileValueC:
                return "_jitProfileValueC";
            case TR_S390jitProfileLongValueC:
                return "_jitProfileLongValueC";
            case TR_S390revertToInterpreter:
                return "__revertToInterpreter";
            case TR_S390CEnvironmentAddress:
                return "__CEnvironmentAddress";
            case TR_S390jitPreJNICallOffloadCheck:
                return "_jitPreJNICallOffloadCheck";
            case TR_S390jitPostJNICallOffloadCheck:
                return "_jitPostJNICallOffloadCheck";
            case TR_S390jitMathHelperDREM:
                return "__jitMathHelperDREM";
            case TR_S390floatRemainder:
                return "__floatRemainder";
            case TR_S390jitMathHelperFREM:
                return "__jitMathHelperFREM";
            case TR_S390jitCallCFunction:
                return "__jitCallCFunction";
            case TR_S390mcc_reservationAdjustment_unwrapper:
                return "__mcc_reservationAdjustment_unwrapper";
            case TR_S390mcc_callPointPatching_unwrapper:
                return "__mcc_callPointPatching_unwrapper";
            case TR_S390mcc_lookupHelperTrampoline_unwrapper:
                return "__mcc_lookupHelperTrampoline_unwrapper";
            case TR_S390jitMathHelperConvertLongToFloat:
                return "__jitMathHelperConvertLongToFloat";
            case TR_S390induceRecompilation:
                return "__induceRecompilation";
            case TR_S390induceRecompilation_unwrapper:
                return "__induceRecompilation_unwrapper";
            case TR_S390OutlinedNew:
                return "outlinedNew";
        }
    }
#elif defined(TR_TARGET_ARM)
    else if (_comp->target().cpu.isARM()) {
        switch (index) {
            case TR_ARMdouble2Long:
                return "__double2Long";
            case TR_ARMdoubleRemainder:
                return "__doubleRemainder";
            case TR_ARMdouble2Integer:
                return "__double2Integer";
            case TR_ARMfloat2Long:
                return "__float2Long";
            case TR_ARMfloatRemainder:
                return "__floatRemainder";
            case TR_ARMinteger2Double:
                return "__integer2Double";
            case TR_ARMlong2Double:
                return "__long2Double";
            case TR_ARMlong2Float:
                return "__long2Float";
            case TR_ARMlongMultiply:
                return "__multi64";
            case TR_ARMintDivide:
                return "__intDivide";
            case TR_ARMintRemainder:
                return "__intRemainder";
            case TR_ARMlongDivide:
                return "__longDivide";
            case TR_ARMlongRemainder:
                return "__longRemainder";
            case TR_ARMlongShiftRightArithmetic:
                return "__longShiftRightArithmetic";
            case TR_ARMlongShiftRightLogical:
                return "__longShiftRightLogical";
            case TR_ARMlongShiftLeftLogical:
                return "__longShiftLeftLogical";
            case TR_ARMarrayCopy:
                return "__arrayCopy";
            case TR_ARMjitCollapseJNIReferenceFrame:
                return "jitCollapseJNIReferenceFrame";
            case TR_ARMinterpreterUnresolvedStaticGlue:
                return "_interpreterUnresolvedStaticGlue";
            case TR_ARMinterpreterUnresolvedSpecialGlue:
                return "_interpreterUnresolvedSpecialGlue";
            case TR_ARMinterpreterUnresolvedDirectVirtualGlue:
                return "_interpreterUnresolvedDirectVirtualGlue";
            case TR_ARMinterpreterUnresolvedClassGlue:
                return "_interpreterUnresolvedClassGlue";
            case TR_ARMinterpreterUnresolvedClassGlue2:
                return "_interpreterUnresolvedClassGlue2";
            case TR_ARMinterpreterUnresolvedStringGlue:
                return "_interpreterUnresolvedStringGlue";
            case TR_ARMinterpreterUnresolvedStaticDataGlue:
                return "_interpreterUnresolvedStaticDataGlue";
            case TR_ARMinterpreterUnresolvedInstanceDataGlue:
                return "_interpreterUnresolvedInstanceDataGlue";
            case TR_ARMinterpreterUnresolvedStaticDataStoreGlue:
                return "_interpreterUnresolvedStaticDataStoreGlue";
            case TR_ARMinterpreterUnresolvedInstanceDataStoreGlue:
                return "_interpreterUnresolvedInstanceDataStoreGlue";
            case TR_ARMvirtualUnresolvedHelper:
                return "_virtualUnresolvedHelper";
            case TR_ARMinterfaceCallHelper:
                return "_interfaceCallHelper";

            case TR_ARMicallVMprJavaSendVirtual0:
                return "icallVMprJavaSendVirtual0";
            case TR_ARMicallVMprJavaSendVirtual1:
                return "icallVMprJavaSendVirtual1";
            case TR_ARMicallVMprJavaSendVirtualJ:
                return "icallVMprJavaSendVirtualJ";
            case TR_ARMicallVMprJavaSendVirtualF:
                return "icallVMprJavaSendVirtualF";
            case TR_ARMicallVMprJavaSendVirtualD:
                return "icallVMprJavaSendVirtualD";
            case TR_ARMinterpreterVoidStaticGlue:
                return "_interpreterVoidStaticGlue";
            case TR_ARMinterpreterSyncVoidStaticGlue:
                return "_interpreterSyncVoidStaticGlue";
            case TR_ARMinterpreterGPR3StaticGlue:
                return "_interpreterGPR3StaticGlue";
            case TR_ARMinterpreterSyncGPR3StaticGlue:
                return "_interpreterSyncGPR3StaticGlue";
            case TR_ARMinterpreterGPR3GPR4StaticGlue:
                return "_interpreterGPR3GPR4StaticGlue";
            case TR_ARMinterpreterSyncGPR3GPR4StaticGlue:
                return "_interpreterSyncGPR3GPR4StaticGlue";
            case TR_ARMinterpreterFPR0FStaticGlue:
                return "_interpreterFPR0FStaticGlue";
            case TR_ARMinterpreterSyncFPR0FStaticGlue:
                return "_interpreterSyncFPR0FStaticGlue";
            case TR_ARMinterpreterFPR0DStaticGlue:
                return "_interpreterFPR0DStaticGlue";
            case TR_ARMinterpreterSyncFPR0DStaticGlue:
                return "_interpreterSyncFPR0DStaticGlue";
            case TR_ARMnativeStaticHelper:
                return "_nativeStaticHelper";

            case TR_ARMinterfaceCompeteSlot2:
                return "_interfaceCompeteSlot2";
            case TR_ARMinterfaceSlotsUnavailable:
                return "_interfaceSlotsUnavailable";
#ifdef ZAURUS
            case TR_ARMjitMathHelperFloatAddFloat:
                return "jitMathHelperFloatAddFloat";
            case TR_ARMjitMathHelperFloatSubtractFloat:
                return "jitMathHelperFloatSubtractFloat";
            case TR_ARMjitMathHelperFloatMultiplyFloat:
                return "jitMathHelperFloatMultiplyFloat";
            case TR_ARMjitMathHelperFloatDivideFloat:
                return "jitMathHelperFloatDivideFloat";
            case TR_ARMjitMathHelperFloatRemainderFloat:
                return "jitMathHelperFloatRemainderFloat";
            case TR_ARMjitMathHelperDoubleAddDouble:
                return "jitMathHelperDoubleAddDouble";
            case TR_ARMjitMathHelperDoubleSubtractDouble:
                return "jitMathHelperDoubleSubtractDouble";
            case TR_ARMjitMathHelperDoubleMultiplyDouble:
                return "jitMathHelperDoubleMultiplyDouble";
            case TR_ARMjitMathHelperDoubleDivideDouble:
                return "jitMathHelperDoubleDivideDouble";
            case TR_ARMjitMathHelperDoubleRemainderDouble:
                return "jitMathHelperDoubleRemainderDouble";
            case TR_ARMjitMathHelperFloatNegate:
                return "jitMathHelperFloatNegate";
            case TR_ARMjitMathHelperDoubleNegate:
                return "jitMathHelperDoubleNegate";
            case TR_ARMjitMathHelperConvertFloatToInt:
                return "helperCConvertFloatToInteger";
            case TR_ARMjitMathHelperConvertFloatToLong:
                return "helperCConvertFloatToLong";
            case TR_ARMjitMathHelperConvertFloatToDouble:
                return "jitMathHelperConvertFloatToDouble";
            case TR_ARMjitMathHelperConvertDoubleToInt:
                return "helperCConvertDoubleToInteger";
            case TR_ARMjitMathHelperConvertDoubleToLong:
                return "helperCConvertDoubleToLong";
            case TR_ARMjitMathHelperConvertDoubleToFloat:
                return "jitMathHelperConvertDoubleToFloat";
            case TR_ARMjitMathHelperConvertIntToFloat:
                return "jitMathHelperConvertIntToFloat";
            case TR_ARMjitMathHelperConvertIntToDouble:
                return "jitMathHelperConvertIntToDouble";
            case TR_ARMjitMathHelperConvertLongToFloat:
                return "jitMathHelperConvertLongToFloat";
            case TR_ARMjitMathHelperConvertLongToDouble:
                return "jitMathHelperConvertLongToDouble";
#else
            case TR_ARMjitMathHelperFloatAddFloat:
                return "helperCFloatPlusFloat";
            case TR_ARMjitMathHelperFloatSubtractFloat:
                return "helperCFloatMinusFloat";
            case TR_ARMjitMathHelperFloatMultiplyFloat:
                return "helperCFloatMultiplyFloat";
            case TR_ARMjitMathHelperFloatDivideFloat:
                return "helperCFloatDivideFloat";
            case TR_ARMjitMathHelperFloatRemainderFloat:
                return "helperCFloatRemainderFloat";
            case TR_ARMjitMathHelperDoubleAddDouble:
                return "helperCDoublePlusDouble";
            case TR_ARMjitMathHelperDoubleSubtractDouble:
                return "helperCDoubleMinusDouble";
            case TR_ARMjitMathHelperDoubleMultiplyDouble:
                return "helperCDoubleMultiplyDouble";
            case TR_ARMjitMathHelperDoubleDivideDouble:
                return "helperCDoubleDivideDouble";
            case TR_ARMjitMathHelperDoubleRemainderDouble:
                return "helperCDoubleRemainderDouble";
            case TR_ARMjitMathHelperFloatNegate:
                return "jitMathHelperFloatNegate";
            case TR_ARMjitMathHelperDoubleNegate:
                return "jitMathHelperDoubleNegate";
            case TR_ARMjitMathHelperConvertFloatToInt:
                return "helperCConvertFloatToInteger";
            case TR_ARMjitMathHelperConvertFloatToLong:
                return "helperCConvertFloatToLong";
            case TR_ARMjitMathHelperConvertFloatToDouble:
                return "helperCConvertFloatToDouble";
            case TR_ARMjitMathHelperConvertDoubleToInt:
                return "helperCConvertDoubleToInteger";
            case TR_ARMjitMathHelperConvertDoubleToLong:
                return "helperCConvertDoubleToLong";
            case TR_ARMjitMathHelperConvertDoubleToFloat:
                return "helperCConvertDoubleToFloat";
            case TR_ARMjitMathHelperConvertIntToFloat:
                return "helperCConvertIntegerToFloat";
            case TR_ARMjitMathHelperConvertIntToDouble:
                return "helperCConvertIntegerToDouble";
            case TR_ARMjitMathHelperConvertLongToFloat:
                return "helperCConvertLongToFloat";
            case TR_ARMjitMathHelperConvertLongToDouble:
                return "helperCConvertLongToDouble";
#endif
            case TR_ARMjitMathHelperFloatCompareEQ:
                return "jitMathHelperFloatCompareEQ";
            case TR_ARMjitMathHelperFloatCompareEQU:
                return "jitMathHelperFloatCompareEQU";
            case TR_ARMjitMathHelperFloatCompareNE:
                return "jitMathHelperFloatCompareNE";
            case TR_ARMjitMathHelperFloatCompareNEU:
                return "jitMathHelperFloatCompareNEU";
            case TR_ARMjitMathHelperFloatCompareLT:
                return "jitMathHelperFloatCompareLT";
            case TR_ARMjitMathHelperFloatCompareLTU:
                return "jitMathHelperFloatoCmpareLTU";
            case TR_ARMjitMathHelperFloatCompareLE:
                return "jitMathHelperFloatCompareLE";
            case TR_ARMjitMathHelperFloatCompareLEU:
                return "jitMathHelperFloatCompareLEU";
            case TR_ARMjitMathHelperFloatCompareGT:
                return "jitMathHelperFloatCompareGT";
            case TR_ARMjitMathHelperFloatCompareGTU:
                return "jitMathHelperFloatoCmpareGTU";
            case TR_ARMjitMathHelperFloatCompareGE:
                return "jitMathHelperFloatCompareGE";
            case TR_ARMjitMathHelperFloatCompareGEU:
                return "jitMathHelperFloatCompareGEU";
            case TR_ARMjitMathHelperDoubleCompareEQ:
                return "jitMathHelperDoubleCompareEQ";
            case TR_ARMjitMathHelperDoubleCompareEQU:
                return "jitMathHelperDoubleCompareEQU";
            case TR_ARMjitMathHelperDoubleCompareNE:
                return "jitMathHelperDoubleCompareNE";
            case TR_ARMjitMathHelperDoubleCompareNEU:
                return "jitMathHelperDoubleCompareNEU";
            case TR_ARMjitMathHelperDoubleCompareLT:
                return "jitMathHelperDoubleCompareLT";
            case TR_ARMjitMathHelperDoubleCompareLTU:
                return "jitMathHelperDoubleCompareLTU";
            case TR_ARMjitMathHelperDoubleCompareLE:
                return "jitMathHelperDoubleCompareLE";
            case TR_ARMjitMathHelperDoubleCompareLEU:
                return "jitMathHelperDoubleCompareLEU";
            case TR_ARMjitMathHelperDoubleCompareGT:
                return "jitMathHelperDoubleCompareGT";
            case TR_ARMjitMathHelperDoubleCompareGTU:
                return "jitMathHelperDoubleCompareGTU";
            case TR_ARMjitMathHelperDoubleCompareGE:
                return "jitMathHelperDoubleCompareGE";
            case TR_ARMjitMathHelperDoubleCompareGEU:
                return "jitMathHelperDoubleCompareGEU";
            case TR_ARMsamplingRecompileMethod:
                return "_samplingRecompileMethod";
            case TR_ARMcountingRecompileMethod:
                return "_countingRecompileMethod";
            case TR_ARMsamplingPatchCallSite:
                return "_samplingPatchCallSite";
            case TR_ARMcountingPatchCallSite:
                return "_countingPatchCallSite";
            case TR_ARMinduceRecompilation:
                return "_induceRecompilation";
            case TR_ARMrevertToInterpreterGlue:
                return "_revertToInterpreterGlue";
        }
    }
#elif defined(TR_TARGET_ARM64)
    else if (_comp->target().cpu.isARM64()) {
        switch (index) {
            case TR_ARM64interpreterUnresolvedStaticGlue:
                return "_interpreterUnresolvedStaticGlue";
            case TR_ARM64interpreterUnresolvedSpecialGlue:
                return "_interpreterUnresolvedSpecialGlue";
            case TR_ARM64interpreterUnresolvedDirectVirtualGlue:
                return "_interpreterUnresolvedDirectVirtualGlue";
            case TR_ARM64interpreterUnresolvedClassGlue:
                return "_interpreterUnresolvedClassGlue";
            case TR_ARM64interpreterUnresolvedClassGlue2:
                return "_interpreterUnresolvedClassGlue2";
            case TR_ARM64interpreterUnresolvedStringGlue:
                return "_interpreterUnresolvedStringGlue";
            case TR_ARM64interpreterUnresolvedConstantDynamicGlue:
                return "_interpreterUnresolvedConstantDynamicGlue";
            case TR_ARM64interpreterUnresolvedStaticDataGlue:
                return "_interpreterUnresolvedStaticDataGlue";
            case TR_ARM64interpreterUnresolvedInstanceDataGlue:
                return "_interpreterUnresolvedInstanceDataGlue";
            case TR_ARM64interpreterUnresolvedStaticDataStoreGlue:
                return "_interpreterUnresolvedStaticDataStoreGlue";
            case TR_ARM64interpreterUnresolvedInstanceDataStoreGlue:
                return "_interpreterUnresolvedInstanceDataStoreGlue";
            case TR_ARM64virtualUnresolvedHelper:
                return "_virtualUnresolvedHelper";
            case TR_ARM64interfaceCallHelper:
                return "_interfaceCallHelper";
            case TR_ARM64icallVMprJavaSendVirtual0:
                return "icallVMprJavaSendVirtual0";
            case TR_ARM64icallVMprJavaSendVirtual1:
                return "icallVMprJavaSendVirtual1";
            case TR_ARM64icallVMprJavaSendVirtualJ:
                return "icallVMprJavaSendVirtualJ";
            case TR_ARM64icallVMprJavaSendVirtualF:
                return "icallVMprJavaSendVirtualF";
            case TR_ARM64icallVMprJavaSendVirtualD:
                return "icallVMprJavaSendVirtualD";
            case TR_ARM64interpreterVoidStaticGlue:
                return "_interpreterVoidStaticGlue";
            case TR_ARM64interpreterSyncVoidStaticGlue:
                return "_interpreterSyncVoidStaticGlue";
            case TR_ARM64interpreterIntStaticGlue:
                return "_interpreterIntStaticGlue";
            case TR_ARM64interpreterSyncIntStaticGlue:
                return "_interpreterSyncIntStaticGlue";
            case TR_ARM64interpreterLongStaticGlue:
                return "_interpreterLongStaticGlue";
            case TR_ARM64interpreterSyncLongStaticGlue:
                return "_interpreterSyncLongStaticGlue";
            case TR_ARM64interpreterFloatStaticGlue:
                return "_interpreterFloatStaticGlue";
            case TR_ARM64interpreterSyncFloatStaticGlue:
                return "_interpreterSyncFloatStaticGlue";
            case TR_ARM64interpreterDoubleStaticGlue:
                return "_interpreterDoubleStaticGlue";
            case TR_ARM64interpreterSyncDoubleStaticGlue:
                return "_interpreterSyncDoubleStaticGlue";
            case TR_ARM64nativeStaticHelper:
                return "_nativeStaticHelper";
            case TR_ARM64interfaceDispatch:
                return "_interfaceDispatch";
            case TR_ARM64samplingRecompileMethod:
                return "_samplingRecompileMethod";
            case TR_ARM64countingRecompileMethod:
                return "_countingRecompileMethod";
            case TR_ARM64samplingPatchCallSite:
                return "_samplingPatchCallSite";
            case TR_ARM64countingPatchCallSite:
                return "_countingPatchCallSite";
            case TR_ARM64induceRecompilation:
                return "_induceRecompilation";
            case TR_ARM64revertToInterpreterGlue:
                return "_revertToInterpreterGlue";
            case TR_ARM64doubleRemainder:
                return "doubleRemainder";
            case TR_ARM64floatRemainder:
                return "floatRemainder";
            case TR_ARM64jitCollapseJNIReferenceFrame:
                return "jitCollapseJNIReferenceFrame";
            case TR_ARM64arrayCopy:
                return "__arrayCopy";
            case TR_ARM64forwardArrayCopy:
                return "__forwardArrayCopy";
            case TR_ARM64backwardArrayCopy:
                return "__backwardArrayCopy";
            case TR_ARM64interfaceCompleteSlot2:
                return "_interfaceCompleteSlot2";
            case TR_ARM64interfaceSlotsUnavailable:
                return "_interfaceSlotsUnavailable";
            case TR_ARM64PatchGCRHelper:
                return "_patchGCRHelper";
            case TR_ARM64arrayTranslateTRTO:
                return "__arrayTranslateTRTO";
            case TR_ARM64arrayTranslateTRTO255:
                return "__arrayTranslateTRTO255";
            case TR_ARM64arrayTranslateTROTNoBreak:
                return "__arrayTranslateTROTNoBreak";
        }
    }
#endif
#endif // J9_PROJECT_SPECIFIC

    return "unknown helper";
}

const char *TR_Debug::getLinkageConventionName(uint8_t lc)
{
    switch ((TR_LinkageConventions)lc) {
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

const char *TR_Debug::getVirtualGuardKindName(TR_VirtualGuardKind kind)
{
    switch (kind) {
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

const char *TR_Debug::getVirtualGuardTestTypeName(TR_VirtualGuardTestType testType)
{
    switch (testType) {
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

const char *TR_Debug::getWriteBarrierKindName(int32_t kind)
{
    switch (kind) {
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

const char *TR_Debug::getSpillKindName(uint8_t kind)
{
    switch ((TR_SpillKinds)kind) {
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

void TR_Debug::dumpSimulatedNode(OMR::Logger *log, TR::Node *node, char tagChar)
{
    log->printf("\n               [%s]", getName(node));
    if (_comp->cg()->simulatedNodeState(node)._willBeRematerialized) {
        log->printf(" R/%-2d", node->getReferenceCount());
    } else if (node->getReferenceCount() >= 1) {
        log->printf("%2d/%-2d", node->getLocalIndex(), node->getReferenceCount());
    } else {
        log->prints("     ");
    }
    log->printf(" %c ", tagChar);

    // 16 chars is enough room for ArrayCopyBNDCHK which is reasonably common
    int32_t padding = 16 - static_cast<int32_t>(strlen(getName(node->getOpCode())));
    if (node->getOpCode().hasSymbolReference()) {
        // Yes, big methods can have 4-digit symref numbers
        log->printf("%s #%-4d", getName(node->getOpCode()), node->getSymbolReference()->getReferenceNumber());
        padding -= 6;
    } else if (node->getOpCode().isBranch()) {
        int32_t destBlockNum = node->getBranchDestination()->getNode()->getBlock()->getNumber();
        log->printf("%s %-4d", getName(node->getOpCode()), destBlockNum);
        padding -= 5;
    } else if (node->getOpCodeValue() == TR::BBStart || node->getOpCodeValue() == TR::BBEnd) {
        log->printf("%s %-4d", getName(node->getOpCode()), node->getBlock()->getNumber());
        padding -= 5;
    } else if (node->getOpCode().isLoadConst()) {
        const int32_t limit = 99999999;
        const char * const opCodeOnlyFormatString = "%s (big)   ";
        const char * const signedFormatString = "%s %-8d";
        const char * const unsignedFormatString = "%s %-8u";
        if (node->getType().isIntegral()) {
            padding -= 9;
            int64_t value;
            switch (node->getDataType()) {
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
                    value = limit + 1;
                    break;
            }
            if (value >= -limit && value <= limit) {
                log->printf(signedFormatString, getName(node->getOpCode()), value);
            } else {
                log->printf(opCodeOnlyFormatString, getName(node->getOpCode()));
            }
        } else if (node->getDataType() == TR::Float) {
            log->printf("%s %-8g", getName(node->getOpCode()), node->getFloat());
            padding -= 9;
        } else if (node->getDataType() == TR::Double) {
            log->printf("%s %-8g", getName(node->getOpCode()), node->getDouble());
            padding -= 9;
        } else if (node->getDataType() == TR::Address && node->getAddress() == 0) {
            log->printf("%s NULL", getName(node->getOpCode()));
            padding -= 5;
        } else {
            log->prints(getName(node->getOpCode()));
        }
    } else {
        log->prints(getName(node->getOpCode()));
    }

    log->printf(" %*s", padding, "");
}

void TR_Debug::dumpGlobalRegisterTable(OMR::Logger *log)
{
    log->prints("Global regs:\n");
    for (int32_t i = 0; i < _comp->cg()->getNumberOfGlobalRegisters(); i++) {
        log->printf("   %d: %s\n", i, getGlobalRegisterName((TR_GlobalRegisterNumber)i));
    }
}

void TR_Debug::startTracingRegisterAssignment(OMR::Logger *log, TR_RegisterKinds kindsToAssign)
{
    if (!_comp->getOption(TR_TraceRA))
        return;

    log->printf("\n\n<regassign method=\"%s\">\n", jitdCurrentMethodSignature(_comp));
    log->prints("<legend>\n"
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

    log->flush();
    _registerAssignmentTraceFlags |= TRACERA_IN_PROGRESS;
    _registerAssignmentTraceCursor = 0;
    _registerKindsToAssign = kindsToAssign;
}

void TR_Debug::pauseTracingRegisterAssignment()
{
    _pausedRegisterAssignmentTraceFlags = _registerAssignmentTraceFlags;
    _registerAssignmentTraceFlags = 0;
}

void TR_Debug::resumeTracingRegisterAssignment()
{
    _registerAssignmentTraceFlags = _pausedRegisterAssignmentTraceFlags;
}

void TR_Debug::stopTracingRegisterAssignment(OMR::Logger *log)
{
    if (!_comp->getOption(TR_TraceRA))
        return;

    if (_registerAssignmentTraceCursor)
        log->println();

    log->prints("</regassign>\n");
    log->flush();
    _registerAssignmentTraceFlags &= ~TRACERA_IN_PROGRESS;
}

void TR_Debug::traceRegisterAssignment(OMR::Logger *log, const char *format, va_list args)
{
    if (!_comp->getOption(TR_TraceRA))
        return;

    if (_registerAssignmentTraceCursor) {
        log->println();
        _registerAssignmentTraceCursor = 0;
    }

    // indent the message.
    log->prints("details:                      ");

    int32_t j = 0;
    int32_t length = static_cast<int32_t>(strlen(format)) + 40;
    char *buffer = (char *)_comp->trMemory()->allocateHeapMemory(length + 1);
    bool sawPercentR = false;

    for (const char *c = format; *c; c++, j++) {
        if (*c == '%' && *(c + 1) == 'R') {
            ++c;
            const char *regName = getName(va_arg(args, TR::Register *));
            int32_t slen = static_cast<int32_t>(strlen(regName));

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
        } else {
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
    log->vprintf(sawPercentR ? buffer : format, args);

    va_end(args);
    log->println();
    log->flush();
}

void TR_Debug::traceRegisterAssignment(OMR::Logger *log, TR::Instruction *instr, bool insertedByRA, bool postRA)
{
    TR_ASSERT(instr, "cannot trace assignment of NULL instructions\n");

    if (!_comp->getOption(TR_TraceRA))
        return;

    if (insertedByRA)
        _registerAssignmentTraceFlags |= TRACERA_INSTRUCTION_INSERTED;
    else if (postRA)
        _registerAssignmentTraceFlags &= ~TRACERA_INSTRUCTION_INSERTED;

    print(log, instr);
    if (_registerAssignmentTraceCursor) {
        log->println();
        _registerAssignmentTraceCursor = 0;
        if (postRA) {
            if (_comp->getOption(TR_TraceRA)) {
                log->prints("<regstates>\n");

                if (_registerKindsToAssign & TR_GPR_Mask) {
                    log->prints("<gprs>\n");
                    TR::RegisterIterator *iter = _comp->cg()->getGPRegisterIterator();
                    for (TR::Register *gpr = iter->getFirst(); gpr; gpr = iter->getNext()) {
                        printFullRegInfo(log, gpr);
                    }
                    log->prints("</gprs>\n");
                }
#if defined(TR_TARGET_S390)
                if (_registerKindsToAssign & TR_VRF_Mask && _cg->getSupportsVectorRegisters()) {
                    log->prints("<vrfs>\n");
                    TR::RegisterIterator *iter = _cg->getVRFRegisterIterator();
                    for (TR::Register *vrf = iter->getFirst(); vrf; vrf = iter->getNext()) {
                        printFullRegInfo(log, vrf);
                    }
                    log->prints("</vrfs>\n");
                }
#endif
                if (_registerKindsToAssign & TR_FPR_Mask) {
                    log->prints("<fprs>\n");
                    TR::RegisterIterator *iter = _comp->cg()->getFPRegisterIterator();
                    for (TR::Register *fpr = iter->getFirst(); fpr; fpr = iter->getNext()) {
                        printFullRegInfo(log, fpr);
                    }
                    log->prints("</fprs>\n");
                }

                if (_registerKindsToAssign & TR_VMR_Mask) {
                    log->prints("<vmrs>\n");
                    TR::RegisterIterator *iter = _comp->cg()->getVMRegisterIterator();
                    for (TR::Register *vmr = iter->getFirst(); vmr; vmr = iter->getNext()) {
                        printFullRegInfo(log, vmr);
                    }
                    log->prints("</vmrs>\n");
                }

                log->prints("</regstates>\n");
            }
            log->println();
        }
    }
}

void TR_Debug::traceRegisterAssigned(OMR::Logger *log, TR_RegisterAssignmentFlags flags, TR::Register *virtReg,
    TR::Register *realReg)
{
    TR_ASSERT(realReg, "Cannot trace assignment of NULL Real Register\n");
    TR_ASSERT(virtReg, "Cannot trace assignment of NULL Virtual Register\n");

    if (!_comp->getOption(TR_TraceRA)) {
        return;
    }

    // Avoid superfluous traces, unless the user asks for them.
    if (virtReg->isPlaceholderReg() && !_comp->getOption(TR_TraceRA))
        return;

    const size_t bufSize = 30;
    char buf[bufSize];
    const char *preCoercionSymbol = flags.testAny(TR_PreDependencyCoercion) ? "!" : "";
    const char *postCoercionSymbol = flags.testAny(TR_PostDependencyCoercion) ? "!" : "";
    const char *regSpillSymbol = flags.testAny(TR_RegisterSpilled) ? "$" : "";
    const char *regReloadSymbol = flags.testAny(TR_RegisterReloaded) ? "$" : "";
    const char *openParen = flags.testAny(TR_ColouringCoercion) ? "{" : (flags.testAny(TR_IndirectCoercion) ? "(" : "");
    const char *closeParen
        = flags.testAny(TR_ColouringCoercion) ? "}" : (flags.testAny(TR_IndirectCoercion) ? ")" : "");
    const char *equalSign = flags.testAny(TR_ByColouring) ? "#" : (flags.testAny(TR_ByAssociation) ? ":" : "=");
    snprintf(buf, bufSize, "%s%s%s%s(%d/%d)%s%s%s%s%s ", preCoercionSymbol, openParen, regReloadSymbol,
        getName(virtReg), virtReg->getFutureUseCount(), virtReg->getTotalUseCount(), equalSign, regSpillSymbol,
        getName(realReg), closeParen, postCoercionSymbol);

    if ((_registerAssignmentTraceCursor += static_cast<int16_t>(strlen(buf))) > 80) {
        _registerAssignmentTraceCursor = static_cast<int16_t>(strlen(buf));
        log->printf("\n%s", buf);
    } else {
        log->prints(buf);
    }
    log->flush();
}

void TR_Debug::traceRegisterFreed(OMR::Logger *log, TR::Register *virtReg, TR::Register *realReg)
{
    TR_ASSERT(realReg, "Cannot trace assignment of NULL Real Register\n");
    TR_ASSERT(virtReg, "Cannot trace assignment of NULL Virtual Register\n");

    if (!_comp->getOption(TR_TraceRA))
        return;

    // Avoid superfluous traces, unless the user asks for them.
    if (virtReg->isPlaceholderReg() && !_comp->getOption(TR_TraceRA))
        return;

    const size_t bufSize = 30;
    char buf[bufSize];
    snprintf(buf, bufSize, "%s(%d/%d)~%s ", getName(virtReg), virtReg->getFutureUseCount(), virtReg->getTotalUseCount(),
        getName(realReg));

    if ((_registerAssignmentTraceCursor += static_cast<int16_t>(strlen(buf))) > 80) {
        _registerAssignmentTraceCursor = static_cast<int16_t>(strlen(buf));
        log->printf("\n%s", buf);
    } else {
        log->prints(buf);
    }
    log->flush();
}

void TR_Debug::traceRegisterInterference(OMR::Logger *log, TR::Register *virtReg, TR::Register *interferingVirtual,
    int32_t distance)
{
    TR_ASSERT(virtReg && interferingVirtual, "cannot trace assignment of NULL registers\n");

    if (!_comp->getOption(TR_TraceRA))
        return;

    const size_t bufSize = 40;
    char buf[bufSize];
    snprintf(buf, bufSize, "%s{%d,%d}? ", getName(interferingVirtual), interferingVirtual->getAssociation(), distance);

    if ((_registerAssignmentTraceCursor += static_cast<int16_t>(strlen(buf))) > 80) {
        _registerAssignmentTraceCursor = static_cast<int16_t>(strlen(buf));
        log->printf("\n%s", buf);
    } else {
        log->prints(buf);
    }
    log->flush();
}

void TR_Debug::traceRegisterWeight(OMR::Logger *log, TR::Register *realReg, uint32_t weight)
{
    TR_ASSERT(realReg, "cannot trace weight of NULL register\n");

    if (!_comp->getOption(TR_TraceRA))
        return;

    const size_t bufSize = 30;
    char buf[bufSize];
    snprintf(buf, bufSize, "%s[0x%x]? ", getName(realReg), weight);

    if ((_registerAssignmentTraceCursor += static_cast<int16_t>(strlen(buf))) > 80) {
        _registerAssignmentTraceCursor = static_cast<int16_t>(strlen(buf));
        log->printf("\n%s", buf);
    } else {
        log->prints(buf);
    }
    log->flush();
}

#if defined(AIXPPC)
#if !defined(J9OS_I5)
#include <unistd.h>
extern "C" void yield(void);

void TR_Debug::setupDebugger(void *addr)
{
    static bool started = false;

    // Only need to start the debugger once.
    if (!started) {
        pid_t ppid = getpid();
        pid_t p;

        if ((p = fork()) == 0) {
            const size_t cfnameSize = 20;
            char cfname[cfnameSize];
            FILE *cf;
            const size_t ppSize = 20;
            char pp[ppSize];
            char *Argv[20];

            yield();
            snprintf(cfname, cfnameSize, "_%" OMR_PRId64 "_", (int64_t)getpid());
            snprintf(pp, ppSize, "%" OMR_PRId64, (int64_t)ppid);
            Argv[1] = (char *)"-a";
            Argv[2] = pp;
            Argv[3] = NULL;

            if ((Argv[0] = ::feGetEnv("TR_DEBUGGER")) == NULL) {
                Argv[0] = (char *)"/usr/bin/dbx";
                if ((cf = fopen(cfname, "wb+")) == 0)
                    cfname[0] = '\0';
                else {
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
        } else
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
    if (!started) {
        Xj9JitBreakPoint();
        started = true;
    }
}

#endif

#elif defined(LINUX)
void TR_Debug::setupDebugger(void *startaddr, void *endaddr, bool before)
{
    static bool started = false;
    const size_t cfnameSize = 256;
    char cfname[cfnameSize];
    FILE *cf;
    const size_t ppSize = 20;
    char pp[ppSize];
    char *Argv[4];

    /* Only need to start the debugger once. */
    if (!started) {
        pid_t p;
        pid_t ppid = getpid();
        if ((p = fork()) == 0) {
            snprintf(cfname, cfnameSize, "/tmp/__TRJIT_%d_", getpid());
            snprintf(pp, ppSize, "%d", ppid);

            if ((Argv[0] = ::feGetEnv("TR_DEBUGGER")) == NULL)
                Argv[0] = "/usr/bin/gdb";

            if ((cf = fopen(cfname, "wb+")) == 0) {
                cfname[0] = '\0';
                printf("ERROR: Couldn't open file %s", cfname);
            } else {
                fprintf(cf, "file /proc/%s/exe\n", pp);
                fprintf(cf, "attach %s\n", pp);
                fprintf(cf, "i sh\n");

                if (before)
                    fprintf(cf, "break *%p\n", startaddr);
                else {
                    printf("\n methodStartAddress = %p", startaddr);
                    printf("\n methodEndAddress = %p\n", endaddr);
                    fprintf(cf, "break *%p\n", startaddr);
                    fprintf(cf, "disassemble %p %p\n", startaddr, endaddr);
                }

                fprintf(cf, "finish\n");
                fprintf(cf, "shell rm %s\n", cfname);
                fclose(cf);

                Argv[1] = "-x";
                Argv[2] = cfname;
                Argv[3] = NULL;
            }
            execvp(Argv[0], Argv);
        } else {
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
    if (!started) {
        char cfname[BUFFER_SIZE];
        FILE *cf;
        char pp[BUFFER_SIZE];
        char *Argv[MAX_NO_ARGUMENTS];
        pid_t ppid = getpid();
        volatile int size = snprintf(cfname, BUFFER_SIZE, "/tmp/_%d_", ppid);
        volatile int x;
        TR_ASSERT(size < BUFFER_SIZE, "assertion failure");
        size = snprintf(pp, BUFFER_SIZE, "%d", ppid);
        TR_ASSERT(size < BUFFER_SIZE, "assertion failure");
        // open new file for writing in the case of break_before
        // in the case of the break_after commands file was already created in final.c
        if ((Argv[0] = ::feGetEnv("TR_DEBUGGER")) == NULL)
            Argv[0] = "/tr/zos-tools/bin/dbxattach";
        // for break_before, although command file is always the same,
        // we generate it dynamically for consistency with other platforms
        cf = fopen(cfname, "wb+");
        if (NULL != cf) {
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
            if (before) {
                fprintf(cf, "stopi at 0x%p\n", startaddr);
                fprintf(cf, "stop in stopBeforeCompile\n");
            } else {
                printf("\n methodStartAddress = %p", startaddr);
                printf("\n methodEndAddress = %p\n", endaddr);
                fprintf(cf, "stopi at 0x%p\n", startaddr);
            }

            fprintf(cf, "status\n");
            // instrumentation to break out of the infinite loop
            // defined in setup_debugger(..) in debug JIT
            fprintf(cf, "assign 0x%p=1\n", &x);
            fclose(cf);
        }
        // check for dbx that cfname file could be read (dbx does not do this check)
        cf = fopen(cfname, "r");
        if (NULL != cf) {
            struct inheritance inh = { 0 }; /* use all the default inheritance stuff */
            int fdCount = 0; /* inherit all file descriptors from parent */
            int *fd = 0;
            char ***__Envn();
#define environ (*(__Envn()))
            const char **env = (const char **)environ; // pass TR environemtn to debugger
            fclose(cf);
            Argv[1] = "-c";
            Argv[2] = cfname;
            Argv[3] = "-a";
            Argv[4] = pp;
            Argv[5] = NULL;
            spawnp(Argv[0], fdCount, NULL, &inh, (const char **)Argv, env);
            printf("Looping on pid:%d while waiting for dbx\n", ppid);
            printf("Be patient, this may take a minute or two...\n");
            printf("If spawn has failed to execute the above command,\n");
            printf(" Execute the following command manually on command line \n");
            printf("%s %s %s %s %s\n", Argv[0], Argv[1], Argv[2], Argv[3], Argv[4]);
            for (x = 0; x == 0;)
                ;
            if (remove(cfname))
                printf("Could not delete %s .\n", cfname);
            started = true;
        } else
            printf("Could not open %s, skipping break !\n", cfname);
    }
}
#elif defined(OMR_OS_WINDOWS)
#ifndef WINDOWS_API_INCLUDED
extern "C" {
long _stdcall GetCurrentProcessId(); // msf - including Windows.h causes problems with UDATA redefinition
long _stdcall CreateProcessA(const char *appName, const char *cmdLine, void *processAttributes, void *threadAttributes,
    int inheritsHandles, long creationFlags, void *envP, const char *currentDirectory, void *startupInfo,
    void *processInfo);
void _stdcall Sleep(long time);
}
#endif
void TR_Debug::setupDebugger(void *startaddr, void *endaddr, bool before)
{
#define BUFFER_SIZE 1024
    static bool started = false;
    /* Only need to start the debugger once. */
    if (!started) {
        char cmdBuffer[BUFFER_SIZE];
        char appBuffer[BUFFER_SIZE];
        uint32_t ppid = GetCurrentProcessId();
        char *srcDir = ::feGetEnv("DBGPATH");
        if (!srcDir) {
            srcDir = ::feGetEnv("%TRHOME%\\jre\\bin\\codegen.dev");
            if (!srcDir) {
                srcDir = ".";
            }
        }
        int *startupInfo = (int *)calloc(20, 4); // msf - yuck. should get system headers and include here
        *startupInfo = 17 * 4; // ditto...
        void *processInfo = calloc(4, 4); // ditto...
        volatile int x;
        int size;

        // instrumentation to break out of the infinite loop
        size = snprintf(cmdBuffer, BUFFER_SIZE, "?? *((int*)0x%p)=1;", &x);
        if (before) {
            size += snprintf(cmdBuffer + size, BUFFER_SIZE - size, "bp 0x%p;", startaddr);
            size += snprintf(cmdBuffer + size, BUFFER_SIZE - size, "bm stopBeforeCompile;g;");
        } else {
            size += snprintf(cmdBuffer + size, BUFFER_SIZE - size, "bp 0x%p;g;", startaddr);
        }

        // check for windbg that cfname file could be read
        size = snprintf(appBuffer, BUFFER_SIZE,
            "\"C:\\Program Files\\Debugging Tools For Windows\\windbg\" -p %d -srcpath %s -c \"%s\"", ppid, srcDir,
            cmdBuffer);
        TR_ASSERT(size < BUFFER_SIZE, "assertion failure");

#ifdef WINDOWS_API_INCLUDED
        CreateProcessA(NULL, appBuffer, NULL, NULL, 0, 0, NULL, NULL, (LPSTARTUPINFOA)startupInfo,
            (LPPROCESS_INFORMATION)processInfo);
#else
        CreateProcessA(NULL, appBuffer, NULL, NULL, 0, 0, NULL, NULL, startupInfo, processInfo);
#endif

        for (x = 0; x == 0;) {
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

    if (c->getCount() == 0) {
        fprintf(counterFile, "     ---   |");
        return;
    }

    double quotient = ((double)rawCount) / ((double)c->getCount());
    if (-1.1 < quotient && quotient < 1.1) // print as percentage
    {
        fprintf(counterFile, " %8.2f%% |", 100.0 * quotient);
    } else // print as fraction
    {
        fprintf(counterFile, " %8.2f  |", quotient);
    }
}

static int counterCompare(const char *left, const char *right)
{
    // Like strcmp but it looks for NUMERIC_SEPARATOR and does numeric ordering where indicated
    // Note: '.' doesn't work like an actual decimal point, ".12" > ".2" because 12 > 2

    // Works by looking at the names as numeric and non-numeric sections, and comparing one section at a time

    static char numericStart[] = { TR::DebugCounterGroup::NUMERIC_SEPARATOR, 0 };
    static char numericEnd[]
        = { TR::DebugCounterGroup::FRACTION_SEPARATOR, TR::DebugCounterGroup::RATIO_SEPARATOR, '.', 0 };

    bool numericComparisonMode = false;

    while (*left != 0 && *right != 0) {
        // The terminator indicates a change in numeric/string comparison mode
        char *terminator = numericComparisonMode ? numericEnd : numericStart;
        auto leftSectionLength = strcspn(left, terminator);
        auto rightSectionLength = strcspn(right, terminator);
        if (leftSectionLength != rightSectionLength) {
            return numericComparisonMode
                ? static_cast<int32_t>(leftSectionLength
                      - rightSectionLength) // Assume that numbers are not 0-padded, so any longer number must be bigger
                : strcmp(left, right);
        }
        // strncmp also works for comparing numbers with an equal number of digits
        int result = strncmp(left, right, leftSectionLength);
        if (result != 0) {
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

static int compareDebugCounter(const void *a, const void *b)
{
    return counterCompare((*((TR::DebugCounter **)a))->getName(), (*((TR::DebugCounter **)b))->getName());
}

static void swap(void *elemA, void *elemB, int32_t elemSize)
{
    if ((elemSize % 4) == 0) {
        for (int i = 0; i < elemSize / 4; i++) {
            int32_t t = ((int32_t *)elemA)[i];
            ((int32_t *)elemA)[i] = ((int32_t *)elemB)[i];
            ((int32_t *)elemB)[i] = t;
        }
    } else {
        for (int i = 0; i < elemSize; i++) {
            char t = ((char *)elemA)[i];
            ((char *)elemA)[i] = ((char *)elemB)[i];
            ((char *)elemB)[i] = t;
        }
    }
}

static int32_t partition(void *array, int32_t first, int32_t last, int32_t elemSize, int (*cmpFunc)(void *, void *))
{
    int32_t i = first;
    int32_t j = last;
    while (1) {
        while (cmpFunc((char *)array + i * elemSize, (char *)array + first * elemSize) < 0)
            i++;
        while (cmpFunc((char *)array + j * elemSize, (char *)array + first * elemSize) > 0)
            j--;
        if (i >= j)
            return j;
        swap((char *)array + i * elemSize, (char *)array + j * elemSize, elemSize);
        i++;
        j--;
    }
}

static void quicksort(void *array, int32_t first, int32_t last, int32_t elemSize, int (*cmpFunc)(void *, void *))
{
    int32_t mid;
    if (first < last) {
        mid = partition(array, first, last, elemSize, cmpFunc);
        quicksort(array, first, mid, elemSize, cmpFunc);
        quicksort(array, mid + 1, last, elemSize, cmpFunc);
    }
}

static void qsort(void *array, int32_t nitems, int32_t elemSize, int (*cmpFunc)(void *, void *))
{
    quicksort(array, 0, nitems - 1, elemSize, cmpFunc);
}

void TR_Debug::printDebugCounters(TR::DebugCounterGroup *counterGroup, const char *name)
{
    ListBase<TR::DebugCounter> &counters = counterGroup->_counters;
    if (counters.isEmpty())
        return;

    TR::DebugCounter **counterArray = (TR::DebugCounter **)(TR::Compiler->regionAllocator.allocate(
        counters.getSize() * sizeof(TR::DebugCounter *)));
    ListIterator<TR::DebugCounter> li(&counters);
    TR::DebugCounter *c;
    int32_t count = 0, i;
    int32_t longestName = 0;

    static FILE *counterFile = openDebugCounterFile(::feGetEnv("TR_DebugCounterFileName"));
    fprintf(counterFile, "\n== %s ==\n", name);

    // We want them printed in order of name.  Given the naming convention with
    // prefixes indicating denominators, sorting makes for a much much more
    // readable report.
    //
    for (c = li.getCurrent(); c; c = li.getNext()) {
        counterArray[count++] = c;
        if (c->getCount() != 0) {
            int32_t nameLength = static_cast<int32_t>(strlen(c->getName()));
            longestName = std::max(longestName, nameLength);
        }
    }
    qsort(counterArray, count, sizeof(TR::DebugCounter *), compareDebugCounter);

    // Avoid displaying redundant counters.  If a nonzero counter C has a denominator D,
    // no other nonzero counter has denominator D, and C's count equals D's count, then D
    // is redundant.  This is actually quite a common occurrence.
    //
    // It would be nice to do this in O(n) time... this loop is O(n^2).
    //
    TR::DebugCounter **breadthFirst = (TR::DebugCounter **)(TR::Compiler->regionAllocator.allocate(
        counters.getSize() * sizeof(TR::DebugCounter *)));
    for (i = 0; i < count; i++) {
        TR::DebugCounter *d = counterArray[i];
        if (d->getCount() == 0)
            continue;
        int32_t numNumerators = 0;
        bool keep = false;
        for (int32_t j = i; j < count && !keep; j++) {
            c = counterArray[j];
            if (c->getDenominator() == d && c->getCount() != 0) {
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

    for (i = 0; i < count; i++) {
        c = counterArray[i];
        if (c && c->getCount() != 0) {
            fprintf(counterFile, "%3d: %-*s | %12.0f | ", i, longestName, c->getName(), (double)(c->getCount()));
            printDenominators(c->getDenominator(), c->getCount(), counterFile);
            fprintf(counterFile, "  __ %3d __\n", i);
        }
    }
}
