/******************************************************************************
 * Top contributors (to current version):
 *   Morgan Deters, Andrew Reynolds, Mathias Preiner
 *
 * This file is part of the cvc5 project.
 *
 * Copyright (c) 2009-2025 by the authors listed in the file AUTHORS
 * in the top-level source directory and their institutional affiliations.
 * All rights reserved.  See the file COPYING in the top-level source
 * directory for licensing information.
 * ****************************************************************************
 *
 * A substitution mapping for theory simplification.
 */

#include "cvc5_private.h"

#ifndef CVC5__THEORY__SUBSTITUTIONS_H
#define CVC5__THEORY__SUBSTITUTIONS_H

//#include <algorithm>
#include <utility>
#include <vector>
#include <unordered_map>

#include "expr/node.h"
#include "context/context.h"
#include "context/cdo.h"
#include "context/cdhashmap.h"
#include "util/hash.h"

namespace cvc5::internal {
namespace theory {

class Rewriter;

/**
 * The type for the Substitutions mapping output by
 * Theory::simplify(), TheoryEngine::simplify(), and
 * Valuation::simplify().  This is in its own header to
 * avoid circular dependences between those three.
 *
 * This map is context-dependent.
 */
class SubstitutionMap
{
 public:
  typedef context::CDHashMap<Node, Node> NodeMap;

  typedef NodeMap::iterator iterator;
  typedef NodeMap::const_iterator const_iterator;

  struct ShouldTraverseCallback
  {
    virtual bool operator()(TNode n) const = 0;
    virtual ~ShouldTraverseCallback() {}
  };

 private:
  typedef std::unordered_map<Node, Node> NodeCache;
  /** A dummy context used by this class if none is provided */
  context::Context d_context;

  /** The variables, in order of addition */
  NodeMap d_substitutions;

  /** Cache of the already performed substitutions */
  NodeCache d_substitutionCache;

  /** Has the cache been invalidated? */
  bool d_cacheInvalidated;

  /** Are we using substitution compression */
  bool d_compress;

  /** Internal method that performs substitution */
  Node internalSubstitute(TNode t,
                          NodeCache& cache,
                          std::set<TNode>* tracker,
                          const ShouldTraverseCallback* stc);

  /** Helper class to invalidate cache on user pop */
  class CacheInvalidator : public context::ContextNotifyObj
  {
    bool& d_cacheInvalidated;

   protected:
    void contextNotifyPop() override { d_cacheInvalidated = true; }

   public:
    CacheInvalidator(context::Context* context, bool& cacheInvalidated)
        : context::ContextNotifyObj(context),
          d_cacheInvalidated(cacheInvalidated)
    {
    }

  }; /* class SubstitutionMap::CacheInvalidator */

  /**
   * This object is notified on user pop and marks the SubstitutionMap's
   * cache as invalidated.
   */
  CacheInvalidator d_cacheInvalidator;

 public:
  /**
   * @param context The context this substitution depends on.
   * @param compress If true, we may update the range of substitutions based
   * on further substitutions. For example, if we add {y -> f(x)} and later
   * add {x -> a}, then we may update the substitution to {y -> f(a), x -> a}.
   */
  SubstitutionMap(context::Context* context = nullptr, bool compress = true);

  /** Get substitutions in this object as a raw map */
  std::unordered_map<Node, Node> getSubstitutions() const;
  /**
   * Return a formula that is equivalent to this substitution, e.g. for
   * [x -> t, y -> s], we return (and (= x t) (= y s)).
   */
  Node toFormula(NodeManager* nm) const;
  /**
   * Adds a substitution from x to t.
   */
  void addSubstitution(TNode x, TNode t, bool invalidateCache = true);

  /**
   * Merge subMap into current set of substitutions
   */
  void addSubstitutions(SubstitutionMap& subMap, bool invalidateCache = true);

  /**
   * Erase substitution. This erases x from the domain of this substitution.
   * This method should only be called if compression is disabled, since
   * if compression is enabled, then the substituion of x may have been
   * applied to the range of other substitutions in this class, and erasing
   * the entry for x would not undo those changes.
   * @param x The variable to erase.
   * @param invalidateCache If true, we clear the cache.
   */
  void eraseSubstitution(TNode x, bool invalidateCache = true);

  /** Size of the substitutions */
  size_t size() const { return d_substitutions.size(); }
  /**
   * Returns true iff x is in the substitution map
   */
  bool hasSubstitution(TNode x) const
  {
    return d_substitutions.find(x) != d_substitutions.end();
  }

  /**
   * Returns the substitution mapping that was given for x via
   * addSubstitution().  Note that the returned value might itself
   * be in the map; for the actual substitution that would be
   * performed for x, use .apply(x).  This getSubstitution() function
   * is mainly intended for constructing assertions about what has
   * already been put in the map.
   */
  TNode getSubstitution(TNode x) const
  {
    AssertArgument(
        hasSubstitution(x), x, "element not in this substitution map");
    return (*d_substitutions.find(x)).second;
  }

  /**
   * Apply the substitutions to the node, optionally rewrite if a non-null
   * Rewriter pointer is passed.
   */
  Node apply(TNode t,
             Rewriter* r = nullptr,
             std::set<TNode>* tracker = nullptr,
             const ShouldTraverseCallback* stc = nullptr);

  /**
   * Apply the substitutions to the node.
   */
  Node apply(TNode t, Rewriter* r = nullptr) const
  {
    return const_cast<SubstitutionMap*>(this)->apply(t, r);
  }

  iterator begin() { return d_substitutions.begin(); }

  iterator end() { return d_substitutions.end(); }

  const_iterator begin() const { return d_substitutions.begin(); }

  const_iterator end() const { return d_substitutions.end(); }

  bool empty() const { return d_substitutions.empty(); }

  /**
   * Print to the output stream
   */
  void print(std::ostream& out) const;
  /** To string */
  std::string toString() const;

  void invalidateCache() {
    d_cacheInvalidated = true;
  }

}; /* class SubstitutionMap */

inline std::ostream& operator << (std::ostream& out, const SubstitutionMap& subst) {
  subst.print(out);
  return out;
}

}  // namespace theory

std::ostream& operator<<(std::ostream& out, const theory::SubstitutionMap::iterator& i);

}  // namespace cvc5::internal

#endif /* CVC5__THEORY__SUBSTITUTIONS_H */
