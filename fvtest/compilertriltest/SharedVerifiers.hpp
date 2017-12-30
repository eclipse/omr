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


/**
 * This Verifier checks if an AND operation exists. 
 *
 * Compilation is stopped by returning a non-zero return code.
 */
class NoAndIlVerifier : public TR::IlVerifier
   {
   public:
   int32_t verifyNode(TR::Node *node)
      {
      if (node->getOpCode().isAnd()) 
         {
         return 1;
         }
      return 0;
      }

   int32_t verify(TR::ResolvedMethodSymbol *sym)
      {
      for(TR::PreorderNodeIterator iter(sym->getFirstTreeTop(), sym->comp()); iter.currentTree(); ++iter)
         {
         int32_t rtn = verifyNode(iter.currentNode());
         if(rtn)
            return rtn;
         }

      return 0;
      }
   };
