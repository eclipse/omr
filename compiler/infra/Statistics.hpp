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

#ifndef _STATISTICS_HPP__
#define _STATISTICS_HPP__

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "env/TRMemory.hpp"

//--------------------------------- TR_Stats ---------------------------------
// Class for colecting simple statistics (MIN/MAX/SUM/MEAN/StdDev)
// Basic usage:
//    TR_Stats stat("Stats name"); // declare a variable of type TR_Stats
//    stat.update(val1); // collect some data
//    stat.update(val2);
//    stat.report(stdout); // display statistics
// If needed the user can print the statistics into a string and then use
// whatever application specific method for printing the string
//----------------------------------------------------------------------------
class TR_Stats
   {
   public:
   TR_ALLOC(TR_Memory::TRStats)
   enum {NAME_LEN=31};
   TR_Stats(const char* name)
      {
      strncpy(_name, name, NAME_LEN);
      _name[NAME_LEN] = 0; // just in case name is longer than _name
      reset();
      }
   TR_Stats() // constructor used for arrays of stats
      {
      _name[0] = 0; // empty string
      reset();
      }
   void setName(const char* name)
      {
      strncpy(_name, name, NAME_LEN);
      _name[NAME_LEN] = 0; // just in case name is longer than _name
      }
   const char *getName() const { return _name;}
   void reset() { _sum = _sumsq = 0.0; _samples = 0;}
   void update(double val)
      {
      if (_samples == 0)
         {
         _maxval = _minval = val;
         }
      else
         {
         if (val > _maxval) _maxval = val;
         if (val < _minval) _minval = val;
         }
      _sum   += val;
      _sumsq += val*val;
      _samples++;
      }
   // make a report on the data collected so far and print it to a stream
   void report(FILE *file) const
      {
      fprintf(file, "Statistics on: %s   Num samples = %u SUM=%f\n", _name, samples(), sum());
      if (_samples > 0)
         fprintf(file, "MAX=%f MIN=%f Mean=%f StdDev=%f\n",
            maxVal(), minVal(), mean(), stddev());
      }
   // make a report on the data collected so far and print it to a string buffer
   // The buffer must be allocated by the caller (256 bytes should be enough)
   void report(char *str) const
      {
      int l = sprintf(str, "Statistics on: %s   Num samples = %u SUM=%f\n", _name, samples(), sum());
      if (_samples > 0)
         sprintf(str+l, "MAX=%f MIN=%f Mean=%f StdDev=%f\n",
            maxVal(), minVal(), mean(), stddev());
      }
   unsigned samples() const {return _samples;}
   double sum()       const {return _sum;}
   double maxVal()    const {return _maxval;}
   double minVal()    const {return _minval;}
   double mean()      const {return _sum/_samples;}
   double stddev()    const
      {
      if (_samples > 1)
         {
         double variance = ((_sumsq-_sum*_sum/_samples)/(_samples-1));
         return sqrt(variance);
         }
      return 0.0;
      }

   protected:
   char     _name[NAME_LEN+1]; // used for reporting/debugging
   double   _maxval;
   double   _minval;
   double   _sum;
   double   _sumsq;
   unsigned _samples;
   }; // TR_Stats


//--------------------------------- TR_StatsHisto ----------------------------
// Extension of the Stats class that computes histogrames too
// We did not include this feature in the base class because we wanted a lean
// and mean class with low overhead (histograms are rarely used)
// The Number of bins is given as a template parameter to avoid any
// dynamically allocated memory. Note that there are two more bins in addition
// to the ones specified by the template parameter: the bin that contains
// values smaller that the minBin, and the bin that contains values larger than
// the maxBin value specified in the constructor.
//----------------------------------------------------------------------------
template <int N>
class TR_StatsHisto : public TR_Stats
   {
   public:
   TR_StatsHisto(const char*name, double minBin, double maxBin) : TR_Stats(name)
      {
      for (int i=0; i < N+2; i++)
         _bins[i] = 0;
       if (minBin > maxBin) // maybe it is a typo, reverse minBin and maxBin
         {
          _minBin = maxBin;
          _maxBin = minBin;
          _binIncr = (minBin - maxBin)/N;
         }
       else
         {
         _minBin = minBin;
         _maxBin = maxBin;
         _binIncr = (maxBin - minBin)/N;
         }
      }
   void reset() {TR_Stats::reset(); for (int i=0; i < N+2; i++) _bins[i]=0;}
   void update(double val)
      {
      TR_Stats::update(val);
      if (val < _minBin)
         _bins[0]++;
      else if (val >= _maxBin)
         _bins[N+1]++;
      else
         _bins[(int)((val-_minBin)/_binIncr) + 1]++;
      }
   #define NUM_STARS 40
   void report(FILE *file) const
      {
       TR_Stats::report(file); // report MIN, MAX, AVG etc
       if (_samples == 0)
          return;
      // prepare buffer of "stars"
      char stars[NUM_STARS+1];
      memset(stars, '*', NUM_STARS);
      stars[NUM_STARS] = 0; // end of string
      // print histogram
      fputs("  --Bin--\t--Value--\n", file); // print header
      double val = _minBin - _binIncr;
      for (int i=0; i < N+2; i++)
         {
         double fraction = (double)_bins[i]/_samples;
         if (i == 0)
            fprintf(file, "<%f\t%6.2f%% |", _minBin, 100.0*fraction);
         else
            fprintf(file, " %f\t%6.2f%% |", val, 100.0*fraction);
         int numStars = (int)(NUM_STARS*fraction); // truncation
         stars[numStars] = 0; // move the end of string
         fputs(stars, file);
         stars[numStars] = '*'; // put back the star
         fputc('\n', file);
         val += _binIncr;
         }
      }
   protected:
   unsigned _bins[N+2];
   double   _minBin;  // value of smallest bin
   double   _maxBin;  // value of larges bin
   double   _binIncr; // difference between 2 consecutive bins
   }; // TR_StatsHisto


//----------------------------- TR_StatsInterval -----------------------------

class TR_StatsInterval : public TR_Stats
   {
public:
   TR_StatsInterval(const char *name):TR_Stats(name),_intervalerr(0){}
   void update(double v, double t)   //Updates a statrec
      {
      if (_samples == 0) // This is the first sample
         {
         _lastt  = t;            // Updates of interval statrecs require last
         _lastv  = v;            // two values to compute an interval, so this
         _maxval = 0.0;          // sample only initializes the first of them
         _minval = 0.0;
         _sumwt  = 0.0;
         }
      else // This is not the first sample
         {
         double interval = t - _lastt;         // New interval computed
         if (interval < 0.0) _intervalerr =1;  // Not a valid interval

         _sum += _lastv * interval;
         _sumsq += interval * _lastv * _lastv;
         _sumwt += interval;

         if (v > _maxval) _maxval = v;
         if (v < _minval) _minval = v;
         _lastv = v;     // Remember last value & time to compute next interval
         _lastt = t;
         }
      _samples++;
      }
   void report(FILE *file) const
      {
      if (samples() >= 2)
         TR_Stats::report(file);
      else
         fprintf(file, "Statistics on: %s   Num samples = %u SUM=%f\n", _name, samples(), sum());
      if (_intervalerr)
         fprintf(file,  "Invalid statistics; interval error\n");
      }
   double stddev() const
      {
      double s = 0.0;
      if (_sumwt != 0.0 && _sumwt != 1.0)
         {
         double variance = ((_sumsq-(_sum/_sumwt)*(_sum/_sumwt)*_sumwt)/(_sumwt-1.0));
         if (variance > 0.0)
            s = sqrt(variance);
         }
      return s;
      }
   unsigned intervals() const { return _samples - 1;}
   double mean() const{return _sum/_sumwt;}         /* Returns the mean */
   double lastValue() const {return _lastv;}
   double lastTime() const {return _lastt;}
private:
   double  _sumwt;      // Accumulated sum of weights
   double  _lastt;     //Last interval point entered with update()
   double  _lastv;     // Last interval value entered with update()
   int     _intervalerr; // Nonzero => a negative interval encountered
};

//------------------------------ TR_StatsEvents ---------------------------
// This class is simply used to count events. An event is designated by an
// integer. The number of events to monitor is given by the template parameter N.
// The ID of the first event is given as parameter to the constructor (minEventId).
// The constructor also needs an array of strings which designate the names
// of the events (binNames). Of course, 'binNames' should have at least N entries.
// There is no MIN/MAX/AVG/STDDEV defined
//--------------------------------------------------------------------------
template <int N>
class TR_StatsEvents
   {
   public:
   enum {NAME_LEN=31};
   TR_StatsEvents() {}
   TR_StatsEvents(const char *name, char **binNames, int minEventId)
      {
      init(name, binNames, minEventId);
      }
   // the init method can be used if we want to declare an array of stats
   // that are initialized later (in a for loop)
   void init(const char *name, char **binNames, int minEventId)
      {
      strncpy(_name, name, NAME_LEN);
      _name[NAME_LEN] = 0; // just in case name is longer than _name
      _minEventId = minEventId;
      _binNames   = binNames;
      reset();
      }
   void reset()
      {
      _numInvalidSamples = _numSamples = 0;
      for (int i=0; i < N; i++)
         _bins[i]=0;
      }
   void update(int eventId)
      {
      if (eventId >= _minEventId && eventId < _minEventId + N)
         {
         _bins[eventId-_minEventId]++;
         _numSamples++;
         }
      else
         {
         _numInvalidSamples++;
         }
      }
   #define NUM_STARS 40
   void reportPercentages(FILE *file) const
      {
      // prepare buffer of "stars"
      char stars[NUM_STARS+1];
      memset(stars, '*', NUM_STARS);
      stars[NUM_STARS] = 0; // end of string
      // print histogram
      fprintf(file, "\nHistogram for %s   NumSamples=%d  NumInvalidSamples=%d\n",
               _name, _numSamples, _numInvalidSamples);
      if (_numSamples == 0)
         return;
      fputs("  ---EventName---    --Percentage--\n", file); // print header
      for (int i=0; i < N; i++)
         {
         double fraction = (double)_bins[i]/_numSamples;
         fprintf(file, " %15s    %6.2f%% |", _binNames[i], 100.0*fraction);
         int numStars = (int)(NUM_STARS*fraction); // truncation
         stars[numStars] = 0; // move the end of string
         fputs(stars, file);
         stars[numStars] = '*'; // put back the star
         fputc('\n', file);
         }
      }
   void report(FILE *file) const
      {
      // print histogram
      fprintf(file, "\nHistogram for %s   NumSamples=%d  NumInvalidSamples=%d\n",
               _name, _numSamples, _numInvalidSamples);
      if (_numSamples == 0)
         return;
      fputs("  ---EventName---                --Occurences--\n", file); // print header
      for (int i=0; i < N; i++)
         fprintf(file, "%32s\t%6d\n", _binNames[i], _bins[i]);
      }
   unsigned samples() const {return _numSamples;}
   protected:
   char     _name[NAME_LEN+1];
   int      _bins[N];
   char**   _binNames;
   int      _minEventId;  // value of smallest bin
   int      _numSamples;
   int      _numInvalidSamples;
   };

#endif
