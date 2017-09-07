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

#ifndef VALUENUMBERINFO_INCL
#define VALUENUMBERINFO_INCL

#include <stddef.h>                 // for NULL
#include <stdint.h>                 // for int32_t, uint32_t
#include "compile/Compilation.hpp"  // for Compilation
#include "cs2/hashtab.h"            // for HashValue, etc
#include "env/TRMemory.hpp"         // for TR_Memory, etc
#include "il/Node.hpp"              // for Node, vcount_t
#include "infra/Array.hpp"          // for TR_Array

class TR_UseDefInfo;
namespace TR { class Optimizer; }
namespace TR { class ParameterSymbol; }

class TR_ValueNumberInfo
   {

   public:
  
   static void *operator new(size_t size, TR::Allocator a)
      { return a.allocate(size); }
   static void  operator delete(void *ptr, size_t size)
      { ((TR_ValueNumberInfo*)ptr)->allocator().deallocate(ptr, size); } /* t->allocator() must return the same allocator as used for new */

   /* Virtual destructor is necessary for the above delete operator to work
    * See "Modern C++ Design" section 4.7
    */
   virtual ~TR_ValueNumberInfo() {}     
 

   TR_ValueNumberInfo(TR::Compilation *);
   TR_ValueNumberInfo(TR::Compilation *, TR::Optimizer *, bool requiresGlobals = false, bool prefersGlobals = true, bool noUseDefInfo = false);

   TR::Compilation *comp() { return _compilation; }
   TR::Optimizer *optimizer() { return _optimizer; }

   //Public Interface
   TR::Allocator allocator() { return comp()->allocator(); }
   TR_Memory *    trMemory()      { return comp()->trMemory(); }
   TR_StackMemory trStackMemory() { return trMemory(); }
   TR_HeapMemory  trHeapMemory()  { return trMemory(); }

   bool infoIsValid()            {return _infoIsValid;}
   bool hasGlobalsValueNumbers() {return _hasGlobalsValueNumbers;}
   bool trace()                  {return _trace;}

   bool canShareValueNumber(TR::Node *);

   int32_t getNumberOfNodes() {return _numberOfNodes;}

   int32_t getNumberOfValues() {return _nextValue;}

   /** Shareable nodes have value numbers 1 to N */
   int32_t getNumberOfShareableValues() {return _numberOfShareableValues;}

   /** Parameters have value numbers 1 to N */
   int32_t getNumberOfParmValues() {return _numberOfParms;}

   int32_t getValueNumber(TR::Node *node)
      {
      int32_t index = node->getGlobalIndex();
      if (index >= _numberOfNodes)
         setUniqueValueNumber(node);
      return _valueNumbers.ElementAt(index);
      }

   /** Get the node at the given index */
   TR::Node *getNode(int32_t index)
      {
      if (index >= _numberOfNodes)
         return NULL;
      return _nodes.ElementAt(index);
      }

   /** Get the next node with the same value number (forms a ring) */
   TR::Node *getNext(TR::Node *node)
      {
      int32_t index = node->getGlobalIndex();
      if (index >= _numberOfNodes)
         return node;
      return _nodes.ElementAt(_nextInRing.ElementAt(index));
      }

   /** Set the value number of the node to that of the other node. */
   void setValueNumber(TR::Node *node, TR::Node *other);

   /** Set the value number of the node to a unique value */
   void setUniqueValueNumber(TR::Node *node);

   /** Change all nodes with the same value number as the given node to the new value number */
   void changeValueNumber(TR::Node *node, int32_t newVN);

   /** Clean up information for a node that is about to be removed. */
   void removeNodeInfo(TR::Node *node);

   void printValueNumberInfo(TR::Node *);

   bool congruentNodes(TR::Node * , TR::Node *);

   void growTo(int32_t index);

   protected:
   void   buildValueNumberInfo();

   virtual  void   allocateNonShareableValueNumbers();
   virtual  void   allocateParmValueNumbers();
   virtual  void   allocateShareableValueNumbers();
   virtual  void   allocateValueNumber(TR::Node *, vcount_t);
   virtual  void     allocateValueNumber(TR::Node *);
   virtual  TR::Node *getValueNumberForLoad(TR::Node *node);
   virtual  int32_t  getVN(TR::Node *node) {return _valueNumbers.ElementAt(node->getGlobalIndex());}

   virtual  void  initializeNode(TR::Node *node, int32_t &negativeValueNumber);

   TR::Compilation          *_compilation;
   TR::Optimizer            *_optimizer;
   TR_UseDefInfo            *_useDefInfo;

   CS2::ArrayOf<TR::Node*, TR::Allocator>       _nodes;
   CS2::ArrayOf<int32_t, TR::Allocator>         _valueNumbers;
   CS2::ArrayOf<int32_t, TR::Allocator>         _nextInRing;
   TR::ParameterSymbol                       **_parmSymbols;

   int32_t                    _numberOfNodes;
   int32_t                    _numberOfParms;
   int32_t                    _numberOfShareableValues;
   int32_t                    _nextValue;

   bool                       _infoIsValid;
   bool                       _hasGlobalsValueNumbers;
   bool                       _trace;
   int32_t                    _recursionDepth;

   private:

   struct NodeEntry
      {
      TR_ALLOC(TR_Memory::ValuePropagation)
      NodeEntry *_next;
      TR::Node   *_node;
      };
   struct CollisionEntry
      {
      TR_ALLOC(TR_Memory::ValuePropagation)
      CollisionEntry *_next;
      NodeEntry      *_nodes;
      };
   struct HashTable
      {
      int32_t          _numBuckets;
      CollisionEntry **_buckets;
      };

   int32_t  hash(TR::Node *);


   /** Temporary field, only used during building value number info */
   TR_Array<CollisionEntry*> *_matchingNodes;
   HashTable                  _hashTable;

   };

class TR_HashValueNumberInfo : public TR_ValueNumberInfo
   {
   public:
   TR_HashValueNumberInfo(TR::Compilation *, TR::Optimizer *, bool requiresGlobals = false, bool prefersGlobals = true, bool noUseDefInfo = false);

   class VNHashKey
      {
      public:
      VNHashKey(TR::Node * node, TR_ValueNumberInfo * VN);
      bool operator==(const VNHashKey & v2) const;
      uint32_t _hashVal;
      private:

      void hash(uint32_t data)
	 {
	 const uint32_t k = 16777619;
	 uint32_t   len  = 4;
	 const unsigned char * byte = (const unsigned char *)&data;
	 for (uint32_t i = 0; i < 4; i++)
	    {
	    _hashVal = _hashVal ^ byte[i];
	    _hashVal = _hashVal * k;
	    }
	 _hashVal += _hashVal << 13;
	 _hashVal ^= _hashVal >> 7;
	 _hashVal += _hashVal << 3;
	 _hashVal ^= _hashVal >> 17;
	 _hashVal += _hashVal << 5;
	 }

      TR::Node * _node;
      TR_ValueNumberInfo * _VN;
      };

   struct VNHashFunc
      {
      static CS2::HashValue  Hash(const VNHashKey &v1, const CS2::HashValue hv = CS2::CS2_DEFAULT_INITIALHASHVALUE)
	 {
	 return v1._hashVal;
	 }
      static bool  Equal (const VNHashKey &v1, const VNHashKey &v2)
	 {
         return (v1 == v2);
	 }
      };


   protected:
   void  initializeNode(TR::Node *node, int32_t &negativeValueNumber);
   void     allocateValueNumber(TR::Node *);

   private:
   typedef CS2::HashTable<VNHashKey, int32_t, TR::Allocator, VNHashFunc>  VNHashTable;
   VNHashTable _nodeHash;

   };

#endif
