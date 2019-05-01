/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef COLLECTORLANGUAGEINTERFACE_HPP_
#define COLLECTORLANGUAGEINTERFACE_HPP_

#include "modronbase.h"
#include "objectdescription.h"

#include "BaseVirtual.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "SlotObject.hpp"

class GC_ObjectScanner;
class MM_CompactScheme;
class MM_EnvironmentStandard;
class MM_ForwardedHeader;
class MM_MarkMap;
class MM_MemorySubSpaceSemiSpace;

/**
 * Class representing a collector language interface. This defines the API between the OMR
 * functionality and the language being implemented.
 */
class MM_CollectorLanguageInterface : public MM_BaseVirtual {

private:

protected:

public:

private:

protected:

public:

	virtual void kill(MM_EnvironmentBase *env) = 0;

	MM_CollectorLanguageInterface()
		: MM_BaseVirtual()
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* COLLECTORLANGUAGEINTERFACE_HPP_ */
