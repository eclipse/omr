/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef OMR_OPTIMIZATIONMANAGER_INLINES_INCL
#define OMR_OPTIMIZATIONMANAGER_INLINES_INCL

#include "optimizer/OMROptimizationManager.hpp"

inline
TR::OptimizationManager *OMR::OptimizationManager::self()
   {
   return (static_cast<TR::OptimizationManager *>(this));
   }

inline
TR::CodeGenerator *OMR::OptimizationManager::cg()
   {
   return self()->comp()->cg();
   }

inline
TR_FrontEnd *OMR::OptimizationManager::fe()
   {
   return self()->comp()->fe();
   }

inline
TR_Debug *OMR::OptimizationManager::getDebug()
   {
   return self()->comp()->getDebug();
   }

inline
TR::SymbolReferenceTable *OMR::OptimizationManager::getSymRefTab()
   {
   return self()->comp()->getSymRefTab();
   }

inline
TR_Memory *OMR::OptimizationManager::trMemory()
   {
   return self()->comp()->trMemory();
   }

inline
TR_StackMemory OMR::OptimizationManager::trStackMemory()
   {
   return self()->comp()->trStackMemory();
   }

inline
TR_HeapMemory OMR::OptimizationManager::trHeapMemory()
   {
   return self()->comp()->trHeapMemory();
   }

inline
TR_PersistentMemory *OMR::OptimizationManager::trPersistentMemory()
   {
   return self()->comp()->trPersistentMemory();
   }

inline
TR::Allocator OMR::OptimizationManager::allocator()
   {
   return self()->comp()->allocator();
   }

#endif
