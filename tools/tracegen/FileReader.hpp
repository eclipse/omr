/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2015
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
