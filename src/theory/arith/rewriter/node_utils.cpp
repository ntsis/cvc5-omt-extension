/******************************************************************************
 * Top contributors (to current version):
 *   Gereon Kremer, Andrew Reynolds, Daniel Larraz
 *
 * This file is part of the cvc5 project.
 *
 * Copyright (c) 2009-2025 by the authors listed in the file AUTHORS
 * in the top-level source directory and their institutional affiliations.
 * All rights reserved.  See the file COPYING in the top-level source
 * directory for licensing information.
 * ****************************************************************************
 *
 * Utilities for nodes in the arithmetic rewriter.
 */

#include "theory/arith/rewriter/node_utils.h"

#include "base/check.h"
#include "theory/arith/rewriter/ordering.h"

namespace cvc5::internal {
namespace theory {
namespace arith {
namespace rewriter {

Node mkMultTerm(const Rational& multiplicity, TNode monomial)
{
  NodeManager* nm = monomial.getNodeManager();
  if (monomial.isConst())
  {
    return mkConst(nm, multiplicity * monomial.getConst<Rational>());
  }
  if (multiplicity.isOne())
  {
    return monomial;
  }
  return NodeManager::mkNode(Kind::MULT, mkConst(nm, multiplicity), monomial);
}

Node mkMultTerm(const RealAlgebraicNumber& multiplicity, TNode monomial)
{
  NodeManager* nm = monomial.getNodeManager();
  Node mterm = mkConst(nm, multiplicity);
  if (mterm.isConst())
  {
    return mkMultTerm(mterm.getConst<Rational>(), monomial);
  }
  if (monomial.isConst())
  {
    return mkConst(nm, multiplicity * monomial.getConst<Rational>());
  }
  std::vector<Node> prod;
  prod.emplace_back(mterm);
  if (monomial.getKind() == Kind::MULT || monomial.getKind() == Kind::NONLINEAR_MULT)
  {
    prod.insert(prod.end(), monomial.begin(), monomial.end());
  }
  else
  {
    prod.emplace_back(monomial);
  }
  Assert(prod.size() >= 2);
  return nm->mkNode(Kind::NONLINEAR_MULT, prod);
}

Node mkMultTerm(NodeManager* nm,
                const RealAlgebraicNumber& multiplicity,
                std::vector<Node>&& monomial)
{
  if (monomial.empty())
  {
    return mkConst(nm, multiplicity);
  }
  Node mterm = mkConst(nm, multiplicity);
  if (mterm.isConst())
  {
    std::sort(monomial.begin(), monomial.end(), rewriter::LeafNodeComparator());
    return mkMultTerm(mterm.getConst<Rational>(),
                      mkNonlinearMult(nm, monomial));
  }
  monomial.emplace_back(mterm);
  std::sort(monomial.begin(), monomial.end(), rewriter::LeafNodeComparator());
  Assert(monomial.size() >= 2);
  return nm->mkNode(Kind::NONLINEAR_MULT, monomial);
}

TNode removeToReal(TNode t) { return t.getKind() == Kind::TO_REAL ? t[0] : t; }

Node maybeEnsureReal(TypeNode tn, TNode t)
{
  // if we require being a real
  if (tn.isReal())
  {
    return ensureReal(t);
  }
  return t;
}

Node ensureReal(TNode t)
{
  if (t.getType().isInteger())
  {
    if (t.isConst())
    {
      // short-circuit
      Node ret = t.getNodeManager()->mkConstReal(t.getConst<Rational>());
      Assert(ret.getType().isReal());
      return ret;
    }
    Trace("arith-rewriter-debug") << "maybeEnsureReal: " << t << std::endl;
    return NodeManager::mkNode(Kind::TO_REAL, t);
  }
  return t;
}

}  // namespace rewriter
}  // namespace arith
}  // namespace theory
}  // namespace cvc5::internal
