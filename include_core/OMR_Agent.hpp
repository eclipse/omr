/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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

#if !defined(OMR_AGENT_HPP_INCLUDED)
#define OMR_AGENT_HPP_INCLUDED

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
