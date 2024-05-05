/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef OMR_RSSREPORT_INCL
#define OMR_RSSREPORT_INCL

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef LINUX
#include <unistd.h>
#endif
#include "env/TRMemory.hpp"
#include "infra/Array.hpp"
#include "infra/CriticalSection.hpp"
#include "infra/List.hpp"
#include "env/VerboseLog.hpp"
#include "ras/DebugCounter.hpp"

namespace TR { class Monitor; }

namespace OMR
{

/*
 * RSSItem describes content of some memory given address and size.
 * RSSItem can be added to RSSRegion. It is possible to associate
 * debug counters with each item.
 */
class RSSItem
   {
   public:
   TR_ALLOC(TR_Memory::Inliner);  // TODO: add new type
   enum Type
      {
      unknown,
      header,
      alignment,
      coldBlocks,
      overEstimate,
      OOL,
      snippet
      };

   static const char *itemNames[];

   RSSItem() : _type(unknown), _addr(NULL), _size(0) {}
   RSSItem(Type type, uint8_t *addr, size_t size, TR_PersistentList<TR::DebugCounterAggregation> counters,
              const char* methodSignature = NULL) : _type(type), _addr(addr), _size(size), _counters(counters) {}

   private:
   enum Type _type;
   uint8_t *_addr;
   size_t _size;
   TR_PersistentList<TR::DebugCounterAggregation> _counters;
   friend class RSSRegion;
   friend class RSSReport;
   };

/*
 * RSSRegion represents a region of memory that has starting address, size,
 * name, and RSSItems associated with it. Regions can be added to RSSReport.
 * Region's size can be updatee after it was added to the report.
 * Regions can grow low to high, as well as high to low addresses.
 */
class RSSRegion
   {
   TR_ALLOC(TR_Memory::UnknownType);  // TODO: add new type
public:
   enum Grows
      {
      lowToHigh,
      highToLow
      };

   RSSRegion(const char *name = NULL, uint8_t *start = 0, uint32_t size = 0,
             Grows grows = lowToHigh, size_t pageSize = 4*1024*1024) :
             _name(name), _start(start), _size(size), _grows(grows), _pageSize(pageSize),
             _pageMap(TR::Compiler->persistentMemory()) { }
   /**
    * \brief
    *  Returns region's start
    *
    * \returns
    *  Lowest address for regions that grow low to high and highest addres otherwise
    */
   uint8_t *getStart() {return _start;}

   /**
    * \brief Adds RSSItem to the region. If the item overlaps with any existing items
    *        those items get removed.
    *
    * \param item
    *        Item to add
    * \param thredId
    *        Thread id that adds the item
    * \param methodName
    *        Name of the method that adds the item
    */
   void addRSSItem(RSSItem *item, int32_t threadId, const char* methodName = NULL);

   /**
    * \brief
    *      Adds item to a sorted list
    *
    * \param itemList
    *        List to add to
    * \param item
    *        Item to add
    */
   void addToListSorted(TR_PersistentList<RSSItem> *itemList, RSSItem *item);

   /**
    * \brief
    *        Returns list of RSSItems for the page given address belongs to
    *
    * \param address
    *        Address
    *
    * \return
    *        Items list
    */
   TR_PersistentList<RSSItem> getRSSItemList(uint8_t *address)
         {
         size_t addressPage = (uintptr_t)address / _pageSize;
         size_t startPage = (uintptr_t)_start / _pageSize;
         int32_t offset = static_cast<uint32_t>((_grows == lowToHigh) ?
                                                 addressPage - startPage : startPage - addressPage);

         TR_ASSERT_FATAL (offset >= 0, "Offset should be >= 0\n");
         return _pageMap[offset];
         }

   const char* _name;
   uint8_t *_start;
   size_t _size;
   Grows _grows;
   size_t _pageSize;
   TR_PersistentArray<TR_PersistentList<RSSItem>> _pageMap;
   };

/*
 * A singleton class that maintains a global list of RSSRegions.
 * Prints either a summary of RSS status for each RSSRegion
 * or a detailed report per page.
 *
 * Note: using this class is relatively expensive for both time and memory.
 */
class RSSReport
   {
   static RSSReport *_instance; // singleton

   static int32_t const MAX_RSS_LINE = 10000;
   static int32_t const PAGE_PRINT_WIDTH = 2;
   static int32_t const REGION_PRINT_WIDTH = 11;

   public:
   RSSReport(TR_PersistentMemory *pm, bool detailed) : _detailed(detailed), _lastRegion(NULL), _printTitle(true)
               { _instance = this;
                 _mutex = TR::Monitor::create("RSSReport-Monitor");
                 if (!_mutex)
                    _instance = NULL;
               }

   class RSSReportCriticalSection : public CriticalSection
      {
      public:
      RSSReportCriticalSection(RSSReport *rssReport) : CriticalSection(rssReport->_mutex) { }
      };

   /**
    * \brief
    *         Returns single instance of the class
    *
    * \return
    *         Single instance
    */
   static RSSReport *instance() { return _instance; }


   /**
    * \brief Counts number of resident pages in the region
    *
    * \param fd
    *        File descriptor for opened /proc/self/pagemap
    * \param rssRegion
    *        Region
    * \param pagesInfo
    *        String where to print page info
    * \return
    *        Number of resident pages
    */
   size_t countResidentPages(int fd, RSSRegion *rssRegion, char *pagesInfo);

   /**
    * \brief
    *        Prints region names
    */
   void printTitle();

   /**
    * \brief
    *        Prints region summary
    */
   void printRegions();


   /**
    * \brief
    *        Adds RSSRegion to the list
    *
    * \param region
    *        Region to add
    */
   void addRegion(RSSRegion *region)
         {
         _printTitle = true;

         RSSReportCriticalSection addRegionToList(this);
         _lastRegion = _regions.addAfter(region, _lastRegion);
         }

   private:
   bool _detailed;
   bool _printTitle;
   TR_PersistentList<RSSRegion> _regions;
   ListElement<RSSRegion>       *_lastRegion;
   TR::Monitor *_mutex;
   };

} // namespace OMR

#endif // !defined(OMR_RSSREPORT_INCL)
