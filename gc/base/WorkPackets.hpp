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
 ******************************************************************************/

/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(WORKPACKETS_HPP_)
#define WORKPACKETS_HPP_

#include "omrcfg.h"
#include "omr.h"
#include "omrthread.h"
#include "omrport.h"
#include "modronopt.h"

#include "BaseVirtual.hpp"
#include "Packet.hpp"
#include "PacketList.hpp"
#include "WorkPacketOverflow.hpp"

class MM_EnvironmentBase;
class MM_GCExtensionsBase;
class MM_Packet;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base
 */
class MM_WorkPackets: public MM_BaseVirtual
{	
	friend class MM_PacketListIterator;

/* Data members / types */
public:
protected:	
	enum {
		_slotsInPacket = 512,
		_packetSize = _slotsInPacket * sizeof(void *),
		_minPacketsInBlock = 4,
		_initialBlocks = 5,
		_increaseFactor = 5,
		_maxPacketsBlocks = _initialBlocks * _increaseFactor,
		_fullPacketThreshold = _slotsInPacket >> 4,
		_satisfactoryCapacity = _slotsInPacket / 2,
		_indexMask = 0xff,
		_maxPacketSearch = 20
	};

	uintptr_t _packetsPerBlock;
	uintptr_t _maxPackets;
	uintptr_t _activePackets;
	uintptr_t _packetsBlocksTop;
	omrthread_monitor_t _allocatingPackets;
	MM_Packet *_packetsStart[_maxPacketsBlocks];
	MM_PacketList _emptyPacketList;  /**< List for empty packets */
	MM_PacketList _fullPacketList;  /**< List for full packets */
	MM_PacketList _relativelyFullPacketList;  /**< List for relatively full packets */
	MM_PacketList _nonEmptyPacketList;  /**< List for non empty packets */
	MM_PacketList _deferredPacketList;  /**< List for deferred packets */
	MM_PacketList _deferredFullPacketList;  /**< List for full deferred packets */
	
	OMRPortLibrary *_portLibrary;

	omrthread_monitor_t _inputListMonitor;
	volatile uintptr_t _inputListWaitCount;
	volatile uintptr_t _inputListDoneIndex;

	MM_WorkPacketOverflow *_overflowHandler;
	MM_GCExtensionsBase *_extensions;

	void emptyToOverflow(MM_EnvironmentBase *env, MM_Packet *packet, MM_OverflowType type);
	virtual MM_Packet *getInputPacketFromOverflow(MM_EnvironmentBase *env);
	bool initWorkPacketsBlock(MM_EnvironmentBase *env);

	MM_Packet *getPacket(MM_EnvironmentBase *env, MM_PacketList *list);
	MM_Packet *getLeastFullPacket(MM_EnvironmentBase *env, int requiredSlots);

	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	
	virtual MM_WorkPacketOverflow *createOverflowHandler(MM_EnvironmentBase *env, MM_WorkPackets *workPackets);

private:
	
/* Methods */
public:
	static MM_WorkPackets  *newInstance(MM_EnvironmentBase *env);
	void kill(MM_EnvironmentBase *env);

	void reuseDeferredPackets(MM_EnvironmentBase *env);

	static uintptr_t getSlotsInPacket() { return _slotsInPacket; }
	MM_Packet *getInputPacketNoWait(MM_EnvironmentBase *env);
	virtual MM_Packet *getInputPacket(MM_EnvironmentBase *env);
	virtual MM_Packet *getOutputPacket(MM_EnvironmentBase *env);
	void putPacket(MM_EnvironmentBase *env, MM_Packet *packet);
	void putOutputPacket(MM_EnvironmentBase *env, MM_Packet *packet);
	
	MM_Packet *getDeferredPacket(MM_EnvironmentBase *env);
	void putDeferredPacket(MM_EnvironmentBase *env, MM_Packet *packet);
	
	/**
	 * Returns TRUE if an input packet is available, FALSE otherwise.
	 */
	bool inputPacketAvailable(MM_EnvironmentBase *env);
	
	/**
	 * Returns TRUE if all packets are empty, FALSE otherwise.
	 * @ingroup GC_Base
	 */
	MMINLINE bool isAllPacketsEmpty()
	{
		return(_emptyPacketList.getCount() == _activePackets);
	};
	
	/**
	 * Returns TRUE if all non-deferred packets are empty, FALSE otherwise.
	 */
	MMINLINE bool tracingExhausted()
	{
		return(_emptyPacketList.getCount() + _deferredPacketList.getCount() + _deferredFullPacketList.getCount() == _activePackets);
	};
	
	MMINLINE uintptr_t getThreadWaitCount() {
		return _inputListWaitCount;
	}

	/**
	 * Returns number of non-empty packets 
	 */
	MMINLINE uintptr_t getNonEmptyPacketCount()
	{
		return(_activePackets - _emptyPacketList.getCount());
	};
	
	/** Returns number of active packets
	 */
	MMINLINE uintptr_t getActivePacketCount()
	{
		return _activePackets;
	};
	
	/**
	 * Returns number of non-empty packets 
	 */
	MMINLINE uintptr_t getEmptyPacketCount()
	{
		return(_emptyPacketList.getCount());
	};

	/**
	 * Returns number of deferred packets 
	 */
	MMINLINE uintptr_t getDeferredPacketCount()
	{
		return(_deferredPacketList.getCount() + _deferredFullPacketList.getCount());
	};

	/**
	 * Returns current value of overflow flag
	 * @return true if flag is set
	 */
	bool getOverflowFlag();
	
	/**
	 * Clear overflow flag
	 */
	void clearOverflowFlag();

	void resetAllPackets(MM_EnvironmentBase *env);
	
	void overflowItem(MM_EnvironmentBase *env, void *item, MM_OverflowType type);

	/**
	 * Reset any state in the receiver in preparation for starting new work
	 * @param env - the current thread
	 */
	void reset(MM_EnvironmentBase *env);

	/**
	 * Handle Work Packet Overflow case
	 * @param env current thread environment
	 * @return true if overflow is triggered since previous Overflow handling
	 */
	bool handleWorkPacketOverflow(MM_EnvironmentBase *env);

	/**
	 * Create a WorkPackets object.
	 */
	MM_WorkPackets(MM_EnvironmentBase *env) :
		MM_BaseVirtual(),
		_packetsPerBlock(0),
		_maxPackets(0),
		_activePackets(0),
		_packetsBlocksTop(0),
		_allocatingPackets(NULL),
		_emptyPacketList(env),
		_fullPacketList(env),
		_relativelyFullPacketList(env),
		_nonEmptyPacketList(env),
		_deferredPacketList(env),
		_deferredFullPacketList(env),
		_inputListMonitor(NULL),
		_inputListWaitCount(0),
		_inputListDoneIndex(0),
		_overflowHandler(NULL)
	{
		_typeId = __FUNCTION__;
	}
	
protected:
	virtual MM_Packet *getPacketByOverflowing(MM_EnvironmentBase *env);
	MM_Packet* getPacketByAdddingWorkPacketBlock(MM_EnvironmentBase *env);
	virtual float getHeapCapacityFactor(MM_EnvironmentBase *env);
	
	/**
	 * New packets are available; notify any waiting threads.
	 * 
	 * @param env - the current thread
	 */
	virtual void notifyWaitingThreads(MM_EnvironmentBase *env);

	
private:
};

#endif /* WORKPACKETS_HPP_ */
