/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef FileReader_HPP_
#define FileReader_HPP_

#include <stdio.h>

#include "Port.hpp"

class FileReader
{
	/*
	 * Data members
	 */
private:
	FILE *_fd; /* current TDF file file descriptor */
	char *_buffer; /* Current line */
	int _strLen; /* Length of current line */
protected:
public:
	const char *_fileName;
	int _lineNumber;

	/*
	 * Function members
	 */
private:
	/**
	 * Read next line into buffer
	 * @param fd Input stream
	 * @param buf Read buffer
	 * @param buffSize Buffer size
	 * @return Line size, RC_FAILED on failure or EOF
	 */
	RCType readline(FILE *fd, char *buf, unsigned int buffSize, int *byteRead);
protected:
public:
	FileReader()
		: _fd(NULL)
		, _buffer(NULL)
		, _strLen(-1) {
	}

	/**
	 * Initialize file reader
	 * @param fileName Name of the file to open for reading
	 * @return RC_OK on success
	 */
	RCType init(const char *fileName);

	/**
	 * Can we read next line from the file?
	 * Will close the file input stream on EOF.
	 * @return True if can read next line, false otherwise
	 */
	bool hasNext();

	/**
	 * Read next line from the file.
	 * @return Line from the file.
	 */
	const char *next();
};

#endif /* FileReader_HPP_ */
