/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
