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
AST_MATCHER_P(AttributedStmt, hasStmtAttr, attr::Kind, kind) {
  auto attrs = Node.getAttrs();
  for (const Attr* attr : attrs) {
    if (attr->getKind() == kind) {
      return true;
    }
  }
  return false;
}
AST_MATCHER_P(AttributedStmt, hasSubStmt, ast_matchers::internal::Matcher<Stmt>, InnerMatcher) {
  const Stmt *const Else = Node.getSubStmt();
  return (Else != nullptr && InnerMatcher.matches(*Else, Finder, Builder));
}
auto hasThrowingFunctionDecl() {
  return hasDeclaration(
    functionDecl(
      unless(
        anyOf(
          isNoThrow(),
          isExternC()))));
}
auto throwingExpr() {
  return expr(
    anyOf(
      cxxConstructExpr(hasThrowingFunctionDecl()),
      callExpr(hasThrowingFunctionDecl())));
}
auto markedDecl() {
  return decl(
    hasAttr(attr::Kind::MaybeUnhandled));
}
auto markedStmt() {
  return attributedStmt(
    hasStmtAttr(attr::Kind::MaybeUnhandled));
}
auto markedIfForWhileSwitchStmt() {
  return stmt(
    anyOf(
      ifStmt(hasParent(markedStmt())),
      forStmt(hasParent(markedStmt())),
      whileStmt(hasParent(markedStmt())),
      switchStmt(hasParent(markedStmt()))));
}
auto throwingDecl() {
  return varDecl(
    hasDescendant(throwingExpr()));
}
auto throwingStmt() {
  return stmt(
    allOf(
      unless(declStmt()), // matched by "throwing-decl"
      unless(cxxConstructExpr()), // matched by "throwing-decl"
      unless(compoundStmt()), // too coarse to be useful
      unless(cxxThrowExpr()), // redundant to annotate throw stmts.
      unless(hasParent(markedStmt())), // already annotated.
      unless(hasParent(markedIfForWhileSwitchStmt())), // already annotated
      unless(hasAncestor( // already annotated 
        stmt(
          allOf(
            unless(compoundStmt()),
            hasParent(markedIfForWhileSwitchStmt()))))),
      anyOf(
        throwingExpr(),
        hasDescendant(throwingExpr()))));
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
