/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

/* Per-thread buffer functions for socket information */

#ifndef OMRSOCKPTB_H_
#define OMRSOCKPTB_H_

#include <stdio.h>
#include <string.h>

#include "omrport.h"
#include "omrporterror.h"
#include "omrportpriv.h"
#include "omrportsock.h"
#include "omrportsocktypes.h"
#include "thread_api.h"

/* Per-thread buffer for socket information */
typedef struct OMRSocketPTB {
	OMRAddrInfoNode addrInfoHints;
	struct OMRPortLibrary *portLibrary;
} OMRSocketPTB;

typedef OMRSocketPTB *omrsock_ptb_t;

omrsock_ptb_t omrsock_ptb_get(struct OMRPortLibrary *portLibrary);
int32_t omrsock_ptb_init(struct OMRPortLibrary *portLibrary);
int32_t omrsock_ptb_shutdown(struct OMRPortLibrary *portLibrary);

#endif /* OMRSOCKPTB_H_ */
