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
 ******************************************************************************/

#include "infra/BitVector.hpp"

#include <stdint.h>                   // for int32_t, uint32_t
#include <stdio.h>                    // for sprintf
#include "compile/Compilation.hpp"    // for Compilation
#include "ras/Debug.hpp"              // for TR_DebugBase

// Number of bits set in a byte containing the index value
//
static int8_t bitsInByte[] =
   {
   0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
   1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
   1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
   2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
   1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
   2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
   2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
   3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
   1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
   2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
   2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
   3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
   2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
   3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
   3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
   4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
   };

int32_t TR_BitVector::elementCount()
   {
   int32_t count = 0;
   for (int32_t i = _firstChunkWithNonZero; i <= _lastChunkWithNonZero; i++)
      {
      if (_chunks[i])
         {
         uint8_t *p = (uint8_t*)&_chunks[i];
         count += bitsInByte[p[0]];
         count += bitsInByte[p[1]];
         count += bitsInByte[p[2]];
         count += bitsInByte[p[3]];
#if BITS_IN_CHUNK == 64
         count += bitsInByte[p[4]];
         count += bitsInByte[p[5]];
         count += bitsInByte[p[6]];
         count += bitsInByte[p[7]];
#endif
         }
      }
   return count;
   }


int32_t TR_BitVector::commonElementCount(TR_BitVector& v2)
   {
   if (v2._lastChunkWithNonZero < _firstChunkWithNonZero || v2._firstChunkWithNonZero > _lastChunkWithNonZero)
      return false; // No intersection
   int32_t low = _firstChunkWithNonZero >= v2._firstChunkWithNonZero ? _firstChunkWithNonZero : v2._firstChunkWithNonZero;
   int32_t high = _lastChunkWithNonZero <= v2._lastChunkWithNonZero ? _lastChunkWithNonZero : v2._lastChunkWithNonZero;
   int32_t count = 0;
   for (int32_t i = low; i <= high; i++)
      {
      chunk_t tmp = _chunks[i] & v2._chunks[i];
      if (tmp)
         {
         uint8_t *p = (uint8_t*)&tmp;
         count += bitsInByte[p[0]];
         count += bitsInByte[p[1]];
         count += bitsInByte[p[2]];
         count += bitsInByte[p[3]];
#if BITS_IN_CHUNK == 64
         count += bitsInByte[p[4]];
         count += bitsInByte[p[5]];
         count += bitsInByte[p[6]];
         count += bitsInByte[p[7]];
#endif
         }
      }
   return count;
   }


bool TR_BitVector::hasMoreThanOneElement()
   {
   if (_firstChunkWithNonZero < _lastChunkWithNonZero)
      return true;
   if (_lastChunkWithNonZero < 0)
      return false;
   int32_t count = 0;
   uint8_t *p = (uint8_t*)&_chunks[_firstChunkWithNonZero];
   count += bitsInByte[p[0]];
   count += bitsInByte[p[1]];
   count += bitsInByte[p[2]];
   count += bitsInByte[p[3]];
#if BITS_IN_CHUNK == 64
   count += bitsInByte[p[4]];
   count += bitsInByte[p[5]];
   count += bitsInByte[p[6]];
   count += bitsInByte[p[7]];
#endif

   return (count > 1);
   }

void TR_BitVector::setChunkSize(int32_t chunkSize)
   {
   if (chunkSize == _numChunks)
      return;

   // This path assumes that _chunks has been initialized
   if (chunkSize == 0)
      {
      if (_chunks && _allocationKind == persistentAlloc)
         jitPersistentFree(_chunks);
      _chunks = NULL;
      _numChunks = 0;
      _firstChunkWithNonZero = 0;
      _lastChunkWithNonZero = -1;
#if BV_SANITY_CHECK
      sanityCheck("setChunkSize");
#endif
      return;
      }

   // This path assumes that _chunks has been initialized
   if (_numChunks > chunkSize)
      {
      // Shrinking the bit vector ... may have lost some bits
      if (_lastChunkWithNonZero < 0)
         _firstChunkWithNonZero = chunkSize;
      else if (_firstChunkWithNonZero >= chunkSize)
         {
         _firstChunkWithNonZero = chunkSize;
         _lastChunkWithNonZero = -1;
         }
      else if (_lastChunkWithNonZero >= chunkSize)
         {
         // Lost some bits at the end
         for (_lastChunkWithNonZero = chunkSize-1; _chunks[_lastChunkWithNonZero] == 0; _lastChunkWithNonZero--)
            ;
         }
      }
   else
      {
      // Growing the bit vector
      if (_lastChunkWithNonZero < 0)
         _firstChunkWithNonZero = chunkSize;
      }

   TR_ASSERT(_growable == growable, "Bit vector is not growable");

   chunk_t *newChunks = (chunk_t*)_trMemory->allocateMemory(chunkSize*sizeof(chunk_t), _allocationKind, TR_Memory::BitVector);
   memset(newChunks, 0, chunkSize*sizeof(chunk_t));
   #ifdef TRACK_TRBITVECTOR_MEMORY
   _memoryUsed += chunkSize*sizeof(chunk_t);
   #endif
   if (_chunks)
      {
      uint32_t chunksToCopy = (chunkSize < _numChunks) ? chunkSize : _numChunks;
      memcpy(newChunks, _chunks, chunksToCopy*sizeof(chunk_t));
      if(_allocationKind == persistentAlloc)
         jitPersistentFree(_chunks);
      }

   _chunks = newChunks;
   _numChunks = chunkSize;
#if BV_SANITY_CHECK
   sanityCheck("setChunkSize");
#endif
   }

// produce a hexadecimal string for this bitvector, high bits first, low last
const char *TR_BitVector::getHexString()
   {
   int32_t chunk_hexlen = (BITS_IN_CHUNK/4);
   char *buf = (char *)_trMemory->allocateMemory(_numChunks*chunk_hexlen + 1, _allocationKind, TR_Memory::BitVector);
   char *pos = buf;
   #ifdef TRACK_TRBITVECTOR_MEMORY
   _memoryUsed += _numChunks*chunk_hexlen + 1;
   #endif
   for (int32_t i = _numChunks-1; i >= 0; i--)
      {
      sprintf(pos, "%0*llX", chunk_hexlen, _chunks[i]);
      pos += chunk_hexlen;
      }
   return buf;
   }

void TR_BitVector::print(TR::Compilation *comp, TR::FILE *file)
   {
   if (comp->getDebug())
      {
      if (file == NULL)
         file = comp->getOutFile();
      comp->getDebug()->print(file, this);
      }
   }

void TR_SingleBitContainer::print(TR::Compilation *comp, TR::FILE *file)
   {
   if (comp->getDebug())
      {
      if (file == NULL)
         file = comp->getOutFile();
      comp->getDebug()->print(file, this);
      }
   }

// Compare this with a bit vector
//
inline bool TR_BitContainer::operator== (TR_BitVector& v2)
   {
   if (_type == bitvector)
      return _bitVector && ((*_bitVector) == v2);
   else
      return (v2.get(_singleBit) && !v2.hasMoreThanOneElement());
   }

bool TR_BitContainer::hasMoreThanOneElement()
   {
   if (_type == bitvector && _bitVector && _bitVector->hasMoreThanOneElement())
      return true;
   return false;
   }

int32_t TR_BitContainer::get(int32_t n)
   {
   if (_type == bitvector && _bitVector)
      return _bitVector->get(n);
   else if (_type == bitvector)
      return 0;
   else
      return (_singleBit == n ? _singleBit : 0);
   }

bool TR_BitContainer::intersects(TR_BitVector& v2)
   {
   if (_type == bitvector && _bitVector)
      return _bitVector->intersects(v2);
   else if (_type == bitvector)
      return false;
   else
      return v2.get(_singleBit);
   }

bool TR_BitContainer::intersects(TR_BitContainer& v2)
   {
   if (v2._type == bitvector && v2._bitVector)
      return intersects(*(v2._bitVector));
   else if (v2._type == bitvector)
      return false;
   else
      {
      // v2 is singleton
      if (_type == bitvector && _bitVector)
         return get(v2._singleBit);
      else if (_type == bitvector)
         return false;
      else
         // both are singletons
         return v2._singleBit == _singleBit;
      }
   }
