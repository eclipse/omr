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
