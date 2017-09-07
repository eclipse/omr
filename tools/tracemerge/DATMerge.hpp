/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
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
