/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "FileReader.hpp"
#include "FileUtils.hpp"

#define MAXSTRINGLENGTH 1024

RCType
FileReader::init(const char *fileName)
{
	RCType rc = RC_FAILED;
	_fileName = fileName;
	_lineNumber = 0;
#if defined(J9ZOS390) && (__CHARSET_LIB == 1)
	_fd = Port::fopen(fileName, "rt");
#else
	_fd = Port::fopen(fileName, "rb");
#endif

	if (NULL == _fd) {
		eprintf("Unable to open file: %s", fileName);
		goto failed;
	}

	_buffer = (char *)Port::omrmem_calloc(MAXSTRINGLENGTH, sizeof(char));
	if (NULL == _buffer) {
		eprintf("Failed to allocate memory");
		goto failed;
	}

	rc = readline(_fd, _buffer, MAXSTRINGLENGTH, &_strLen);
	if (RC_OK != rc) {
		_strLen = -1;
	}

	return hasNext() ? RC_OK : RC_FAILED;
failed:
	return rc;
}

bool
FileReader::hasNext()
{
	bool rc = (-1 != _strLen);

	if (!rc) {
		Port::omrmem_free((void **)&_buffer);
		_fileName = NULL;
		_lineNumber = -1;
		if (NULL != _fd) {
			fclose(_fd);
			_fd = NULL;
		}
	}

	return rc;
}

const char *
FileReader::next()
{
	const char *result = strdup(_buffer);
	if (NULL == result) {
		eprintf("Failed to allocate memory");
		goto failed;
	}

	/* Clear the buffer and read the next line */
	memset(_buffer, 0, MAXSTRINGLENGTH);
	if (RC_OK != readline(_fd, _buffer, MAXSTRINGLENGTH, &_strLen)) {
		_strLen = -1;
	}

	return result;

failed:
	return NULL;
}


/*
 * Simple function to read the next line of a file into a
 * _buffer, if there's no data or the line doesn't fit it returns
 * RC_FAILED;
 */
RCType
FileReader::readline(FILE *_fd, char *buf, unsigned int buffSize, int *byteRead)
{
	size_t bytesRead = 0;
	long startPos = 0;

	startPos = ftell(_fd);
	if (-1 == startPos) {
		goto failed;
	}

	bytesRead = fread(buf, 1, buffSize, _fd);

	if (bytesRead < 1) {
		if (ferror(_fd)) {
			perror("fread");
		}
		goto failed; /* EOF */
	}

	for (unsigned int i = 0; i < buffSize; i++) {
		if ('\n' == buf[i]) {
			_lineNumber += 1;
		}
		if ('\n' == buf[i] || '\r' == buf[i]) {
			/* Null terminate, leave the file descriptor ready for the next read. */
			buf[i] = 0;
			fseek(_fd, startPos + i + 1, SEEK_SET);
			*byteRead = i;
			return RC_OK;
		} else if ('\0' == buf[i]) {
			*byteRead = i;
			return RC_OK;
		}
	}

	return RC_FAILED;

failed:
	return RC_FAILED;
}

