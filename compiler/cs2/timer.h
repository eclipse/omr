/*******************************************************************************
 * Copyright IBM Corp. and others 1996
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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

/***************************************************************************/
/*                                                                         */
/*  File name: timer.h                                                     */
/*  Purpose:   General purpose timer and meters for Compiler phase metering*/
/*                                                                         */
/***************************************************************************/

#ifndef CS2TIMER_H
#define CS2TIMER_H
#define IN_CS2TIMER_H

#include <stdio.h>

#include "cs2/allocator.h"
#include "cs2/listof.h"
#include "cs2/hashtab.h"

#if defined(OMR_OS_WINDOWS)
#include <time.h>
#include "windows_api.h"
#else
#include <sys/time.h>
#endif /* defined(OMR_OS_WINDOWS) */


#ifdef CS2_ALLOCINFO
#define allocate(x) allocate(x, __FILE__, __LINE__)
#define deallocate(x,y) deallocate(x, y, __FILE__, __LINE__)
#define reallocate(x,y,z) reallocate(x, y, z, __FILE__, __LINE__)
#endif

namespace CS2 {

#if defined(_AIX)

/**
 * \brief AIX-specific timer class.
 *
 *
 * Timer value is returned in microseconds. Uses AIX mechanisms that
 * access the time base register, with <<1ms granularity
 */
class AIXTimer {
 public:
  uint64_t Read(void) const {
    time_base_to_time(&start, TIMEBASE_SZ);
    time_base_to_time(&stop, TIMEBASE_SZ);

    uint64_t start_t = uint64_t(start.tb_high)*1000000 + start.tb_low/1000;
    uint64_t stop_t  = uint64_t(stop .tb_high)*1000000 + stop .tb_low/1000;

    if (stop_t <= start_t) return 0;
    return stop_t - start_t;
  }
  void Start(void) { read_real_time(&start, TIMEBASE_SZ);}
  void Stop(void)  { read_real_time(&stop, TIMEBASE_SZ); }
  void Reset(void) { memset (&start,0,TIMEBASE_SZ); memset (&stop,0,TIMEBASE_SZ); }
 private:
  mutable timebasestruct_t start, stop;
};

typedef AIXTimer PlatformTimer;
#elif defined(LINUX) || defined(OSX) || defined (__MVS__)
/**
 * \brief Linux-specific timer class.
 *
 * Timer value is returned in microseconds. Uses Linux/BSD mechanism that
 * has microsecond granularity.
 */
class BSDTimer {
public:
  uint64_t Read() const {
    uint64_t start = fStart.tv_usec + uint64_t(fStart.tv_sec) * 1000000;
    uint64_t stop = fStop.tv_usec + uint64_t(fStop.tv_sec) * 1000000;
    if (stop <= start)
      return 0;
    return stop - start;
  }
  void Start() { gettimeofday(&fStart, NULL); }
  void Stop()  { gettimeofday(&fStop, NULL); }
  void Reset() {
    memset(&fStart, 0, sizeof(fStart));
    memset(&fStop, 0, sizeof(fStop));
  }

private:
  struct timeval fStart;
  struct timeval fStop;
};

typedef BSDTimer PlatformTimer;
#elif defined(OMR_OS_WINDOWS)
/**
 * \brief Windows-specific timer class.
 *
 * Timer value is returned in microseconds but the actual granularity
 * is the reciprocal of the number returned by QueryPerformanceFrequency call.
 */
class WindowsTimer {
public:
  uint64_t Read() const {
    uint64_t start = fStart.QuadPart;
    uint64_t stop = fStop.QuadPart;
    if (stop <= start)
      return 0;
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return static_cast<uint64_t>(double(stop - start)/freq.QuadPart*1000000);
  }
  void Start() { QueryPerformanceCounter(&fStart); }
  void Stop()  { QueryPerformanceCounter(&fStop); }
  void Reset() {
    memset(&fStart, 0, sizeof(fStart));
    memset(&fStop, 0, sizeof(fStop));
  }

private:
  LARGE_INTEGER fStart;
  LARGE_INTEGER fStop;
};

typedef WindowsTimer PlatformTimer;
#else

/**
 * \brief A portable timer class.
 *
 * A general purpose timer class that can be started, stopped and reset. This
 * class is not threadsafe.
 *
 * Timer value is returned in microseconds. Actual granularity of this
 * implementation is 1-second. Architecture-specific implementations may
 * have better granularity.
 */
class BaseTimer {
 public:
  // Return timer value in microseconds
  uint64_t Read(void) const { return uint64_t(t)*1000000;}

  void Start(void) { time_t tt; time(&tt); t=tt;}
  void Stop(void)  { time_t tt; time(&tt); if (tt>=t) t=tt-t; else t=0;}
  void Reset(void) { t = 0;}
 private:
  time_t t;
};

typedef BaseTimer PlatformTimer;
#endif /* defined(_AIX) */

/**
 * \brief A general purpose timer class.
 *
 * A general purpose timer class that can be started, stopped and reset. This
 * class is not threadsafe.
 *
 * This class was originally tightly coupled with the -qdebug=timing feature
 * for timing optimization phases and included methods for dumping the timing
 * summary. It has been split into two classes, Timer and PhaseMeasuringSummary.
 */
class Timer : private PlatformTimer
{
public:
  typedef uint64_t Metric;

  /**
   * \brief Starts the timer from the currently stored time. Has no effect if
   * the timer is already running.
   */
  void Start(void) {
    PlatformTimer::Start();
  }

  /**
   * \brief Stops the timer. The time counted so far is kept and resuming the
   * timer with Start() will resume counting from this point. Call Reset() to
   * reset the count to 0.
   */
  void Stop(void) {
    PlatformTimer::Stop();
  }

  /**
   * \brief Resets the timer to a count of 0. Stops the timer if it was running.
   */
  void Reset(void) {
    PlatformTimer::Reset();
  }

  /**
   * \brief Returns the value of this timer (in microseconds)
   */
  Metric Read(void) const {
    return PlatformTimer::Read();
  }

  static const char* Name(bool csv = false) {
    if (csv)
      return "Timing";
    else
      return "Timings (elapsed wall time)";
  }

  // must be 13 chars or less, alternativeFormat is in seconds/millisecs
  static const char *UnitsText(bool alternativeFormat = false) {
    if (alternativeFormat)
      return "  ssssss.msec (% total)";
    else
      return "hh:mm:ss.msec (% total)";
  }

  // alternativeFormat is in seconds/millisecs
  static uint32_t sprintfMetric(char *line, Metric value, Metric total, bool alternativeFormat = false, bool csv = false) {
    uint32_t offset = 0;
    if (csv)
      offset = sprintf(line, "%.4f", (double) (value) / 1000000.0);
    else {
      uint64_t usecs = value;

      if (usecs/1000) {
        uint64_t msecs = usecs/1000;
        uint32_t ms    = msecs%1000;

        uint64_t secs  = msecs/1000;
        uint32_t s     = secs%60;

        uint64_t mins  = secs/60;
        uint32_t m     = mins%60;

        uint64_t hours = mins/60;
        uint32_t h     = static_cast<uint32_t>(hours);

        float ratio = total?(float(usecs)/float(total))*100:0;

        if (alternativeFormat) {
          offset += sprintf(line+offset, "%8lld.%03d ",(long long)secs, ms);
        } else {
          if (h)
            offset += sprintf(line+offset, "%0d:%02d:%02d.%03d ",h,m,s,ms);
          else
            offset += sprintf(line+offset, "   %d:%02d.%03d ",m,s,ms);
        }
        if (ratio<0.01 || ratio>99.99)
          offset += sprintf(line+offset, " (%d%%)", int(ratio));
        else
          offset += sprintf(line+offset, " (%.2f%%)", ratio);
      } else
        offset += sprintf(line+offset, "nil");
    }
    return offset;
  }
};

template <class Meter>
class RunnableMeter : public Meter
{
public:

  RunnableMeter(void) : Meter(),
    fIsRunning (false)
  {}

  /**
   * \brief Copy constructs the meter. If the meter passed in is currently
   * running, the copy is performed and the newly constructed meter is set to
   * to be running also.
   */
  RunnableMeter(const RunnableMeter & t) : Meter(),
    fIsRunning (t.fIsRunning)
  {}

  /**
   * \brief Copies the meter. If the meter passed in is currently running,
   * the copy is performed and the meter being set becomes running as well.
   */
  const RunnableMeter & operator= (const RunnableMeter & t) {
    fIsRunning = t.fIsRunning;
    return *this;
  }

  /**
   * \brief Starts the meter from the currently stored metric. Has no effect if
   * the meter is already running.
   */
  void Start(void) {
    if (fIsRunning) return;
    fIsRunning = true;
    Meter::Start();
  }

  /**
   * \brief Stops the meter. The metric counted so far is kept and resuming the
   * meter with Start() will resume counting from this point. Call Reset() to
   * reset the count to 0.
   */
  void Stop(void) {
    if (!fIsRunning) return;
    fIsRunning = false;
    Meter::Stop();
  }

  /**
   * \brief Resets the meter to a count of 0. Stops the meter if it was running.
   */
  void Reset(void) {
    fIsRunning = false;
    Meter::Reset();
  }

  /**
   * \brief Returns true if the meter is running.
   */
  bool IsRunning(void) const {
    return fIsRunning;
  }

private:
  bool fIsRunning;
};

template <class Meter, class Allocator>
  class PhaseMeasuringNode : private Allocator
{
 public:
  typedef typename Meter::Metric Metric;

  PhaseMeasuringNode(const char * name=NULL, const Allocator &a = Allocator()) : Allocator(a),
  fName(NULL), fParent(0), fChildMap(a),
  fTotalMetric(0),fCount(0),fRunning(false) {
    if (name) {
      size_t len = strlen(name);
      fName = (char *)Allocator::allocate(len+1);
      memcpy(fName, name, len+1);
    }
  }

 PhaseMeasuringNode(const PhaseMeasuringNode &node) : Allocator(node) ,
 fName(NULL), fParent(node.fParent), fChildMap(node.fChildMap),
 fTotalMetric(node.fTotalMetric),fCount(node.fCount),fRunning(node.fRunning) {
    if (node.fName) {
      size_t len = strlen(node.fName);
      fName = (char *)Allocator::allocate(len+1);
      memcpy(fName, node.fName, len+1);
    }
  }

 ~PhaseMeasuringNode() {
   if (fName) {
     Allocator::deallocate(fName, strlen(fName)+1);
   }
 }

 const Allocator& allocator() const { return *this; }

 void SetName(char *name) { fName = name;}
 char *Name() const { return fName;}

 void SetParent(ListIndex parent) {fParent = parent;}
 ListIndex Parent() const {return fParent;}

 void Start(void) {
   fCount+=1;
   CS2Assert(!fRunning,
             ("Starting meter already running: %s",
              Name()));
   fRunning=true;
   // printf("%-40.40s ", fName);
   fMeter.Start();
 }

 void Stop(void) {
   // printf("%-40.40s ", fName);
   fMeter.Stop();
   CS2Assert(fRunning,
             ("Starting stopped meter: %s",
              Name()));
   fRunning=false;
   fTotalMetric += fMeter.Read();
 }

 bool IsRunning(void) const { return fRunning;}

 Metric Read(void) const {
   CS2Assert(!fRunning,
             ("Cannot read running meter: %s",
              Name()));
   return fTotalMetric;
 }

 uint32_t sprintfMetric(char *line, Metric total, bool alternativeFormat = false, bool csv = false) const {
   CS2Assert(!fRunning,
             ("Cannot read running meter: %s",
              Name()));
   return Meter::sprintfMetric(line, fTotalMetric, total, alternativeFormat, csv);
 }

 ListIndex FindChild(const char *name, HashValue hv=0) const {
   HashIndex hi;
   if (fChildMap.Locate((char *)name, hi, hv))
     return (fChildMap)[hi];
   return 0;
 }

 void AddChild(char *name, ListIndex node) {
   HashIndex hi;
   fChildMap.Add(name, node, hi);
 }

 template <class ostream>
 void Dump(ostream &out, uint32_t indent = 0, Metric total = 0, bool running = false, bool alternativeFormat = false, bool csv = false);
 private:
 char * fName;
 ListIndex fParent;

 HashTable <char *, uint32_t, Allocator> fChildMap;
 Meter fMeter;
 Metric fTotalMetric;
 uint32_t fCount;
 bool fRunning;
};

/**
 * \brief A singleton for storing phase metering and ultimately outputting them.
 *
 * This class maintains the runtime meter hierarchy. It is
 * thread-safe; it maintains the current meter hierarchy for each
 * threads using the meter.
 */
template <class Meter, class Allocator>
class PhaseMeasuringSummary
{

public:
  typedef typename Meter::Metric Metric;
  // gcc requires fully qualified name on LHS of this typedef to avoid ambiguity
  typedef CS2::PhaseMeasuringNode<Meter, Allocator> PhaseMeasuringNode;

  /**
   * \brief Do nothing constructor.
   */
  PhaseMeasuringSummary(const char * name = "Total elapsed metric", const Allocator &a = Allocator(), bool collect = true) :
    fNodes(a) ,
    fNode(0) ,
    fCollect(collect) {

    if (collect)  {
      PhaseMeasuringNode node(name, fNodes.allocator());
      ListIndex index = fNodes.Add(node);
      fNodes[index].Start();

      SetCurrentMeter(index);
    }
  }

  /**
   * \brief Print the list of phase names and the amount of metric they took to
   * complete. Output is to the supplied stream object.
   */
  template <class ostream>
  void DumpSummary(ostream & s, bool alternativeFormat = false, bool csv = false);

  /**
   * \brief Create a new meter or retrieve an existing one
   */
  ListIndex Find(const char *name, HashValue hv=0) {
    ListIndex node = GetCurrentMeter();
    ListIndex child;
    {
      child = fNodes[node].FindChild(name, hv);
    }
    if (!child) {
      child = fNodes[node].FindChild(name, hv);
      if (!child) { // We really need to check again (or upgrade the lock)
        PhaseMeasuringNode n(name, fNodes.allocator());
        child = fNodes.Add(n);

        fNodes[node].AddChild(fNodes[child].Name(), child);
        fNodes[child].SetParent(node);
      }
    }
    return child;
  }

  /**
   * \brief Start a meter
   */
  void Start(ListIndex child) {
    CS2Assert(fNodes[child].Parent() == GetCurrentMeter(),
              ("Incorrect meter nesting for meters %s %s",
               fNodes[GetCurrentMeter()].Name(),
               fNodes[child].Name()));
    fNodes[child].Start();
    SetCurrentMeter(child);
  }

  ListIndex FindStart(const char *name) {
    ListIndex ret = Find(name);
    Start(ret);
    return ret;
  }

  /**
   * \brief Stops a meter. Adds the metric to the correct entry in the summary
   */
  void Stop(ListIndex child) {
    ListIndex parent;
    fNodes[child].Stop();
    CS2Assert(child == GetCurrentMeter(),
              ("Incorrect meter nesting for meters %s %s",
               fNodes[GetCurrentMeter()].Name(),
               fNodes[child].Name()));
    parent = fNodes[child].Parent();
    SetCurrentMeter(parent);
  }

  /**
   * \brief Mechanism to read/set the current meter index. The main
   * objective of these routines is to set the current meter on worker
   * threads when threading starts.
   *
   * SetCurrentMeter() should only be called with a value previously
   * returned by GetCurrentMeter()
   */

  ListIndex GetCurrentMeter() const {
    return fNode;
  }
  void SetCurrentMeter(ListIndex node) {
    fNode = node;
  }

  bool Collect() { return fCollect;}

  static PhaseMeasuringSummary<Meter, Allocator> fGlobal;
  static PhaseMeasuringSummary<Meter, Allocator> &Global() {
    return fGlobal;
  }
private:

  template <class ostream>
  void DumpSummaryNode(ostream & s, ListIndex root, uint32_t indent, Metric total, bool running=false, bool alternativeFormat = false, bool csv = false);

  ListOf <PhaseMeasuringNode , Allocator>  fNodes;
  ListIndex                                 fNode;
  bool                                      fCollect;
  // Should not be copy constructed or copied.
  PhaseMeasuringSummary(PhaseMeasuringSummary &);
  PhaseMeasuringSummary& operator= (PhaseMeasuringSummary &);
};

template <size_t N>
inline HashValue HashFunction (char  const (&strVal)[N]) {
  int i;
  uint32_t wordValue=0;
  for (i=0; i< N; i++) {
    wordValue = wordValue ^ ( ((uint32_t)strVal[i]) << (((i + i%4)%4) *8));
  }
  return Hash_FNV ((const unsigned char *)&wordValue, sizeof(wordValue));
}

// should get the Meter to format and output this
template < class Meter, class Allocator>
template < class ostream >
inline void PhaseMeasuringNode<Meter, Allocator>::Dump(ostream &out, uint32_t indent, Metric total, bool running, bool alternativeFormat, bool csv)
{
  char line[2048];

  if (csv)
      {
      uint32_t offset = sprintf(line, "%d,\"%s\",", indent, fName);
      offset += sprintfMetric(line+offset, total, alternativeFormat, csv);
      //offset = sprintf(line+offset, "%.4f", (double) (Read()) / 1000000.0);
      offset += sprintf(line+offset, ",%d", fCount);
      out << line << "\n";
      return;
      }

  uint32_t offset=indent;
  if (indent>12)
    offset = sprintf(line,"|%10.10d>",indent);
  else
    if (indent > 0)
      memset(line,'|',indent);

  if (fRunning) {
    running = true;
    Stop();
  }

  offset += sprintf(line+offset, "%-40.40s ", fName);
  offset += sprintfMetric(line+offset, total, alternativeFormat, csv);

  if (offset<72)
    offset += sprintf(line+offset,"%*s",72-offset, "");
  offset += sprintf(line+offset, "|%d", int(fCount));
  if (running)
    offset += sprintf(line+offset, "*");
  out << line << "\n";
}

template <class Meter, class Allocator>
template < class ostream >
inline void PhaseMeasuringSummary<Meter, Allocator>::DumpSummaryNode(ostream & out, ListIndex root, uint32_t indent, Metric total, bool running, bool alternativeFormat, bool csv) {
  fNodes[root].Dump(out, indent, total, running, alternativeFormat, csv);

  uint32_t i,n = fNodes.NumberOfElements();
  for (i=root+1; i<n; i+=1) {
    if (fNodes[i].Parent() == root) {
      DumpSummaryNode(out, i, indent+1, total, false, alternativeFormat, csv);
    }
  }
}

template <class Meter, class Allocator>
template < class ostream >
inline void PhaseMeasuringSummary<Meter, Allocator>::DumpSummary(ostream & out, bool alternativeFormat, bool csv) {
  bool running=fNodes[0].IsRunning();
  bool wasRunning=running;
  if (running) {
    fNodes[0].Stop();
    // Do not report the root meter as running if it the current one
    if (GetCurrentMeter()==0) running = false;
  }
  Metric total = fNodes[0].Read();

  if (csv) {
    out << "Level, Phase, " << Meter::Name(csv) << ", Count" << "\n";
    DumpSummaryNode(out, 0, 0, total, running, alternativeFormat, csv);
  } else {
    char line[256];
    uint32_t offset = 0;
    out << "Summary of Phase " << Meter::Name(csv) << "\n"
        << "========================================================================" << "\n";

    offset = sprintf(line, "Phase                           %s  |count *=active", Meter::UnitsText(alternativeFormat));

    out << line << "\n";
    out << "========================================================================" << "\n";
    DumpSummaryNode(out, 0, 0, total, running, alternativeFormat, csv);
    out << "========================================================================" << "\n";
  }
  if (wasRunning)
    fNodes[0].Start();
}

  /////////////////////////
/**
 * \brief A convenience class for metering the metric spent in an optimization phase.
 * Metering starts when Start() is called. When Stop() is called, the meter is
 * stopped and the metric us submitted to gPhaseMeasuringSummary.
 */
template <class Meter, class Allocator, class PhaseMeasuringSummary = CS2::PhaseMeasuringSummary<Meter, Allocator> >
class PhaseProfiler {
 private:
  ListIndex fIndex;
  PhaseMeasuringSummary &fSummaryMeter;
 public:
  /**
   * \brief Constructs a PhaseProfiler with a NULL phase name.
   */
  PhaseProfiler(const char *name=NULL,
             PhaseMeasuringSummary  &s = PhaseMeasuringSummary::Global() ) :
    fIndex(0), fSummaryMeter(s) {if (name) SetName(name);}

  void SetName(const char *name, HashValue hv=0) {
    if (fSummaryMeter.Collect())
      fIndex = fSummaryMeter.Find(name, hv);
  }

  /**
   * \brief Starts the meter for the specified phase. If the meter is
   * already running, stops the meter and reports for the previous
   * specified phase and restarts for the new phase.
   */
  void Start(void) {
    if (fSummaryMeter.Collect()) fSummaryMeter.Start(fIndex);
  }

  /**
   * \brief Stops the meter and reports the result to
   * gPhaseMeasuringSummary. If the meter has not been started it has no
   * effect.
   */
  void Stop(void) {
    if (fSummaryMeter.Collect()) fSummaryMeter.Stop(fIndex);
  }
};

/**
 * \brief A convenience class for metering the metric spent in a lexical block. Metering
 * starts at construction and ends at destruction. The metric is automatically reported
 * to gPhaseMeasuringSummary.
 */
template <class Meter, class Allocator, class PhaseMeasuringSummary = CS2::PhaseMeasuringSummary<Meter, Allocator> >
class LexicalBlockProfiler : private PhaseProfiler<Meter, Allocator> {
private:
  // Should not be copy constructed or copied.
  LexicalBlockProfiler(LexicalBlockProfiler&);
  LexicalBlockProfiler& operator= (LexicalBlockProfiler&);

 public:
  /**
   * \brief Stores the phase name and starts a meter. Phase name must not be null.
   */
  template <size_t N>
  LexicalBlockProfiler(const char (&phase)[N],
                    PhaseMeasuringSummary &s = PhaseMeasuringSummary::Global()) :
    PhaseProfiler<Meter, Allocator>(phase, s)
  {
    if (s.Collect()) {
      HashValue value = HashFunction(phase);

      PhaseProfiler<Meter, Allocator>::SetName(phase, value);
      PhaseProfiler<Meter, Allocator>::Start();
    }
  }
  LexicalBlockProfiler(const char * phase,
                    PhaseMeasuringSummary &s = PhaseMeasuringSummary::Global()):
    PhaseProfiler<Meter, Allocator>(phase, s)
  {
    if (s.Collect()) {
      PhaseProfiler<Meter, Allocator>::Start();
    }
  }
  LexicalBlockProfiler(const char * phase, uint32_t count,
                    PhaseMeasuringSummary&s = PhaseMeasuringSummary::Global()):
    PhaseProfiler<Meter, Allocator>(NULL, s)
  {
    if (s.Collect()) {
      char pname[1024];
      sprintf(pname, "%s %d", phase, count);
      PhaseProfiler<Meter, Allocator>::SetName(pname);
      PhaseProfiler<Meter, Allocator>::Start();
    }
  }
  LexicalBlockProfiler(const char * phase, const char *name,
                    PhaseMeasuringSummary &s = PhaseMeasuringSummary::Global()):
    PhaseProfiler<Meter, Allocator>(NULL, s)
  {
    if (s.Collect()) {
      char pname[1024];
      sprintf(pname, "%s %s", phase, name);
      PhaseProfiler<Meter, Allocator>::SetName(pname);
      PhaseProfiler<Meter, Allocator>::Start();
    }
  }

  /**
   * \brief Stops the meter and reports the metric for the phase
   */
  ~LexicalBlockProfiler(void) {
    PhaseProfiler<Meter, Allocator>::Stop();
  }
};

typedef RunnableMeter<Timer> RunnableTimer;

// following is equivalent is the closest equivalent to "template aliasing" PhaseMeasuringSummary in C++11:
//
// template <class Allocator = CS2::allocator>
//   using PhaseTimingSummary = PhaseMeasuringSummary<Timer, Allocator>;
// so can use
//   typedef PhaseTimingSummary<Allocator> PhaseTimingSummary;
//
template <class Allocator>
class PhaseTimingSummary : public PhaseMeasuringSummary<RunnableTimer, Allocator>
{
private:
  // Should not be copy constructed or copied.
  PhaseTimingSummary(PhaseTimingSummary &);
  PhaseTimingSummary& operator= (PhaseTimingSummary &);

public:
  PhaseTimingSummary(const char * name = "Total elapsed time", const Allocator &a = Allocator(), bool collect = true) :
    PhaseMeasuringSummary<RunnableTimer, Allocator>(name, a, collect)
  {}
};

// following is equivalent is the closest equivalent to "template aliasing" LexicalBlockProfiler in C++11:
//
// template <class Allocator = CS2::allocator>
//   using LexicalBlockTimer = LexicalBlockProfiler<RunnableTimer , Allocator>;
// so can use
//   typedef LexicalBlockTimer<Allocator> LexicalTimer;
//
template <class Allocator, class PhaseTimingSummary = CS2::PhaseMeasuringSummary<RunnableTimer, Allocator> >
class LexicalBlockTimer : public LexicalBlockProfiler<RunnableTimer, Allocator, PhaseTimingSummary> {
private:
  // Should not be copy constructed or copied.
  LexicalBlockTimer(LexicalBlockTimer&);
  LexicalBlockTimer& operator= (LexicalBlockTimer&);

 public:
  template <size_t N>
  LexicalBlockTimer(const char (&phase)[N],
                    PhaseTimingSummary &s = PhaseTimingSummary::Global()) :
      LexicalBlockProfiler<RunnableTimer, Allocator, PhaseTimingSummary>(phase, s)
  {}

  LexicalBlockTimer(const char * phase,
                    PhaseTimingSummary &s = PhaseTimingSummary::Global()):
      LexicalBlockProfiler<RunnableTimer, Allocator, PhaseTimingSummary>(phase, s)
  {}

  LexicalBlockTimer(const char * phase, uint32_t count,
                    PhaseTimingSummary &s = PhaseTimingSummary::Global()):
      LexicalBlockProfiler<RunnableTimer, Allocator, PhaseTimingSummary>(phase, count, s)
  {}

  LexicalBlockTimer(const char * phase, const char *name,
                    PhaseTimingSummary &s = PhaseTimingSummary::Global()):
      LexicalBlockProfiler<RunnableTimer, Allocator, PhaseTimingSummary>(phase, name, s)
  {}

};

#ifdef CS2_ALLOCINFO
#undef allocate
#undef deallocate
#undef reallocate
#endif

}

#endif
