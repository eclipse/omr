/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/


/**
 * Description: Two extensible classes inherit from the same
 *    non-extensible class. This is an error because only one
 *    extensible class is allowed to inherit from a non-extensible
 *    class. However, a defect in OMRChecker causes it to think
 *    this is an infinitly recursive extensible class hierarchy,
 *    which is impossible. Hence, the error generated is that there
 *    are more than 50 extensible classes in the hierarchy.
 */

#define OMR_EXTENSIBLE __attribute__((annotate("OMR_Extensible")))

namespace OMR { class NonExtClass {}; }
namespace TR  { class NonExtClass : public OMR::NonExtClass {}; }

namespace OMR { class OMR_EXTENSIBLE FirstExtClass : public TR::NonExtClass {}; }
namespace OMR { class OMR_EXTENSIBLE SeconExtClass : public TR::NonExtClass {}; }
