/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#if !defined(VERBOSEWRITTERHOOK_HPP_)
#define VERBOSEWRITTERHOOK_HPP_
 
#include "omrcfg.h"

#include "VerboseWriter.hpp"

/**
 * Ouptut agent which directs verbosegc output to a tracepoint.
 */
class MM_VerboseWriterHook : public MM_VerboseWriter
{
private:

protected:
	MM_VerboseWriterHook(MM_EnvironmentBase *env);

	virtual bool initialize(MM_EnvironmentBase *env);
	
public:
	static MM_VerboseWriterHook *newInstance(MM_EnvironmentBase *env);

	virtual bool reconfigure(MM_EnvironmentBase *env, const char *filename, uintptr_t fileCount, uintptr_t iterations) { return true; };
	
	virtual void endOfCycle(MM_EnvironmentBase *env);
	
	virtual void closeStream(MM_EnvironmentBase *env);
	
	virtual void outputString(MM_EnvironmentBase *env, const char* string);
};

#endif /* VERBOSEWRITTERHOOK_HPP_ */
