/******************************************************************************
 * Top contributors (to current version):
 *   Andrew Reynolds, Aina Niemetz, Haniel Barbosa
 *
 * This file is part of the cvc5 project.
 *
 * Copyright (c) 2009-2025 by the authors listed in the file AUTHORS
 * in the top-level source directory and their institutional affiliations.
 * All rights reserved.  See the file COPYING in the top-level source
 * directory for licensing information.
 * ****************************************************************************
 *
 * Preprocessor for the theory of quantifiers.
 */

#include "theory/quantifiers/quantifiers_preprocess.h"

#include "expr/node_algorithm.h"
#include "options/quantifiers_options.h"
#include "theory/quantifiers/quant_util.h"
#include "theory/quantifiers/skolemize.h"

using namespace cvc5::internal::kind;

namespace cvc5::internal {
namespace theory {
namespace quantifiers {

QuantifiersPreprocess::QuantifiersPreprocess(Env& env)
    : EnvObj(env), d_qrew(nodeManager(), env.getRewriter(), options())
{
}

Node QuantifiersPreprocess::computePrenexAgg(
    Node n, std::map<Node, Node>& visited) const
{
  std::map<Node, Node>::iterator itv = visited.find(n);
  if (itv != visited.end())
  {
    return itv->second;
  }
  if (!expr::hasClosure(n))
  {
    // trivial
    return n;
  }
  NodeManager* nm = nodeManager();
  Node ret = n;
  if (n.getKind() == Kind::NOT)
  {
    ret = computePrenexAgg(n[0], visited).negate();
  }
  else if (n.getKind() == Kind::FORALL)
  {
    std::vector<Node> children;
    children.push_back(computePrenexAgg(n[1], visited));
    std::vector<Node> args;
    args.insert(args.end(), n[0].begin(), n[0].end());
    // only combine if standard
    if (d_qrew.isStandard(n, options()))
    {
      // for each child, strip top level quant
      for (unsigned i = 0; i < children.size(); i++)
      {
        if (children[i].getKind() == Kind::FORALL)
        {
          args.insert(args.end(), children[i][0].begin(), children[i][0].end());
          children[i] = children[i][1];
        }
      }
    }
    // keep the pattern
    std::vector<Node> iplc;
    if (n.getNumChildren() == 3)
    {
      iplc.insert(iplc.end(), n[2].begin(), n[2].end());
    }
    Node nb = nm->mkOr(children);
    ret = d_qrew.mkForall(args, nb, iplc, true);
  }
  else
  {
    Node q;
    std::vector<Node> args, nargs;
    Node nn = d_qrew.computePrenex(q, n, args, nargs, true, true);
    std::unordered_set<Node> argsSet(args.begin(), args.end());
    std::unordered_set<Node> nargsSet(nargs.begin(), nargs.end());
    Assert(n != nn || argsSet.empty());
    Assert(n != nn || nargsSet.empty());
    if (n != nn)
    {
      Node nnn = computePrenexAgg(nn, visited);
      // merge prenex
      if (nnn.getKind() == Kind::FORALL)
      {
        argsSet.insert(nnn[0].begin(), nnn[0].end());
        nnn = nnn[1];
        // pos polarity variables are inner
        if (!argsSet.empty())
        {
          nnn = d_qrew.mkForall({argsSet.begin(), argsSet.end()}, nnn, true);
        }
        argsSet.clear();
      }
      else if (nnn.getKind() == Kind::NOT && nnn[0].getKind() == Kind::FORALL)
      {
        nargsSet.insert(nnn[0][0].begin(), nnn[0][0].end());
        nnn = nnn[0][1].negate();
      }
      if (!nargsSet.empty())
      {
        nnn = d_qrew
                  .mkForall(
                      {nargsSet.begin(), nargsSet.end()}, nnn.negate(), true)
                  .negate();
      }
      if (!argsSet.empty())
      {
        nnn = d_qrew.mkForall({argsSet.begin(), argsSet.end()}, nnn, true);
      }
      ret = nnn;
    }
  }
  visited[n] = ret;
  return ret;
}

Node QuantifiersPreprocess::preSkolemizeQuantifiers(
    Node n,
    bool polarity,
    std::vector<TNode>& fvs,
    std::unordered_map<std::pair<Node, bool>, Node, NodePolPairHashFunction>&
        visited) const
{
  Assert(options().quantifiers.preSkolemQuant
         != options::PreSkolemQuantMode::OFF);
  std::pair<Node, bool> key(n, polarity);
  std::unordered_map<std::pair<Node, bool>, Node, NodePolPairHashFunction>::
      iterator it = visited.find(key);
  if (it != visited.end())
  {
    return it->second;
  }
  NodeManager* nm = nodeManager();
  Trace("pre-sk") << "Pre-skolem " << n << " " << polarity << " " << fvs.size()
                  << std::endl;
  if (n.getKind() == Kind::FORALL)
  {
    Node ret = n;
    if (n.getNumChildren() == 3)
    {
      // Do not pre-skolemize quantified formulas with three children.
      // This includes non-standard quantified formulas
      // like recursive function definitions, or sygus conjectures, and
      // quantified formulas with triggers.
    }
    else if (polarity)
    {
      if (options().quantifiers.preSkolemQuantNested)
      {
        std::vector<Node> children;
        children.push_back(n[0]);
        // add children to current scope
        std::vector<TNode> fvss;
        fvss.insert(fvss.end(), fvs.begin(), fvs.end());
        fvss.insert(fvss.end(), n[0].begin(), n[0].end());
        // process body in a new context
        std::unordered_map<std::pair<Node, bool>, Node, NodePolPairHashFunction>
            visitedSub;
        Node pbody = preSkolemizeQuantifiers(n[1], polarity, fvss, visitedSub);
        children.push_back(pbody);
        // return processed quantifier
        ret = nm->mkNode(Kind::FORALL, children);
      }
    }
    else
    {
      // will skolemize current, process body
      Node nn = preSkolemizeQuantifiers(n[1], polarity, fvs, visited);
      std::vector<Node> sk;
      Node sub;
      std::vector<unsigned> sub_vars;
      // return skolemized body
      ret = Skolemize::mkSkolemizedBodyInduction(
          options(), n, nn, fvs, sk, sub, sub_vars);
    }
    visited[key] = ret;
    return ret;
  }
  // check if it contains a quantifier as a subterm
  // if so, we may preprocess this node
  if (!expr::hasClosure(n))
  {
    visited[key] = n;
    return n;
  }
  Kind k = n.getKind();
  Node ret = n;
  Assert(n.getType().isBoolean());
  if (k == Kind::ITE || (k == Kind::EQUAL && n[0].getType().isBoolean()))
  {
    if (options().quantifiers.preSkolemQuant
        == options::PreSkolemQuantMode::AGG)
    {
      Node nn;
      // must remove structure
      if (k == Kind::ITE)
      {
        nn = nm->mkNode(Kind::AND,
                        nm->mkNode(Kind::OR, n[0].notNode(), n[1]),
                        nm->mkNode(Kind::OR, n[0], n[2]));
      }
      else if (k == Kind::EQUAL)
      {
        nn = nm->mkNode(Kind::AND,
                        nm->mkNode(Kind::OR, n[0].notNode(), n[1]),
                        nm->mkNode(Kind::OR, n[0], n[1].notNode()));
      }
      ret = preSkolemizeQuantifiers(nn, polarity, fvs, visited);
    }
  }
  else if (k == Kind::AND || k == Kind::OR || k == Kind::NOT
           || k == Kind::IMPLIES)
  {
    std::vector<Node> children;
    for (size_t i = 0, nchild = n.getNumChildren(); i < nchild; i++)
    {
      bool newHasPol;
      bool newPol;
      QuantPhaseReq::getPolarity(n, i, true, polarity, newHasPol, newPol);
      Assert(newHasPol);
      children.push_back(preSkolemizeQuantifiers(n[i], newPol, fvs, visited));
    }
    ret = nm->mkNode(k, children);
  }
  visited[key] = ret;
  return ret;
}

TrustNode QuantifiersPreprocess::preprocess(Node n, bool isInst) const
{
  Node prev = n;
  if (options().quantifiers.preSkolemQuant != options::PreSkolemQuantMode::OFF)
  {
    if (!isInst || !options().quantifiers.preSkolemQuantNested)
    {
      Trace("quantifiers-preprocess-debug")
          << "Pre-skolemize " << n << "..." << std::endl;
      // apply pre-skolemization to existential quantifiers
      std::vector<TNode> fvs;
      std::unordered_map<std::pair<Node, bool>, Node, NodePolPairHashFunction>
          visited;
      n = preSkolemizeQuantifiers(prev, true, fvs, visited);
    }
  }
  // pull all quantifiers globally
  if (options().quantifiers.prenexQuant == options::PrenexQuantMode::NORMAL)
  {
    Trace("quantifiers-prenex") << "Prenexing : " << n << std::endl;
    std::map<Node, Node> visited;
    n = computePrenexAgg(n, visited);
    n = rewrite(n);
    Trace("quantifiers-prenex") << "Prenexing returned : " << n << std::endl;
  }
  if (n != prev)
  {
    Trace("quantifiers-preprocess") << "Preprocess " << prev << std::endl;
    Trace("quantifiers-preprocess") << "..returned " << n << std::endl;
    return TrustNode::mkTrustRewrite(prev, n, nullptr);
  }
  return TrustNode::null();
}

}  // namespace quantifiers
}  // namespace theory
}  // namespace cvc5::internal
