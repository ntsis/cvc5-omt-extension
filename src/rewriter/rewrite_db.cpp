/******************************************************************************
 * Top contributors (to current version):
 *   Andrew Reynolds, Abdalrhman Mohamed, Aina Niemetz
 *
 * This file is part of the cvc5 project.
 *
 * Copyright (c) 2009-2025 by the authors listed in the file AUTHORS
 * in the top-level source directory and their institutional affiliations.
 * All rights reserved.  See the file COPYING in the top-level source
 * directory for licensing information.
 * ****************************************************************************
 *
 * Rewrite database
 */

#include "rewriter/rewrite_db.h"

#include "expr/node_algorithm.h"
#include "rewriter/rewrite_db_term_process.h"
#include "rewriter/rewrites.h"

using namespace cvc5::internal::kind;

namespace cvc5::internal {
namespace rewriter {

uint32_t IsListTypeClassCallback::getTypeClass(TNode v)
{
  return expr::isListVar(v) ? 1 : 0;
}

RewriteDb::RewriteDb(NodeManager* nm) : d_canonCb(), d_canon(&d_canonCb)
{
  d_true = nm->mkConst(true);
  d_false = nm->mkConst(false);
  rewriter::addRules(nm, *this);

  if (TraceIsOn("rewrite-db"))
  {
    Trace("rewrite-db") << "Rewrite database:" << std::endl;
    Trace("rewrite-db") << "START" << std::endl;
    Trace("rewrite-db") << d_mt.debugPrint();
    Trace("rewrite-db") << "END" << std::endl;
  }
}

void RewriteDb::addRule(ProofRewriteRule id,
                        const std::vector<Node> fvs,
                        Node a,
                        Node b,
                        Node cond,
                        Node context,
                        Level _level)
{
  NodeManager* nm = a.getNodeManager();
  std::vector<Node> fvsf = fvs;
  std::vector<Node> condsn;
  Node eq = a.eqNode(b);
  // we canonize left-to-right, hence we should traverse in the opposite
  // order, since we index based on conclusion, we make a dummy node here
  std::vector<Node> tmpArgs;
  tmpArgs.push_back(eq);
  tmpArgs.push_back(cond);
  if (!context.isNull())
  {
    tmpArgs.push_back(context);
  }
  Node tmp = nm->mkNode(Kind::SEXPR, tmpArgs);

  // must canonize
  Trace("rewrite-db") << "Add rule " << id << ": " << cond << " => " << a
                      << " == " << b << std::endl;
  Assert(a.getType().isComparableTo(b.getType()));
  Node ctmp = d_canon.getCanonicalTerm(tmp, false, false);
  if (!context.isNull())
  {
    context = ctmp[2];
  }

  Node condC = ctmp[1];
  std::vector<Node> conds;
  if (condC.getKind() == Kind::AND)
  {
    for (const Node& c : condC)
    {
      // should flatten in proof inference listing
      Assert(c.getKind() != Kind::AND);
      conds.push_back(c);
    }
  }
  else if (!condC.isConst())
  {
    conds.push_back(condC);
  }
  else if (!condC.getConst<bool>())
  {
    // skip those with false condition
    return;
  }
  // make as expected matching: top symbol of all conditions is equality
  // this means (not p) becomes (= p false), p becomes (= p true)
  for (size_t i = 0, nconds = conds.size(); i < nconds; i++)
  {
    if (conds[i].getKind() == Kind::NOT)
    {
      conds[i] = conds[i][0].eqNode(d_false);
    }
    else if (conds[i].getKind() != Kind::EQUAL)
    {
      conds[i] = conds[i].eqNode(d_true);
    }
  }
  // register side conditions?

  Node eqC = ctmp[0];
  Assert(eqC.getKind() == Kind::EQUAL);

  // add to discrimination tree
  Trace("proof-db-debug") << "Add (canonical) rule " << eqC << std::endl;
  d_mt.addTerm(eqC[0]);

  // match to get canonical variables
  std::unordered_map<Node, Node> msubs;
  if (!expr::match(eq, eqC, msubs))
  {
    Assert(false);
  }
  std::unordered_map<Node, Node>::iterator its;
  std::vector<Node> ofvs;
  std::vector<Node> cfvs;
  for (const Node& v : fvsf)
  {
    its = msubs.find(v);
    if (its != msubs.end())
    {
      ofvs.push_back(v);
      cfvs.push_back(its->second);
      if (expr::isListVar(v))
      {
        // mark the canonical variable as a list variable as well
        expr::markListVar(its->second);
      }
    }
    else
    {
      Unhandled() << "In DSL rule " << id << ", variable " << v
                  << " is unused, dropping it" << std::endl;
    }
    // remember the free variables
    d_allFv.insert(v);
  }

  // initialize rule
  d_rewDbRule[id].init(id, ofvs, cfvs, conds, eqC, context, _level);
  d_concToRules[eqC].push_back(id);
  d_headToRules[eqC[0]].push_back(id);
}

void RewriteDb::getMatches(const Node& eq, expr::NotifyMatch* ntm)
{
  d_mt.getMatches(eq, ntm);
}

const RewriteProofRule& RewriteDb::getRule(ProofRewriteRule id) const
{
  std::map<ProofRewriteRule, RewriteProofRule>::const_iterator it =
      d_rewDbRule.find(id);
  Assert(it != d_rewDbRule.end());
  return it->second;
}

const std::vector<ProofRewriteRule>& RewriteDb::getRuleIdsForConclusion(
    const Node& eq) const
{
  std::map<Node, std::vector<ProofRewriteRule> >::const_iterator it =
      d_concToRules.find(eq);
  if (it != d_concToRules.end())
  {
    return it->second;
  }
  return d_emptyVec;
}

const std::vector<ProofRewriteRule>& RewriteDb::getRuleIdsForHead(
    const Node& eq) const
{
  std::map<Node, std::vector<ProofRewriteRule> >::const_iterator it =
      d_headToRules.find(eq);
  if (it != d_headToRules.end())
  {
    return it->second;
  }
  return d_emptyVec;
}
const std::unordered_set<Node>& RewriteDb::getAllFreeVariables() const
{
  return d_allFv;
}

const std::map<ProofRewriteRule, RewriteProofRule>& RewriteDb::getAllRules()
    const
{
  return d_rewDbRule;
}

}  // namespace rewriter
}  // namespace cvc5::internal
