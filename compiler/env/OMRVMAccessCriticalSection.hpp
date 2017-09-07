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

#ifndef OMR_VMACCESSCRITICALSECTION_INCL
#define OMR_VMACCESSCRITICALSECTION_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_VMACCESSCRITICALSECTION_CONNECTOR
#define OMR_VMACCESSCRITICALSECTION_CONNECTOR
namespace OMR { class VMAccessCriticalSection; }
namespace OMR { typedef OMR::VMAccessCriticalSection VMAccessCriticalSectionConnector; }
#endif


#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "infra/Annotations.hpp"


namespace OMR
{

class OMR_EXTENSIBLE VMAccessCriticalSection
   {
public:

   enum VMAccessAcquireProtocol
      {
      acquireVMAccessIfNeeded,
      tryToAcquireVMAccess
      };

protected:

   VMAccessCriticalSection(
         VMAccessAcquireProtocol protocol,
         TR::Compilation *comp) :
      _protocol(protocol),
      _acquiredVMAccess(false),
      _hasVMAccess(false),
      _comp(comp),
      _omrVMThread(NULL),
      _vmAccessReleased(false),
      _initializedBySubClass(false)
      {
      // Implementation provided by subclass
      }

   VMAccessCriticalSection(
         VMAccessAcquireProtocol protocol,
         OMR_VMThread *omrVMThread) :
      _protocol(protocol),
      _acquiredVMAccess(false),
      _hasVMAccess(false),
      _comp(NULL),
      _omrVMThread(omrVMThread),
      _vmAccessReleased(false),
      _initializedBySubClass(false)
      {
      // Implementation provided by subclass
      }

public:

   VMAccessCriticalSection(
         TR::Compilation *comp,
         VMAccessAcquireProtocol protocol) :
      _protocol(protocol),
      _acquiredVMAccess(false),
      _hasVMAccess(false),
      _omrVMThread(NULL),
      _comp(comp),
      _vmAccessReleased(false),
      _initializedBySubClass(false)
      {
      switch (protocol)
         {
         case acquireVMAccessIfNeeded:
            _acquiredVMAccess = TR::Compiler->vm.acquireVMAccessIfNeeded(comp);
            _hasVMAccess = true;
            break;

         case tryToAcquireVMAccess:
            _hasVMAccess = TR::Compiler->vm.tryToAcquireAccess(comp, &_acquiredVMAccess);
         }
      }


   VMAccessCriticalSection(
         OMR_VMThread *omrVMThread,
         VMAccessAcquireProtocol protocol) :
      _protocol(protocol),
      _acquiredVMAccess(false),
      _hasVMAccess(false),
      _omrVMThread(omrVMThread),
      _comp(NULL),
      _vmAccessReleased(false),
      _initializedBySubClass(false)
      {
      switch (protocol)
         {
         case acquireVMAccessIfNeeded:
            _acquiredVMAccess = TR::Compiler->vm.acquireVMAccessIfNeeded(omrVMThread);
            _hasVMAccess = true;
            break;

         case tryToAcquireVMAccess:
            _hasVMAccess = TR::Compiler->vm.tryToAcquireAccess(omrVMThread, &_acquiredVMAccess);
            break;
         }
      }

   ~VMAccessCriticalSection()
      {

      // Do not run this destructor unless it was the OMR class that acquired access
      //
      if (!_initializedBySubClass)
         {

         // Do not run this destructor if a subclass destructor has already released access
         //
         if (!_vmAccessReleased)
            {
            if (_comp)
               {
               switch (_protocol)
                  {
                  case acquireVMAccessIfNeeded:
                     TR::Compiler->vm.releaseVMAccessIfNeeded(_comp, _acquiredVMAccess);
                     break;

                  case tryToAcquireVMAccess:
                     if (_hasVMAccess && _acquiredVMAccess)
                        TR::Compiler->vm.releaseAccess(_comp);
                  }
               }
            else
               {
               switch (_protocol)
                  {
                  case acquireVMAccessIfNeeded:
                     TR::Compiler->vm.releaseVMAccessIfNeeded(_omrVMThread, _acquiredVMAccess);
                     break;

                  case tryToAcquireVMAccess:
                     if (_hasVMAccess && _acquiredVMAccess)
                        TR::Compiler->vm.releaseAccess(_omrVMThread);
                  }
               }

            _vmAccessReleased = true;
            }

         }

      }

   bool acquiredVMAccess() { return _acquiredVMAccess; }
   bool hasVMAccess() { return _hasVMAccess; }

protected:

   bool _initializedBySubClass;
   bool _vmAccessReleased;
   bool _acquiredVMAccess;
   bool _hasVMAccess;
   VMAccessAcquireProtocol _protocol;
   TR::Compilation *_comp;
   OMR_VMThread *_omrVMThread;
   };

}

#endif
