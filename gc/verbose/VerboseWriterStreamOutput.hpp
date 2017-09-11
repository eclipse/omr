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
