/*******************************************************************************
 * Copyright (c) 2020, 2022 IBM Corp. and others
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

#include "gtest/gtest.h"
#include "omrcomp.h"

#include "codegen/CCData.hpp"
#include "codegen/CCData_inlines.hpp"

using TR::CCData;

const CCData::index_t INVALID_INDEX = std::numeric_limits<CCData::index_t>::max();

template <typename T>
class CCDataTest : public testing::Test
   {
   };

struct odd_sized_t
   {
   odd_sized_t() {}
   odd_sized_t(const uint8_t x) { set_em_all(x); }
   odd_sized_t& operator=(const uint8_t x) { set_em_all(x); return *this; }
   odd_sized_t operator-() const { return odd_sized_t(-_data[0]); }
   bool operator==(const odd_sized_t &other) const { return _data[0] == other._data[0]; };
   private:
      void set_em_all(const uint8_t x)
         {
         for (auto i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
            _data[i] = x;
         }
      uint8_t _data[7];
   };

using CCDataTestTypes = testing::Types<uint8_t, uint16_t, uint32_t, uint64_t,
                                       int8_t, int16_t, int32_t, int64_t, odd_sized_t
                                       /* Floats and doubles don't have unique object representations.
                                       See the documentation for CCData::key().
                                       The templated tests below would need to be re-written to create keys
                                       differently for at least floats and doubles.*/
                                       //,float, double
                                      >;

TYPED_TEST_SUITE(CCDataTest, CCDataTestTypes);

TYPED_TEST(CCDataTest, test_basics_templated)
   {
   std::vector<char>    storage(256);
   CCData               table(&storage[0], storage.size());
   const TypeParam      data = 99;
   // Generate a key from the data being stored.
   const CCData::key_t  key = CCData::key(data);

   // Nothing should be found by this key yet.
   ASSERT_FALSE(table.find(key));

   CCData::index_t index = INVALID_INDEX;

   // Put the data in the table, associate it with the key, retrieve the index.
   ASSERT_TRUE(table.put(data, &key, index));
   // Make sure the index was written.
   ASSERT_NE(index, INVALID_INDEX);

   const TypeParam * const dataPtr = table.get<TypeParam>(index);

   // Make sure the data was written.
   ASSERT_TRUE(dataPtr != NULL);
   // Make sure it was written to an aligned address.
   ASSERT_EQ(reinterpret_cast<size_t>(dataPtr) & (OMR_ALIGNOF(data) - 1), 0);
   // Make sure it was written correctly.
   ASSERT_EQ(*dataPtr, data);
   // We should be able to find something with this key now.
   ASSERT_TRUE(table.find(CCData::key(data)));

   TypeParam out_data = -data;

   // Retrieve the data via the index.
   ASSERT_TRUE(table.get(index, out_data));
   // Make sure it matches what was stored.
   ASSERT_EQ(data, out_data);
   // Make sure both copies generate equal keys.
   ASSERT_EQ(CCData::key(data), CCData::key(out_data));

   CCData::index_t find_index = INVALID_INDEX;

   // Find the index via the key.
   ASSERT_TRUE(table.find(CCData::key(data), &find_index));
   // Make sure it's the same index.
   ASSERT_EQ(index, find_index);
   }

TYPED_TEST(CCDataTest, test_fill_table_templated)
   {
   std::vector<char>    storage(256);
   CCData               table(&storage[0], storage.size());
   const TypeParam      data = 99;
   ssize_t              spaceLeft = storage.size();
   bool                 putSucceeded = true;
   CCData::index_t      index = INVALID_INDEX;

   // Make sure we can fill the table.
   do
      {
      putSucceeded = table.put(data, NULL, index);
      if (putSucceeded)
         {
         spaceLeft -= sizeof(data);
         }
      }
   while (spaceLeft >= 0 && putSucceeded);
   ASSERT_FALSE(putSucceeded);
   ASSERT_GE(spaceLeft, 0);
   }

TYPED_TEST(CCDataTest, test_arbitrary_data_templated)
   {
   std::vector<char>    storage(256);
   CCData               table(&storage[0], storage.size());
   const TypeParam      d = 99;
   const TypeParam      data[3] = {d, d, d};
   // Generate a key from the data being stored.
   const CCData::key_t  key = CCData::key(&data[0], sizeof(data));

   // Nothing should be found by this key yet.
   ASSERT_FALSE(table.find(key));

   CCData::index_t index = INVALID_INDEX;

   // Put the data in the table, associate it with the key, retrieve the index.
   ASSERT_TRUE(table.put(&data[0], sizeof(data), OMR_ALIGNOF(data), &key, index));
   // Make sure the index was written.
   ASSERT_NE(index, INVALID_INDEX);

   const TypeParam * const dataPtr = table.get<TypeParam>(index);

   // Make sure the data was written.
   ASSERT_TRUE(dataPtr != NULL);
   // Make sure it was written to an aligned address.
   ASSERT_EQ(reinterpret_cast<size_t>(dataPtr) & (OMR_ALIGNOF(data) - 1), 0);
   // Make sure it was written correctly.
   ASSERT_TRUE(std::equal(data, data + sizeof(data)/sizeof(data[0]), dataPtr));
   // We should be able to find something with this key now.
   ASSERT_TRUE(table.find(CCData::key(&data[0], sizeof(data))));

   const TypeParam dminus = -d; // Negating `d` here avoids narrowing conversion warnings/errors on the next line.
   TypeParam out_data[3] = {dminus, dminus, dminus};

   // Retrieve the data via the index.
   ASSERT_TRUE(table.get(index, &out_data[0], sizeof(out_data)));
   // Make sure it matches what was stored.
   ASSERT_TRUE(std::equal(data, data + sizeof(data)/sizeof(data[0]), out_data));
   // Make sure both copies generate equal keys.
   ASSERT_EQ(CCData::key(&data[0], sizeof(data)), CCData::key(&out_data[0], sizeof(out_data)));

   CCData::index_t find_index = INVALID_INDEX;

   // Find the index via the key.
   ASSERT_TRUE(table.find(CCData::key(&data[0], sizeof(data)), &find_index));
   // Make sure it's the same index.
   ASSERT_EQ(index, find_index);
   }

TYPED_TEST(CCDataTest, test_fill_table_arbitrary_data_templated)
   {
   std::vector<char>    storage(256);
   CCData               table(&storage[0], storage.size());
   const TypeParam      d = 99;
   const TypeParam      data[3] = {d, d, d};
   ssize_t              spaceLeft = storage.size();
   bool                 putSucceeded = true;
   CCData::index_t      index = INVALID_INDEX;

   // Make sure we can fill the table.
   do
      {
      putSucceeded = table.put(&data[0], sizeof(data), OMR_ALIGNOF(data), NULL, index);
      if (putSucceeded)
         {
         spaceLeft -= sizeof(data);
         }
      }
   while (spaceLeft >= 0 && putSucceeded);
   ASSERT_FALSE(putSucceeded);
   ASSERT_GE(spaceLeft, 0);
   }

TYPED_TEST(CCDataTest, test_no_data_templated)
   {
   std::vector<char>    storage(256, 0);
   CCData               table(&storage[0], storage.size());
   const TypeParam      data = 99;
   CCData::index_t      index = INVALID_INDEX;

   // Shouldn't be able to put.
   ASSERT_FALSE(table.put(NULL, sizeof(data), OMR_ALIGNOF(data), NULL, index));
   // Make sure the index didn't change.
   ASSERT_EQ(index, INVALID_INDEX);
   // Make sure storage wasn't written to.
   for (std::vector<char>::iterator i = storage.begin(); i != storage.end(); i++)
      {
      ASSERT_EQ(*i, 0);
      }
   }

TYPED_TEST(CCDataTest, test_no_data_reservation_templated)
   {
   std::vector<char>    storage(256);
   CCData               table(&storage[0], storage.size());
   size_t               reservationSize = 32, reservationAlignment = 16;
   // Generate an arbitrary key.
   const CCData::key_t  key = CCData::key("It was the best of times, it was the worst of times.");

   // Nothing should be found by this key yet.
   ASSERT_FALSE(table.find(key));

   CCData::index_t index = INVALID_INDEX;
   CCData::index_t index2 = INVALID_INDEX;

   // Reserve space in the table, associate it with the key, retrieve the index.
   ASSERT_TRUE(table.reserve(reservationSize, reservationAlignment, &key, index));
   // Make sure the index was written.
   ASSERT_NE(index, INVALID_INDEX);
   // Try to update the value via the key.
   ASSERT_TRUE(table.reserve(reservationSize, reservationAlignment, &key, index2));
   // Make sure the index was written.
   ASSERT_EQ(index2, index);

   const TypeParam * const dataPtr = table.get<TypeParam>(index);

   // Make sure the reservation was done.
   ASSERT_TRUE(dataPtr != NULL);
   // Make sure we got an aligned address.
   ASSERT_EQ(reinterpret_cast<size_t>(dataPtr) & (reservationAlignment - 1), 0);
   // We should be able to find something with this key now.
   ASSERT_TRUE(table.find(key));

   CCData::index_t find_index = INVALID_INDEX;

   // Find the index via the key.
   ASSERT_TRUE(table.find(key, &find_index));
   // Make sure it's the same index.
   ASSERT_EQ(index, find_index);
   }

TYPED_TEST(CCDataTest, test_fill_table_no_data_reservation_templated)
   {
   std::vector<char>    storage(256);
   CCData               table(&storage[0], storage.size());
   size_t               reservationSize = 32, reservationAlignment = 16;
   ssize_t              spaceLeft = storage.size();
   bool                 putSucceeded = true;
   CCData::index_t      index = INVALID_INDEX;

   // Make sure we can fill the table.
   do
      {
      putSucceeded = table.reserve(reservationSize, reservationAlignment, NULL, index);
      if (putSucceeded)
         {
         spaceLeft -= reservationSize;
         }
      }
   while (spaceLeft >= 0 && putSucceeded);
   ASSERT_FALSE(putSucceeded);
   ASSERT_GE(spaceLeft, 0);
   }

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

TYPED_TEST(CCDataTest, test_arbitrary_keys_templated)
   {
   std::vector<char>    storage(256);
   CCData               table(&storage[0], storage.size());
   const TypeParam      data = 99;
   const CCData::key_t  keyFromData = CCData::key(data);
   const CCData::key_t  keyFromFileAndLine = CCData::key(__FILE__ ":" TOSTRING(__LINE__));

   // Nothing should be found by either key yet.
   ASSERT_FALSE(table.find(keyFromData));
   ASSERT_FALSE(table.find(keyFromFileAndLine));

   CCData::index_t index1 = INVALID_INDEX;
   CCData::index_t index2 = INVALID_INDEX;

   // Put the data in the table twice, associate it with each key, retrieve the indices.
   ASSERT_TRUE(table.put(data, &keyFromData, index1));
   ASSERT_TRUE(table.put(data, &keyFromFileAndLine, index2));
   // Make sure the indices were written.
   ASSERT_NE(index1, INVALID_INDEX);
   ASSERT_NE(index2, INVALID_INDEX);
   // We should be able to find something with both keys now.
   ASSERT_TRUE(table.find(CCData::key(data)));
   ASSERT_TRUE(table.find(keyFromFileAndLine));
   // The data should be in two different indices, based on key.
   ASSERT_NE(index1, index2);
   }

TYPED_TEST(CCDataTest, test_error_conditions_templated)
   {
      {
      std::vector<char> storage(256, 0);
      CCData            zeroTable(&storage[0], 0);
      const TypeParam   data = 99;
      CCData::index_t   index = INVALID_INDEX;

      // Shouldn't be able to put.
      ASSERT_FALSE(zeroTable.put(data, NULL, index));
      // Make sure the index didn't change.
      ASSERT_EQ(index, INVALID_INDEX);
      // Make sure storage wasn't written to.
      for (std::vector<char>::iterator i = storage.begin(); i != storage.end(); i++)
         {
         ASSERT_EQ(*i, 0);
         }
      }

      {
      const size_t         smallSize = 16;
      std::vector<char>    storage(sizeof(TypeParam) * smallSize);
      CCData               smallTable(&storage[0], storage.size());
      const TypeParam      data = 99;
      CCData::index_t      lastIndex = INVALID_INDEX;
      CCData::index_t      curIndex = INVALID_INDEX;

      int count = 0;

      while (smallTable.put(data, NULL, curIndex))
         {
         // Make sure the index changed.
         ASSERT_NE(curIndex, lastIndex);
         lastIndex = curIndex;
         ++count;
         }

      // Make sure the index didn't change on the last iteration.
      ASSERT_EQ(curIndex, lastIndex);

      // Make sure we put at least some data in the table.
      ASSERT_GT(count, 0);
      }
   }

TYPED_TEST(CCDataTest, test_updating_templated)
   {
   std::vector<char>    storage(256);
   CCData               table(&storage[0], storage.size());
   const TypeParam      data1 = 99;
   const TypeParam      data2 = -data1;
   const CCData::key_t  key = CCData::key("data");
   CCData::index_t      index1 = INVALID_INDEX;
   CCData::index_t      index2 = INVALID_INDEX;

   // Put the data in the table, associate it with the key, retrieve the index.
   ASSERT_TRUE(table.put(data1, &key, index1));
   // Make sure the index was written.
   ASSERT_NE(index1, INVALID_INDEX);
   // Update the value via the key.
   ASSERT_TRUE(table.put(data2, &key, index2));
   // Make sure the index was written.
   ASSERT_EQ(index2, index1);

   TypeParam * const dataPtr = table.get<TypeParam>(index1);

   // Make sure the data was written.
   ASSERT_TRUE(dataPtr != NULL);
   // Make sure it wasn't updated.
   ASSERT_EQ(*dataPtr, data1);
   // Change the value via a pointer.
   *dataPtr = data2;

   TypeParam out_data;
   // Make sure the data was written.
   ASSERT_TRUE(table.get(index1, out_data));
   // Make sure it matches.
   ASSERT_EQ(out_data, data2);
   }

TEST(AllTypesCCDataTest, test_basics)
   {
   std::vector<char>    storage(256);
   CCData               table(&storage[0], storage.size());
   const void           * const classAptr = reinterpret_cast<void *>(0x1), * const classBptr = reinterpret_cast<void *>(0x2);
   const void           * const funcAptr = reinterpret_cast<void *>(0x100), * const funcBptr = reinterpret_cast<void *>(0x200);
   const int            intA = 99, intB = 101;
   const short          shortA = 999, shortB = -999;
   const float          floatA = 10000.99f, floatB = 0.99999f;
   const double         doubleA = 1245.6789, doubleB = 3.14159;

   const CCData::key_t classAptrKey = CCData::key(classAptr);
   const CCData::key_t classBptrKey = CCData::key(classBptr);
   const CCData::key_t funcAptrKey = CCData::key(funcAptr);
   const CCData::key_t funcBptrKey = CCData::key(funcBptr);
   const CCData::key_t intAKey = CCData::key(intA);
   const CCData::key_t intBKey = CCData::key(intB);
   const CCData::key_t shortAKey = CCData::key(shortA);
   const CCData::key_t shortBKey = CCData::key(shortB);
   // Can't use float and double values for keys, see the documentation for CCData::key().
   const CCData::key_t floatAKey = CCData::key("floatA");
   const CCData::key_t floatBKey = CCData::key("floatB");
   const CCData::key_t doubleAKey = CCData::key("doubleA");
   const CCData::key_t doubleBKey = CCData::key("doubleB");

   ASSERT_FALSE(table.find(classAptrKey));
   ASSERT_FALSE(table.find(classBptrKey));
   ASSERT_FALSE(table.find(funcAptrKey));
   ASSERT_FALSE(table.find(funcBptrKey));
   ASSERT_FALSE(table.find(intAKey));
   ASSERT_FALSE(table.find(intBKey));
   ASSERT_FALSE(table.find(shortAKey));
   ASSERT_FALSE(table.find(shortBKey));
   ASSERT_FALSE(table.find(floatAKey));
   ASSERT_FALSE(table.find(floatBKey));
   ASSERT_FALSE(table.find(doubleAKey));
   ASSERT_FALSE(table.find(doubleBKey));

   CCData::index_t classAptrIndex = INVALID_INDEX;
   CCData::index_t classBptrIndex = INVALID_INDEX;
   CCData::index_t funcAptrIndex = INVALID_INDEX;
   CCData::index_t funcBptrIndex = INVALID_INDEX;
   CCData::index_t intAIndex = INVALID_INDEX;
   CCData::index_t intBIndex = INVALID_INDEX;
   CCData::index_t shortAIndex = INVALID_INDEX;
   CCData::index_t shortBIndex = INVALID_INDEX;
   CCData::index_t floatAIndex = INVALID_INDEX;
   CCData::index_t floatBIndex = INVALID_INDEX;
   CCData::index_t doubleAIndex = INVALID_INDEX;
   CCData::index_t doubleBIndex = INVALID_INDEX;

   ASSERT_TRUE(table.put(classAptr, &classAptrKey, classAptrIndex));
   ASSERT_TRUE(table.put(classBptr, &classBptrKey, classBptrIndex));
   ASSERT_TRUE(table.put(funcAptr, &funcAptrKey, funcAptrIndex));
   ASSERT_TRUE(table.put(funcBptr, &funcBptrKey, funcBptrIndex));
   ASSERT_TRUE(table.put(intA, &intAKey, intAIndex));
   ASSERT_TRUE(table.put(intB, &intBKey, intBIndex));
   ASSERT_TRUE(table.put(shortA, &shortAKey, shortAIndex));
   ASSERT_TRUE(table.put(shortB, &shortBKey, shortBIndex));
   ASSERT_TRUE(table.put(floatA, &floatAKey, floatAIndex));
   ASSERT_TRUE(table.put(floatB, &floatBKey, floatBIndex));
   ASSERT_TRUE(table.put(doubleA, &doubleAKey, doubleAIndex));
   ASSERT_TRUE(table.put(doubleB, &doubleBKey, doubleBIndex));

   ASSERT_NE(classAptrIndex, INVALID_INDEX);
   ASSERT_NE(classBptrIndex, INVALID_INDEX);
   ASSERT_NE(funcAptrIndex, INVALID_INDEX);
   ASSERT_NE(funcBptrIndex, INVALID_INDEX);
   ASSERT_NE(intAIndex, INVALID_INDEX);
   ASSERT_NE(intBIndex, INVALID_INDEX);
   ASSERT_NE(shortAIndex, INVALID_INDEX);
   ASSERT_NE(shortBIndex, INVALID_INDEX);
   ASSERT_NE(floatAIndex, INVALID_INDEX);
   ASSERT_NE(floatBIndex, INVALID_INDEX);
   ASSERT_NE(doubleAIndex, INVALID_INDEX);
   ASSERT_NE(doubleBIndex, INVALID_INDEX);

   ASSERT_TRUE(table.find(CCData::key(classAptr)));
   ASSERT_TRUE(table.find(CCData::key(classBptr)));
   ASSERT_TRUE(table.find(CCData::key(funcAptr)));
   ASSERT_TRUE(table.find(CCData::key(funcBptr)));
   ASSERT_TRUE(table.find(CCData::key(intA)));
   ASSERT_TRUE(table.find(CCData::key(intB)));
   ASSERT_TRUE(table.find(CCData::key(shortA)));
   ASSERT_TRUE(table.find(CCData::key(shortB)));
   ASSERT_TRUE(table.find(CCData::key("floatA")));
   ASSERT_TRUE(table.find(CCData::key("floatB")));
   ASSERT_TRUE(table.find(CCData::key("doubleA")));
   ASSERT_TRUE(table.find(CCData::key("doubleB")));

   const void* * const ptr_classAptr = table.get<const void*>(classAptrIndex);
   const void* * const ptr_classBptr = table.get<const void*>(classBptrIndex);
   const void* * const ptr_funcAptr = table.get<const void*>(funcAptrIndex);
   const void* * const ptr_funcBptr = table.get<const void*>(funcBptrIndex);
   const int * const ptr_intA = table.get<const int>(intAIndex);
   const int * const ptr_intB = table.get<const int>(intBIndex);
   const short * const ptr_shortA = table.get<const short>(shortAIndex);
   const short * const ptr_shortB = table.get<const short>(shortBIndex);
   const float * const ptr_floatA = table.get<const float>(floatAIndex);
   const float * const ptr_floatB = table.get<const float>(floatBIndex);
   const double * const ptr_doubleA = table.get<const double>(doubleAIndex);
   const double * const ptr_doubleB = table.get<const double>(doubleBIndex);

   ASSERT_EQ(reinterpret_cast<size_t>(ptr_classAptr) & (OMR_ALIGNOF(*ptr_classAptr) - 1), 0);
   ASSERT_EQ(reinterpret_cast<size_t>(ptr_classBptr) & (OMR_ALIGNOF(*ptr_classBptr) - 1), 0);
   ASSERT_EQ(reinterpret_cast<size_t>(ptr_funcAptr) & (OMR_ALIGNOF(*ptr_funcAptr) - 1), 0);
   ASSERT_EQ(reinterpret_cast<size_t>(ptr_funcBptr) & (OMR_ALIGNOF(*ptr_funcBptr) - 1), 0);
   ASSERT_EQ(reinterpret_cast<size_t>(ptr_intA) & (OMR_ALIGNOF(*ptr_intA) - 1), 0);
   ASSERT_EQ(reinterpret_cast<size_t>(ptr_intB) & (OMR_ALIGNOF(*ptr_intB) - 1), 0);
   ASSERT_EQ(reinterpret_cast<size_t>(ptr_shortA) & (OMR_ALIGNOF(*ptr_shortA) - 1), 0);
   ASSERT_EQ(reinterpret_cast<size_t>(ptr_shortB) & (OMR_ALIGNOF(*ptr_shortB) - 1), 0);
   ASSERT_EQ(reinterpret_cast<size_t>(ptr_floatA) & (OMR_ALIGNOF(*ptr_floatA) - 1), 0);
   ASSERT_EQ(reinterpret_cast<size_t>(ptr_floatB) & (OMR_ALIGNOF(*ptr_floatB) - 1), 0);
   ASSERT_EQ(reinterpret_cast<size_t>(ptr_doubleA) & (OMR_ALIGNOF(*ptr_doubleA) - 1), 0);
   ASSERT_EQ(reinterpret_cast<size_t>(ptr_doubleB) & (OMR_ALIGNOF(*ptr_doubleB) - 1), 0);

   void     *out_classAptr = NULL, *out_classBptr = NULL;
   void     *out_funcAptr = NULL, *out_funcBptr = NULL;
   int      out_intA = ~intA, out_intB = ~intB;
   short    out_shortA = ~shortA, out_shortB = ~shortB;
   float    out_floatA = -floatA, out_floatB = -floatB;
   double   out_doubleA = -doubleA, out_doubleB = -doubleB;

   ASSERT_TRUE(table.get(classAptrIndex, out_classAptr));
   ASSERT_TRUE(table.get(classBptrIndex, out_classBptr));
   ASSERT_TRUE(table.get(funcAptrIndex, out_funcAptr));
   ASSERT_TRUE(table.get(funcBptrIndex, out_funcBptr));
   ASSERT_TRUE(table.get(intAIndex, out_intA));
   ASSERT_TRUE(table.get(intBIndex, out_intB));
   ASSERT_TRUE(table.get(shortAIndex, out_shortA));
   ASSERT_TRUE(table.get(shortBIndex, out_shortB));
   ASSERT_TRUE(table.get(floatAIndex, out_floatA));
   ASSERT_TRUE(table.get(floatBIndex, out_floatB));
   ASSERT_TRUE(table.get(doubleAIndex, out_doubleA));
   ASSERT_TRUE(table.get(doubleBIndex, out_doubleB));

   ASSERT_EQ(classAptr, out_classAptr);
   ASSERT_EQ(classBptr, out_classBptr);
   ASSERT_EQ(funcAptr, out_funcAptr);
   ASSERT_EQ(funcBptr, out_funcBptr);
   ASSERT_EQ(intA, out_intA);
   ASSERT_EQ(intB, out_intB);
   ASSERT_EQ(shortA, out_shortA);
   ASSERT_EQ(shortB, out_shortB);
   ASSERT_EQ(floatA, out_floatA);
   ASSERT_EQ(floatB, out_floatB);
   ASSERT_EQ(doubleA, out_doubleA);
   ASSERT_EQ(doubleB, out_doubleB);

   ASSERT_EQ(CCData::key(classAptr), CCData::key(out_classAptr));
   ASSERT_EQ(CCData::key(classBptr), CCData::key(out_classBptr));
   ASSERT_EQ(CCData::key(funcAptr), CCData::key(out_funcAptr));
   ASSERT_EQ(CCData::key(funcBptr), CCData::key(out_funcBptr));
   ASSERT_EQ(CCData::key(intA), CCData::key(out_intA));
   ASSERT_EQ(CCData::key(intB), CCData::key(out_intB));
   ASSERT_EQ(CCData::key(shortA), CCData::key(out_shortA));
   ASSERT_EQ(CCData::key(shortB), CCData::key(out_shortB));

   CCData::index_t find_classAptrIndex = INVALID_INDEX;
   CCData::index_t find_classBptrIndex = INVALID_INDEX;
   CCData::index_t find_funcAptrIndex = INVALID_INDEX;
   CCData::index_t find_funcBptrIndex = INVALID_INDEX;
   CCData::index_t find_intAIndex = INVALID_INDEX;
   CCData::index_t find_intBIndex = INVALID_INDEX;
   CCData::index_t find_shortAIndex = INVALID_INDEX;
   CCData::index_t find_shortBIndex = INVALID_INDEX;
   CCData::index_t find_floatAIndex = INVALID_INDEX;
   CCData::index_t find_floatBIndex = INVALID_INDEX;
   CCData::index_t find_doubleAIndex = INVALID_INDEX;
   CCData::index_t find_doubleBIndex = INVALID_INDEX;

   ASSERT_TRUE(table.find(CCData::key(classAptr), &find_classAptrIndex));
   ASSERT_TRUE(table.find(CCData::key(classBptr), &find_classBptrIndex));
   ASSERT_TRUE(table.find(CCData::key(funcAptr), &find_funcAptrIndex));
   ASSERT_TRUE(table.find(CCData::key(funcBptr), &find_funcBptrIndex));
   ASSERT_TRUE(table.find(CCData::key(intA), &find_intAIndex));
   ASSERT_TRUE(table.find(CCData::key(intB), &find_intBIndex));
   ASSERT_TRUE(table.find(CCData::key(shortA), &find_shortAIndex));
   ASSERT_TRUE(table.find(CCData::key(shortB), &find_shortBIndex));
   ASSERT_TRUE(table.find(CCData::key("floatA"), &find_floatAIndex));
   ASSERT_TRUE(table.find(CCData::key("floatB"), &find_floatBIndex));
   ASSERT_TRUE(table.find(CCData::key("doubleA"), &find_doubleAIndex));
   ASSERT_TRUE(table.find(CCData::key("doubleB"), &find_doubleBIndex));

   ASSERT_EQ(classAptrIndex, find_classAptrIndex);
   ASSERT_EQ(classBptrIndex, find_classBptrIndex);
   ASSERT_EQ(funcAptrIndex, find_funcAptrIndex);
   ASSERT_EQ(funcBptrIndex, find_funcBptrIndex);
   ASSERT_EQ(intAIndex, find_intAIndex);
   ASSERT_EQ(intBIndex, find_intBIndex);
   ASSERT_EQ(shortAIndex, find_shortAIndex);
   ASSERT_EQ(shortBIndex, find_shortBIndex);
   ASSERT_EQ(floatAIndex, find_floatAIndex);
   ASSERT_EQ(floatBIndex, find_floatBIndex);
   ASSERT_EQ(doubleAIndex, find_doubleAIndex);
   ASSERT_EQ(doubleBIndex, find_doubleBIndex);
   }
