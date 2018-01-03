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

#ifndef OMR_ILVALUE_INCL
#define OMR_ILVALUE_INCL

#ifndef TR_ILVALUE_DEFINED
#define TR_ILVALUE_DEFINED
#define PUT_OMR_ILVALUE_INTO_TR
#endif // !defined(TR_ILVALUE_DEFINED)


namespace OMR { class IlBuilder; }
namespace TR { class IlValueImpl; }
namespace TR { class MethodBuilder; }


namespace OMR
{

class IlValue
   {
   friend class OMR::IlBuilder;

protected:
   IlValue(TR::MethodBuilder *mb, int32_t id)
      : _methodBuilder(mb), _id(id)
      { }

public:

   /**
    * TR::IlValue has no public constructor because JitBuilder clients should not create their own IlValues
    */

   /**
    * @brief returns a unique identifier for this IlValue object within its MethodBuilder
    * Note that IlValues from different MethodBuilders can have the same ID.
    */
   int32_t getID() // TODO: capitalize for consistency in public API
      {
      return _id;
      }

   /**
    * @brief returns the TR::MethodBuilder object that owns this IlValue
    */
   TR::MethodBuilder *methodBuilder()
      {
      return _methodBuilder;
      }

protected:
   /**
    * @brief returns the TR::IlBuilderImpl object corresponding to this IlValue
    * Currently, IlValueImpl is a superclass so only need to static_cast this
    */
   TR::IlValueImpl *impl();

private:

   /**
    * @brief Method builder object where value is computed
    */
   TR::MethodBuilder   * _methodBuilder;

   /**
    * @brief identifying number for values guaranteed to be unique per MethodBuilder
    */
   int32_t               _id;

   };

} // namespace OMR



#if defined(PUT_OMR_ILVALUE_INTO_TR)

namespace TR
{
   class IlValue : public OMR::IlValue
      {
      public:
         IlValue(TR::MethodBuilder *mb, int32_t id)
            : OMR::IlValue(mb, id)
            { }
      };

} // namespace TR

#endif // defined(PUT_OMR_ILVALUE_INTO_TR)

#endif // !defined(OMR_ILVALUE_INCL)
