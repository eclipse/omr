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

#ifndef DATMERGE_HPP_
#define DATMERGE_HPP_

#include "TDFTypes.hpp"

RCType startTraceMerge(int argc, char *argv[]);

class DATMerge
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
	/**
	 * Merge partial DAT file into the target DAT
	 * @param options command-line options
	 * @param fromFileName Name of the partial DAT file
	 * @return RC_OK on success.
	 */
	RCType merge(J9TDFOptions *options, const char *fromFileName);
protected:
public:
	/**
	 * Start merging process, visit all subdirectories and merge all partial DAT files.
	 * @return RC_OK on success
	 */
	RCType start(J9TDFOptions *options);

	static RCType mergeCallback(void *targetObject, J9TDFOptions *options, const char *fromFileName);
};

#endif /* DATMERGE_HPP_ */
