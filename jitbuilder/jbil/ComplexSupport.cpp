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

#include <assert.h>
#include "complex.hpp"
#include "ComplexSupport.hpp"
#include "DynamicType.hpp"
#include "DynamicOperation.hpp"
#include "LiteralValue.hpp"
#include "Mapper.hpp"
#include "Operation.hpp"
#include "OperationReplacer.hpp"
#include "TextWriter.hpp"
#include "Type.hpp"
#include "TypeDictionary.hpp"
#include "TypeReplacer.hpp"

namespace OMR
{
namespace JitBuilder
{

DynamicType *Complex = NULL;
OperationBuilder *ConstComplexBuilder = NULL;
OperationBuilder *ConjugateBuilder = NULL;

static void
ComplexPrinter(TextWriter *w, const Type *t, void *p)
   {
   assert(t == Complex);
   complex<double> *pd = reinterpret_cast<complex<double> *>(p);
   (*w) << pd->real << "+i" << pd->imag;
   }

static LiteralMapper *
ComplexTypeExploder(TypeDictionary *dict, LiteralValue *value, LiteralMapper *m)
   {
   assert(value->kind() == T_dynamic && value->type() == Complex);
   complex<double> lv = *(reinterpret_cast<complex<double> *>(value->getDynamicTypeValue()));

   if (!m)
      m = new LiteralMapper;
   // if m was passed in, assume it was already cleared

   m->add(LiteralValue::create(dict, lv.real));
   m->add(LiteralValue::create(dict, lv.imag));
 
   return m; // ownership of memory, if allocated here, passes to caller
   }

static bool
ComplexTypeReplacer(OperationReplacer *replacer)
   {
   Operation *op = replacer->operation();
   if (op->action() == aMul)
      {
      Value *left = op->operand(0);
      Value *right = op->operand(1);
      if (left->type() == Complex && right->type() == Complex)
         {
         Builder *b = replacer->builder();
         ValueMapper *leftMapper = replacer->operandMapper(0);
         ValueMapper *rightMapper = replacer->operandMapper(1);

         // cross multiply the elements and add corresponding real, imag parts
         Value * l_imag = leftMapper->next();
         Value * l_real = leftMapper->next();
         Value * r_imag = rightMapper->next();
         Value * r_real = rightMapper->next();

         Value *res_real = b->Sub( b->Mul(l_real, r_real), b->Mul(l_imag, r_imag) );
         Value *res_imag = b->Add( b->Mul(l_real, r_imag), b->Mul(l_imag, r_real) );

         ValueMapper *resultMapper = replacer->resultMapper();
         resultMapper->add(res_imag);
         resultMapper->add(res_real);

         return true;
         }
      }

   return false;
   }

static void
ComplexTypeRegistrar(DynamicType *Complex, TypeDictionary *dict, TypeGraph *graph)
   {
   Type *NoType = dict->NoType;
   Type *Double = dict->Double;

   graph->registerValidOperation(Complex, aAdd, Complex, Complex);
   graph->registerValidOperation(Complex, aAdd, Complex, Double);
   graph->registerValidOperation(Complex, aAdd, Double, Complex);
   graph->registerValidOperation(Complex, aSub, Complex, Complex);
   graph->registerValidOperation(Complex, aSub, Double, Complex);
   graph->registerValidOperation(Complex, aSub, Complex, Double);
   graph->registerValidOperation(Complex, aMul, Complex, Complex);
   graph->registerValidOperation(Complex, aMul, Double, Complex);
   graph->registerValidOperation(Complex, aMul, Complex, Double);
   #if 0 // not needed in ComplexMatMult, haven't been tested
   // probably need a Sqrt operation before we can implement some of these
   graph->registerValidOperation(NoType, aIfCmpGreaterThan,    Complex, Complex);
   graph->registerValidOperation(NoType, aIfCmpLessThan,       Complex, Complex  );
   graph->registerValidOperation(NoType, aIfCmpGreaterOrEqual, Complex, Complex  );
   graph->registerValidOperation(NoType, aIfCmpLessOrEqual,    Complex, Complex  );
   graph->registerValidOperation(NoType, aForLoop, Complex, Complex, Complex);
   graph->registerValidOperation(NoType, aReturn, Complex);
   #endif
   }

static bool
ConstComplexExpander(OperationReplacer *replacer)
   {
   assert(ConstComplexBuilder);
   Operation *op = replacer->operation();
   assert(op->action() == ConstComplexBuilder->action());
   Builder *b = replacer->builder();
   LiteralMapper *lm = replacer->literalMapper();
   ValueMapper *resultMapper = replacer->resultMapper();

   if (lm->current()->type() == b->fb()->dict()->Double)
      {
      resultMapper->add( b->ConstDouble(lm->next()->getDouble()) );
      resultMapper->add( b->ConstDouble(lm->next()->getDouble()) );
      return true;
      }

   return false;
   }

static void
ConstComplexPrinter(TextWriter *w, Operation *op)
   {
   assert(ConstComplexBuilder);
   assert(op->action() == ConstComplexBuilder->action());
   (*w) << op->result() << " = ConstComplex " << op->literal() << w->endl();
   }

static bool
ConjugateExpander(OperationReplacer *replacer)
   {
   assert(ConjugateBuilder);
   Operation *op = replacer->operation();
   assert(op->action() == ConjugateBuilder->action());
   auto explodedTypes = replacer->explodedTypes();
   if (explodedTypes->find(Complex) != explodedTypes->end())
      {
      Builder *b = replacer->builder();
      ValueMapper *m = replacer->operandMapper();
      ValueMapper *resultMapper = replacer->resultMapper();

      Value *v_imag = m->next();
      Value *v_real = m->next();

      resultMapper->add(b->Sub(b->ConstDouble(0.0), v_imag));
      resultMapper->add(v_real);

      return true;
      }

   return false;
   }

static void
ConjugatePrinter(TextWriter *w, Operation *op)
   {
   assert(ConjugateBuilder);
   assert(op->action() == ConjugateBuilder->action());
   (*w) << op->result() << " = Conjugate " << op->operand();
   }

static void
ConjugateRegistrar(TypeDictionary *dict, TypeGraph *graph)
   {
   graph->registerValidOperation(Complex, ConjugateBuilder->action(), Complex);
   }

void initializeComplexSupport(TypeDictionary *dict)
   {
   const size_t size = sizeof(complex<double>) * 8;
   StructType *layout = dict->DefineStruct("Complex::layout", size);
   LiteralValue *realName = LiteralValue::create(dict, std::string("real"));
   dict->DefineField(layout, realName, dict->Double, 8*offsetof(complex<double>, real));
   LiteralValue *imagName = LiteralValue::create(dict, std::string("imag"));
   dict->DefineField(layout, imagName, dict->Double, 8*offsetof(complex<double>, imag));
   dict->CloseStruct(layout);
   Complex = DynamicType::create(dict, "Complex", size, ComplexPrinter, layout, ComplexTypeExploder, ComplexTypeReplacer, ComplexTypeRegistrar);

   assert(ConstComplexBuilder == NULL);
   ConstComplexBuilder = new OperationBuilder();
   ConstComplexBuilder->newAction("ConstComplex")
                      ->setNumResults(1)
                      ->addResultType(Complex)
                      ->setNumLiterals(1)
                      ->setExpander(ConstComplexExpander)
                      ->setPrinter(ConstComplexPrinter);
                      // no registrar needed, though LiteralValues should really be considered...

   assert(ConjugateBuilder == NULL);
   ConjugateBuilder = new OperationBuilder();
   ConjugateBuilder->newAction("Conjugate")
                   ->setNumResults(1)
                   ->addResultType(Complex)
                   ->setNumOperands(1)
                   ->setExpander(ConjugateExpander)
                   ->setPrinter(ConjugatePrinter)
                   ->setRegistrar(ConjugateRegistrar);
   }

} // namespace JitBuilder

} // namespace OMR
