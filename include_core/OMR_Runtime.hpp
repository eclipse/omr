/*******************************************************************************
 * Copyright (c) 2013, 2014 IBM Corp. and others
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

#if !defined(OMR_RUNTIME_HPP_)
#define OMR_RUNTIME_HPP_

/*
 * @ddr_namespace: default
 */

#include "omr.h"
#include "OMR_BaseNonVirtual.hpp"
#include "OMR_RuntimeConfiguration.hpp"

extern "C" {

/**
 * Internal: Attach a VM to the runtime.
 *
 * @param[in] *runtime the runtime
 * @param[in] *vm the VM to attach
 *
 * @return an OMR error code
 */
omr_error_t attachVM(OMR_Runtime *runtime, OMR_VM *vm);

/**
 * Internal: Detach a VM from the runtime.
 *
 * @param[in] *runtime the runtime
 * @param[in] *vm the VM to detach
 *
 * @return an OMR error code
 */
omr_error_t detachVM(OMR_Runtime *runtime, OMR_VM *vm);

}

#endif /* OMR_RUNTIME_HPP_ */
