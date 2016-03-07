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

#if !defined(VERBOSEWRITERSTREAMOUTPUT_HPP_)
#define VERBOSEWRITERSTREAMOUTPUT_HPP_
 
#include "omrcfg.h"

#include "VerboseWriter.hpp"

class MM_EnvironmentBase;

class MM_VerboseWriterStreamOutput : public MM_VerboseWriter
{
public:
protected:
private:
	typedef enum {
		STDERR = 1,
		STDOUT
	} StreamID;

	StreamID _currentStream;

public:
	static MM_VerboseWriterStreamOutput *newInstance(MM_EnvironmentBase *env, const char *filename);

	virtual bool reconfigure(MM_EnvironmentBase *env, const char *filename, uintptr_t fileCount, uintptr_t iterations);

	virtual void endOfCycle(MM_EnvironmentBase *env);
	
	void closeStream(MM_EnvironmentBase *env);
	
	virtual void outputString(MM_EnvironmentBase *env, const char* string);

protected:
	MM_VerboseWriterStreamOutput(MM_EnvironmentBase *env);

private:
	bool initialize(MM_EnvironmentBase *env, const char *filename);
	virtual void tearDown(MM_EnvironmentBase *env);

	MM_VerboseWriterStreamOutput::StreamID getStreamID(MM_EnvironmentBase *env, const char *string);

	MMINLINE void setStream(MM_VerboseWriterStreamOutput::StreamID stream) { _currentStream = stream; }
};

#endif /* VERBOSEWRITERSTREAMOUTPUT_HPP_ */
