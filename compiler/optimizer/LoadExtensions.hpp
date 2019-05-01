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

#ifndef LOADEXTENSIONS_INCL
#define LOADEXTENSIONS_INCL

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"

/** \brief
 *     Examines how often a load is being used to feed into a signed vs. unsigned conversion (ex. i2l vs iu2l) and
 *     attempts to skip the conversion by sign/zero extending the load at the point it is evaluated (where the source 
 *     value is actually loaded). This codegen optimization relies on the assumption that the target codegen supports 
 *     load and sign/zero extend instructions and that emitting such an instruction is no more expensive than an 
 *     ordinary load.
 *
 *  \details
 *     This codegen phase pass consists of two steps; the find and the flag preferred load extensions steps.
 *
 *     1. Find preferred load extensions
 *
 *        We iterate through every node in the current compilation unit looking for conversion operations with loads
 *        as the operand. For example a conversion from int to long of a field load o.f:
 *
 *        \code
 *        i2l
 *          iloadi <f>
 *            aload <o>
 *        \endcode
 *
 *        For each such conversion, depending on the type, i.e. sign/zero extension, we record how often each unique
 *        load feeds into such a conversion. In the above snippet the conversion is a sign extension so we would bias
 *        the load towards sign extensions. If the same load were to appear underneath an iu2l conversion we would bias
 *        the load towards zero extensions.
 *
 *        When all trees in the compilation unit have been processed for each load we know exactly how many times it
 *        appears underneath a signed vs. unsigned conversion.
 *
 *     2. Flag preferred load extensions
 *
 *        Since we know the preference of sign vs. zero extension for each load we can now traverse the compilation
 *        unit again and based on the preference of a load we can mark it with the signExtendTo[32|64]BitAtSource or 
 *        zeroExtendTo[32|64]BitAtSource flags to force the extension to happen right at the source. Since we have an
 *        implicit assumption that such extensions are effectively free we can now skip the evaluation of all 
 *        conversions of the respective extensions.
 *
 *        For example in the above code snippet if the load was found to prefer sign extensions we can mark the load
 *        with the signExtendTo64BitAtSource flag and subsequently the i2l conversion can be skipped by marking it with
 *        the unneededConv flag. The final trees will look like this:
 *
 *        \code
 *        i2l (unneededConv)
 *          iloadi <f> (signExtendTo64BitAtSource)
 *            aload <o>
 *        \endcode
 *
 *        Note that if the same load were to appear underneath an iu2l conversion, this conversion cannot be skipped
 *        because of the extension preference of the load.
 *
 *     All load evaluators must respect the signExtendTo[32|64]BitAtSource and zeroExtendTo[32|64]BitAtSource flags.
 *
 *  \section Debug Counters
 *     You can track the locations of where this optimization succeeded via the following debug counter:
 *
 *     \code
 *     -Xjit:staticDebugCounters={codegen/LoadExtensions/success/unneededConversion*}
 *     \endcode
 *
 *  \section Node Flags
 *     This optimization sets the following node flags:
 *        - unneededConv
 *        - signExtendTo32BitAtSource
 *        - signExtendTo64BitAtSource
 *        - zeroExtendTo32BitAtSource
 *        - zeroExtendTo64BitAtSource
 */
class TR_LoadExtensions : public TR::Optimization
   {
   public:

   /** \brief
    *     Helper function to create an instance of the LoadExtensions optimization using the
    *     OptimizationManager's default allocator.
    *
    *  \param manager
    *     The optimization manager.
    */
   static TR::Optimization* create(TR::OptimizationManager* manager)
      {
      return new (manager->allocator()) TR_LoadExtensions(manager);
      }

   /** \brief
    *     Initializes the LoadExtensions codegen phase.
    *
    *  \param manager
    *     The optimization manager for this local optimization.
    */
   TR_LoadExtensions(TR::OptimizationManager* manager);

   /** \brief
    *     Performs the optimization on this compilation unit.
    *
    *  \return
    *     1 if any transformation was performed; 0 otherwise.
    */
   int32_t perform();

   virtual const char* optDetailString() const throw()
      {
      return "O^O LOAD EXTENSIONS: ";
      }

   private:

   /** \brief
    *     Determines whether the conversion of a load can be skipped based on the loads extension preference and if the
    *     conversion can be skipped determine whether the respective load needs to be sign/zero extended.
    *
    *  \param conversion
    *     The conversion to examine.
    *
    *  \param child
    *     The child on which the conversion acts. Note this may not necessarily be the conversions first child as the
    *     conversion may act indirectly on a globally allocated register.
    *
    *  \param forceExtension
    *     Determines whether the respective child load needs to be sign/zero extended.
    *
    *  \return
    *     <c>true</c> if this \p conversion can be skipped; <c>false</c> otherwise.
    */
   const bool canSkipConversion(TR::Node* conversion, TR::Node* child, bool& forceExtension);

   /** \brief
    *     Finds the (zero/sign) extension preference of a node.
    *
    *  \param node
    *     The node to examine.
    */
   void findPreferredLoadExtensions(TR::Node* node);

   /** \brief
    *     Flags conversions with the unneededConv flag and sets preference of loads to zero/sign extend at source by
    *     flagging them with signExtendTo[32|64]BitAtSource and zeroExtendTo[32|64]BitAtSource flags.
    *
    *  \param node
    *     The node to flag.
    */
   void flagPreferredLoadExtensions(TR::Node* node);

   /** \brief
    *     Determines whether a node is of the supported type for skipping conversion operations or forcing sign or zero
    *     extensions on loads.
    *
    *  \param node
    *     The node to examine.
    *
    *  \return
    *     <c>true</c> if this \p node is a load which is a candidate type for this optimization; <c>false</c> otherwise.
    */
   const bool isSupportedType(TR::Node* node) const;

   /** \brief
    *     Determines whether a node is a load candidate for skipping conversion operations.
    *
    *  \param node
    *     The node to examine.
    *
    *  \return
    *     <c>true</c> if this \p node is a load which is a candidate type for this optimization; <c>false</c> otherwise.
    */
   const bool isSupportedLoad(TR::Node* node) const;

   /** \brief
    *     Gets the zero/sign extension preference of a load.
    *
    *  \param load
    *     The load to examine.
    *
    *  \return
    *     Negative value if this \p load prefers to be zero extended and a positive value if this \p load prefers to be
    *     sign extended.
    */
   const int32_t getExtensionPreference(TR::Node* load) const;

   /** \brief
    *     Sets the zero/sign extension preference of a load based on the conversion it feeds into.
    *
    *  \param load
    *     The load to examine.
    *
    *  \return
    *     Negative value if this \p load prefers to be zero extended and a positive value if this \p load prefers to be
    *     sign extended.
    */
   const int32_t setExtensionPreference(TR::Node* load, TR::Node* conversion);

   private:

   typedef TR::typed_allocator<std::pair<const TR::Node* const, int32_t>, TR::Region&> NodeToIntTableAllocator;
   typedef std::less<const TR::Node*> NodeToIntTableComparator;
   typedef std::map<const TR::Node*, int32_t, NodeToIntTableComparator, NodeToIntTableAllocator> NodeToIntTable;

   /** \brief
    *     Keeps track of all nodes which should be excluded from consideration in this optimization.
    */
   NodeToIntTable* excludedNodes;

   /** \brief
    *     Keeps track of the load extension preference where a negative value indicates a preference towards a zero
    *     extension and a positive value indicates a preference towards a sign extension.
    */
   NodeToIntTable* loadExtensionPreference;
   };

#endif /* LOADEXTENSIONS_INCL_ */
