/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#ifndef TEXTWRITER_INCL
#define TEXTWRITER_INCL


#include <iostream>
#include <iomanip>
#include <vector>
#include <deque>
#include "Visitor.hpp"
#include "Builder.hpp"
#include "Case.hpp"
#include "FunctionBuilder.hpp"
#include "Operation.hpp"
#include "Symbol.hpp"
#include "Type.hpp"
#include "TypeDictionary.hpp"
#include "TypeGraph.hpp"
#include "Value.hpp"

namespace OMR
{

namespace JitBuilder
{

class TextWriter : public Visitor
   {
public:
   TextWriter(FunctionBuilder * fb, std::ostream & os, std::string perIndent)
      : Visitor(fb)
      , _os(os)
      , _perIndent(perIndent)
      , _indent(0)
      {
      os << std::showbase // show the 0x prefix
            << std::internal
            << std::setfill('0'); // fill pointers with 0s
      }

   void print()               { start(); }
   void print(Builder * b)    { start(b); }
   void print(Operation * op) { start(op); }

   friend TextWriter &operator<<(TextWriter &w, const bool v)
      {
      w._os << v;
      return w;
      }
   friend TextWriter &operator<<(TextWriter &w, const int8_t v)
      {
      w._os << v;
      return w;
      }
   friend TextWriter &operator<<(TextWriter &w, const int16_t v)
      {
      w._os << v;
      return w;
      }
   friend TextWriter &operator<<(TextWriter &w, const int32_t v)
      {
      w._os << v;
      return w;
      }
   friend TextWriter &operator<<(TextWriter &w, const int64_t v)
      {
      w._os << v;
      return w;
      }
   friend TextWriter &operator<<(TextWriter &w, const uint64_t v)
      {
      w._os << v;
      return w;
      }
   friend TextWriter &operator<<(TextWriter &w, const size_t v)
      {
      w._os << v;
      return w;
      }
   friend TextWriter &operator<<(TextWriter &w, const void * v)
      {
      w._os << v;
      return w;
      }
   friend TextWriter &operator<<(TextWriter &w, const float v)
      {
      w._os << v;
      return w;
      }
   friend TextWriter &operator<<(TextWriter &w, const double v)
      {
      w._os << v;
      return w;
      }
   //
   // User type handlng
   // BEGIN {

   // } END
   // User type handling
   //

   friend TextWriter &operator<<(TextWriter &w, const std::string s)
      {
      w._os << s;
      return w;
      }
   friend TextWriter &operator<<(TextWriter &w, const char *s)
      {
      w._os << s;
      return w;
      }
   friend TextWriter & operator<<(TextWriter &w, const Builder *b)
      {
      w << "B" << b->id();
      if (b->name().length() > 0)
         w << " \""" \"" << b->name() << "\"";
      return w;
      }
   friend TextWriter &operator<<(TextWriter &w, const Case *c)
      {
      w << c->value() << " : " << c->builder();
      if (c->fallsThrough())
         w << " fallthrough ";
      return w;
      }
   friend TextWriter & operator<< (TextWriter &w, FunctionBuilder *fb)
      {
      return w << "F" << fb->id();
      }
   friend TextWriter & operator<< (TextWriter &w, Operation *op)
      {
      return w << "o" << op->id();
      }
   friend TextWriter & operator<< (TextWriter &w, const ParameterSymbol *param)
      {
      return w << "p" << param->index() << "_" << param->type() << " \"" << param->name() << "\"";
      }
   friend TextWriter & operator<< (TextWriter &w, const Symbol *s)
      {
      return w << "s" << s->id() << "_" << s->type() << " \"" << s->name() << "\"";
      }
   friend TextWriter &operator<<(TextWriter &w, const Type *t)
      {
      return w << "t" << t->id();
      }
   friend TextWriter & operator<<(TextWriter &w, const TypeDictionary *dict)
      {
      return w << "D" << dict->id();
      }
   friend TextWriter & operator<<(TextWriter &w, const TypeGraph *graph)
      {
      return w << "G" << graph->id();
      }
   friend TextWriter &operator<<(TextWriter &w, const Value *v)
      {
      return w << "v" << v->id() << "_" << v->type();
      }

   void writeDictionary(TypeDictionary *types);
   void writeType(Type *type, bool indent=true);
   void writeOperation(Operation *op);

   std::string endl()
      {
      return std::string("\n");
      }

   TextWriter & indent()
      {
      for (int32_t in=0;in < _indent;in++)
         _os << _perIndent;
      return *this;
      }
   void indentIn()
      {
      _indent++;
      }
   void indentOut()
      {
      _indent--;
      }

   protected:

      virtual void visitFunctionBuilderPreOps(FunctionBuilder * fb);
      virtual void visitFunctionBuilderPostOps(FunctionBuilder * fb);
      virtual void visitBuilderPreOps(Builder * b);
      virtual void visitBuilderPostOps(Builder * b);
      virtual void visitOperation(Operation * op);
      virtual void visitEnd();

      void printTypePrefix(Type * type, bool indent=true);
      void printOperationPrefix(Operation * op);

      std::ostream & _os;
      std::string _perIndent;
      int32_t _indent;
   };

// RAII class for indenting log output
class LogIndent
   {
   public:
   LogIndent(TextWriter *log)
      : _log(log)
      {
      if (log)
         log->indentIn();
      }

   ~LogIndent()
      {
      if (_log)
         _log->indentOut();
      }
   private:
   TextWriter *_log;
   };

// This macro can be used to bracket a code region where log output should be indented
#define LOG_INDENT_REGION(log) if (true) { LogIndent __log__indent__var(log);
#define LOG_OUTDENT            }

} // namespace JitBuilder

} // namespace OMR

#endif // defined(TEXTWRITER_INCL)
