/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "optimizer/PreExistence.hpp"

#include "env/CompilerEnv.hpp"
#include "env/KnownObjectTable.hpp"
#include "compile/Compilation.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "env/VMAccessCriticalSection.hpp"
#endif
#include "optimizer/Inliner.hpp"

PrexKnowledgeLevel
TR_PrexArgument::knowledgeLevel(TR_PrexArgument *pa)
   {
   if (!pa)
      return NONE;
   else if (pa->hasKnownObjectIndex())
      return KNOWN_OBJECT;
   else if (pa->classIsFixed())
      return FIXED_CLASS;
   else if (pa->classIsPreexistent())
      return PREEXISTENT;
   else
      return NONE;
   }

TR_PrexArgument::TR_PrexArgument(
      TR::KnownObjectTable::Index knownObjectIndex,
      TR::Compilation *comp) :
   _classKind(ClassIsUnknown),
   _class(0),
   _profiledClazz(0),
   _knownObjectIndex(knownObjectIndex)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR::VMAccessCriticalSection prexArgumentCriticalSection(comp,
                                                            TR::VMAccessCriticalSection::tryToAcquireVMAccess);

   if (prexArgumentCriticalSection.hasVMAccess())
      {
      _class = TR::Compiler->cls.objectClass(comp, comp->getKnownObjectTable()->getPointer(knownObjectIndex));
      _classKind = ClassIsFixed;
      }
#endif
   }
