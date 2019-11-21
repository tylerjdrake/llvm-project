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

#include <iostream>

namespace clang {

namespace {
// Matcher for AttributedStmt that matches a certain Attr kind.
AST_MATCHER_P(Stmt, hasStmtAttr, attr::Kind, kind) {
  if (Node.getStmtClass() == Node.AttributedStmtClass) {
    auto attrs = static_cast<const AttributedStmt&>(Node).getAttrs();
    for (const Attr* attr : attrs) {
      if (attr->getKind() == kind) {
        return true;
      }
    }
  }
  return false;
}
auto throwingExpr()
{
  return expr(
    anyOf(
      cxxConstructExpr(hasDeclaration(functionDecl(unless(isNoThrow())))),
      callExpr(callee(functionDecl(unless(isNoThrow()))))));
}
auto markedDecl()
{
  return decl(
    hasAttr(attr::Kind::MaybeUnhandled));
}
auto markedStmt()
{
  return stmt(
    hasStmtAttr(attr::Kind::MaybeUnhandled));
}
auto throwingDecl()
{
  return varDecl(
    hasDescendant(throwingExpr()));
}
auto throwingStmt()
{
  return stmt(
    allOf(
      unless(declStmt()), // matched by "throwing-decl"
      unless(cxxConstructExpr()), // matched by "throwing-decl"
      unless(compoundStmt()), // too coarse to be useful
      unless(hasAncestor(hasStmtAttr(attr::Kind::MaybeUnhandled))),
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
    diag(m->getBeginLoc(), "Cannot throw, remove '[[clang::maybe_unused]]'");
  }
  else if (const auto* m = Result.Nodes.getNodeAs<Decl>("decl-missing-mark")) {
    diag(m->getBeginLoc(), "May throw, add '[[clang::maybe_unused]]'");
  }
  else if (const auto* m = Result.Nodes.getNodeAs<Stmt>("stmt-bad-mark")) {
    diag(m->getBeginLoc(), "Cannot throw, remove '[[clang::maybe_unused]]'");
  }
  else if (const auto* m = Result.Nodes.getNodeAs<Stmt>("stmt-missing-mark")) {
    diag(m->getBeginLoc(), "May throw, add '[[clang::maybe_unused]]'");
  }
}

} // namespace readability
} // namespace tidy
} // namespace clang
