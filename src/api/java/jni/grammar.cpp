/******************************************************************************
 * Top contributors (to current version):
 *   Mudathir Mohamed, Aina Niemetz, Andres Noetzli
 *
 * This file is part of the cvc5 project.
 *
 * Copyright (c) 2009-2025 by the authors listed in the file AUTHORS
 * in the top-level source directory and their institutional affiliations.
 * All rights reserved.  See the file COPYING in the top-level source
 * directory for licensing information.
 * ****************************************************************************
 *
 * The cvc5 Java API.
 */

#include <cvc5/cvc5.h>

#include "api_utilities.h"
#include "io_github_cvc5_Grammar.h"

using namespace cvc5;

/*
 * Class:     io_github_cvc5_Grammar
 * Method:    copyGrammar
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL
Java_io_github_cvc5_Grammar_copyGrammar(JNIEnv* env, jclass, jlong pointer)
{
  CVC5_JAVA_API_TRY_CATCH_BEGIN;
  Grammar* current = reinterpret_cast<Grammar*>(pointer);
  Grammar* retPointer = new Grammar(*current);
  return reinterpret_cast<jlong>(retPointer);
  CVC5_JAVA_API_TRY_CATCH_END_RETURN(env, 0);
}

/*
 * Class:     io_github_cvc5_Grammar
 * Method:    deletePointer
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_io_github_cvc5_Grammar_deletePointer(JNIEnv*, jobject, jlong pointer)
{
  delete reinterpret_cast<Grammar*>(pointer);
}

/*
 * Class:     io_github_cvc5_Grammar
 * Method:    isNull
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_io_github_cvc5_Grammar_isNull(JNIEnv* env,
                                                              jobject,
                                                              jlong pointer)
{
  CVC5_JAVA_API_TRY_CATCH_BEGIN;
  Grammar* current = reinterpret_cast<Grammar*>(pointer);
  return static_cast<jboolean>(current->isNull());
  CVC5_JAVA_API_TRY_CATCH_END_RETURN(env, static_cast<jboolean>(false));
}

/*
 * Class:     io_github_cvc5_Grammar
 * Method:    equals
 * Signature: (JJ)Z
 */
JNIEXPORT jboolean JNICALL Java_io_github_cvc5_Grammar_equals(JNIEnv* env,
                                                              jobject,
                                                              jlong pointer1,
                                                              jlong pointer2)
{
  CVC5_JAVA_API_TRY_CATCH_BEGIN;
  Grammar* grammar1 = reinterpret_cast<Grammar*>(pointer1);
  Grammar* grammar2 = reinterpret_cast<Grammar*>(pointer2);
  // We compare the actual grammars, not their pointers.
  return static_cast<jboolean>(*grammar1 == *grammar2);
  CVC5_JAVA_API_TRY_CATCH_END_RETURN(env, static_cast<jboolean>(false));
}

/*
 * Class:     io_github_cvc5_Grammar
 * Method:    addRule
 * Signature: (JJJ)V
 */
JNIEXPORT void JNICALL
Java_io_github_cvc5_Grammar_addRule(JNIEnv* env,
                                        jobject,
                                        jlong pointer,
                                        jlong ntSymbolPointer,
                                        jlong rulePointer)
{
  CVC5_JAVA_API_TRY_CATCH_BEGIN;
  Grammar* current = reinterpret_cast<Grammar*>(pointer);
  Term* ntSymbol = reinterpret_cast<Term*>(ntSymbolPointer);
  Term* rule = reinterpret_cast<Term*>(rulePointer);
  current->addRule(*ntSymbol, *rule);
  CVC5_JAVA_API_TRY_CATCH_END(env);
}

/*
 * Class:     io_github_cvc5_Grammar
 * Method:    addRules
 * Signature: (JJ[J)V
 */
JNIEXPORT void JNICALL
Java_io_github_cvc5_Grammar_addRules(JNIEnv* env,
                                         jobject,
                                         jlong pointer,
                                         jlong ntSymbolPointer,
                                         jlongArray rulePointers)
{
  CVC5_JAVA_API_TRY_CATCH_BEGIN;
  Grammar* current = reinterpret_cast<Grammar*>(pointer);
  Term* ntSymbol = reinterpret_cast<Term*>(ntSymbolPointer);
  std::vector<Term> rules = getObjectsFromPointers<Term>(env, rulePointers);
  current->addRules(*ntSymbol, rules);
  CVC5_JAVA_API_TRY_CATCH_END(env);
}

/*
 * Class:     io_github_cvc5_Grammar
 * Method:    addAnyConstant
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_io_github_cvc5_Grammar_addAnyConstant(
    JNIEnv* env, jobject, jlong pointer, jlong ntSymbolPointer)
{
  CVC5_JAVA_API_TRY_CATCH_BEGIN;
  Grammar* current = reinterpret_cast<Grammar*>(pointer);
  Term* ntSymbol = reinterpret_cast<Term*>(ntSymbolPointer);
  current->addAnyConstant(*ntSymbol);
  CVC5_JAVA_API_TRY_CATCH_END(env);
}

/*
 * Class:     io_github_cvc5_Grammar
 * Method:    addAnyVariable
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_io_github_cvc5_Grammar_addAnyVariable(
    JNIEnv* env, jobject, jlong pointer, jlong ntSymbolPointer)
{
  CVC5_JAVA_API_TRY_CATCH_BEGIN;
  Grammar* current = reinterpret_cast<Grammar*>(pointer);
  Term* ntSymbol = reinterpret_cast<Term*>(ntSymbolPointer);
  current->addAnyVariable(*ntSymbol);
  CVC5_JAVA_API_TRY_CATCH_END(env);
}

/*
 * Class:     io_github_cvc5_Grammar
 * Method:    toString
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_io_github_cvc5_Grammar_toString(JNIEnv* env, jobject, jlong pointer)
{
  CVC5_JAVA_API_TRY_CATCH_BEGIN;
  Grammar* current = reinterpret_cast<Grammar*>(pointer);
  return env->NewStringUTF(current->toString().c_str());
  CVC5_JAVA_API_TRY_CATCH_END_RETURN(env, nullptr);
}

/*
 * Class:     io_github_cvc5_Grammar
 * Method:    hashCode
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_io_github_cvc5_Grammar_hashCode(JNIEnv* env,
                                                            jobject,
                                                            jlong pointer)
{
  CVC5_JAVA_API_TRY_CATCH_BEGIN;
  Grammar* grammar = reinterpret_cast<Grammar*>(pointer);
  return static_cast<jint>(std::hash<cvc5::Grammar>()(*grammar));
  CVC5_JAVA_API_TRY_CATCH_END_RETURN(env, 0);
}
