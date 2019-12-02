//===--- VisibleExceptionPropagationCheck.cpp - clang-tidy ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "VisibleExceptionPropagationCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang {

  namespace {

    // Match attributed stmt with certain attribute.
    AST_MATCHER_P(AttributedStmt, hasStmtAttr, attr::Kind, kind) {
      auto attrs = Node.getAttrs();
      for (const Attr* attr : attrs) {
        if (attr->getKind() == kind) {
          return true;
        }
      }
      return false;
    }

    // Match attributed stmt with certain sub statment.
    AST_MATCHER_P(AttributedStmt, hasSubStmt, ast_matchers::internal::Matcher<Stmt>, InnerMatcher) {
      const Stmt *const Else = Node.getSubStmt();
      return (Else != nullptr && InnerMatcher.matches(*Else, Finder, Builder));
    }

    // Helper for throwingExpr.
    auto hasThrowingFunctionDecl() {
      return hasDeclaration(
        functionDecl(
          unless(
            anyOf(
              isNoThrow(),
              isExternC()))));
    }

    // Match expressions that can throw.
    auto throwingExpr() {
      return expr(
        anyOf(
          cxxConstructExpr(hasThrowingFunctionDecl()),
          callExpr(hasThrowingFunctionDecl())));
    }
    auto throwingExprSelfOrDescendant() {
      return expr(
        anyOf(
          throwingExpr(),
          hasDescendant(throwingExpr())));
    }

    // Match decl's and stmt's that are annotated.
    auto markedDecl() {
      return decl(
        hasAttr(attr::Kind::MaybeUnhandled));
    }
    auto markedStmt() {
      return stmt(hasParent(attributedStmt(
        hasStmtAttr(attr::Kind::MaybeUnhandled))));
    }

    // Match decl's that have a throwing subexpression.
    auto throwingDecl() {
      return varDecl(
        hasDescendant(throwingExpr()));
    }

    auto throwingIfStmt()
    {
      return stmt(
        ifStmt(
          hasCondition(
            throwingExprSelfOrDescendant())));
    }
    auto throwingForStmt()
    {
      return stmt(
        forStmt(
          anyOf(
            hasLoopInit(
              throwingExprSelfOrDescendant()),
            hasCondition(
              throwingExprSelfOrDescendant()),
            hasIncrement(
              throwingExprSelfOrDescendant()))));
    }
    auto throwingWhileStmt()
    {
      return stmt(
        whileStmt(
          hasCondition(
            throwingExprSelfOrDescendant())));
    }
    auto throwingSwitchStmt()
    {
      return stmt(
        switchStmt(
          hasCondition(
            throwingExprSelfOrDescendant())));
    }
    auto throwingDoStmt()
    {
      return stmt(
        doStmt(
          hasCondition(
            throwingExprSelfOrDescendant())));
    }
    auto throwingRegularStmt()
    {
      return stmt(
        allOf(
          unless(ifStmt()),
          unless(forStmt()),
          unless(whileStmt()),
          unless(switchStmt()),
          unless(doStmt()),
          throwingExprSelfOrDescendant()));
    }

    // Match stmt's that have a throwing subexpression...
    auto throwingStmt() {
      return stmt(
        allOf(
          unless(declStmt()), // matched by "throwing-decl"
          unless(compoundStmt()), // too course to be useful.
          unless(cxxThrowExpr()), // explicitly throws already.
          unless(attributedStmt()), // need only to match attributedStmt substmt.
          anyOf(
            hasParent(compoundStmt()), // needs to be an attributable stmt.
            hasParent(attributedStmt()), // or already attributed.
            hasParent(caseStmt())),
          anyOf(
            throwingIfStmt(),
            throwingForStmt(),
            throwingWhileStmt(),
            throwingSwitchStmt(),
            throwingDoStmt(),
            throwingRegularStmt())));
    }
  }

namespace tidy {
namespace readability {

void VisibleExceptionPropagationCheck::registerMatchers(MatchFinder *Finder) {

  Finder->addMatcher(
    decl(
      allOf(
        markedDecl(),
        unless(throwingDecl())))
    .bind("decl-bad-mark"), this);

  Finder->addMatcher(
    decl(
      allOf(
        unless(markedDecl()),
        throwingDecl()))
    .bind("decl-missing-mark"), this);

  Finder->addMatcher(
    stmt(
      allOf(
        markedStmt(),
        unless(throwingStmt())))
    .bind("stmt-bad-mark"), this);

  Finder->addMatcher(
    stmt(
      allOf(
        unless(markedStmt()),
        throwingStmt()))
    .bind("stmt-missing-mark"), this);
}

void VisibleExceptionPropagationCheck::check(const MatchFinder::MatchResult &Result) {

  if (const auto* m = Result.Nodes.getNodeAs<Decl>("decl-bad-mark")) {
    diag(m->getBeginLoc(), "Cannot implicitly throw, remove '[[clang::maybe_unhandled]]'");
  }
  else if (const auto* m = Result.Nodes.getNodeAs<Decl>("decl-missing-mark")) {
    diag(m->getBeginLoc(), "May implicitly throw, add '[[clang::maybe_unhandled]]'");
  }
  else if (const auto* m = Result.Nodes.getNodeAs<Stmt>("stmt-bad-mark")) {
    diag(m->getBeginLoc(), "Cannot implicitly throw, remove '[[clang::maybe_unhandled]]'");
  }
  else if (const auto* m = Result.Nodes.getNodeAs<Stmt>("stmt-missing-mark")) {
    diag(m->getBeginLoc(), "May implicitly throw, add '[[clang::maybe_unhandled]]'");
  }
}

} // namespace readability
} // namespace tidy
} // namespace clang
