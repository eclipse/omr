/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#ifndef TYPE_LAYOUT_INCL
#define TYPE_LAYOUT_INCL

#include "il/DataTypes.hpp"
#include "infra/vector.hpp"

namespace TR {

/** \brief
 *     TypeLayoutEntry represents a field. 
 * 
 *     For each field entry its TR::DataType and 
 *     its offset (including the object header size) are recorded.  
 */
class TypeLayoutEntry
   {
   public:
   TypeLayoutEntry(TR::DataType datatype,
                   int32_t offset,
                   char * fieldname,
                   bool isVolatile = false,
                   bool isPrivate = false,
                   bool isFinal = false,
                   const char* typeSignature = NULL)
      : _datatype(datatype),
        _offset(offset),
        _fieldname(fieldname),
        _isVolatile(isVolatile),
        _isPrivate(isPrivate),
        _isFinal(isFinal),
        _typeSignature(typeSignature)
       {}
   TR::DataType _datatype;
   int32_t _offset;
   const char * _fieldname;
   bool _isVolatile;
   bool _isPrivate;
   bool _isFinal;
   const char * _typeSignature;
   };

/** \brief
 *     TypeLayout represents the layout of fields.
 * 
 *     It contains TypeLayoutEntry that belongs to 
 *     a single class. It stores a `TR::vector` of TypeLayoutEntry objects.
 */
class TypeLayout
   {
   public:
   size_t count() const { return _entries.size(); }

   /** \brief
    *     Finds the corresponding entry based on index given.
    * 
    *  \param ind
    *     The index of the entry that wants to be returned.
    * 
    *  \return
    *     The TypeLayoutEntry object stored in the vector at given index.
    */
   const TypeLayoutEntry& entry(size_t ind) const { return _entries.at(ind); } 

   /** \brief
    *     Queries for the index of the field at a given offset.
    * 
    *  \param offset
    *     The offset of the field shadow.
    * 
    *  \return
    *     The index of the entry that wants to be queried for.
    */
   int32_t fieldIndex (int32_t offset) const
      {
      int32_t ind = 0;
      while (_entries.at(ind)._offset != offset)
         {
         ind++;
         }  
      return ind;
      }
      
   private:
   TR::vector<TR::TypeLayoutEntry, TR::Region&> _entries; 
   TypeLayout(TR::Region& region) : _entries(region) {}

   struct CompareOffset
      {
      bool operator() (const TypeLayoutEntry& entry1, const TypeLayoutEntry& entry2) const 
         { 
         return entry1._offset < entry2._offset; 
         }
      };

   /** \brief
    *     Sorts TypeLayout entries by offset.
    */
   void sort() 
      { 
      CompareOffset compareOffset;
      std::sort(_entries.begin(), _entries.end(), compareOffset);
      }

   friend class TypeLayoutBuilder;
   };

/** \brief
 *     TypeLayoutBuilder is a builder class for TypeLayout.
 * 
 *     It encapsulates building a TypeLayout instance.
 */
class TypeLayoutBuilder
   {
   public:
   TypeLayoutBuilder(TR::Region & region) : tl(new (region) TypeLayout(region)) {}

   /** \brief
    *     Add TypeLayoutEntry to TypeLayout being built.
    * 
    *  \param entry
    *     The entry being added.
    */
   void add(TR::TypeLayoutEntry entry)
      {
      tl->_entries.push_back(entry);
      }

   /** \brief
    *     Return built TypeLayout object.
    * 
    *  \return
    *     The built TypeLayout.
    */  
   TypeLayout* build()
      {
      tl->sort();
      auto* temp = tl;
      tl = NULL;
      return temp;
      }

   private:
   TypeLayout* tl;
   };

} //TR

#endif
