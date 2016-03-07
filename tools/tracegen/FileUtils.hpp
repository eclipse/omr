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

#ifndef FILEUTILS_HPP_
#define FILEUTILS_HPP_

#include <stdio.h>

#include "Port.hpp"
#include "TDFTypes.hpp"

typedef RCType (*VisitFileCallBack)(void *targetObject, J9TDFOptions *options, const char *fileName);

#define eprintf(fmtString, ...)														\
do {																		\
	FileUtils::printError("%s:%d " fmtString "\n", __FILE__, __LINE__, ##__VA_ARGS__);	\
} while (false)

class FileUtils
{
	/*
	 * Data members
	 */
private:
protected:
public:

	/*
	 * Function members
	 */
private:
protected:
public:
	/**
	 * Remove path to file and return just the file name
	 */
	static const char *getFileName(const char *path);

	/**
	 * Gets the extension of a filename.
	 * @param fileName path to file
	 * @return the extension of the file or a NULL if none exists
	 */
	static const char *getFileExt(const char *fileName);


	/**
	 * Get tdf file relative or CWD relative target file name
	 */
	static const char *getTargetFileName(J9TDFOptions *options, const char *tdfFileName, const char *filePrefix, const char *fileName, const char *extension);

	/**
	 *  Get file modification time
	 */
	static time_t getMtime(const char *filePath);

	/**
	 * Recursively traverse the filesystem and call merge on every partial DAT file.
	 * @param options command-line options
	 * @param dirName Current directory
	 * @param fileExtFilter
	 * @param targetObject
	 * @param callback
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	static RCType visitDirectory(J9TDFOptions *options, const char *dirName, const char *fileExtFilter, void *targetObject, VisitFileCallBack callback);

	/**
	 * Print message to stderr
	 */
	static void printError(const char *format, ...);
};

#endif /* FILEUTILS_HPP_ */
