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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef CFILEWRITER_HPP_
#define CFILEWRITER_HPP_

#include "Port.hpp"
#include "TDFTypes.hpp"

class CFileWriter
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
	 * Output levels, groups and module info
	 *
	 * @param fd Output stream
	 * @param tdf Parsed TDF file
	 * @param ntracepoints
	 * @param earlyAssertDefaults
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType writeActiveArray(FILE *fd, J9TDFFile *tdf, unsigned int ntracepoints, unsigned int earlyAssertDefaults[]);

	/**
	 * Output levels
	 *
	 * @param fd Output stream
	 * @param tdf Parsed TDF file
	 * @param ntracepoints
	 * @param levels
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType writeLevels(FILE *fd, J9TDFFile *tdf, unsigned int ntracepoints, unsigned int levels[]);

	/**
	 * Output groups
	 *
	 * @param fd Output stream
	 * @param tdf Parsed TDF file
	 * @param groups
	 * @param groupsCount
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType writeGroups(FILE *fd, J9TDFFile *tdf, J9TDFGroup *groups, unsigned int groupsCount);

	/**
	 * Output module info
	 *
	 * @param fd Output stream
	 * @param tdf Parsed TDF file
	 * @param ntracepoints number of tracepoints
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType writeModuleInfo(FILE *fd, J9TDFFile *tdf, unsigned int ntracepoints);

	/**
	 * Output active arrays, level, group and module info
	 *
	 * @param fd Output stream
	 * @param tdf Parsed TDF file
	 * @param ntracepoints Number of trace points
	 * @param levels Trace point Levels array
	 * @param earlyAssertDefaults
	 * @param groups Trace Groups
	 * @param groupsCount Trace group count
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType writeComponentDataOnStream(FILE *fd, J9TDFFile *tdf, unsigned int ntracepoints, unsigned int *levels, unsigned int *earlyAssertDefaults, J9TDFGroup *groups, unsigned int groupsCount);

	/**
	 * @param fd Output stream
	 * @param tdf Parsed TDF file
	 * @param directRegistration
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType writeRegistrationFunctionsOnStream(FILE *fd, J9TDFFile *tdf);

	/**
	 * Output registration functions
	 *
	 * @param fd Output stream
	 * @param tdf Parsed TDF file
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType writeComponentDataForNonTraceEnabledBuildsOnStream(FILE *fd, J9TDFFile *tdf);
protected:
public:

	/**
	 * Output trace C file
	 *
	 * @param fd Output stream
	 * @param tdf Parsed TDF file
	 * @param groups Trace Groups
	 * @param groupCount Trace group count
	 * @return RC_OK on success, RC_FAILED on failure
	 */
	RCType writeOutputFiles(J9TDFOptions *options, J9TDFFile *tdf, J9TDFGroup *groups, unsigned int groupCount);
};
#endif /* CFILEWRITER_HPP_ */
