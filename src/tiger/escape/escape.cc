#include "tiger/escape/escape.h"
#include "tiger/absyn/absyn.h"

#include <iostream>

namespace esc {
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
} // namespace esc

namespace absyn {

void AbsynTree::Traverse(esc::EscEnvPtr env) {
  root_->Traverse(env, 0);
}

void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
  esc::EscapeEntry * entry = env->Look(sym_);
  if (entry && entry->depth_ < depth) {
    *(entry->escape_) = true;
  }
}

void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
}

void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
  subscript_->Traverse(env, depth);
}

void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
}

void NilExp::Traverse(esc::EscEnvPtr env, int depth) {
}

void IntExp::Traverse(esc::EscEnvPtr env, int depth) {
}

void StringExp::Traverse(esc::EscEnvPtr env, int depth) {
}

void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
  for (Exp * arg : args_->GetList())
    arg->Traverse(env, depth);
}

void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
  left_->Traverse(env, depth);
  right_->Traverse(env, depth);
}

void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
  for (EField * field : fields_->GetList()) 
    field->exp_->Traverse(env, depth);
}

void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
  for (Exp * exp : seq_->GetList())
    exp->Traverse(env, depth);
}

void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
  exp_->Traverse(env, depth);
}

void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
  test_->Traverse(env, depth);
  then_->Traverse(env, depth);
  if (elsee_)
    elsee_->Traverse(env, depth);
}

void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
  test_->Traverse(env, depth);
  body_->Traverse(env, depth);
}

void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
  lo_->Traverse(env, depth);
  hi_->Traverse(env, depth);

  env->BeginScope();
  escape_ = false;
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
  body_->Traverse(env, depth);
  env->EndScope();
}

void BreakExp::Traverse(esc::EscEnvPtr env, int depth) {
}

void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
  env->BeginScope();
  for (Dec * dec : decs_->GetList())
    dec->Traverse(env, depth);
  body_->Traverse(env, depth);
  env->EndScope();
}

void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
  size_->Traverse(env, depth);
  init_->Traverse(env, depth);
}

void VoidExp::Traverse(esc::EscEnvPtr env, int depth) {
}

void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {
  for (FunDec * function : functions_->GetList()) {
    env->BeginScope();
    for (Field * param : function->params_->GetList()) {
      param->escape_ = false;
      env->Enter(param->name_, new esc::EscapeEntry(depth + 1, &(param->escape_)));
    }
    function->body_->Traverse(env, depth + 1);
    env->EndScope();
  }
}

void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
  escape_ = false;
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
  init_->Traverse(env, depth);
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) {
}

} // namespace absyn
