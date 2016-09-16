/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#ifndef TR_OPTIMIZATIONS_INCL
#define TR_OPTIMIZATIONS_INCL

namespace OMR
   {
      enum Optimizations
      {
      #define OPTIMIZATION(name) name,
      #define OPTIMIZATION_ENUM_ONLY(entry) entry,
      #include "optimizer/Optimizations.enum"
      OPTIMIZATION_ENUM_ONLY(numOpts)  // after all project-specific optimization enums
      #include "optimizer/OptimizationGroups.enum"
      OPTIMIZATION_ENUM_ONLY(numGroups)  // after all project-specific optimization group enums
      #undef OPTIMIZATION
      #undef OPTIMIZATION_ENUM_ONLY
      };
   }

#endif
