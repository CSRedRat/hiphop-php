/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010- Facebook, Inc. (http://www.facebook.com)         |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#include <compiler/statement/function_statement.h>
#include <compiler/analysis/analysis_result.h>
#include <compiler/expression/expression_list.h>
#include <compiler/analysis/file_scope.h>
#include <compiler/analysis/function_scope.h>
#include <compiler/statement/statement_list.h>
#include <util/util.h>
#include <compiler/expression/parameter_expression.h>
#include <compiler/expression/modifier_expression.h>
#include <compiler/option.h>
#include <compiler/analysis/variable_table.h>
#include <compiler/analysis/class_scope.h>
#include <sstream>

using namespace HPHP;

///////////////////////////////////////////////////////////////////////////////
// constructors/destructors

FunctionStatement::FunctionStatement
(STATEMENT_CONSTRUCTOR_PARAMETERS,
 ModifierExpressionPtr modifiers, bool ref, const std::string &name,
 ExpressionListPtr params, const std::string &retTypeConstraint,
 StatementListPtr stmt, int attr, const std::string &docComment,
 ExpressionListPtr attrList)
  : MethodStatement(STATEMENT_CONSTRUCTOR_PARAMETER_VALUES(FunctionStatement),
                    modifiers, ref, name, params, retTypeConstraint, stmt,
                    attr, docComment, attrList, false), m_ignored(false) {
}

StatementPtr FunctionStatement::clone() {
  FunctionStatementPtr stmt(new FunctionStatement(*this));
  stmt->m_stmt = Clone(m_stmt);
  stmt->m_params = Clone(m_params);
  stmt->m_modifiers = Clone(m_modifiers);
  return stmt;
}

///////////////////////////////////////////////////////////////////////////////
// parser functions

void FunctionStatement::onParse(AnalysisResultConstPtr ar, FileScopePtr scope) {
  // Correctness checks are normally done before adding function to scope.
  if (m_params) {
    for (int i = 0; i < m_params->getCount(); i++) {
      ParameterExpressionPtr param =
        dynamic_pointer_cast<ParameterExpression>((*m_params)[i]);
      if (param->hasTypeHint() && param->defaultValue()) {
        param->compatibleDefault();
      }
    }
  }
  // note it's important to add to scope, not a pushed FunctionContainer,
  // as a function may be declared inside a class's method, yet this function
  // is a global function, not a class method.
  FunctionScopePtr fs = onInitialParse(ar, scope);
  FunctionScope::RecordFunctionInfo(m_name, fs);
  if (!scope->addFunction(ar, fs)) {
    m_ignored = true;
    return;
  }

  if (Option::PersistenceHook) {
    fs->setPersistent(Option::PersistenceHook(fs, scope));
  }
}

///////////////////////////////////////////////////////////////////////////////
// static analysis functions

std::string FunctionStatement::getName() const {
  return string("Function ") + getScope()->getName();
}

void FunctionStatement::analyzeProgram(AnalysisResultPtr ar) {
  FunctionScopeRawPtr fs = getFunctionScope();
  // redeclared functions are automatically volatile
  if (fs->isVolatile()) {
    FunctionScopeRawPtr func =
      getScope()->getOuterScope()->getContainingFunction();
    if (func) {
      func->getVariables()->setAttribute(VariableTable::NeedGlobalPointer);
    }
  }
  MethodStatement::analyzeProgram(ar);
}

void FunctionStatement::inferTypes(AnalysisResultPtr ar) {
}

///////////////////////////////////////////////////////////////////////////////
// code generation functions

void FunctionStatement::outputPHPHeader(CodeGenerator &cg,
                                        AnalysisResultPtr ar) {
  cg_printf("function ");
  if (m_ref) cg_printf("&");
  if (!ParserBase::IsClosureOrContinuationName(m_name)) {
    cg_printf("%s", m_name.c_str());
  }
  cg_printf("(");
  if (m_params) m_params->outputPHP(cg, ar);
  cg_printf(")");
}

void FunctionStatement::outputPHPBody(CodeGenerator &cg,
                                      AnalysisResultPtr ar) {
  FunctionScopeRawPtr funcScope = getFunctionScope();
  cg_indentBegin(" {\n");
  funcScope->outputPHP(cg, ar);
  if (m_stmt) m_stmt->outputPHP(cg, ar);
  cg_indentEnd("}\n");
}

void FunctionStatement::outputPHP(CodeGenerator &cg, AnalysisResultPtr ar) {
  FunctionScopeRawPtr funcScope = getFunctionScope();
  if (funcScope->isUserFunction()) {
    outputPHPHeader(cg, ar);
    outputPHPBody(cg, ar);
  }
}

bool FunctionStatement::hasImpl() const {
  return true;
}
