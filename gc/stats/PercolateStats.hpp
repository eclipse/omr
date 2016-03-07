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

#if !defined(PERCOLATESTATS_HPP_)
#define PERCOLATESTATS_HPP_

#include "omrcomp.h"
#include "modronbase.h"

#include "Base.hpp"

class MM_PercolateStats : public MM_Base {
	PercolateReason _lastPercolateReason;
	uintptr_t _scavengesSincePercolate;

public:
	MMINLINE void setLastPercolateReason(PercolateReason reason) { _lastPercolateReason = reason; }
	MMINLINE PercolateReason getLastPercolateReason() { return _lastPercolateReason; }
	MMINLINE void resetLastPercolateReason() { _lastPercolateReason = NONE_SET; }

	void incrementScavengesSincePercolate() { _scavengesSincePercolate++; }
	void clearScavengesSincePercolate() { _scavengesSincePercolate = 0; }
	uintptr_t getScavengesSincePercolate() { return _scavengesSincePercolate; }

	MM_PercolateStats()
		: MM_Base()
		, _lastPercolateReason(NONE_SET)
		, _scavengesSincePercolate(0) {};
};

#endif /* PERCOLATESTATS_HPP_ */
