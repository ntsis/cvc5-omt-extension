/******************************************************************************
 * Top contributors (to current version):
 *   Mudathir Mohamed, Daniel Larraz, Andrew Reynolds
 *
 * This file is part of the cvc5 project.
 *
 * Copyright (c) 2009-2025 by the authors listed in the file AUTHORS
 * in the top-level source directory and their institutional affiliations.
 * All rights reserved.  See the file COPYING in the top-level source
 * directory for licensing information.
 * ****************************************************************************
 *
 * A simple demonstration of the Sygus API.
 *
 * A simple demonstration of how to use the Sygus API to synthesize a simple
 * invariant. This is a direct translation of sygus-inv.cpp.
 */

import static io.github.cvc5.Kind.*;

import io.github.cvc5.*;

public class SygusInv
{
  public static void main(String args[]) throws CVC5ApiException
  {
    TermManager tm = new TermManager();
    Solver slv = new Solver(tm);
    {
      // required options
      slv.setOption("sygus", "true");
      slv.setOption("incremental", "false");

      // set the logic
      slv.setLogic("LIA");

      Sort integer = tm.getIntegerSort();
      Sort bool = tm.getBooleanSort();

      Term zero = tm.mkInteger(0);
      Term one = tm.mkInteger(1);
      Term ten = tm.mkInteger(10);

      // declare input variables for functions
      Term x = tm.mkVar(integer, "x");
      Term xp = tm.mkVar(integer, "xp");

      // (ite (< x 10) (= xp (+ x 1)) (= xp x))
      Term ite = tm.mkTerm(ITE,
          tm.mkTerm(LT, x, ten),
          tm.mkTerm(EQUAL, xp, tm.mkTerm(ADD, x, one)),
          tm.mkTerm(EQUAL, xp, x));

      // define the pre-conditions, transition relations, and post-conditions
      Term pre_f = slv.defineFun("pre-f", new Term[] {x}, bool, tm.mkTerm(EQUAL, x, zero));
      Term trans_f = slv.defineFun("trans-f", new Term[] {x, xp}, bool, ite);
      Term post_f = slv.defineFun("post-f", new Term[] {x}, bool, tm.mkTerm(LEQ, x, ten));

      // declare the invariant-to-synthesize
      Term inv_f = slv.synthFun("inv-f", new Term[] {x}, bool);

      slv.addSygusInvConstraint(inv_f, pre_f, trans_f, post_f);

      // print solutions if available
      if (slv.checkSynth().hasSolution())
      {
        // Output should be equivalent to:
        // (
        //   (define-fun inv-f ((x Int)) Bool (not (>= x 11)))
        // )
        Term[] terms = new Term[] {inv_f};
        Utils.printSynthSolutions(terms, slv.getSynthSolutions(terms));
      }
    }
    Context.deletePointers();
  }
}
