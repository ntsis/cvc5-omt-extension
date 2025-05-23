/******************************************************************************
 * Top contributors (to current version):
 *   Andrew Reynolds, Abdalrhman Mohamed, Haniel Barbosa
 *
 * This file is part of the cvc5 project.
 *
 * Copyright (c) 2009-2025 by the authors listed in the file AUTHORS
 * in the top-level source directory and their institutional affiliations.
 * All rights reserved.  See the file COPYING in the top-level source
 * directory for licensing information.
 * ****************************************************************************
 *
 * The proof manager of the SMT engine.
 */

#include "smt/proof_manager.h"

#include "options/base_options.h"
#include "options/main_options.h"
#include "options/smt_options.h"
#include "proof/alethe/alethe_node_converter.h"
#include "proof/alethe/alethe_post_processor.h"
#include "proof/alethe/alethe_printer.h"
#include "proof/alf/alf_printer.h"
#include "proof/dot/dot_printer.h"
#include "proof/lfsc/lfsc_post_processor.h"
#include "proof/lfsc/lfsc_printer.h"
#include "proof/proof_checker.h"
#include "proof/proof_node_algorithm.h"
#include "proof/proof_node_manager.h"
#include "rewriter/rewrite_db.h"
#include "smt/assertions.h"
#include "smt/difficulty_post_processor.h"
#include "smt/env.h"
#include "smt/preprocess_proof_generator.h"
#include "smt/proof_logger.h"
#include "smt/proof_post_processor.h"
#include "smt/smt_solver.h"

using namespace cvc5::internal::rewriter;
namespace cvc5::internal {
namespace smt {

PfManager::PfManager(Env& env)
    : EnvObj(env),
      d_rewriteDb(nullptr),
      d_pchecker(nullptr),
      d_pnm(nullptr),
      d_pfpp(nullptr),
      d_pppg(nullptr),
      d_finalCb(env),
      d_finalizer(env, d_finalCb)
{
  // construct the rewrite db only if DSL rewrites are enabled
  if (options().proof.proofGranularityMode
          == options::ProofGranularityMode::DSL_REWRITE
      || options().proof.proofGranularityMode
             == options::ProofGranularityMode::DSL_REWRITE_STRICT)
  {
    d_rewriteDb.reset(new RewriteDb(nodeManager()));
    // maybe output rare rules?
    bool isNormalOut = isOutputOn(OutputTag::RARE_DB);
    bool isExpertOut = isOutputOn(OutputTag::RARE_DB_EXPERT);
    if (isNormalOut || isExpertOut)
    {
      if (options().proof.proofFormatMode != options::ProofFormatMode::CPC)
      {
        Warning()
            << "WARNING: Assuming --proof-format=cpc when printing the RARE "
               "database with -o rare-db(-expert)"
            << std::endl;
      }
      proof::AlfNodeConverter atp(nodeManager());
      proof::AlfPrinter alfp(d_env, atp, d_rewriteDb.get());
      const std::map<ProofRewriteRule, RewriteProofRule>& rules =
          d_rewriteDb->getAllRules();
      for (const std::pair<const ProofRewriteRule, RewriteProofRule>& r : rules)
      {
        // only output if the signature level is what we want
        Level l = r.second.getSignatureLevel();
        if (l == Level::NORMAL && isNormalOut)
        {
          std::ostream& os = output(OutputTag::RARE_DB);
          alfp.printDslRule(os, r.first);
        }
        else if (l == Level::EXPERT && isExpertOut)
        {
          std::ostream& os = output(OutputTag::RARE_DB_EXPERT);
          alfp.printDslRule(os, r.first);
        }
      }
    }
  }

  // enable the proof checker and the proof node manager
  d_pchecker.reset(
      new ProofChecker(statisticsRegistry(),
                       options().proof.proofCheck,
                       static_cast<uint32_t>(options().proof.proofPedantic),
                       d_rewriteDb.get()));
  d_pnm.reset(new ProofNodeManager(env.getNodeManager(),
                                   env.getOptions(),
                                   env.getRewriter(),
                                   d_pchecker.get()));
  // Now, initialize the proof postprocessor with the environment.
  // By default the post-processor will update all assumptions, which
  // can lead to SCOPE subproofs of the form
  //   A
  //  ...
  //   B1    B2
  //  ...   ...
  // ------------
  //      C
  // ------------- SCOPE [B1, B2]
  // B1 ^ B2 => C
  //
  // where A is an available assumption from outside the scope (note
  // that B1 was an assumption of this SCOPE subproof but since it could
  // be inferred from A, it was updated). This shape is problematic for
  // the Alethe reconstruction, so we disable the update of scoped
  // assumptions (which would disable the update of B1 in this case).
  d_pfpp = std::make_unique<ProofPostprocess>(
      env,
      d_rewriteDb.get(),
      options().proof.proofFormatMode != options::ProofFormatMode::ALETHE);

  // add rules to eliminate here
  if (options().proof.proofGranularityMode
      != options::ProofGranularityMode::MACRO)
  {
    d_pfpp->setEliminateRule(ProofRule::MACRO_SR_EQ_INTRO);
    d_pfpp->setEliminateRule(ProofRule::MACRO_SR_PRED_INTRO);
    d_pfpp->setEliminateRule(ProofRule::MACRO_SR_PRED_ELIM);
    d_pfpp->setEliminateRule(ProofRule::MACRO_SR_PRED_TRANSFORM);
    // Alethe does not require macro resolution to be expanded
    if (options().proof.proofFormatMode != options::ProofFormatMode::ALETHE)
    {
      d_pfpp->setEliminateRule(ProofRule::MACRO_RESOLUTION_TRUST);
      d_pfpp->setEliminateRule(ProofRule::MACRO_RESOLUTION);
    }
    d_pfpp->setEliminateRule(ProofRule::MACRO_ARITH_SCALE_SUM_UB);
    if (options().proof.proofGranularityMode
        != options::ProofGranularityMode::REWRITE)
    {
      d_pfpp->setEliminateRule(ProofRule::SUBS);
      d_pfpp->setEliminateRule(ProofRule::MACRO_REWRITE);
      // if in a DSL rewrite mode
      if (options().proof.proofGranularityMode
          != options::ProofGranularityMode::THEORY_REWRITE)
      {
        // this eliminates theory rewriting steps with finer-grained DSL rules
        d_pfpp->setEliminateAllTrustedRules();
      }
    }
    // theory-specific lazy proof reconstruction
    d_pfpp->setEliminateRule(ProofRule::MACRO_STRING_INFERENCE);
    d_pfpp->setEliminateRule(ProofRule::MACRO_BV_BITBLAST);
    // we only try to eliminate TRUST if not macro level
    d_pfpp->setEliminateRule(ProofRule::TRUST);
  }
  d_false = nodeManager()->mkConst(false);

  d_pppg = std::make_unique<PreprocessProofGenerator>(
      d_env, userContext(), "smt::PreprocessProofGenerator");
}

PfManager::~PfManager() {}

// TODO: Remove in favor of `std::erase_if` with C++ 20+ (see cvc5-wishues#137).
template <class T, class Alloc, class Pred>
constexpr typename std::vector<T, Alloc>::size_type erase_if(
    std::vector<T, Alloc>& c, Pred pred)
{
  typename std::vector<T, Alloc>::iterator it =
      std::remove_if(c.begin(), c.end(), pred);
  typename std::vector<T, Alloc>::size_type r = std::distance(it, c.end());
  c.erase(it, c.end());
  return r;
}

void PfManager::startProofLogging(std::ostream& out, Assertions& as)
{
  // by default, CPC proof logger
  d_plog.reset(new ProofLoggerCpc(d_env, out, this, as, d_pfpp.get()));
}

std::shared_ptr<ProofNode> PfManager::connectProofToAssertions(
    std::shared_ptr<ProofNode> pfn, Assertions& as, ProofScopeMode scopeMode)
{
  // Note this assumes that connectProofToAssertions is only called once per
  // unsat response. This method would need to cache its result otherwise.
  Trace("smt-proof")
      << "SolverEngine::connectProofToAssertions(): get proof body...\n";

  if (TraceIsOn("smt-proof-debug"))
  {
    Trace("smt-proof-debug")
        << "SolverEngine::connectProofToAssertions(): Proof node for false:\n";
    Trace("smt-proof-debug") << *pfn.get() << std::endl;
    Trace("smt-proof-debug") << "=====" << std::endl;
  }
  std::vector<Node> assertions;
  getAssertions(as, assertions);

  if (TraceIsOn("smt-proof"))
  {
    Trace("smt-proof")
        << "SolverEngine::connectProofToAssertions(): get free assumptions..."
        << std::endl;
    std::vector<Node> fassumps;
    expr::getFreeAssumptions(pfn.get(), fassumps);
    Trace("smt-proof") << "SolverEngine::connectProofToAssertions(): initial "
                          "free assumptions are:\n";
    for (const Node& a : fassumps)
    {
      Trace("smt-proof") << "- " << a << std::endl;
    }

    Trace("smt-proof")
        << "SolverEngine::connectProofToAssertions(): assertions are:\n";
    for (const Node& n : assertions)
    {
      Trace("smt-proof") << "- " << n << std::endl;
    }
    Trace("smt-proof") << "=====" << std::endl;
  }

  Trace("smt-proof")
      << "SolverEngine::connectProofToAssertions(): postprocess...\n";
  Assert(d_pfpp != nullptr);
  // Note that in incremental mode, we cannot set assertions here, as it
  // permits the postprocessor to merge subproofs at a higher user context
  // level into proofs that are used in a lower user context level.
  if (!options().base.incrementalSolving)
  {
    d_pfpp->setAssertions(assertions, false);
  }
  d_pfpp->process(pfn, d_pppg.get());

  switch (scopeMode)
  {
    case ProofScopeMode::NONE:
    {
      return pfn;
    }
    // Now make the final scope(s), which ensure(s) that the only open leaves
    // of the proof are the assertions (and definitions). If we are pruning
    // the input, we will try to minimize the used assertions (and definitions).
    case ProofScopeMode::UNIFIED:
    {
      Trace("smt-proof") << "SolverEngine::connectProofToAssertions(): make "
                            "unified scope...\n";
      return d_pnm->mkScope(
          pfn, assertions, true, options().proof.proofPruneInput);
    }
    case ProofScopeMode::DEFINITIONS_AND_ASSERTIONS:
    {
      Trace("smt-proof")
          << "SolverEngine::connectProofToAssertions(): make split scope...\n";
      // To support proof pruning for nested scopes, we need to:
      // 1. Minimize assertions of closed unified scope.
      std::vector<Node> unifiedAssertions;
      getAssertions(as, unifiedAssertions);
      Pf pf = d_pnm->mkScope(
          pfn, unifiedAssertions, true, options().proof.proofPruneInput);
      // if this is violated, there is unsoundness since we have shown
      // false that does not depend on the input.
      AlwaysAssert(pf->getRule() == ProofRule::SCOPE);
      // 2. Extract minimum unified assertions from the scope node.
      std::unordered_set<Node> minUnifiedAssertions;
      minUnifiedAssertions.insert(pf->getArguments().cbegin(),
                                  pf->getArguments().cend());
      // 3. Split those assertions into minimized definitions and assertions.
      std::vector<Node> minDefinitions;
      std::vector<Node> minAssertions;
      getDefinitionsAndAssertions(as, minDefinitions, minAssertions);
      std::function<bool(Node)> predicate = [&minUnifiedAssertions](Node n) {
        return minUnifiedAssertions.find(n) == minUnifiedAssertions.cend();
      };
      erase_if(minDefinitions, predicate);
      erase_if(minAssertions, predicate);
      // 4. Extract proof from unified scope and encapsulate it with split
      // scopes introducing minimized definitions and assertions.
      return d_pnm->mkNode(
          ProofRule::SCOPE,
          {d_pnm->mkNode(ProofRule::SCOPE, pf->getChildren(), minAssertions)},
          minDefinitions);
    }
    default: Unreachable();
  }
}

void PfManager::checkFinalProof(std::shared_ptr<ProofNode> pfn)
{
  // take stats and check pedantic
  d_finalCb.initializeUpdate();
  d_finalizer.process(pfn);

  std::stringstream serr;
  bool wasPedanticFailure = d_finalCb.wasPedanticFailure(serr);
  if (wasPedanticFailure)
  {
    AlwaysAssert(!wasPedanticFailure)
        << "ProofPostprocess::process: pedantic failure:" << std::endl
        << serr.str();
  }
}

void PfManager::printProof(std::ostream& out,
                           std::shared_ptr<ProofNode> fp,
                           options::ProofFormatMode mode,
                           ProofScopeMode scopeMode,
                           const std::map<Node, std::string>& assertionNames)
{
  Trace("smt-proof") << "PfManager::printProof: start " << mode << std::endl;
  // We don't want to invalidate the proof nodes in fp, since these may be
  // reused in further check-sat calls, or they may be used again if the
  // user asks for the proof again (in non-incremental mode). We don't need to
  // clone if the printing below does not modify the proof, which is the case
  // for proof formats ALF and NONE.
  if (mode != options::ProofFormatMode::CPC
      && mode != options::ProofFormatMode::NONE)
  {
    fp = fp->clone();
  }

  // according to the proof format, post process and print the proof node
  if (mode == options::ProofFormatMode::DOT)
  {
    proof::DotPrinter dotPrinter(d_env);
    dotPrinter.print(out, fp.get());
  }
  else if (mode == options::ProofFormatMode::CPC)
  {
    proof::AlfNodeConverter atp(nodeManager());
    proof::AlfPrinter alfp(d_env, atp, d_rewriteDb.get());
    alfp.print(out, fp, scopeMode);
  }
  else if (mode == options::ProofFormatMode::ALETHE)
  {
    options::ProofCheckMode oldMode = options().proof.proofCheck;
    d_pnm->getChecker()->setProofCheckMode(options::ProofCheckMode::NONE);
    proof::AletheNodeConverter anc(nodeManager(),
                                   options().proof.proofAletheDefineSkolems);
    proof::AletheProofPostprocess vpfpp(d_env, anc);
    if (vpfpp.process(fp))
    {
      proof::AletheProofPrinter vpp(d_env, anc);
      vpp.print(out, fp, assertionNames);
    }
    else
    {
      out << "(error " << vpfpp.getError() << ")";
    }
    d_pnm->getChecker()->setProofCheckMode(oldMode);
  }
  else if (mode == options::ProofFormatMode::LFSC)
  {
    Assert(fp->getRule() == ProofRule::SCOPE);
    proof::LfscNodeConverter ltp(nodeManager());
    proof::LfscProofPostprocess lpp(d_env, ltp);
    lpp.process(fp);
    proof::LfscPrinter lp(d_env, ltp, d_rewriteDb.get());
    lp.print(out, fp.get());
  }
  else
  {
    // otherwise, print using default printer
    // we call the printing method explicitly because we may want to print the
    // final proof node with conclusions
    fp->printDebug(out, options().proof.proofPrintConclusion);
  }
}

void PfManager::translateDifficultyMap(std::map<Node, Node>& dmap,
                                       Assertions& as)
{
  Trace("difficulty-proc") << "Translate difficulty start" << std::endl;
  Trace("difficulty") << "PfManager::translateDifficultyMap" << std::endl;
  if (dmap.empty())
  {
    return;
  }
  std::map<Node, Node> dmapp = dmap;
  dmap.clear();
  Trace("difficulty-proc") << "Get ppAsserts" << std::endl;
  std::vector<Node> ppAsserts;
  for (const std::pair<const Node, Node>& ppa : dmapp)
  {
    Trace("difficulty") << "  preprocess difficulty: " << ppa.second << " for "
                        << ppa.first << std::endl;
    // The difficulty manager should only report difficulty for preprocessed
    // assertions, or we will get an open proof below. This is ensured
    // internally by the difficuly manager.
    ppAsserts.push_back(ppa.first);
  }
  Trace("difficulty-proc") << "Make SAT refutation" << std::endl;
  // assume a SAT refutation from all input assertions that were marked
  // as having a difficulty
  CDProof cdp(d_env);
  Node fnode = nodeManager()->mkConst(false);
  cdp.addStep(fnode, ProofRule::SAT_REFUTATION, ppAsserts, {});
  std::shared_ptr<ProofNode> pf = cdp.getProofFor(fnode);
  Trace("difficulty-proc") << "Get final proof" << std::endl;
  std::shared_ptr<ProofNode> fpf = connectProofToAssertions(pf, as);
  Trace("difficulty-debug") << "Final proof is " << *fpf.get() << std::endl;
  // We are typically a SCOPE here, although if we are not, then the proofs
  // have no free assumptions. If this is the case, then the only difficulty
  // was incremented on auxiliary lemmas added during preprocessing. Since
  // there are no dependencies, then the difficulty map is empty.
  if (fpf->getRule() != ProofRule::SCOPE)
  {
    return;
  }
  fpf = fpf->getChildren()[0];
  // analyze proof
  Assert(fpf->getRule() == ProofRule::SAT_REFUTATION);
  const std::vector<std::shared_ptr<ProofNode>>& children = fpf->getChildren();
  DifficultyPostprocessCallback dpc;
  ProofNodeUpdater dpnu(d_env, dpc);
  Trace("difficulty-proc") << "Compute accumulated difficulty" << std::endl;
  // For each child of SAT_REFUTATION, we increment the difficulty on all
  // "source" free assumptions (see DifficultyPostprocessCallback) by the
  // difficulty of the preprocessed assertion.
  for (const std::shared_ptr<ProofNode>& c : children)
  {
    Node res = c->getResult();
    Assert(dmapp.find(res) != dmapp.end());
    Trace("difficulty-debug") << "  process: " << res << std::endl;
    Trace("difficulty-debug") << "  .dvalue: " << dmapp[res] << std::endl;
    Trace("difficulty-debug") << "  ..proof: " << *c.get() << std::endl;
    if (!dpc.setCurrentDifficulty(dmapp[res]))
    {
      continue;
    }
    dpnu.process(c);
  }
  // get the accumulated difficulty map from the callback
  dpc.getDifficultyMap(nodeManager(), dmap);
  Trace("difficulty-proc") << "Translate difficulty end" << std::endl;
}

ProofChecker* PfManager::getProofChecker() const { return d_pchecker.get(); }

ProofNodeManager* PfManager::getProofNodeManager() const { return d_pnm.get(); }

ProofLogger* PfManager::getProofLogger() const { return d_plog.get(); }

rewriter::RewriteDb* PfManager::getRewriteDatabase() const
{
  return d_rewriteDb.get();
}

PreprocessProofGenerator* PfManager::getPreprocessProofGenerator() const
{
  return d_pppg.get();
}

void PfManager::getAssertions(Assertions& as, std::vector<Node>& assertions)
{
  // note that the assertion list is always available
  const context::CDList<Node>& al = as.getAssertionList();
  for (const Node& a : al)
  {
    assertions.push_back(a);
  }
}

void PfManager::getDefinitionsAndAssertions(Assertions& as,
                                            std::vector<Node>& definitions,
                                            std::vector<Node>& assertions)
{
  const context::CDList<Node>& defs = as.getAssertionListDefinitions();
  for (const Node& d : defs)
  {
    // Keep treating (mutually) recursive functions as declarations +
    // assertions.
    if (d.getKind() == Kind::EQUAL)
    {
      definitions.push_back(d);
    }
  }
  const context::CDList<Node>& asserts = as.getAssertionList();
  for (const Node& a : asserts)
  {
    if (std::find(definitions.cbegin(), definitions.cend(), a)
        == definitions.cend())
    {
      assertions.push_back(a);
    }
  }
}

}  // namespace smt
}  // namespace cvc5::internal
