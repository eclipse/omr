/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 ******************************************************************************/

/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(WORKSTACK_HPP_)
#define WORKSTACK_HPP_

#include "omr.h"
#include "omrport.h"

#include "BaseNonVirtual.hpp"
#include "Packet.hpp"

class MM_EnvironmentBase;
class MM_WorkPackets;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base
 */
class MM_WorkStack : public MM_BaseNonVirtual
{
/* data members */
private:
	MM_WorkPackets *_workPackets;
	MM_Packet *_inputPacket;
	MM_Packet *_outputPacket;
	MM_Packet *_deferredPacket;
	
	uintptr_t 		_pushCount;

/* function members */
private:
	/**
	 * Push an element to work stack in case if it can be done for current packet (full or does not exist)
	 * @param env[in] The thread which owns the work stack
	 * @param element[in] The element to push
	 */
	void pushFailed(MM_EnvironmentBase *env, void *element);

	/**
	 * Push two elements to work stack in case if it can be done for current packet (full or does not exist)
	 * @param env[in] The thread which owns the work stack
	 * @param element1[in] The first element to push
	 * @param element2[in] The second element to push
	 */
	void pushFailed(MM_EnvironmentBase *env, void *element1, void *element2);

	/**
	 * Pop a reference from input packet in case if it can be done for current packet (empty or does not exist)
	 * Wait if nothing available
	 * @param env[in] The thread which owns the work stack
	 * @return reference
	 */
	void *popFailed(MM_EnvironmentBase *env);

	/**
	 * Pop a reference from input packet in case if it can be done for current packet (empty or does not exist)
	 * No wait if nothing available
	 * @param env[in] The thread which owns the work stack
	 * @return reference
	 */
	void *popNoWaitFailed(MM_EnvironmentBase *env);

public:
	void reset(MM_EnvironmentBase *env, MM_WorkPackets *workPackets);
	/**
	 * Ensures that this work stack is backing onto the given workPackets so that it can be used for work
	 * @param env[in] The thread which owns the work stack
	 * @param workPackets[in] The work packets we want the receiver to use during the mark
	 */
	void prepareForWork(MM_EnvironmentBase *env, MM_WorkPackets *workPackets);
	void flush(MM_EnvironmentBase *env);

	/**
	 * Immediately flush the output packet back to the shared pool so that it can be processed 
	 * by another thread.
	 * @param env[in] The thread which owns the work stack
	 */
	void flushOutputPacket(MM_EnvironmentBase *env);

	void pushDefer(MM_EnvironmentBase *env, void *element);
	void *peek(MM_EnvironmentBase *env);
	
	/* Following 2 functions are intended to allow a caller to count how
	 * many slots have been pushed to a thread's workstack between 2 points 
	 * in time.
	 */ 
	MMINLINE void clearPushCount()	{ _pushCount = 0; };
	MMINLINE uintptr_t getPushCount()	{ return _pushCount; };	

	/**
	 * Return back true if input packets list is not empty
	 */
	MMINLINE bool inputPacketAvailable()
	{
		return (NULL != _inputPacket);
	}

	/**
	 * Return back true if output packets list is not empty
	 */
	MMINLINE bool outputPacketAvailable()
	{
		return (NULL != _outputPacket);
	}

	/**
	 * Return back true if deferred packets list is not empty
	 */
	MMINLINE bool deferredPacketAvailable()
	{
		return (NULL != _deferredPacket);
	}

	/**
	 * Push an element onto the work stack (inlined part)
	 * @param env[in] The thread which owns the work stack
	 * @param element[in] The element to push
	 */
	MMINLINE void push(MM_EnvironmentBase *env, void *element)
	{
		if(_outputPacket && (_outputPacket->push(env, element))) {
			_pushCount++;
		} else {
			pushFailed(env, element);
		}
	}

	/**
	 * Push two elements onto the work stack (inlined part)
	 * @param env[in] The thread which owns the work stack
	 * @param element1[in] The first element to push
	 * @param element2[in] The second element to push
	 */
	MMINLINE void push(MM_EnvironmentBase *env, void *element1, void *element2)
	{
		if(_outputPacket && (_outputPacket->push(env, element1, element2))) {
			_pushCount += 2;
		} else {
			pushFailed(env, element1, element2);
		}
	}

	/*
	 * Pop a reference from input packet (inlined part)
	 *
	 * Pop a reference from the stacks input packet. If no references remain
	 * in current packet then we wait for an input packet to become available
	 * When all packets have been processed return NULL.
	 *
	 * @param env[in] The thread which owns the work stack
	 * @return Object reference or NULL all packets procesed.
	 *
	 */
	MMINLINE void *pop(MM_EnvironmentBase *env)
	{
		void *result;

		if((NULL != _inputPacket) && (NULL != (result = _inputPacket->pop(env)))) {
			return result;
		} else {
			return popFailed(env);
		}
	}

	/*
	 * Pop a reference from input packet (inlined part)
	 *
	 * Pop a reference from the stacks input packet. If no references remain
	 * in current packet then get a new input packet. If no more input packets
	 * available then return NULL.
	 *
	 * @param env[in] The thread which owns the work stack
	 * @return Object reference or NULL if all input packets empty.
	 *
	 */
	MMINLINE void *popNoWait(MM_EnvironmentBase *env)
	{
		void *result;

		if((NULL != _inputPacket) && (NULL != (result = _inputPacket->pop(env)))) {
			return result;
		} else {
			return popNoWaitFailed(env);
		}
	}

	/**
	 * Create a WorkStack object.
	 */
	MM_WorkStack() :
		MM_BaseNonVirtual(),
		_workPackets(NULL),
		_inputPacket(NULL),
		_outputPacket(NULL),
		_deferredPacket(NULL)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* WORKSTACK_HPP_ */
