/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
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

#if !defined(OMR_AGENT_HPP_INCLUDED)
#define OMR_AGENT_HPP_INCLUDED

/*
 * @ddr_namespace: default
 */

#include "omr.h"
#include "omragent.h"


struct OMR_Agent
{
/*
 * Data members
 */
public:
	static OMR_TI const theOmrTI;

	/* The following private data has been made public so that this header file can be processed for DDR */

	typedef omr_error_t (*onloadfunc_t)(OMR_TI const *ti, OMR_VM *vm, char const *options, OMR_AgentCallbacks *agentCallbacks, ...);
	typedef omr_error_t (*onunloadfunc_t)(OMR_TI const *ti, OMR_VM *vm);

	enum State {
		UNINITIALIZED,
		INITIALIZED,
		LIBRARY_OPENED,
		ONLOAD_COMPLETED,
		OPEN_LIBRARY_ERROR,
		LOOKUP_ONLOAD_ERROR,
		LOOKUP_ONUNLOAD_ERROR,
		ONLOAD_ERROR,
		ONUNLOAD_SUCCESS
	};

	uintptr_t _handle;
	OMR_VM *_vm;
	onloadfunc_t _onload;
	onunloadfunc_t _onunload;
	char const *_dllpath;
	char const *_options;
	State _state;
	OMR_AgentCallbacks _agentCallbacks;

protected:
private:


/*
 * Function members
 */
public:
	/**
	 * Create an OMR agent.
	 *
	 * @param[in] vm The OMR VM.
	 * @param[in] arg An argument of the form "<agent-path>[=<options>]".
	 * arg can be freed after this function is called.
	 *
	 * @return An OMR agent. NULL on failure.
	 */
	static OMR_Agent *createAgent(OMR_VM *vm, char const *arg);

	/**
	 * Destroy an OMR agent.
	 *
	 * @param[in] agent The OMR agent.
	 */
	static void	destroyAgent(OMR_Agent *agent);

	/**
	 * Load the agent library, and lookup the OnLoad and OnUnload functions.
	 * @return OMR_ERROR_NONE if the load and lookups succeeded,
	 * OMR_ERROR_ILLEGAL_ARGUMENT otherwise.
	 */
	omr_error_t openLibrary(void);

	/**
	 * Invoke the agent's OnLoad function.
	 * @return the return value from the OnLoad function,
	 * OMR_ERROR_ILLEGAL_ARGUMENT if the agent is in an invalid state for invoking OnLoad.
	 */
	omr_error_t callOnLoad(void);

	/**
	 * Invoke the agent's OnUnload function.
	 * @return the return value from the agent's OnUnload function
	 */
	omr_error_t callOnUnload(void);

	/**
	 * Invoke the agent's OnPreFork function.
	 * @return the return value from the agent's OnPreFork function
	 */
	omr_error_t callOnPreFork(void);

	/**
	 * Invoke the agent's OnPostForkParent function.
	 * @return the return value from the agent's OnPostForkParent function
	 */
	omr_error_t callOnPostForkParent(void);

	/**
	 * Invoke the agent's OnPostForkChild function.
	 * @return the return value from the agent's OnPostForkChild function
	 */
	omr_error_t callOnPostForkChild(void);

protected:

private:
	OMR_Agent(OMR_VM *vm, char const *arg);
	void *operator new(size_t size, void *memoryPtr)
	{
		return memoryPtr;
	}

	static omr_error_t onPreForkDefault(void);
	static omr_error_t onPostForkParentDefault(void);
	static omr_error_t onPostForkChildDefault(void);
};
#endif /* OMR_AGENT_HPP_INCLUDED */
