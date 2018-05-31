/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "MarkingDelegate.hpp"

#include <OMR/Om/Array.hpp>
#include <OMR/Om/Context.inl.hpp>
#include <OMR/Om/MemorySystem.hpp>
#include <OMR/Om/Object.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/RootRef.hpp>
#include <OMR/Om/TransitionSet.hpp>

#include "EnvironmentBase.hpp"
#include "MarkingScheme.hpp"
#include "OMRVMThreadListIterator.hpp"
#include "omr.h"
#include "omrhashtable.h"

namespace OMR
{
namespace Om
{

void
MarkingDelegate::scanRoots(MM_EnvironmentBase* env)
{
	auto& cx      = getContext(env);
	auto& system = cx.system();
	MM_MarkingScheme::MarkingVisitor marker(env, _markingScheme);
	system.visit(marker);

	for (ConstRootListIterator it = cx.stackRoots().cbegin(); it != cx.stackRoots().cend(); ++it) {
		// std::cout << "NATIVE_STACK: " << p << std::endl;
		_markingScheme->markObject(env, (Cell*)(*it).ref);
	}

/*
	/// TODO: Reimplement user marking callbacks.
	for (auto& fn : cx.userRoots()) {
		fn(marker);
	}
*/
}

void
MarkingDelegate::masterCleanupAfterGC(MM_EnvironmentBase* env)
{
}

}  // namespace Om
}  // namespace OMR
