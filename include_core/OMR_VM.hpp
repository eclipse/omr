/*******************************************************************************
 * Copyright (c) 2013, 2015 IBM Corp. and others
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

#if !defined(OMR_VM_HPP_)
#define OMR_VM_HPP_

/*
 * @ddr_namespace: default
 */

#include "omr.h"
#include "OMR_BaseNonVirtual.hpp"
#include "OMR_VMConfiguration.hpp"

extern "C" {

/**
 * Internal: Attach a vmthread to the VM.
 *
 * @param[in] vm The VM
 * @param[in] vmthread The vmthread to attach. NOTE: Need NOT be the current thread!
 *                     vmthread->_os_thread must be initialized.
 *
 * @return an OMR error code
 */
omr_error_t attachThread(OMR_VM *vm, OMR_VMThread *vmthread);

/**
 * Internal: Detach a vmthread from the VM.
 *
 * @param[in] vm The VM
 * @param[in] vmthread The vmthread to detach. NOTE: Need NOT be the current thread!
 *
 * @return an OMR error code
 */
omr_error_t detachThread(OMR_VM *vm, OMR_VMThread *vmthread);

}

#endif /* OMR_VM_HPP_ */
