/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
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
	_fd = Port::fopen(fileName, "rb");

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

