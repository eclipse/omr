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

/**
 * OMR Rewriter:
 *
 * Analyzes the source according to the OMR Checker rules, and then rewrites
 * failing expressions.
 *
 * \FIXME: There is a duplciation of logic inside of VisitCXXMemberCallExpr
 *         that I would prefer to avoid if possible. As it stands this
 *         tool must be dual maintained with OMRChecker.hpp
 *
 * The overall flow of this was inspired by Eli Bendersky's excellent llvm
 * samples, in particular this one:
 * https://github.com/eliben/llvm-clang-samples/blob/master/src_clang/tooling_sample.cpp
 */

#include <sstream>
#include <string>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

#include "OMRChecker.hpp"
using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;
using namespace OMRChecker;

static llvm::cl::OptionCategory OMRRewriterCategory("OMR Rewriter");

#define TRACEOPT (getenv("OMR_REWRITE_TRACE"))
#define trace(x) if ( TRACEOPT ) { llvm::errs() << x << "\n"; }
#define traceExpr(x) if ( TRACEOPT ) { x->dump(); }
/**
 * Rewrite C++ member call expressions that are extensible
 */
class OMRThisRewritingVisitor : public RecursiveASTVisitor<OMRThisRewritingVisitor> {
   ASTContext*              Context;
   OMRThisCheckingVisitor*  ThisChecker;
   OMRClassCheckingVisitor* ClassChecker;
   Rewriter&                TheRewriter;
   SourceManager&           SrcMgr;
   std::map<FileID,int>     replacements;
   std::vector<int>         RepeatedMacros;

   /**
    * Delegated isSelfCall to OMRThisCheckingVisitor
    */
   bool isSelfCall(CXXMemberCallExpr* call) {
      return ThisChecker->isSelfCall(call);
   }

   /**
    * Delegated isExtensibleMemberCall to OMRThisCheckingVisitor
    */
   bool isExtensibleMemberCall(CXXMemberCallExpr* call) {
      return ThisChecker->isExtensibleMemberCall(call);
   }

   /**
    * Delegated isStaticExtensibleMemberCall to OMRThisCheckingVisitor
    */
   bool isStaticExtensibleMemberCall(CallExpr* call) {
      return ThisChecker->isStaticExtensibleMemberCall(call);
   }

   /**
    * Check if a given macro has been replaced already.
    */
   bool isUnique(int RawEncoding) {
      int i = 0;
      while (i < RepeatedMacros.size() &&
             RepeatedMacros[i] != RawEncoding)
         { i++; }
      return (i == RepeatedMacros.size());
   }

   /* Diagnostic string at location
    */
   void diagnose_warning(std::string diagnostic, SourceLocation loc) {
      DiagnosticsEngine &diagEngine = Context->getDiagnostics();
      unsigned diagID               = diagEngine.getCustomDiagID(DiagnosticsEngine::Warning, "%0");
      diagEngine.Report(loc, diagID) << diagnostic;
   }

   /**
    * Return the SourceRange for an expression, seeing through macro expansion
    * as necessary.
    *
    * Current behaviour when encountering a macro:
    *  * Inside a macro body: do not replace, raise warning.
    *  * Contains a macro body: do not replace, raise warning.
    *  * Overlaps a macro expansion: do not replace, raise warning.
    *  * Inside a macro argument: replace in argument, raise warning.
    *
    * FIXME: The current behaviour does not have the capability to detect every
    *        macro, so macro expansions may be silently replaced if the replacement
    *        range contains a macro expansion. An example of where this occurs is
    *
    *            static_cast<MACRO*>(this)
    *
    *        Where MACRO expands to a concrete type in this compilation.
    *
    *
    */
   void ReplaceMacroCheck(Expr * e) {
      SourceLocation startLoc = e->getLocStart();
      SourceLocation endLoc = e->getLocEnd();
      SourceRange range;
      std::string diagnostic;

      if (startLoc.isMacroID() && endLoc.isMacroID()) {
         if (SrcMgr.isMacroBodyExpansion(startLoc)) {
            diagnostic += "Expression is inside a macro body expansion; replace aborted.";
            trace(diagnostic);
            trace("-----------------");
            diagnose_warning(diagnostic, startLoc);
            return;
         } else if (SrcMgr.isMacroArgExpansion(startLoc)) {
            trace("Expression is inside a macro argument; attempting replacement.");
            range.setBegin(SrcMgr.getSpellingLoc(startLoc));
            range.setEnd(SrcMgr.getSpellingLoc(endLoc));
            if (isUnique(range.getBegin().getRawEncoding())) {
               ReplaceWithSelf(e, range);
               RepeatedMacros.push_back(range.getBegin().getRawEncoding());
            } else {
               trace("Expression has already been fixed, replace aborted.");
            }
            return;
         }
      } else if ((!startLoc.isMacroID() && endLoc.isMacroID()) || (startLoc.isMacroID() && !endLoc.isMacroID())) {
         diagnostic += "Expression crosses macro boundary; replace aborted.";
         trace(diagnostic);
         trace("-----------------");
         diagnose_warning(diagnostic, startLoc);
      } else {
         bool contains_macro = false;
         StmtIterator itr = e->child_begin();
         StmtIterator nullItr;

         while (itr != nullItr) {
            contains_macro = contains_macro || SrcMgr.isMacroBodyExpansion(itr->getLocStart());
            //trace(SrcMgr.isMacroBodyExpansion(itr->getLocStart()));
            //traceExpr(itr);
            itr = itr->child_begin();
         }

         if (contains_macro) {
            trace("Expression contains a macro expansion; attempting replacement."); }
         ReplaceWithSelf(e, e->getSourceRange());
         return;
      }
   }

   /**
    * Return a location for insertion, undoing macro expansion if necessary.
    *
    * Current behaviour when encounting a macro:
    *   Inside a macro body: do not insert, raise warning.
    *   Inside a macro argument: insert into argument, raise warning.
    */
   void InsertMacroCheck(Expr * e) {
      SourceLocation eLoc = e->getExprLoc();
      std::string diagnostic;
      if (eLoc.isMacroID()) {
         if (SrcMgr.isMacroBodyExpansion(eLoc)) {
            diagnostic += "Expression is inside a macro body; insertion aborted.";
            trace(diagnostic);
            trace("-----------------");
            diagnose_warning(diagnostic, eLoc);
            //InsertSelf(e, SrcMgr.getSpellingLoc(eLoc));
         } else if (SrcMgr.isMacroArgExpansion(eLoc)) {
            trace("Expression is inside a macro argument; attempting insertion.");
            if (isUnique(SrcMgr.getSpellingLoc(eLoc).getRawEncoding())) {
               InsertSelf(e, SrcMgr.getSpellingLoc(eLoc));
               RepeatedMacros.push_back(SrcMgr.getSpellingLoc(eLoc).getRawEncoding());
            } else {
               trace("Expression has already been fixed; insertion aborted.");
            }
         }
      } else {
         InsertSelf(e, SrcMgr.getSpellingLoc(eLoc));
      }
   }

   /**
    * Inserts the specified scope qualifier while undoing macro expansion if necessary.
    *
    * Current behaviour is similar to that of InserMacroCheck.
    */
   void InsertScopeQualifierWithMacroCheck(Expr * e, const std::string& qualifier) {
      SourceLocation eLoc = e->getExprLoc();
      if (eLoc.isMacroID()) {
         if (SrcMgr.isMacroBodyExpansion(eLoc)) {
            std::string diagnostic = "Expression is inside a macro body; insertion aborted.";
            trace(diagnostic);
            trace("-----------------");
            diagnose_warning(diagnostic, eLoc);
            //InsertSelf(e, SrcMgr.getSpellingLoc(eLoc));
         } else if (SrcMgr.isMacroArgExpansion(eLoc)) {
            trace("Expression is inside a macro argument; attempting insertion.");
            if (isUnique(SrcMgr.getSpellingLoc(eLoc).getRawEncoding())) {
               InsertScopeQualifier(e, SrcMgr.getSpellingLoc(eLoc), qualifier);
               RepeatedMacros.push_back(SrcMgr.getSpellingLoc(eLoc).getRawEncoding());
            } else {
               trace("Expression has already been fixed; insertion aborted.");
            }
         }
      } else {
         InsertScopeQualifier(e, SrcMgr.getSpellingLoc(eLoc), qualifier);
      }
   }

   /**
    * Replace an expression with self()
    */
   void ReplaceWithSelf(Expr * e, SourceRange range) {
      std::string self = "self()";
      trace("Replacing " );
      traceExpr(e);
      TheRewriter.ReplaceText(range, self);
      trace("-----------------");
      replacements[SrcMgr.getFileID(range.getBegin())]++;
   }

   /**
    * Insert a call to self()-> ahead of the given location
    */
   void InsertSelf(Expr * e, SourceLocation loc) {
      std::string staticCastHint = "self()->";
      trace("-- Inserting " << staticCastHint << "before");
      traceExpr(e);
      TheRewriter.InsertText(loc, staticCastHint);
      trace("-----------------");
      replacements[SrcMgr.getFileID(loc)]++;
   }

   /**
    * Insert a scope qualifier ahead of the given location
    */
   void InsertScopeQualifier(Expr * e, SourceLocation loc, const std::string& qualifier) {
      trace("-- Inserting" << qualifier << "before");
      traceExpr(e);
      TheRewriter.InsertText(loc, qualifier);
      trace("-----------------");
      replacements[SrcMgr.getFileID(loc)]++;
   }

public:
   explicit OMRThisRewritingVisitor(ASTContext *C,
                                    OMRThisCheckingVisitor  *TC,
                                    OMRClassCheckingVisitor *CC,
                                    Rewriter& R) :
      ThisChecker(TC),
      TheRewriter(R),
      ClassChecker(CC),
      SrcMgr(R.getSourceMgr()),
      Context(C) { }

   bool VisitCallExpr(CallExpr *call) {
      if (!isStaticExtensibleMemberCall(call)) {
         trace("Not an extensible class static member call");
         traceExpr(call);
         trace("-----------------");
         return true;
      }

      // check that the function has a scope qualifier
      DeclRefExpr *referencedFunc;
      if ((referencedFunc = dyn_cast<DeclRefExpr>(call->getCallee()->IgnoreImplicit())) && !referencedFunc->hasQualifier()) {
         CXXMethodDecl * methodDecl;
         auto calleeDecl = call->getCalleeDecl();   // needed to avoid potential segfault with dyn_cast and g++
         if (!calleeDecl || !(methodDecl = dyn_cast<CXXMethodDecl>(calleeDecl))) {
            return false;
         }
         /*DiagnosticsEngine &diagEngine = Context->getDiagnostics();
         unsigned diagID = diagEngine.getCustomDiagID(DiagnosticsEngine::Error, "Static member function must be called with scope qualifier. Suggested fix:");
         DiagnosticBuilder builder = diagEngine.Report(call->getExprLoc(), diagID);
         std::string qualifierHint = "TR::" + methodDecl->getParent()->getNameAsString() + "::";
         builder.AddFixItHint(FixItHint::CreateInsertion(call->getExprLoc(), qualifierHint));*/
         std::string qualifierHint = "TR::" + methodDecl->getParent()->getNameAsString() + "::";
         InsertScopeQualifierWithMacroCheck(referencedFunc, qualifierHint);

      } else {
         trace("-<>- Has scope qualifier");
         traceExpr(call);
         trace("-----------------");
      }
      return true;
   }

   bool VisitCXXMemberCallExpr(CXXMemberCallExpr *call) {
      if (!isExtensibleMemberCall(call)) {
         trace("Not an extensible member call");
         traceExpr(call);
         trace("-----------------");
         return true;
      }

      Expr *receiver = call->getImplicitObjectArgument()->IgnoreParenImpCasts();

      if (receiver->isImplicitCXXThis()) {
         if (!isSelfCall(call)) {
            MemberExpr * memberFunc;
            if ((memberFunc = dyn_cast<MemberExpr>(call->getCallee())) && memberFunc->hasQualifier()) {
               return true;
            }
            trace("(1): Insert self() in lieu of implicit this");
            InsertMacroCheck(receiver);
         } else {
            trace("-<>- Is a self() call");
            trace("-----------------");
         }
      } else if (isa<CXXStaticCastExpr>(receiver)) {
         CXXStaticCastExpr *cast = dyn_cast<CXXStaticCastExpr>(receiver);
         CXXRecordDecl *targetClass = cast->getType()->getAs<PointerType>() ? cast->getType()->getAs<PointerType>()->getPointeeType()->getAsCXXRecordDecl()->getCanonicalDecl() : NULL;
         trace("targetClass of static cast" << (targetClass ? targetClass->getQualifiedNameAsString() : "NULL" ) );
         CXXThisExpr *thisExpr = getThisExpr(cast->getSubExpr());
         if (thisExpr) {
            const CXXRecordDecl *thisConcrete = ClassChecker->getAssociatedConcreteType(thisExpr->getType()->getAs<PointerType>()->getPointeeType()->getAsCXXRecordDecl());
            if (thisConcrete) {
               trace("(2): Replacing " );
               ReplaceMacroCheck(call->getImplicitObjectArgument());
            } else {
               trace("-<>- didn't find a most derived for static cast target");
               trace("-----------------");
            }
         } else {
            trace("-<>- Didn't find this expr of cast expr");
            trace("-----------------");
         }
      } else {
         Expr *thisExpr = getThisExpr(receiver->IgnoreParenCasts());
         if (thisExpr) {
            trace("(3): Replacing ");
            ReplaceMacroCheck(call->getImplicitObjectArgument());
         } else {
            trace("-<>- no this found for member call expr....");
            trace("-----------------");
         }
      }
      return true;
   }

   void RewriteFiles() {
      for (std::map<FileID,int>::iterator files = replacements.begin(); files != replacements.end(); files++) {
         std::string filename = std::string(SrcMgr.getFileEntryForID(files->first)->getName()) + ".OMRRewritten";
         llvm::errs() << "\nPerformed " << files->second << " replacements\n";
         if (SrcMgr.getFileManager().getFile(filename) == NULL) {
            llvm::errs() << "** Writing out results: " << filename << "\n";
            std::error_code ec;
            llvm::raw_fd_ostream fs(filename, ec, llvm::sys::fs::OpenFlags::F_Text) ;
            TheRewriter.getEditBuffer(files->first).write(fs);
            fs.close();
         } else {
            llvm::errs() << "Output file already exists, aborting write.\n";
         }
      }
   }
};


class OMRRewriteConsumer : public ASTConsumer {
   Rewriter& TheRewriter;

public:
   explicit OMRRewriteConsumer(Rewriter& R) :  TheRewriter(R) { }

   virtual void HandleTranslationUnit(ASTContext &Context) {
      trace("** OMR Rewrite Consumer handle translation unit");
      // Visit the classes, gathering information.
      ExtensibleClassDiscoveryVisitor extVisitor(&Context);
      if (extVisitor.TraverseDecl(Context.getTranslationUnitDecl())) {
         OMRClassCheckingVisitor ClassVisitor(&Context, extVisitor);
         if (ClassVisitor.TraverseDecl(Context.getTranslationUnitDecl())) {
            // Visit this expressions, printing diagnostics.
            if (getenv("OMR_CHECK_TRACE_RELATIONS") || getenv("OMR_CHECK_TRACE")) {
               ClassVisitor.printRelations();
            }

            ClassVisitor.VerifyTypeStructure();

            if (ClassVisitor.emptyMap() && !getenv("OMR_CHECK_FORCE_THIS_VISIT")) {
               trace("** Returning early, we have an empty map");
               return;
            }

            OMRThisCheckingVisitor  ThisChecker(&Context, &ClassVisitor);
            OMRThisRewritingVisitor ThisRewriter(&Context, &ThisChecker, &ClassVisitor, TheRewriter);
            if (!ThisRewriter.TraverseDecl(Context.getTranslationUnitDecl())) {
               llvm::errs() << "This visitor ended early?\n";
            } else {
               ThisRewriter.RewriteFiles();
            }
         } else {
            trace("Returning early, class checker returned false!");
         }
      } else {
         trace("Returning early, extensible class discovery visitor returned false!");
      }
      trace("** End HandleTranslationUnit");
   }
};

// For each source file provided to the tool, a new FrontendAction is created.
class OMRRewriteFrontendAction : public ASTFrontendAction {
public:

OMRRewriteFrontendAction() {}

std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                               StringRef file) override {
   llvm::errs() << "** Starting work on file: " << file << "\n";
   TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
   return llvm::make_unique<OMRRewriteConsumer>(TheRewriter);
}

private:
Rewriter TheRewriter;
};

int main(int argc, const char **argv) {
   CommonOptionsParser op(argc, argv, OMRRewriterCategory);
   ClangTool Tool(op.getCompilations(), op.getSourcePathList());

   return Tool.run(newFrontendActionFactory<OMRRewriteFrontendAction>().get());
}

