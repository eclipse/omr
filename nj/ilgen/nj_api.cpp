/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH
 *Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#include "nj_api.h"

#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/Method.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/CompileMethod.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/NJIlGenerator.hpp"
#include "infra/Cfg.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <stdarg.h>

#define TraceEnabled (injector->comp()->getOption(TR_TraceILGen))
#define TraceIL(m, ...)                                                        \
  {                                                                            \
    if (TraceEnabled) {                                                        \
      traceMsg(injector->comp(), m, ##__VA_ARGS__);                            \
    }                                                                          \
  }


class TR_Memory;

extern "C" bool initializeJit();
extern "C" void shutdownJit();

namespace nj {

struct FunctionBuilder;

struct ResolvedMethodWrapper {
  std::string file_;
  std::string line_;
  std::string name_;
  std::vector<TR::DataType> params_;
  TR::ResolvedMethod resolvedMethod_;

  ResolvedMethodWrapper(const char *fileName, const char *lineNumber,
                        const char *name, std::vector<TR::DataType> &params,
                        TR::DataType returnType, void *entryPoint)
      : file_(fileName), line_(lineNumber), name_(name), params_(params),
        // FIXME
        resolvedMethod_((char *)file_.data(), (char *)line_.data(),
                        (char *)name_.data(), (int32_t)params_.size(),
                        params_.data(), returnType, entryPoint, NULL) {}
};

struct Context {
  Context(): function_id_(0) {}
  FunctionBuilder *newFunctionBuilder(const char *name, JIT_Type return_type,
                                      JIT_ILBuilder ilbuilder, void *userdata);
  FunctionBuilder *newFunctionBuilder(const char *name, JIT_Type return_type,
                                      int argc, const JIT_Type *args,
                                      JIT_ILBuilder ilbuilder, void *userdata);
  void registerFunction(const char *name, TR::DataType return_type,
                        std::vector<TR::DataType> &args, void *ptr);
  TR::ResolvedMethod *getFunction(const char *name);

  /* Functions by name - include both JITed and external functions */
  typedef std::map<std::string, std::shared_ptr<ResolvedMethodWrapper>>
      FunctionMap;
  FunctionMap functions_;
  unsigned int function_id_;	/* For generating names for indirect calls TODO needs to be atomic */
};

static std::mutex s_jitlock;
static volatile int s_ctxcount;

struct ShadowSymInfo {
	ShadowSymInfo *next;
	//TR::SymbolReference *typeSymbol;
	uint64_t typeSymbol;
	TR::DataType type;
	int32_t offset;
	TR::SymbolReference *shadowSymbol;
};

/*
This is a cutdown version of JitBuilder's ILInjector
class.
*/
struct SimpleILInjector : public TR::IlGenerator
{
  SimpleILInjector(FunctionBuilder *function_builder)
      : _currentBlock(nullptr),
        _currentBlockNumber(-1),
        _numBlocks(0),
        _blocks(nullptr),
        function_builder_(function_builder),
		shadow_symbols_(nullptr) {}

  bool injectIL() override; /* override */

  TR::Block *block(int32_t num) override { return _blocks[num]; }
  int32_t numBlocks() const override { return _numBlocks; }
  TR::Block *getCurrentBlock() override
  {
    return _currentBlock;
  }
  void
  allocateBlocks(int32_t num)
  {
    _numBlocks = num;
    _blocks = (TR::Block **)_comp->trMemory()->allocateHeapMemory(num * sizeof(TR::Block *));
  }

  TR::Block *
  newBlock()
  {
    return TR::Block::createEmptyBlock(comp());
  }
  
  TR::SymbolReference *
	  findOrCreateShadowSymRef(/*TR::SymbolReference *typeSymbol, */ uint64_t typeSymbol, TR::DataType type, int32_t offset) {
	  //fprintf(stderr, "Searching for type %lld offset %d\n", typeSymbol, offset);
	  for (ShadowSymInfo *syminfo = shadow_symbols_; syminfo != nullptr; syminfo = syminfo->next) {
		  if (syminfo->typeSymbol == typeSymbol && syminfo->type == type && syminfo->offset == offset) {
			  //fprintf(stderr, "Found shadow %p\n", syminfo->shadowSymbol);
			  return syminfo->shadowSymbol;
		  }
	  }
	  TR::Symbol *sym = TR::Symbol::createShadow(_comp->trHeapMemory(), type, TR::DataType::getSize(type));
	  TR::SymbolReference *symRef = new (_comp->trHeapMemory())
		  TR::SymbolReference(_comp->getSymRefTab(), sym, _comp->getMethodSymbol()->getResolvedMethodIndex(), -1);
	  symRef->setOffset(offset);

	  // conservative aliasing
	  int32_t refNum = symRef->getReferenceNumber();
	  if (type == TR::Address)
		  _comp->getSymRefTab()->aliasBuilder.addressShadowSymRefs().set(refNum);
	  else if (type == TR::Int32)
		  _comp->getSymRefTab()->aliasBuilder.intShadowSymRefs().set(refNum);
	  else
		  _comp->getSymRefTab()->aliasBuilder.nonIntPrimitiveShadowSymRefs().set(refNum);
	  ShadowSymInfo *info = new (_comp->trHeapMemory()) ShadowSymInfo();
	  info->type = type;
	  info->offset = offset;
	  info->shadowSymbol = symRef;
	  info->typeSymbol = typeSymbol;
	  info->next = shadow_symbols_;
	  shadow_symbols_ = info;
	  //fprintf(stderr, "Created shadow %p\n", info->shadowSymbol);
	  return info->shadowSymbol;
  }

  void
  generateToBlock(int32_t b)
  {
    _currentBlockNumber = b;
    _currentBlock = _blocks[b];
  }

  void
  createBlocks(int32_t num)
  {
    allocateBlocks(num);
    for (int32_t b = 0; b < num; b++)
    {
      _blocks[b] = newBlock();
      cfg()->addNode(_blocks[b]);
    }
    cfg()->addEdge(cfg()->getStart(), block(0));
    for (int32_t b = 0; b < num - 1; b++)
      _blocks[b]->getExit()->join(_blocks[b + 1]->getEntry());

    _methodSymbol->setFirstTreeTop(_blocks[0]->getEntry());

    generateToBlock(0);
  }

  TR::Block *_currentBlock;
  int32_t _currentBlockNumber;
  int32_t _numBlocks;

  TR::Block **_blocks;

  FunctionBuilder *function_builder_;   
  ShadowSymInfo* shadow_symbols_;
};

struct FunctionBuilder
{
  Context *context_;
  SimpleILInjector ilgenerator_;
  char name_[128];
  TR::DataTypes return_type_;
  std::vector<TR::DataTypes> args_;
  JIT_ILBuilder ilbuilder_;
  void *userdata_;
  char file_[30];
  char line_[30];

  FunctionBuilder(Context *ctx, const char *name,
                  JIT_Type return_type, int argc, const JIT_Type *args,
                  JIT_ILBuilder ilbuilder, void *userdata)
      : context_(ctx), ilgenerator_(this),
        return_type_((TR::DataTypes)return_type), ilbuilder_(ilbuilder),
        userdata_(userdata)
  {
    strcpy(file_, "file");
    strcpy(line_, "line");
    strncpy(name_, name, sizeof name_);
    name_[sizeof name_ - 1] = 0;
    for (int i = 0; i < argc; i++)
    {
      args_.push_back((TR::DataTypes)args[i]);
    }
  }

  void *compile(int opt_level)
  {
    // construct a `TR::ResolvedMethod` instance from the IL generator and use
    // to compile the method
    auto argIlTypes = std::vector<TR::DataType>(args_.size());
    for (int i = 0; i < args_.size(); i++)
    {
      argIlTypes[i] = args_[i];
    }
    TR::ResolvedMethod resolvedMethod(
        file_, line_, name_, argIlTypes.size(), argIlTypes.data(),
        return_type_, 0, &ilgenerator_);
    TR::IlGeneratorMethodDetails methodDetails(&resolvedMethod);
    int32_t rc = 0;
    auto hotness = opt_level == 0 ? TR_Hotness::noOpt : (opt_level == 1 ? TR_Hotness::warm : TR_Hotness::hot);
    uint8_t *entry_point =
        compileMethodFromDetails(NULL, methodDetails, hotness, rc);
    if (entry_point)
    {
      context_->registerFunction(name_, return_type_, argIlTypes, entry_point);
      return entry_point;
    }
    return nullptr;
  }
};

FunctionBuilder *Context::newFunctionBuilder(const char *name,
                                             JIT_Type return_type,
                                             JIT_ILBuilder ilbuilder,
                                             void *userdata) {
  FunctionBuilder *function_builder = new FunctionBuilder(
      this, name, return_type, 0, NULL, ilbuilder, userdata);
  return function_builder;
}

FunctionBuilder *Context::newFunctionBuilder(const char *name,
                                             JIT_Type return_type, int argc,
                                             const JIT_Type *args,
                                             JIT_ILBuilder ilbuilder,
                                             void *userdata) {
  FunctionBuilder *function_builder = new FunctionBuilder(
      this, name, return_type, argc, args, ilbuilder, userdata);
  return function_builder;
}

void Context::registerFunction(const char *name, TR::DataType return_type,
                               std::vector<TR::DataType> &argIlTypes,
                               void *ptr) {
  std::shared_ptr<ResolvedMethodWrapper> resolvedMethod =
      std::make_shared<ResolvedMethodWrapper>("file", "line", name, argIlTypes,
                                              return_type,
                                              ptr);
  functions_.insert(
      std::pair<std::string, std::shared_ptr<ResolvedMethodWrapper>>(
          std::string(name), resolvedMethod));
}

TR::ResolvedMethod *Context::getFunction(const char *name) {
  auto opcode = functions_.find(name);
  if (opcode == functions_.cend())
    return nullptr;
  return &opcode->second->resolvedMethod_;
}

static inline JIT_ContextRef wrap_context(Context *p) {
  return reinterpret_cast<JIT_ContextRef>(p);
}

static inline Context *unwrap_context(JIT_ContextRef p) {
  return reinterpret_cast<Context *>(p);
}

static inline JIT_ILInjectorRef wrap_ilinjector(SimpleILInjector *p) {
  return reinterpret_cast<JIT_ILInjectorRef>(p);
}

static inline SimpleILInjector *unwrap_ilinjector(JIT_ILInjectorRef p) {
  return reinterpret_cast<SimpleILInjector *>(p);
}

static inline JIT_FunctionBuilderRef wrap_function_builder(FunctionBuilder *p) {
  return reinterpret_cast<JIT_FunctionBuilderRef>(p);
}

static inline FunctionBuilder *
unwrap_function_builder(JIT_FunctionBuilderRef p) {
  return reinterpret_cast<FunctionBuilder *>(p);
}

static inline JIT_BlockRef wrap_block(TR::Block *p) {
  return reinterpret_cast<JIT_BlockRef>(p);
}

static inline TR::Block *unwrap_block(JIT_BlockRef p) {
  return reinterpret_cast<TR::Block *>(p);
}

static inline JIT_NodeRef wrap_node(TR::Node *p) {
  return reinterpret_cast<JIT_NodeRef>(p);
}

static inline TR::Node *unwrap_node(JIT_NodeRef p) {
  return reinterpret_cast<TR::Node *>(p);
}

static inline JIT_TreeTopRef wrap_treetop(TR::TreeTop *p) {
  return reinterpret_cast<JIT_TreeTopRef>(p);
}

static inline TR::TreeTop *unwrap_node(JIT_TreeTopRef p) {
  return reinterpret_cast<TR::TreeTop *>(p);
}

static inline JIT_CFGNodeRef wrap_cfgnode(TR::CFGNode *p) {
  return reinterpret_cast<JIT_CFGNodeRef>(p);
}

static inline TR::CFGNode *unwrap_cfgnode(JIT_CFGNodeRef p) {
  return reinterpret_cast<TR::CFGNode *>(p);
}

static inline JIT_SymbolRef wrap_symbolref(TR::SymbolReference *p) {
  return reinterpret_cast<JIT_SymbolRef>(p);
}

static inline TR::SymbolReference *unwrap_symbolref(JIT_SymbolRef p) {
  return reinterpret_cast<TR::SymbolReference *>(p);
}

bool SimpleILInjector::injectIL() {
  return (function_builder_->ilbuilder_(wrap_ilinjector(this),
                                        function_builder_->userdata_));
}

} // namespace nj

using namespace nj;

extern "C" {

JIT_ContextRef JIT_CreateContext() {
  {
    std::lock_guard<std::mutex> g(s_jitlock);
    if (s_ctxcount == 0)
      if (!initializeJit())
        return nullptr;
    s_ctxcount++;
  }

  Context *context = new Context();

  return wrap_context(context);
}

void JIT_DestroyContext(JIT_ContextRef ctx) {
  Context *context = unwrap_context(ctx);
  if (!context)
    return;
  {
    delete context;
    std::lock_guard<std::mutex> g(s_jitlock);
    s_ctxcount--;
    if (s_ctxcount == 0)
      shutdownJit();
  }
}

void JIT_RegisterFunction(JIT_ContextRef ctx, const char *name,
                          enum JIT_Type return_type, int param_count,
                          const JIT_Type *parameters, void *ptr) {
  Context *context = unwrap_context(ctx);
  auto argIlTypes = std::vector<TR::DataType>(param_count);
  for (int i = 0; i < param_count; i++) {
    argIlTypes[i] = 
		TR::DataType((TR::DataTypes)parameters[i]);
  }
  context->registerFunction(name, TR::DataType((TR::DataTypes)return_type),
                            argIlTypes, ptr);
}

void *JIT_GetFunction(JIT_ContextRef ctx, const char *name) {
  Context *context = unwrap_context(ctx);
  auto resolvedMethod = context->getFunction(name);
  if (resolvedMethod)
    return resolvedMethod->getEntryPoint();
  return nullptr;
}

JIT_FunctionBuilderRef
JIT_CreateFunctionBuilder(JIT_ContextRef ctx, const char *name,
                          JIT_Type return_type, int param_count,
                          const JIT_Type *parameters,
                          JIT_ILBuilder ilbuilder, void *userdata) {
  Context *context = unwrap_context(ctx);
  return wrap_function_builder(context->newFunctionBuilder(
      name, return_type, param_count, parameters, ilbuilder, userdata));
}

void JIT_DestroyFunctionBuilder(JIT_FunctionBuilderRef fb) {
  FunctionBuilder *function_builder = unwrap_function_builder(fb);
  delete function_builder;
}

void *JIT_Compile(JIT_FunctionBuilderRef fb, int opt_level) {
  FunctionBuilder *function_builder = unwrap_function_builder(fb);
  return function_builder->compile(opt_level);
}

void JIT_CreateBlocks(JIT_ILInjectorRef ilinjector, int32_t num) {
  auto injector = unwrap_ilinjector(ilinjector);
  injector->createBlocks(num);
}

void JIT_SetCurrentBlock(JIT_ILInjectorRef ilinjector, int32_t b) {
  auto injector = unwrap_ilinjector(ilinjector);
  TR_ASSERT(b < injector->numBlocks(), "Block number %b is out of range\n", b);
  injector->generateToBlock(b);
}

JIT_BlockRef JIT_GetCurrentBlock(JIT_ILInjectorRef ilinjector) {
  auto injector = unwrap_ilinjector(ilinjector);
  return wrap_block(injector->getCurrentBlock());
}

JIT_BlockRef JIT_GetBlock(JIT_ILInjectorRef ilinjector, int32_t b) {
  auto injector = unwrap_ilinjector(ilinjector);
  return wrap_block(injector->block(b));
}

JIT_Type JIT_GetNodeType(JIT_NodeRef node) {
  auto n1 = unwrap_node(node);
  return (JIT_Type)n1->getDataType().getDataType();
}

JIT_NodeRef JIT_ConstAddress(void *value) {
  return wrap_node(TR::Node::aconst((uintptrj_t)value));
}

JIT_NodeRef JIT_ConstInt32(int32_t i) { return wrap_node(TR::Node::iconst(i)); }

JIT_NodeRef JIT_ConstInt64(int64_t i) { return wrap_node(TR::Node::lconst(i)); }

JIT_NodeRef JIT_ConstInt16(int16_t i) { return wrap_node(TR::Node::sconst(i)); }

JIT_NodeRef JIT_ConstInt8(int8_t i) { return wrap_node(TR::Node::bconst(i)); }

JIT_NodeRef JIT_ConstUInt32(uint32_t i) { return wrap_node(TR::Node::iuconst(i)); }

JIT_NodeRef JIT_ConstUInt64(uint64_t i) { return wrap_node(TR::Node::luconst(i)); }

JIT_NodeRef JIT_ConstUInt8(uint8_t i) { return wrap_node(TR::Node::buconst(i)); }


JIT_NodeRef JIT_ConstFloat(float value) {
  TR::Node *node = TR::Node::create(0, TR::fconst, 0);
  node->setFloat(value);
  return wrap_node(node);
}

JIT_NodeRef JIT_ConstDouble(double value) {
  TR::Node *node = TR::Node::create(0, TR::dconst, 0);
  node->setDouble(value);
  return wrap_node(node);
}

JIT_NodeRef JIT_CreateNode(JIT_NodeOpCode opcode) {
  return wrap_node(TR::Node::create((TR::ILOpCodes)opcode));
}

JIT_NodeRef JIT_CreateNode1C(JIT_NodeOpCode opcode, JIT_NodeRef c1) {
  auto n1 = unwrap_node(c1);
  return wrap_node(TR::Node::create((TR::ILOpCodes)opcode, 1, n1));
}

JIT_NodeRef JIT_CreateNode2C(JIT_NodeOpCode opcode, JIT_NodeRef c1,
                             JIT_NodeRef c2) {
  auto n1 = unwrap_node(c1);
  auto n2 = unwrap_node(c2);
  return wrap_node(TR::Node::create((TR::ILOpCodes)opcode, 2, n1, n2));
}

JIT_NodeRef JIT_CreateNode3C(JIT_NodeOpCode opcode, JIT_NodeRef c1,
                             JIT_NodeRef c2, JIT_NodeRef c3) {
  auto n1 = unwrap_node(c1);
  auto n2 = unwrap_node(c2);
  auto n3 = unwrap_node(c3);
  return wrap_node(TR::Node::create((TR::ILOpCodes)opcode, 3, n1, n2, n3));
}

JIT_TreeTopRef JIT_GenerateTreeTop(JIT_ILInjectorRef ilinjector,
                                   JIT_NodeRef n) {
  auto injector = unwrap_ilinjector(ilinjector);
  auto node = unwrap_node(n);
  return wrap_treetop(injector->genTreeTop(node));
}

void JIT_CFGAddEdge(JIT_ILInjectorRef ilinjector, JIT_CFGNodeRef from,
                    JIT_CFGNodeRef to) {
  auto injector = unwrap_ilinjector(ilinjector);
  auto node1 = unwrap_cfgnode(from);
  auto node2 = unwrap_cfgnode(to);
  injector->cfg()->addEdge(node1, node2);
}

JIT_CFGNodeRef JIT_BlockAsCFGNode(JIT_BlockRef b) {
  auto block = unwrap_block(b);
  return wrap_cfgnode(static_cast<TR::CFGNode *>(block));
}

JIT_CFGNodeRef JIT_GetCFGEnd(JIT_ILInjectorRef ilinjector) {
  auto injector = unwrap_ilinjector(ilinjector);
  return wrap_cfgnode(injector->cfg()->getEnd());
}

JIT_SymbolRef JIT_CreateTemporary(JIT_ILInjectorRef ilinjector, JIT_Type type) {
  auto injector = unwrap_ilinjector(ilinjector);
  auto newSymRef = injector->symRefTab()->createTemporary(
	  injector->methodSymbol(), TR::DataType((TR::DataTypes)type));
  newSymRef->getSymbol()->setNotCollected();
  return wrap_symbolref(newSymRef);
}

JIT_SymbolRef JIT_CreateLocalByteArray(JIT_ILInjectorRef ilinjector,
                                       uint32_t size) {
  auto injector = unwrap_ilinjector(ilinjector);
  auto newSymRef = injector->symRefTab()->createLocalPrimArray(
	  size, injector->methodSymbol(),
	  8 /*apparently 8 means Java bytearray! */);
  newSymRef->setStackAllocatedArrayAccess();
  newSymRef->getSymbol()->setNotCollected();
  return wrap_symbolref(newSymRef);
}

JIT_NodeRef JIT_LoadTemporary(JIT_ILInjectorRef ilinjector,
                              JIT_SymbolRef symbol) {
  auto injector = unwrap_ilinjector(ilinjector);
  return wrap_node(injector->loadTemp(unwrap_symbolref(symbol)));
}

void JIT_StoreToTemporary(JIT_ILInjectorRef ilinjector, JIT_SymbolRef symbol,
                          JIT_NodeRef value) {
  auto injector = unwrap_ilinjector(ilinjector);
  return injector->storeToTemp(unwrap_symbolref(symbol), unwrap_node(value));
}

void JIT_SetAutoAddressTaken(JIT_ILInjectorRef ilinjector,
	JIT_SymbolRef symbol) {
//  NO-OP for now as requires OMR enhancement  
//	auto injector = unwrap_ilinjector(ilinjector);
//	auto symref = unwrap_symbolref(symbol);
//	if (symref->isTemporary(injector->comp())) {
//		injector->comp()->getSymRefTab()->aliasBuilder.addressTakenAutos().set(symref->getReferenceNumber());
//	}
}

JIT_NodeRef JIT_LoadAddress(JIT_ILInjectorRef ilinjector,
                            JIT_SymbolRef symbol) {
  auto injector = unwrap_ilinjector(ilinjector);
  auto symref = unwrap_symbolref(symbol);
  if (symref->getSymbol()->isResolvedMethod()) {
	  /* Special handling of function symbols */
	  auto rsm = symref->getSymbol()->getResolvedMethodSymbol();
	  auto m = rsm->getResolvedMethod();
	  if (m) {
		  void *ep = m->startAddressForJittedMethod();
		  if (ep) {
			  return wrap_node(TR::Node::aconst((uintptrj_t) ep));
		  }
	  }
	  TR_ASSERT(false,
		  "Function Symbol must resolve to a static address");
  }
  return wrap_node(TR::Node::createWithSymRef(TR::loadaddr, 0, symref));
}

/**
 * Given base array address and an offset, return address+offset
 */
static TR::Node *get_array_element_address(SimpleILInjector *injector,
                                           TR::DataType elemType,
                                           TR::Node *baseNode,
                                           TR::Node *indexNode) {
  TR_ASSERT(baseNode->getDataType() == TR::Address,
            "IndexAt must be called with a pointer base");
  TR_ASSERT(elemType != TR::NoType,
            "Cannot use IndexAt with pointer to NoType.");
  TR::ILOpCodes addOp;
  TR::DataType indexType = indexNode->getDataType();
  if (TR::Compiler->target.is64Bit()) {
    if (indexType != TR::Int64) {
      TR::ILOpCodes op =
          TR::DataType::getDataTypeConversion(indexType, TR::Int64);
      indexNode = TR::Node::create(op, 1, indexNode);
    }
    addOp = TR::aladd;
  } else {
    TR::DataType targetType = TR::Int32;
    if (indexType != targetType) {
      TR::ILOpCodes op =
          TR::DataType::getDataTypeConversion(indexType, targetType);
      indexNode = TR::Node::create(op, 1, indexNode);
    }
    addOp = TR::aiadd;
  }
  TR::Node *addrNode = TR::Node::create(addOp, 2, baseNode, indexNode);
  return addrNode;
}

JIT_NodeRef JIT_ArrayLoad(JIT_ILInjectorRef ilinjector, JIT_NodeRef basenode,
                          JIT_NodeRef indexnode, JIT_Type dt) {
  auto injector = unwrap_ilinjector(ilinjector);
  auto base = unwrap_node(basenode);
  auto index = unwrap_node(indexnode);
  auto type = TR::DataType((TR::DataTypes)dt);
  auto array_offset = get_array_element_address(injector, type, base, index);
  auto loadOp = TR::ILOpCode::indirectLoadOpCode(type);
#if 1
  TR::SymbolReference *symRef =
      injector->symRefTab()->findOrCreateArrayShadowSymbolRef(type, base);
  TR::Node *load = TR::Node::createWithSymRef(
      loadOp, 1,
      array_offset, 0, symRef);
#else
  TR::Symbol *sym = TR::Symbol::createShadow(injector->comp()->trHeapMemory(), type, TR::DataType::getSize(type));
  TR::SymbolReference *symRef = new (injector->comp()->trHeapMemory())
	  TR::SymbolReference(injector->comp()->getSymRefTab(), sym, injector->comp()->getMethodSymbol()->getResolvedMethodIndex(), -1);
  
  // conservative aliasing
  int32_t refNum = symRef->getReferenceNumber();
  if (type == TR::Address)
	  injector->comp()->getSymRefTab()->aliasBuilder.addressShadowSymRefs().set(refNum);
  else if (type == TR::Int32)
	  injector->comp()->getSymRefTab()->aliasBuilder.intShadowSymRefs().set(refNum);
  else
	  injector->comp()->getSymRefTab()->aliasBuilder.nonIntPrimitiveShadowSymRefs().set(refNum);
  TR::Node *load = TR::Node::createWithSymRef(
	  loadOp, 1,
	  array_offset, 0, symRef);
#endif
  return wrap_node(load);
}

JIT_NodeRef JIT_ArrayLoadAt(JIT_ILInjectorRef ilinjector, uint64_t symbolId,
                            JIT_NodeRef basenode, int64_t idx, JIT_Type dt) {
  auto injector = unwrap_ilinjector(ilinjector);
  auto base = unwrap_node(basenode);
  auto index = TR::Node::lconst(idx);
  auto type = TR::DataType((TR::DataTypes)dt);
  auto aoffset = get_array_element_address(injector, type, base, index);
  auto loadOp = TR::ILOpCode::indirectLoadOpCode(type);
  TR::SymbolReference *symRef = nullptr;
#if 0
  if (symbolId) {
    symRef = injector->findOrCreateShadowSymRef(symbolId, type, (int32_t)idx);
  }
#endif
  if (!symRef)
    symRef =
        injector->symRefTab()->findOrCreateArrayShadowSymbolRef(type, base);
  TR::Node *load = TR::Node::createWithSymRef(loadOp, 1, aoffset, 0, symRef);
  return wrap_node(load);
}

void JIT_ArrayStore(JIT_ILInjectorRef ilinjector, JIT_NodeRef basenode,
                    JIT_NodeRef indexnode, JIT_NodeRef valuenode) {
  auto injector = unwrap_ilinjector(ilinjector);
  auto base = unwrap_node(basenode);
  auto index = unwrap_node(indexnode);
  auto value = unwrap_node(valuenode);
  auto type = value->getDataType();

  TR::ILOpCodes storeOp =
	  injector->comp()->il.opCodeForIndirectArrayStore(type);
  auto array_offset = get_array_element_address(injector, type, base, index);
#if 1
  TR::SymbolReference *symRef =
      injector->symRefTab()->findOrCreateArrayShadowSymbolRef(type, base);
  TR::Node *store = TR::Node::createWithSymRef(
      storeOp, 2, array_offset, value,
      0, symRef);
#else
  TR::Symbol *sym = TR::Symbol::createShadow(injector->comp()->trHeapMemory(), type, TR::DataType::getSize(type));
  TR::SymbolReference *symRef = new (injector->comp()->trHeapMemory()) 
	  TR::SymbolReference(injector->comp()->getSymRefTab(), sym, injector->comp()->getMethodSymbol()->getResolvedMethodIndex(), -1);
	symRef->setReallySharesSymbol();

  // conservative aliasing
  int32_t refNum = symRef->getReferenceNumber();
  if (type == TR::Address)
	  injector->comp()->getSymRefTab()->aliasBuilder.addressShadowSymRefs().set(refNum);
  else if (type == TR::Int32)
	  injector->comp()->getSymRefTab()->aliasBuilder.intShadowSymRefs().set(refNum);
  else
	  injector->comp()->getSymRefTab()->aliasBuilder.nonIntPrimitiveShadowSymRefs().set(refNum);
  TR::Node *store = TR::Node::createWithSymRef(
	  storeOp, 2, array_offset, value,
	  0, symRef);
#endif
  injector->genTreeTop(store);
}

void JIT_ArrayStoreAt(JIT_ILInjectorRef ilinjector, uint64_t symbolId,
                      JIT_NodeRef basenode, int64_t idx,
                      JIT_NodeRef valuenode) {
  auto injector = unwrap_ilinjector(ilinjector);
  auto base = unwrap_node(basenode);
  auto index = TR::Node::lconst(idx);
  auto value = unwrap_node(valuenode);
  auto type = value->getDataType();
  TR::SymbolReference *symRef = nullptr;
  TR::ILOpCodes storeOp =
      injector->comp()->il.opCodeForIndirectArrayStore(type);
  auto aoffset = get_array_element_address(injector, type, base, index);
#if 0
  if (symbolId) {
    symRef = injector->findOrCreateShadowSymRef(symbolId, type, (int32_t)idx);
  }
#endif
  if (!symRef)
    symRef =
        injector->symRefTab()->findOrCreateArrayShadowSymbolRef(type, base);
  TR::Node *store =
      TR::Node::createWithSymRef(storeOp, 2, aoffset, value, 0, symRef);
  injector->genTreeTop(store);
}

JIT_NodeRef JIT_LoadParameter(JIT_ILInjectorRef ilinjector, int32_t slot) {
  auto injector = unwrap_ilinjector(ilinjector);
  auto function_builder = injector->function_builder_;
  TR_ASSERT(slot >= 0 && slot < function_builder->args_.size(),
            "Invalid argument slot %d", slot);
  if (slot < 0 || slot >= function_builder->args_.size()) {
    return nullptr;
  }
  auto type = TR::DataType(function_builder->args_[slot]);
  auto symbol = injector->symRefTab()->findOrCreateAutoSymbol(
	  injector->methodSymbol(), slot, type, true, false, true);
  symbol->getSymbol()->setNotCollected();
  auto node = TR::Node::createLoad(symbol);
  return wrap_node(node);
}

static TR::Node *convertTo(TR::IlGenerator *injector, TR::DataType typeTo,
                           TR::Node *v, bool needUnsigned = false) {
  TR::DataType typeFrom = v->getDataType();
  if (typeFrom == typeTo)
	  return v;
  TR::ILOpCodes convertOp =
      TR::ILOpCode::getProperConversion(typeFrom, typeTo, needUnsigned);
  TR_ASSERT(
      convertOp != TR::BadILOp,
      "Unknown conversion requested from TR::DataType %d to TR::DataType %d",
      (int)typeFrom, (int)typeTo);
  if (convertOp == TR::BadILOp)
    return nullptr;
  TR::Node *result = TR::Node::create(convertOp, 1, v);
  return result;
}

JIT_NodeRef JIT_ConvertTo(JIT_ILInjectorRef ilinjector, JIT_NodeRef value,
                          JIT_Type targetType, bool needUnsigned) {
  return wrap_node(convertTo(unwrap_ilinjector(ilinjector),
                             TR::DataType((TR::DataTypes)targetType),
                             unwrap_node(value), needUnsigned));
}

JIT_NodeRef JIT_Call(JIT_ILInjectorRef ilinjector, const char *functionName,
                     int32_t numArgs, JIT_NodeRef* args) {
  auto injector = unwrap_ilinjector(ilinjector);
  auto function_builder = injector->function_builder_;
  TR::ResolvedMethod *resolvedMethod =
      function_builder->context_->getFunction(functionName);
  TR_ASSERT(resolvedMethod, "Could not identify function %s\n", functionName);
  if (resolvedMethod == nullptr)
    return nullptr;
  TR::SymbolReference *methodSymRef =
      injector->symRefTab()->findOrCreateComputedStaticMethodSymbol(
          JITTED_METHOD_INDEX, -1, resolvedMethod);
  TR::DataType returnType = methodSymRef->getSymbol()
                                ->castToMethodSymbol()
                                ->getMethod()
                                ->returnType();
  TR::Node *callNode = TR::Node::createWithSymRef(
      TR::ILOpCode::getDirectCall(returnType), numArgs, methodSymRef);
  // TODO: should really verify argument types here
  int32_t childIndex = 0;
  TR::DataType targetType = TR::Int32;
  if (TR::Compiler->target.is64Bit())
    targetType = TR::Int64;
  for (int32_t a = 0; a < numArgs; a++) {
    JIT_NodeRef arg = args[a];
    TR::Node *node = unwrap_node(arg);
    if (node->getDataType() == TR::Int8 || node->getDataType() == TR::Int16 ||
        (targetType == TR::Int64 && node->getDataType() == TR::Int32))
      node = convertTo(injector, targetType, node);
    callNode->setAndIncChild(childIndex++, node);
  }
  // callNode must be anchored by itself
  injector->genTreeTop(callNode);
  return wrap_node(callNode);
}

JIT_SymbolRef JIT_GetFunctionSymbol(JIT_ILInjectorRef ilinjector, const char *name) {
	auto injector = unwrap_ilinjector(ilinjector);
	auto resolvedMethod = injector->function_builder_->context_->getFunction(name);
	if (resolvedMethod) {
		auto symref = injector->symRefTab()->findOrCreateComputedStaticMethodSymbol(
			JITTED_METHOD_INDEX, -1, resolvedMethod);
		return wrap_symbolref(symref);
	}
	return nullptr;
}

JIT_NodeRef JIT_IndirectCall(JIT_ILInjectorRef ilinjector, JIT_NodeRef funcptr,
	JIT_Type return_type, int32_t numArgs, JIT_NodeRef* args) {
	auto injector = unwrap_ilinjector(ilinjector);
	auto function_builder = injector->function_builder_;
	auto function_pointer = unwrap_node(funcptr);
	TR::DataType returnType = TR::DataType((TR::DataTypes)return_type);

	TR::DataType targetType = TR::Int32;
	if (TR::Compiler->target.is64Bit())
		targetType = TR::Int64;
	std::vector<TR::DataType> argtypes;
	for (int i = 0; i < numArgs; i++) {
		JIT_NodeRef arg = args[i];
		TR::Node *node = unwrap_node(arg);
		TR::DataType type = node->getDataType();
		if (type == TR::Int8 || type == TR::Int16 ||
			(targetType == TR::Int64 && type == TR::Int32))
			type = targetType;
		argtypes.push_back(type);
	}
	char function_name[80];
	snprintf(function_name, sizeof function_name,
		"__fpr_%d__", function_builder->context_->function_id_++); //FIXME increment must be atomic
	function_builder->context_->registerFunction(function_name, returnType, argtypes, nullptr);

	TR::ResolvedMethod *resolvedMethod =
		function_builder->context_->getFunction(function_name);
	TR::SymbolReference *methodSymRef =
		injector->symRefTab()->findOrCreateComputedStaticMethodSymbol(
			JITTED_METHOD_INDEX, -1, resolvedMethod);
	TR::Node *callNode = TR::Node::createWithSymRef(
		TR::ILOpCode::getIndirectCall(returnType), numArgs+1, methodSymRef);
	int32_t childIndex = 0;
	callNode->setAndIncChild(childIndex++, function_pointer);
	for (int32_t a = 0; a < numArgs; a++) {
		JIT_NodeRef arg = args[a];
		TR::Node *node = unwrap_node(arg);
		if (node->getDataType() == TR::Int8 || node->getDataType() == TR::Int16 ||
			(targetType == TR::Int64 && node->getDataType() == TR::Int32))
			node = convertTo(injector, targetType, node);
		callNode->setAndIncChild(childIndex++, node);
	}
	// callNode must be anchored by itself
	injector->genTreeTop(callNode);
	return wrap_node(callNode);
}


JIT_NodeRef JIT_Goto(JIT_ILInjectorRef ilinjector, JIT_BlockRef block) {
  auto injector = unwrap_ilinjector(ilinjector);
  auto target_block = unwrap_block(block);
  TR::Node *gotoNode = TR::Node::create(NULL, TR::Goto);
  gotoNode->setBranchDestination(target_block->getEntry());
  injector->genTreeTop(gotoNode);
  injector->cfg()->addEdge(injector->getCurrentBlock(), target_block);
  return wrap_node(gotoNode);
}

JIT_NodeRef JIT_ReturnValue(JIT_ILInjectorRef ilinjector, JIT_NodeRef value) {
  auto injector = unwrap_ilinjector(ilinjector);
  auto valueNode = unwrap_node(value);
  TR::Node *returnNode = TR::Node::create(
      TR::ILOpCode::returnOpCode(valueNode->getDataType()), 1, valueNode);
  injector->genTreeTop(returnNode);
  injector->cfg()->addEdge(injector->getCurrentBlock(),
                           injector->cfg()->getEnd());
  return wrap_node(returnNode);
}

JIT_NodeRef JIT_ReturnNoValue(JIT_ILInjectorRef ilinjector) {
  auto injector = unwrap_ilinjector(ilinjector);
  TR::Node *returnNode =
      TR::Node::create(TR::ILOpCode::returnOpCode(TR::NoType));
  injector->genTreeTop(returnNode);
  injector->cfg()->addEdge(injector->getCurrentBlock(),
                           injector->cfg()->getEnd());
  return wrap_node(returnNode);
}

JIT_NodeRef JIT_ZeroValue(JIT_ILInjectorRef ilinjector, JIT_Type type) {
  auto injector = unwrap_ilinjector(ilinjector);
  TR::DataTypes t = (TR::DataTypes)type;
  TR::Node *n = NULL;
  switch (t) {
  case TR::DataTypes::Int8:
    n = TR::Node::bconst(0);
    break;
  case TR::DataTypes::Int16:
    n = TR::Node::sconst(0);
    break;
  case TR::DataTypes::Int32:
    n = TR::Node::iconst(0);
    break;
  case TR::DataTypes::Int64:
    n = TR::Node::lconst(0);
    break;
  case TR::DataTypes::Float:
    n = TR::Node::create(0, TR::fconst, 0);
    n->setFloat(0.0);
    break;
  case TR::DataTypes::Double:
    n = TR::Node::create(0, TR::dconst, 0);
    n->setDouble(0.0);
    break;
  case TR::DataTypes::Address:
    n = TR::Node::aconst(0);
    break;
  }
  return wrap_node(n);
}

JIT_NodeRef JIT_IfNotZeroValue(JIT_ILInjectorRef ilinjector, JIT_NodeRef value,
                               JIT_BlockRef blockOnNonZero) {
  JIT_NodeRef zeroValue = JIT_ZeroValue(ilinjector, JIT_GetNodeType(value));
  auto injector = unwrap_ilinjector(ilinjector);
  auto zv = unwrap_node(zeroValue);
  auto v = unwrap_node(value);
  auto targetBlock = unwrap_block(blockOnNonZero);
  if (!zeroValue)
    return NULL;
  TR::Node *ifNode =
      TR::Node::createif(TR::ILOpCode::ifcmpneOpCode(v->getDataType()), v, zv,
                         targetBlock->getEntry());
  injector->genTreeTop(ifNode);
  injector->cfg()->addEdge(injector->getCurrentBlock(), targetBlock);
  return wrap_node(ifNode);
}

JIT_NodeRef JIT_IfZeroValue(JIT_ILInjectorRef ilinjector, JIT_NodeRef value,
                            JIT_BlockRef blockOnZero) {
  JIT_NodeRef zeroValue = JIT_ZeroValue(ilinjector, JIT_GetNodeType(value));
  auto injector = unwrap_ilinjector(ilinjector);
  auto zv = unwrap_node(zeroValue);
  auto v = unwrap_node(value);
  auto targetBlock = unwrap_block(blockOnZero);
  if (!zeroValue)
    return NULL;
  TR::Node *ifNode =
      TR::Node::createif(TR::ILOpCode::ifcmpeqOpCode(v->getDataType()), v, zv,
                         targetBlock->getEntry());
  injector->genTreeTop(ifNode);
  injector->cfg()->addEdge(injector->getCurrentBlock(), targetBlock);
  return wrap_node(ifNode);
}

JIT_NodeRef JIT_Switch(JIT_ILInjectorRef ilinjector, JIT_NodeRef expr,
	JIT_BlockRef default_branch, int num_cases, JIT_BlockRef *case_branches, int32_t *case_values) {

	auto injector = unwrap_ilinjector(ilinjector);
	auto exprNode = unwrap_node(expr);
	auto defaultBlock = unwrap_block(default_branch);

	auto switchBlock = injector->getCurrentBlock();

	TR::Node *defaultNode = TR::Node::createCase(0, defaultBlock->getEntry());
	TR::Node *lookupNode = TR::Node::create(TR::lookup, num_cases + 2, exprNode, defaultNode);
	injector->genTreeTop(lookupNode);
	for (int i = 0; i < num_cases; i++) {
		JIT_BlockRef block = case_branches[i];
		int32_t value = case_values[i];

		TR::Block *caseBlock = unwrap_block(block);
		injector->cfg()->addEdge(switchBlock, caseBlock);

		TR::Node *caseNode = TR::Node::createCase(0, caseBlock->getEntry(), value);
		lookupNode->setAndIncChild(i + 2, caseNode);
	}
	injector->cfg()->addEdge(switchBlock, defaultBlock);
	return wrap_node(lookupNode);
}

JIT_Type JIT_GetSymbolType(JIT_SymbolRef sym) {
  auto symbolref = unwrap_symbolref(sym);
  return (JIT_Type)symbolref->getSymbol()->getDataType().getDataType();
}

bool JIT_IsTemporary(JIT_ILInjectorRef ilinjector, JIT_SymbolRef sym) {
	auto injector = unwrap_ilinjector(ilinjector);
	auto symbolref = unwrap_symbolref(sym);
	return symbolref->isTemporary(injector->comp());
}

JIT_NodeOpCode JIT_GetOpCode(JIT_NodeRef noderef) {
	auto node = unwrap_node(noderef);
	return (JIT_NodeOpCode) node->getOpCodeValue();
}

JIT_SymbolRef JIT_GetSymbolForNode(JIT_NodeRef noderef) {
	auto node = unwrap_node(noderef);
	if (node->getOpCode().hasSymbolReference()) {
		auto symref = node->getSymbolReference();
		return wrap_symbolref(symref);
	}
	else {
		return wrap_symbolref(nullptr);
	}
}

void JIT_SetMayHaveLoops(JIT_ILInjectorRef ilinjector) {
	auto injector = unwrap_ilinjector(ilinjector);
	injector->comp()->getMethodSymbol()->setMayHaveLoops(true);
}

void JIT_SetMayHaveNestedLoops(JIT_ILInjectorRef ilinjector) {
	auto injector = unwrap_ilinjector(ilinjector);
	injector->comp()->getMethodSymbol()->setMayHaveNestedLoops(true);
}

} // extern "C"
