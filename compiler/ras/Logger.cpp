/*******************************************************************************
 * Copyright IBM Corp. and others 2025
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

#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#ifdef LINUX
#include <unistd.h>
#endif
#include "env/FrontEnd.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"
#include "ras/Logger.hpp"

OMR::Logger::Logger()
{
    // The creator will decide when this Logger is ready to accept content
    //
    setEnabled_DEPRECATED(false);

    setLoggerClosed(false);

    static char *envSkipLoggerFlushes = feGetEnv("TR_skipLoggerFlushes");
    setSkipFlush(envSkipLoggerFlushes ? true : false);
}

int32_t OMR::Logger::prints_len(const char *string)
{
    int32_t rc = prints(string);
    return (rc >= 0) ? static_cast<int32_t>(strlen(string)) : rc;
}

int32_t OMR::Logger::printc_len(char c)
{
    int32_t rc = printc(c);
    return (rc >= 0) ? 1 : rc;
}

/*
 * -----------------------------------------------------------------------------
 * NullLogger
 * -----------------------------------------------------------------------------
 */

int32_t OMR::NullLogger::close()
{
    setLoggerClosed(true);
    setEnabled_DEPRECATED(false);
    return this->flush();
}

// Explicit instantiations
//
template OMR::NullLogger *OMR::NullLogger::create(TR_HeapMemory t);

template OMR::NullLogger *OMR::NullLogger::create(PERSISTENT_NEW_DECLARE t);

template OMR::NullLogger *OMR::NullLogger::create(TR::RawAllocator t);

/*
 * -----------------------------------------------------------------------------
 * AssertingLogger
 * -----------------------------------------------------------------------------
 */

int32_t OMR::AssertingLogger::printf(const char *format, ...)
{
    TR_ASSERT_FATAL(false, "Unexpected Logger printf with format string: %s", format);
    return -1;
}

int32_t OMR::AssertingLogger::prints(const char *string)
{
    TR_ASSERT_FATAL(false, "Unexpected Logger prints with string: %s", string);
    return -1;
}

int32_t OMR::AssertingLogger::printc(char c)
{
    TR_ASSERT_FATAL(false, "Unexpected Logger printc with char: %c (%02x)", c, c);
    return -1;
}

int32_t OMR::AssertingLogger::println()
{
    TR_ASSERT_FATAL(false, "Unexpected Logger println");
    return -1;
}

int32_t OMR::AssertingLogger::vprintf(const char *format, va_list args)
{
    TR_ASSERT_FATAL(false, "Unexpected Logger vprintf with format string: %s", format);
    return -1;
}

int64_t OMR::AssertingLogger::tell()
{
    TR_ASSERT_FATAL(false, "Unexpected Logger tell");
    return -1;
}

void OMR::AssertingLogger::rewind() { TR_ASSERT_FATAL(false, "Unexpected Logger rewind"); }

int32_t OMR::AssertingLogger::flush()
{
    TR_ASSERT_FATAL(false, "Unexpected Logger flush");
    return -1;
}

int32_t OMR::AssertingLogger::close()
{
    setLoggerClosed(true);
    setEnabled_DEPRECATED(false);
    return 0;
}

// Explicit instantiations
//
template OMR::AssertingLogger *OMR::AssertingLogger::create(TR_HeapMemory t);

template OMR::AssertingLogger *OMR::AssertingLogger::create(PERSISTENT_NEW_DECLARE t);

/*
 * -----------------------------------------------------------------------------
 * CStdIOStreamLogger
 * -----------------------------------------------------------------------------
 */
OMR::CStdIOStreamLogger::CStdIOStreamLogger(::FILE *stream, bool requiresStreamClose)
    : _stream(stream)
    , _requiresStreamClose(requiresStreamClose)
{}

OMR::CStdIOStreamLogger::~CStdIOStreamLogger()
{
    if (!getLoggerClosed()) {
        this->close();
    }
}

int32_t OMR::CStdIOStreamLogger::printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int32_t length = ::vfprintf(getStream(), format, args);
    va_end(args);
    return length;
}

int32_t OMR::CStdIOStreamLogger::prints(const char *str) { return ::fputs(str, getStream()); }

int32_t OMR::CStdIOStreamLogger::printc(char c) { return ::fputc(c, getStream()); }

int32_t OMR::CStdIOStreamLogger::println() { return ::fputc('\n', getStream()); }

int32_t OMR::CStdIOStreamLogger::vprintf(const char *format, va_list args)
{
    return ::vfprintf(getStream(), format, args);
}

size_t OMR::CStdIOStreamLogger::read(char *buf, size_t bufSizeInBytes)
{
    return ::fread(buf, sizeof(char), bufSizeInBytes, getStream());
}

int64_t OMR::CStdIOStreamLogger::tell() { return ::ftell(getStream()); }

int32_t OMR::CStdIOStreamLogger::flush()
{
    if (!getSkipFlush())
        return ::fflush(getStream());

    return 0;
}

void OMR::CStdIOStreamLogger::rewind() { ::fseek(getStream(), 0, SEEK_SET); }

int32_t OMR::CStdIOStreamLogger::close()
{
    setLoggerClosed(true);

    // Disable the Logger
    //
    setEnabled_DEPRECATED(false);

    int32_t result = this->flush();
    if (result != 0) {
        return result;
    }

    // result must be 0 at this point

    if (getRequiresStreamClose()) {
        // Indicate that the stream was closed (or at least attempted to close)
        // to prevent further attempts
        //
        setRequiresStreamClose(false);

        result = ::fclose(getStream());
    }

    return result;
}

OMR::CStdIOStreamLogger *OMR::CStdIOStreamLogger::_stderr = NULL;
OMR::CStdIOStreamLogger *OMR::CStdIOStreamLogger::_stdout = NULL;

OMR::CStdIOStreamLogger *OMR::CStdIOStreamLogger::Stderr()
{
    if (!_stderr) {
        /**
         * Initializing this static field can be an unlikely race between compilation threads.
         * However, the worst case result is that there will be multiple wrappers around stderr,
         * which is perfectly fine.
         */
        _stderr = OMR::CStdIOStreamLogger::create(trPersistentMemory, stderr);
    }

    return _stderr;
}

OMR::CStdIOStreamLogger *OMR::CStdIOStreamLogger::Stdout()
{
    if (!_stdout) {
        /**
         * Initializing this static field can be an unlikely race between compilation threads.
         * However, the worst case result is that there will be multiple wrappers around stdout,
         * which is perfectly fine.
         */
        _stdout = OMR::CStdIOStreamLogger::create(trPersistentMemory, stdout);
    }

    return _stdout;
}

// Explicit instantiations
//
template OMR::CStdIOStreamLogger *OMR::CStdIOStreamLogger::create(TR_HeapMemory t, ::FILE *stream);

template OMR::CStdIOStreamLogger *OMR::CStdIOStreamLogger::create(PERSISTENT_NEW_DECLARE t, ::FILE *stream);

template OMR::CStdIOStreamLogger *OMR::CStdIOStreamLogger::create(TR::RawAllocator t, ::FILE *stream);

template OMR::CStdIOStreamLogger *OMR::CStdIOStreamLogger::create(TR_HeapMemory t, const char *filename,
    const char *fileMode);

template OMR::CStdIOStreamLogger *OMR::CStdIOStreamLogger::create(PERSISTENT_NEW_DECLARE t, const char *filename,
    const char *fileMode);

/*
 * -----------------------------------------------------------------------------
 * TRIOStreamLogger
 * -----------------------------------------------------------------------------
 */
OMR::TRIOStreamLogger::TRIOStreamLogger(TR::FILE *stream, bool requiresStreamClose)
    : _stream(stream)
    , _requiresStreamClose(requiresStreamClose)
{}

int32_t OMR::TRIOStreamLogger::printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int32_t length = TR::IO::vfprintf(getStream(), format, args);
    va_end(args);
    return length;
}

int32_t OMR::TRIOStreamLogger::prints(const char *str) { return TR::IO::fprintf(getStream(), "%s", str); }

int32_t OMR::TRIOStreamLogger::printc(char c) { return TR::IO::fprintf(getStream(), "%c", c); }

int32_t OMR::TRIOStreamLogger::println() { return TR::IO::fprintf(getStream(), "\n"); }

int32_t OMR::TRIOStreamLogger::vprintf(const char *format, va_list args)
{
    return TR::IO::vfprintf(getStream(), format, args);
}

size_t OMR::TRIOStreamLogger::read(char *buf, size_t bufSizeInBytes)
{
    return TR::IO::fread(getStream(), buf, bufSizeInBytes);
}

int64_t OMR::TRIOStreamLogger::tell() { return TR::IO::ftell(getStream()); }

int32_t OMR::TRIOStreamLogger::flush()
{
    if (!getSkipFlush()) {
        // TR::IO::fflush does not have error checking
        TR::IO::fflush(getStream());
    }

    return 0;
}

void OMR::TRIOStreamLogger::rewind() { TR::IO::fseek(getStream(), 0, SEEK_SET); }

int32_t OMR::TRIOStreamLogger::close()
{
    setLoggerClosed(true);

    // Do not close the stream as the Logger is simply a wrapper around it.
    // Just disable the Logger and flush it.
    //
    setEnabled_DEPRECATED(false);

    int32_t result = this->flush();
    if (result != 0) {
        return result;
    }

    // result must be 0 at this point

    if (getRequiresStreamClose()) {
        // Indicate that the stream was closed (or at least attempted to close)
        // to prevent further attempts
        //
        setRequiresStreamClose(false);

        result = TR::IO::fclose(getStream());
    }

    return result;
}

// Explicit instantiations
//
template OMR::TRIOStreamLogger *OMR::TRIOStreamLogger::create(TR_HeapMemory t, TR::FILE *stream);

template OMR::TRIOStreamLogger *OMR::TRIOStreamLogger::create(PERSISTENT_NEW_DECLARE t, TR::FILE *stream);

template OMR::TRIOStreamLogger *OMR::TRIOStreamLogger::create(TR_HeapMemory t, const char *filename,
    const char *fileMode);

template OMR::TRIOStreamLogger *OMR::TRIOStreamLogger::create(PERSISTENT_NEW_DECLARE t, const char *filename,
    const char *fileMode);

/*
 * -----------------------------------------------------------------------------
 * CircularLogger
 * -----------------------------------------------------------------------------
 */
OMR::CircularLogger::CircularLogger(OMR::Logger *innerLogger, int64_t rewindThresholdInChars)
    : _innerLogger(innerLogger)
    , _rewindThresholdInChars(rewindThresholdInChars)
{
    TR_ASSERT_FATAL(innerLogger->supportsRewinding(),
        "Inner logger must support rewinding for use in a circular logger");
    TR_ASSERT_FATAL(rewindThresholdInChars > 0, "Circular log threshold must be a non-zero, positive integer");
}

int32_t OMR::CircularLogger::printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    if (getInnerLogger()->tell() > getRewindThresholdInChars()) {
        getInnerLogger()->rewind();
    }

    int32_t length = getInnerLogger()->vprintf(format, args);
    va_end(args);
    return length;
}

int32_t OMR::CircularLogger::prints(const char *str)
{
    if (getInnerLogger()->tell() > getRewindThresholdInChars()) {
        getInnerLogger()->rewind();
    }

    return getInnerLogger()->prints(str);
}

int32_t OMR::CircularLogger::printc(char c)
{
    if (getInnerLogger()->tell() > getRewindThresholdInChars()) {
        getInnerLogger()->rewind();
    }

    return getInnerLogger()->printc(c);
}

int32_t OMR::CircularLogger::println()
{
    if (getInnerLogger()->tell() > getRewindThresholdInChars()) {
        getInnerLogger()->rewind();
    }

    return getInnerLogger()->println();
}

int32_t OMR::CircularLogger::vprintf(const char *format, va_list args)
{
    if (getInnerLogger()->tell() > getRewindThresholdInChars()) {
        getInnerLogger()->rewind();
    }

    return getInnerLogger()->vprintf(format, args);
}

int64_t OMR::CircularLogger::tell() { return getInnerLogger()->tell(); }

int32_t OMR::CircularLogger::flush() { return getInnerLogger()->flush(); }

void OMR::CircularLogger::rewind() { getInnerLogger()->rewind(); }

int32_t OMR::CircularLogger::close()
{
    setLoggerClosed(true);
    setEnabled_DEPRECATED(false);
    return getInnerLogger()->close();
}

// Explicit instantiations
//
template OMR::CircularLogger *OMR::CircularLogger::create(TR_HeapMemory t, OMR::Logger *innerLogger,
    int64_t rewindThresholdInChars);

template OMR::CircularLogger *OMR::CircularLogger::create(PERSISTENT_NEW_DECLARE t, OMR::Logger *innerLogger,
    int64_t rewindThresholdInChars);

/*
 * -----------------------------------------------------------------------------
 * MemoryBufferLogger
 * -----------------------------------------------------------------------------
 */

OMR::MemoryBufferLogger::MemoryBufferLogger(char *buf, size_t maxBufLen)
    : _buf(buf)
    , _bufCursor(buf)
    , _maxBufLen(maxBufLen)
    , _maxRemainingChars(maxBufLen)
{
    TR_ASSERT_FATAL(maxBufLen > 0, "Expecting a non-zero sized base buffer for MemoryBufferLogger");

    // Initialize the backing array to an empty state
    //
    buf[0] = '\0';
}

int32_t OMR::MemoryBufferLogger::printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int32_t result = OMR::MemoryBufferLogger::vprintf(format, args);
    va_end(args);

    return result;
}

int32_t OMR::MemoryBufferLogger::prints(const char *string) { return OMR::MemoryBufferLogger::printf("%s", string); }

int32_t OMR::MemoryBufferLogger::printc(char c) { return OMR::MemoryBufferLogger::printf("%c", c); }

int32_t OMR::MemoryBufferLogger::println() { return OMR::MemoryBufferLogger::printf("\n"); }

int32_t OMR::MemoryBufferLogger::vprintf(const char *format, va_list args)
{
    int32_t charsWritten = ::vsnprintf(_bufCursor, _maxRemainingChars, format, args);

    if (charsWritten >= 0) {
        if (charsWritten < _maxRemainingChars) {
            _bufCursor += charsWritten;
            _maxRemainingChars -= charsWritten;
            return charsWritten;
        } else {
            // Buffer would overflow, but only max allowable chars were written
            //
            _bufCursor += _maxRemainingChars;
            _maxRemainingChars = 0;
            return -charsWritten;
        }
    } else {
        // Non-overflow error
        //
        return INT_MIN;
    }
}

int64_t OMR::MemoryBufferLogger::tell() { return static_cast<int64_t>(_bufCursor - _buf); }

void OMR::MemoryBufferLogger::rewind()
{
    _bufCursor = _buf;
    _maxRemainingChars = _maxBufLen;
}

int32_t OMR::MemoryBufferLogger::flush() { return 0; }

int32_t OMR::MemoryBufferLogger::close()
{
    setLoggerClosed(true);
    setEnabled_DEPRECATED(false);
    return this->flush();
}

// Explicit instantiations
//
template OMR::MemoryBufferLogger *OMR::MemoryBufferLogger::create(TR_HeapMemory t, char *buf, size_t maxBufLen);

template OMR::MemoryBufferLogger *OMR::MemoryBufferLogger::create(PERSISTENT_NEW_DECLARE t, char *buf,
    size_t maxBufLen);

OMR::MemoryBufferLogger *OMR::MemoryBufferLogger::create(TR::Region &region, char *buf, size_t maxBufLen)
{
    return new (region) OMR::MemoryBufferLogger(buf, maxBufLen);
}
