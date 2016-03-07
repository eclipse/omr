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

#ifndef STRINGUTILS_HPP_
#define STRINGUTILS_HPP_

#include "Port.hpp"

class StringUtils
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
	 * Find an integer value delimited by white space associated
	 * with the given key.
	 * @param line Format "prefix space number"
	 * @param key String prefix
	 * @return Value after the key. RC_FAILED if not found
	 */
	static RCType getPositiveIntValue(const char *line, const char *key, unsigned int *result);

	/**
	 * Check if text starts with prefix (case insensitive)
	 * @param text
	 * @param prefix
	 * @return true if the text starts with prefix, otherwise false.
	 */
	static bool startsWithUpperLower(const char *text, const char *prefix);

	/**
	 * Check if text contains toFind (case insensitive)
	 * @param text
	 * @param toFind
	 * @return Pointer to location in text that contains toFind, otherwise NULL
	 */
	static const char *containsUpperLower(const char *text, const char *toFind);
};
#endif /* STRINGUTILS_HPP_ */
