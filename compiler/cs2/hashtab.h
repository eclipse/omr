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

/***************************************************************************/
/*                                                                         */
/*  File name:  hashtab.h                                                  */
/*  Purpose:    Definition of the HashTable template class.                */
/*                                                                         */
/***************************************************************************/

#include "cs2/allocator.h"
#ifndef CS2_HASHTAB_H
#define CS2_HASHTAB_H

#include "cs2/cs2.h"
#include "cs2/bitmanip.h"

#ifdef CS2_ALLOCINFO
#define allocate(x) allocate(x, __FILE__, __LINE__)
#define deallocate(x,y) deallocate(x, y, __FILE__, __LINE__)
#define reallocate(x,y,z) reallocate(x, y, z, __FILE__, __LINE__)
#endif

namespace CS2 {

// ------------------------------------------------------------------------
// HashTable
//
// An extensible hash table with arbitrary key and data types.  This
// template provides the basic functions of a hash table for given key
// and data types.  A specialization of struct HashInfo can be defined
// to customize the hash table for specific key-types.
// ------------------------------------------------------------------------

typedef uint32_t HashIndex;
typedef uint32_t HashValue; // If this is changed, then the Hash_FNV constants *must* be updated

const uint32_t kMinimumHashTableSize = 16;

#define CS2_HT_TEMPARGS <AKeyType, ADataType, Allocator, AHashInfo>
#define CS2_HT_TEMP template <typename AKeyType, typename ADataType, class Allocator, class AHashInfo>
#define CS2_HT_DECL HashTable <AKeyType, ADataType, Allocator, AHashInfo>
#define CS2_HTC_DECL HashTable <AKeyType, ADataType, Allocator, AHashInfo>::Cursor


/*
   An implementation of the public domain FNV 1a (Fowler/Noll/Vo) 32-bit hash function.
   For information on this hash see the IETF draft:
    Fowler, et al. "The FNV Non-Cryptographic Hash Algorithm"
    Last known URL: http://tools.ietf.org/html/draft-eastlake-fnv-06
*/
const HashValue CS2_FNV_PRIME = 0x01000193u;
const HashValue CS2_FNV_OFFSETBASIS = 0x811C9DC5u;
inline HashValue  Hash_FNV (const unsigned char *s, uint32_t len, HashValue v = CS2_FNV_OFFSETBASIS) {
  for (uint32_t i=0; i<len; i++) {
    v = v ^ s[i];
    v = v * CS2_FNV_PRIME;
  }

  if (v==0) v = (s[0] ^ len)|HashValue(1); // ensure it is non-zero
  return v;
}

/*
  A note on using and/or specializing HashInfo.

  The 'hv' parameter of HashInfo::Hash() is intended to provide a way to iteratively
  build up a hash from complex data that does not lend itself to encapsulation in
  a packed aggregate (ex: from info acquired during a tree traversal).

  Example usage:
  HashValue hv = CS2_DEFAULT_INITIALHASHVALUE
  For each node, N, in a pre-order traversal of an expression tree:
     hv = HashInfo::Hash( N.somefield, hv );
     hv = HashInfo::Hash( N.someotherfield, hv );
     /// ... and so on
  return hv;
 */

// The value here should match what's expected by the default hash function used in HashInfo.
const HashValue CS2_DEFAULT_INITIALHASHVALUE = CS2_FNV_OFFSETBASIS;

template <class AKeyType>
struct HashInfo {
  static HashValue  Hash  (const AKeyType &v1, const HashValue hv=CS2_DEFAULT_INITIALHASHVALUE) {
       return Hash_FNV((const unsigned char *)&v1, sizeof(v1), hv);
  }
  static bool       Equal (const AKeyType &v1, const AKeyType &v2) { return v1==v2;}
};

template <>
struct HashInfo<const char *> {
  static HashValue  Hash (const char * const &v1, const HashValue hv = CS2_FNV_OFFSETBASIS) {
    uint32_t i;
    const unsigned char *lv1 = (const unsigned char *)(v1);
    for (i=0; lv1[i]; i++) ;
    return Hash_FNV (lv1, i, hv);
  }

  static bool       Equal (const char * const &v1, const char * const &v2) {
    const char *lv1(v1), *lv2(v2);
    while (*lv1 == *lv2) {
      if (*lv1==0) return true;
      lv1+=1; lv2+=1;
    }
    return false;
  }
};

template <>
  struct HashInfo<char *> : public HashInfo<const char *> { };

template <typename First, typename Second>
struct CompoundHashKey {
  First  fFirst;
  Second fSecond;

  CompoundHashKey(const First &f, const Second &s):fFirst(f),fSecond(s){}
};

template <typename First, typename Second>
struct HashInfo<CompoundHashKey<First, Second> > {
  typedef CompoundHashKey<First, Second> Key;
  static HashValue  Hash (const Key &v1, const HashValue hv = CS2_DEFAULT_INITIALHASHVALUE) {
    HashValue h1 = HashInfo<First>::Hash(v1.fFirst, hv);
    HashValue h2 = HashInfo<Second>::Hash(v1.fSecond, h1);
    return h2;
  }

  static bool       Equal (const Key &v1, const Key &v2) {
    return HashInfo<First>::Equal(v1.fFirst, v2.fFirst) && HashInfo<Second>::Equal(v1.fSecond, v2.fSecond);
  }
};

template <typename AKeyType, typename ADataType,
          class Allocator,
          class AHashInfo = CS2::HashInfo<AKeyType> >
class HashTable : private Allocator {
 private:
  void init(HashIndex numElements);
  public:

 static  const uint32_t DefaultSize = 0; // for backward compat

  HashTable (uint32_t ignored, const Allocator &a = Allocator()) : Allocator(a),
    fTable(NULL),
    fMask(0),
    fNextFree(0),
    fTableSize(0),
    fHighestIndex(0)
    {
    }

  HashTable (const Allocator &a = Allocator()) : Allocator(a),
    fTable(NULL),
    fMask(0),
    fNextFree(0),
    fTableSize(0),
    fHighestIndex(0)
    {
    }

  ~HashTable();
  HashTable (const CS2_HT_DECL &);

  const Allocator& allocator() const { return *this;}

  CS2_HT_DECL &operator= (const CS2_HT_DECL &);

  // Return the data value at the given index
  const ADataType & operator[] (HashIndex) const;
  ADataType & operator[] (HashIndex);
  const ADataType &DataAt (HashIndex) const;
  ADataType &DataAt (HashIndex);

  // Set the data value at the given index
  void SetDataAt (HashIndex, const ADataType &);

  // Return the key value at the given index
  const AKeyType &KeyAt (HashIndex) const;

  // Return the data value for the given key. It is considered a program
  // error for the key not to exist given a call to this function. An assert
  // will occur in development builds.
  const ADataType& Get(const AKeyType&) const;

  // Locate a record with the given key.  Return true iff a record is
  // found.  If the record is found, set the index reference parameter.
  // If a hash value is given, use it instead of computing a new value.
  bool Locate(const AKeyType&) const;
  bool Locate(const AKeyType&, HashIndex&) const;
  bool Locate(const AKeyType&, HashIndex&, HashValue& hashValue) const;

  // Add a record given a key and data record.  Return true iff the record
  // is successfully added.  If a record with the given key already exists,
  // the add will fail.  If the add succeeds, the index reference parameter
  // is set to the index at which the record was added.  If a hash value is
  // given, it is used instead of recomputing the value based on the key.
  bool Add(const AKeyType&, const ADataType&, HashIndex&, HashValue hashValue = 0, bool noLocate = false);
  bool Add(const AKeyType&, const ADataType&);

  // Remove the record at the given index.
  void Remove (HashIndex);

  // Return the highest allocated index in the hash table
  HashIndex HighestIndex() const;

  // Return the number of bytes of memory used by the hash table
  unsigned long MemoryUsage() const;

  // Check is empty
  bool IsEmpty() const;

  // Clear the hashtab
  void MakeEmpty();

  // For preemptive growth of table.
  void GrowTo(uint32_t);

  // Dump hash table statistics to cout
  template <class str>
    void DumpStatistics(str &out);

  template <class ostr>
  friend ostr &operator << (ostr &out, CS2_HT_DECL &table) {

    typename CS2_HTC_DECL c(table);
    for (c.SetToFirst(); c.Valid(); c.SetToNext())
      out << "[" << table.KeyAt(c) << "]:" << table.DataAt(c) << "\n";
    return out;
  }

  class Cursor {
    public:

    Cursor (const CS2_HT_DECL &);
    Cursor (const Cursor &);
    // ~Cursor();

    // Set the cursor position
    HashIndex SetToFirst();
    HashIndex SetToNext();

    // Determine if the cursor points at a valid position
    bool Valid() const;

    // Convert the cursor to a table index.
    operator HashIndex() const;

    private:

    Cursor &operator= (const Cursor &);
    const CS2_HT_DECL &fTable;
    uint32_t         fIndex;
  };

  friend class Cursor ;

 protected:

  #define CS2_HTE_DECL CS2_HT_DECL::HashTableEntry

  class HashTableEntry {
    public:

    void *operator new (size_t, void *);

    HashTableEntry();
    ~HashTableEntry();
    HashTableEntry (const AKeyType &, const ADataType &, HashValue, HashIndex);
    HashTableEntry (const HashTableEntry &);
    HashTableEntry &operator= (const HashTableEntry &);

    const AKeyType  &Key() const;
    void             SetKey (const AKeyType &);

    ADataType       &Data();
    const ADataType &Data() const;
    void             SetData (const ADataType &);

    HashValue HashCode() const;
    void      SetHashCode (HashValue);

    bool   Valid() const;
    void      Invalidate();

    HashIndex CollisionChain() const;
    void      SetCollisionChain (HashIndex);

    private:

    AKeyType   fKey;
    ADataType  fData;
    HashValue  fHashCode;    // unmasked hash value
    HashIndex  fChain;       // collision chain

  };

  void Grow();
                            // handle growth of internal tables
  void GrowAndRehash( HashIndex, HashTableEntry *, HashIndex, HashIndex);

  HashTableEntry *fTable;
  uint32_t fTableSize;        // Total table size (closed + open)
  uint32_t fMask;             // Mask to compute modulus for closed area
  uint32_t fNextFree;         // Next free slot in the open area
  HashIndex fHighestIndex;  // Highest allocated index

};

// -----------------------------------------------------------------------
// HashTableEntry methods
// -----------------------------------------------------------------------

// HashTableEntry::operator new
//
// new operator that does not allocate storage.

CS2_HT_TEMP inline void *CS2_HTE_DECL::operator new (size_t, void *ptr) {
  return ptr;
}

// HashTableEntry::HashTableEntry
//
// Construct a hash table entry.

CS2_HT_TEMP inline CS2_HTE_DECL::HashTableEntry() :
fHashCode(0) { }

// HashTableEntry::HashTableEntry (...)
//
// Construct a hash table entry from parts.

CS2_HT_TEMP inline CS2_HTE_DECL::HashTableEntry (const AKeyType &key,
                                             const ADataType &data,
                                             HashValue value, HashIndex chain) :
fKey(key), fData(data), fHashCode(value), fChain(chain) { }

// HashTableEntry::~HashTableEntry
//
// Destroy a hash table entry.

CS2_HT_TEMP inline CS2_HTE_DECL::~HashTableEntry() {
  Invalidate();
}

// HashTableEntry::HashTableEntry (const HashTableEntry &)
//
// Copy construct a hash table entry.

CS2_HT_TEMP inline CS2_HTE_DECL::HashTableEntry (const typename CS2_HTE_DECL &entry) :
fKey(entry.fKey),
fData(entry.fData),
fHashCode(entry.fHashCode),
fChain(entry.fChain) { }

// HashTableEntry::operator=
//
// Assign a hash table entry to another.

CS2_HT_TEMP inline typename CS2_HTE_DECL &CS2_HTE_DECL::operator= (const typename CS2_HTE_DECL &entry) {
  fKey = entry.fKey;
  fData = entry.fData;
  fHashCode = entry.fHashCode;
  fChain = entry.fChain;

  return *this;
}

// HashTableEntry::Key
//
// Return a handle to the key.

CS2_HT_TEMP inline const AKeyType &CS2_HTE_DECL::Key() const {
  return fKey;
}

// HashTableEntry::SetKey
//
// Set the key value.

CS2_HT_TEMP inline void CS2_HTE_DECL::SetKey (const AKeyType &key) {
  fKey = key;
}

// HashTableEntry::Data
//
// Return a handle to the data.

CS2_HT_TEMP inline const ADataType &CS2_HTE_DECL::Data() const {
  return fData;
}

CS2_HT_TEMP inline ADataType &CS2_HTE_DECL::Data() {
  return fData;
}

// HashTableEntry::SetData
//
// Set the data value.

CS2_HT_TEMP inline void CS2_HTE_DECL::SetData (const ADataType &data) {
  fData = data;
}

// HashTableEntry::HashCode
//
// Return the hash code.

CS2_HT_TEMP inline HashValue CS2_HTE_DECL::HashCode() const {
  return fHashCode;
}

// HashTableEntry::SetHashCode
//
// Set the hash code.

CS2_HT_TEMP inline void CS2_HTE_DECL::SetHashCode (HashValue value) {
  fHashCode = value;
}

// HashTableEntry::Valid
//
// Predicate to determine if the entry is valid.

CS2_HT_TEMP inline bool CS2_HTE_DECL::Valid() const {
  return (fHashCode != 0);
}

// HashTableEntry::Invalidate
//
// Invalidate the entry.

CS2_HT_TEMP inline void CS2_HTE_DECL::Invalidate() {
  fHashCode = 0;
}

// HashTableEntry::CollisionChain
//
// Return the collision chain index.

CS2_HT_TEMP inline HashIndex CS2_HTE_DECL::CollisionChain() const {
  return fChain;
}

// HashTableEntry::SetCollisionChain
//
// Set the collision chain index.

CS2_HT_TEMP inline void CS2_HTE_DECL::SetCollisionChain (HashIndex chainIndex) {
  fChain = chainIndex;
}

// -----------------------------------------------------------------------
// HashTable methods
// -----------------------------------------------------------------------

// HashTable::operator[]
//
// Return a reference to the data at the given index.

CS2_HT_TEMP inline const ADataType &CS2_HT_DECL::operator[] (HashIndex index) const {
  return DataAt(index);
}

CS2_HT_TEMP inline ADataType &CS2_HT_DECL::operator[] (HashIndex index) {
  return DataAt(index);
}

// HashTable::DataAt
//
// Return a reference to the data at the given index.

CS2_HT_TEMP inline const ADataType &CS2_HT_DECL::DataAt (HashIndex index) const {
  CS2Assert (index < fTableSize, ("Hash index %d out of range", index));
  CS2Assert (fTable[index].Valid(), ("Invalid hash table entry"));

  return fTable[index].Data();
}

CS2_HT_TEMP inline ADataType &CS2_HT_DECL::DataAt (HashIndex index) {
  CS2Assert (index < fTableSize, ("Hash index %d out of range", index));
  CS2Assert (fTable[index].Valid(), ("Invalid hash table entry"));

  return fTable[index].Data();
}

// HashTable::SetDataAt
//
// Set the data at the given index.

CS2_HT_TEMP inline void CS2_HT_DECL::SetDataAt (HashIndex index, const ADataType & data) {
  CS2Assert (index < fTableSize, ("Hash index %d out of range", index));
  CS2Assert (fTable[index].Valid(), ("Invalid hash table entry"));

  fTable[index].SetData (data);
}

// HashTable::KeyAt
//
// Return a reference to the key at the given index.

CS2_HT_TEMP inline const AKeyType &CS2_HT_DECL::KeyAt (HashIndex index) const {
  CS2Assert (index < fTableSize, ("Hash index %d out of range", index));
  CS2Assert (fTable[index].Valid(), ("Invalid hash table entry"));

  return fTable[index].Key();
}

// HashTable::Locate
//
// Locate an entry in the hash table.

CS2_HT_TEMP inline bool CS2_HT_DECL::Locate(const AKeyType& key) const
{
  HashIndex tempIdx = 0;
  HashValue tempVal = 0;
  return Locate(key, tempIdx, tempVal);
}

CS2_HT_TEMP inline bool CS2_HT_DECL::Locate(const AKeyType& key, HashIndex& index) const
{
  HashValue tempVal = 0;
  return Locate(key, index, tempVal);
}

CS2_HT_TEMP inline bool CS2_HT_DECL::Add(const AKeyType& key, const ADataType& data)
{
  HashIndex hashIndex;
  return Add(key, data, hashIndex);
}

CS2_HT_TEMP inline const ADataType& CS2_HT_DECL::Get(const AKeyType& key) const
{
  HashIndex hashIndex;
  const bool found = Locate(key, hashIndex);
  CS2Assert(found, ("Key was not found."));
  return DataAt(hashIndex);
}

// HashTable::HighestIndex
//
// Return the highest allocated index in the hash table

CS2_HT_TEMP inline HashIndex CS2_HT_DECL::HighestIndex() const {
  return fHighestIndex;
}

// HashTable::IsEmpty
//
// Predicate to determine if the hash table is empty.

CS2_HT_TEMP inline bool CS2_HT_DECL::IsEmpty() const {
  return (fHighestIndex == 0);
}

// -----------------------------------------------------------------------
// HashTable::Cursor methods
// -----------------------------------------------------------------------

// HashTable::Cursor::Cursor
//
// Construct a hash table cursor.

CS2_HT_TEMP inline CS2_HTC_DECL::Cursor (const CS2_HT_DECL &inputTable) :
fTable(inputTable),
fIndex(0) { }

CS2_HT_TEMP inline CS2_HTC_DECL::Cursor (const typename CS2_HTC_DECL &other) :
fTable(other.fTable),
fIndex(other.fIndex) { }

// HashTable::Cursor::SetToFirst
//
// Set the cursor to point to the first valid entry.

CS2_HT_TEMP inline HashIndex CS2_HTC_DECL::SetToFirst() {
  fIndex = 0;
  return SetToNext();
}

// HashTable::Cursor::SetToNext
//
// Set the cursor to point to the next valid entry.

CS2_HT_TEMP inline HashIndex CS2_HTC_DECL::SetToNext() {
  for (++fIndex; fIndex < fTable.fTableSize; ++fIndex) {
    if (Valid()) return fIndex;
  }

  return (fIndex = 0);
}

// HashTable::Cursor::Valid
//
// Predicate to determine if the entry currently pointed to is valid.

CS2_HT_TEMP inline bool CS2_HTC_DECL::Valid() const {
  return (fIndex != 0) && (fTable.fTable[fIndex].Valid() != 0);
}

// HashTable::Cursor::operator HashIndex
//
// Return the index of the entry currently pointed to.

CS2_HT_TEMP inline CS2_HTC_DECL::operator HashIndex() const {
  return fIndex;
}

// -----------------------------------------------------------------------
// HashTable methods
// -----------------------------------------------------------------------

CS2_HT_TEMP void
inline CS2_HT_DECL::init(HashIndex numElements) {
  HashIndex freeIndex, hashIndex;

  HashIndex const closedAreaSize =
    BitManipulator::CeilingPowerOfTwo(numElements) < kMinimumHashTableSize ?
      kMinimumHashTableSize :
      BitManipulator::CeilingPowerOfTwo(numElements);

  HashIndex const openAreaSize = closedAreaSize / 4;

  HashIndex const newSize = closedAreaSize + openAreaSize;
  HashTableEntry * const newTable = (HashTableEntry *) Allocator::allocate(newSize * sizeof(HashTableEntry));
  if (newTable == NULL)
    SystemResourceError::Memory();

  fTable = newTable;
  fTableSize = newSize;
  fMask = closedAreaSize - 1;
  fNextFree = closedAreaSize + 1;
  fHighestIndex = 0;

  // Invalidate all of the hash table entries.
  for (hashIndex = 0; hashIndex < fNextFree; ++hashIndex) {
    fTable[hashIndex].Invalidate();
  }

  // Initialize the rehash area to link up the free chain
  for (freeIndex = fNextFree;
       freeIndex < fTableSize - 1;
       ++freeIndex) {
    fTable[freeIndex].Invalidate();
    fTable[freeIndex].SetCollisionChain (freeIndex + 1);
  }
  fTable[fTableSize - 1].Invalidate();
  fTable[fTableSize - 1].SetCollisionChain (0);
}

// HashTable::~HashTable
//
// Destroy a hash table.

CS2_HT_TEMP inline CS2_HT_DECL::~HashTable () {
  MakeEmpty();
}

// HashTable::HashTable (const HashTable &)
//
// Copy construct a hash table.

CS2_HT_TEMP
inline CS2_HT_DECL::HashTable (const CS2_HT_DECL &table) :
Allocator(table),
fTable(
  table.fTableSize > 0 ?
    (HashTableEntry *) Allocator::allocate(table.fTableSize * sizeof(HashTableEntry)) :
    NULL),
fTableSize(table.fTableSize),
fMask(table.fMask),
fNextFree(table.fNextFree),
fHighestIndex(table.fHighestIndex) {

   if (fTableSize > 0) {
    for (HashIndex hashIndex = 0; hashIndex < fTableSize; ++hashIndex) {
      HashTableEntry &entry = table.fTable[hashIndex];

      if (entry.Valid()) {
        new (fTable + hashIndex) HashTableEntry (entry);
      } else {
        fTable[hashIndex].Invalidate();
              fTable[hashIndex].SetCollisionChain (entry.CollisionChain());
      }
    }
  }

}

// HashTable::operator=
//
// Assign a hash table to another.

CS2_HT_TEMP
inline CS2_HT_DECL &CS2_HT_DECL::operator= (const CS2_HT_DECL &table) {

  if (fTableSize < table.fTableSize) {
    HashTableEntry *newTable = (HashTableEntry *)Allocator::allocate(table.fTableSize * sizeof(HashTableEntry));
    if (newTable == NULL)
      SystemResourceError::Memory();
    memcpy(newTable, fTable, fTableSize * sizeof(HashTableEntry));
    Allocator::deallocate(fTable, fTableSize * sizeof(HashTableEntry));
    fTable = newTable;
  } else if (fTableSize > table.fTableSize) {
    for (HashIndex hashIndex = table.fTableSize; hashIndex < fTableSize; ++hashIndex) {
      if (fTable[hashIndex].Valid()) {
        // Destroy this entry.
        fTable[hashIndex].~HashTableEntry();
        fTable[hashIndex].Invalidate();
      }
    }
  }

  for (HashIndex hashIndex = 0; hashIndex < table.fTableSize; ++hashIndex) {
    const HashTableEntry &entry = table.fTable[hashIndex];
    bool thisEntryValid;

    thisEntryValid = (hashIndex < fTableSize && fTable[hashIndex].Valid());

    if (entry.Valid()) {
      if (thisEntryValid) {
        fTable[hashIndex] = entry;
      } else {
        new (fTable + hashIndex) HashTableEntry (entry);
      }
    } else {
      if (thisEntryValid) {
        // Destroy this entry.
        fTable[hashIndex].~HashTableEntry();
      }

      fTable[hashIndex].Invalidate();
      fTable[hashIndex].SetCollisionChain (entry.CollisionChain());
    }
  }

  fTableSize = table.fTableSize;
  fMask = table.fMask;
  fNextFree = table.fNextFree;

  return *this;
}

// HashTable::GrowTo(n)
// Grow table to handle at least newSize elements
CS2_HT_TEMP void
inline CS2_HT_DECL::GrowTo(uint32_t newSize) {
  uint32_t    closedAreaSize, openAreaSize;
  HashIndex   oldSize;
  HashTableEntry *oldBase;

  closedAreaSize = BitManipulator::CeilingPowerOfTwo(newSize);
  if (closedAreaSize < kMinimumHashTableSize)
      closedAreaSize = kMinimumHashTableSize;
  openAreaSize = closedAreaSize / 4;

  if((closedAreaSize + openAreaSize) < fTableSize ) return;

  // Record old base and size
  oldBase = fTable;
  oldSize = fTableSize;

  GrowAndRehash(oldSize,oldBase,closedAreaSize,openAreaSize);
}


// HashTable::Grow
//
// Grow the hash table to double the current size and rehash entries in
// the new table.

CS2_HT_TEMP void
inline CS2_HT_DECL::Grow() {
  HashTableEntry *oldBase;
  HashIndex       oldSize, closedAreaSize, openAreaSize,newSize;

  // Record old base and size
  oldBase = fTable;
  oldSize = fTableSize;

  // Calculate the new mask value and table size
  if (oldSize==0) newSize = kMinimumHashTableSize - 1;
  else
    newSize =  (fMask << 1) | 1;     // double the size
  closedAreaSize = newSize  + 1;    // make it even
  openAreaSize = closedAreaSize / 4;

  GrowAndRehash(oldSize,oldBase,closedAreaSize,openAreaSize);
}

// Grow and rehash table
CS2_HT_TEMP void
inline CS2_HT_DECL::GrowAndRehash(HashIndex oldSize,
                              HashTableEntry *oldBase,
                              HashIndex closedAreaSize,
                              HashIndex openAreaSize) {
  HashIndex      freeIndex, oldIndex, hashIndex;
  //DumpStatistics();
  CS2Assert(closedAreaSize,("closedAreaSize not set"));
  CS2Assert(openAreaSize,("openAreaSize is 0"));

  HashIndex const newTableSize = closedAreaSize + openAreaSize;
  HashTableEntry * newTable = (HashTableEntry *) Allocator::allocate(newTableSize * sizeof(HashTableEntry));
  if (newTable == NULL)
    SystemResourceError::Memory();

  fTable = newTable;
  fTableSize = newTableSize;
  fMask = closedAreaSize - 1;
  fNextFree = closedAreaSize + 1;
  fHighestIndex = 0; // Let Add figure out the new highest index

  // Invalidate all of the hash table entries.
  for (hashIndex = 0; hashIndex < fNextFree; ++hashIndex) {
    fTable[hashIndex].Invalidate();
  }

  // Initialize the rehash area to link up the free chain
  for (freeIndex = fNextFree; freeIndex < fTableSize - 1; ++freeIndex) {
    fTable[freeIndex].Invalidate();
    fTable[freeIndex].SetCollisionChain (freeIndex + 1);
  }
  fTable[fTableSize - 1].Invalidate();
  fTable[fTableSize - 1].SetCollisionChain (0);

  // Rehash everything since the hash function is based on the table size.
  for (oldIndex = 0; oldIndex < oldSize; ++oldIndex) {
    if (oldBase[oldIndex].Valid()) {   // this is a valid entry
      bool found;
      HashValue oldHash = oldBase[oldIndex].HashCode();

      // Attempt to locate a position for this entry in the new table.
      found = Locate (oldBase[oldIndex].Key(), hashIndex, oldHash);
      CS2Assert (! found, ("Unable to rehash entry %d", oldIndex));

      // hashIndex points at either an invalid hash table entry or the last
      // entry in a collision chain for this key

      if (fTable[hashIndex].Valid()) {
        fTable[hashIndex].SetCollisionChain(fNextFree);
        CS2Assert (fNextFree != 0 && fNextFree<fTableSize, ("1:Invalid hash index %d %d", (int)hashIndex, (int)fNextFree));
        hashIndex = fNextFree;
        fNextFree = fTable[fNextFree].CollisionChain();
      }

      if (hashIndex > fHighestIndex)
        fHighestIndex = hashIndex;

      // Just bitwise copy the old element to the new.
      memcpy (fTable + hashIndex, oldBase + oldIndex, sizeof(HashTableEntry));
      fTable[hashIndex].SetCollisionChain (0);
    }
  }

  if (oldBase)
    Allocator::deallocate(oldBase, oldSize * sizeof(HashTableEntry));
}

// HashTable::DumpStatistics
//
// Dump hash table statistics
CS2_HT_TEMP
template <class str>
void
inline CS2_HT_DECL::DumpStatistics (str &out) {
  int32_t nentries, partsize, low, high, cnt, tcnt;

  Cursor hi(*this);
  nentries = fTableSize;
  partsize = nentries/5;
  nentries = partsize*5;
  low = 0;
  high = partsize;
  cnt = 0;
  tcnt = 0;
  for (hi.SetToFirst(); hi.Valid(); hi.SetToNext()) {
    if (hi >= high) {
      out << low << " --> " << high << " cnt = " << cnt << "\n";
      tcnt += cnt;
      low = high;
      high = low + partsize;
      cnt = 0;
    }
    ++cnt;
  }
  if (cnt > 0) {
    out << low << " --> " << high << " cnt = " << cnt << "\n";
    tcnt += cnt;
    if (high < nentries) cnt = 0;
  }
  if (tcnt == 0)
    out << "** empty **" << "\n";
  else {
    out << "collisions = " << cnt << " total cnt = " << tcnt;
    out << " percent col = " << (cnt * 100)/tcnt << "\n";
  }
}

// HashTable::Locate
//
// Search the hash table for an entry matching the given key.  Return
// true iff an entry is found.  If an entry is found, also set the hash
// index parameter to the hash table index where the entry is located.

CS2_HT_TEMP bool
inline CS2_HT_DECL::Locate (const AKeyType &key, HashIndex &hashIndex,
                            HashValue &hashValue) const {

  if (fTableSize==0) return false;

  if (hashValue == 0) {
    hashValue = AHashInfo::Hash (key);
    CS2Assert (hashValue != 0, ("Invalid hash value"));
  }

  hashIndex = (hashValue & fMask) + 1;
  CS2Assert (hashIndex != 0 && hashIndex<fTableSize, ("Invalid hash index %d", (int)hashIndex));

  // Search the hash table
  if (! fTable[hashIndex].Valid()) return false;

  while ((fTable[hashIndex].HashCode() != hashValue) ||
         ! AHashInfo::Equal (key, fTable[hashIndex].Key())){

    // Set index to next entry in the collision chain or, if empty, end search.
    if (fTable[hashIndex].CollisionChain() != 0) {
      HashIndex hi2 = fTable[hashIndex].CollisionChain();
      CS2Assert (hi2 != 0 && hi2<fTableSize, ("2:Invalid hash index %d %d %p", (int)hi2, (int)hashIndex, (void *)&fTable[0]));
      hashIndex = hi2;
    } else {
      return false;
    }
  }
  return true;
}

// HashTable::Add
//
// Add the given entry to the hash table.  Return true iff the entry is
// successfully added (ie. the entry did not already exists).  If the
// entry is successfully added, set the hash index parameter to the hash
// table index where the entry is located.  If the entry is not successfully
// added, set the hash index parameter to the entry that matched the given
// key.

CS2_HT_TEMP bool

inline CS2_HT_DECL::Add (const AKeyType &key, const ADataType &data,
              HashIndex &hashIndex, HashValue hashValue, bool noLocate) {
  // Search the table for a record matching this key.
  // If a matching record is found, then fail.

  if (!noLocate && Locate (key, hashIndex, hashValue)) {
    return false;
  }

  // If the hash table is full, then we need to grow.
  if (fNextFree == 0) {
    Grow();

    // The grow routine rehashes everything, so we need to rehash this key.
    bool foundThisRecord = Locate (key, hashIndex, hashValue);
    CS2Assert (! foundThisRecord,
            ("Failed to relocate entry to an empty record\n"));
  }

  // hashIndex points at either an invalid hash table entry or the last
  // entry in a collision chain for this key

  if (fTable[hashIndex].Valid()) {
    fTable[hashIndex].SetCollisionChain(fNextFree);
    hashIndex = fNextFree;
    fNextFree = fTable[fNextFree].CollisionChain();  // maintain free chain
  }

  if (hashIndex > fHighestIndex)
    fHighestIndex = hashIndex;

  new (fTable + hashIndex) HashTableEntry (key, data, hashValue, 0);
  return true;  // record was successfully added
}

// HashTable::Remove
//
// Remove the given entry from the hash table.

CS2_HT_TEMP void
inline CS2_HT_DECL::Remove (HashIndex hashIndex) {
  CS2Assert (fTable[hashIndex].Valid(),
          ("Attempt to remove invalid hash table entry\n"));

  // If the entry is in the rehash area, locate the head of the hash chain and
  // follow it to unlink this entry from the chain.  Then return the rehash area
  // space to the free pool.

  if (hashIndex > (fMask+1)) {
    HashIndex headOfChain = (fTable[hashIndex].HashCode() & fMask) + 1;
    HashIndex collisionIndex;

    for (collisionIndex = headOfChain;
         fTable[collisionIndex].CollisionChain() != hashIndex;
         collisionIndex = fTable[collisionIndex].CollisionChain()) {
      CS2Assert (collisionIndex != 0,
		 ("Cannot find record on expected hash chain\n"));
    }

    fTable[collisionIndex].SetCollisionChain (fTable[hashIndex].CollisionChain());
    fTable[hashIndex].SetCollisionChain (fNextFree);
    fTable[hashIndex].~HashTableEntry();

    fNextFree = hashIndex;
  } else {
    HashIndex collisionChain = fTable[hashIndex].CollisionChain();
    fTable[hashIndex].~HashTableEntry();

    // The entry is in the closed hash table section.
    // If it has any chained entries in the rehash area, then choose one
    // to put in its place.
    if (collisionChain) {
      HashIndex firstCollision = collisionChain;

      memcpy (fTable + hashIndex, fTable + firstCollision,
              sizeof(HashTableEntry));
      fTable[firstCollision].SetCollisionChain (fNextFree);
      fTable[firstCollision].Invalidate();
      fNextFree = firstCollision;
      if (firstCollision > hashIndex) hashIndex = firstCollision;
    }
  }

  // If we are deleting the highest allocated index, then walk backward to
  // determine the next highest index
  if (hashIndex == fHighestIndex) {
    HashIndex maxIndex;

    for (maxIndex = hashIndex - 1; maxIndex > 0; --maxIndex) {
      if (fTable[maxIndex].Valid()) break;
    }

    fHighestIndex = maxIndex;
  }
}

// HashTable::MemoryUsage
//
// Return the number of bytes of memory used by the hash table.

CS2_HT_TEMP unsigned long
inline CS2_HT_DECL::MemoryUsage() const {
  unsigned long sizeInBytes;

  sizeInBytes = sizeof(CS2_HT_DECL);
  sizeInBytes += fTableSize * sizeof(HashTableEntry);

  return sizeInBytes;
}

// HashTable::MakeEmpty
//
// Destroy all valid entries in the table.

CS2_HT_TEMP void
inline CS2_HT_DECL::MakeEmpty() {
  HashIndex hashIndex;

  // Destroy valid entries.
  for (hashIndex = 0; hashIndex < fTableSize; ++hashIndex) {
    if (fTable[hashIndex].Valid())
      fTable[hashIndex].~HashTableEntry();
  }

  if (fTable)
    Allocator::deallocate(fTable, fTableSize * sizeof(HashTableEntry));

  fMask = fNextFree = fTableSize = fHighestIndex = 0;
  fTable = NULL;
}

}

#undef CS2_HT_TEMPARGS
#undef CS2_HT_TEMP
#undef CS2_HT_DECL
#undef CS2_HTC_DECL
#undef CS2_HTE_DECL

#ifdef CS2_ALLOCINFO
#undef allocate
#undef deallocate
#undef reallocate
#endif

#endif
