#include "tiger/absyn/absyn.h"
#include "tiger/semant/semant.h"

#include <iostream>
#include <unordered_map>

using namespace std;
namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry* entry = venv->Look(sym_);
  if (!entry || typeid(*entry) != typeid(env::VarEntry)) {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
    return type::IntTy::Instance();
  }
  return (static_cast<env::VarEntry*>(entry))->ty_->ActualTy();
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty* varTy;

  varTy = var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*varTy) != typeid(type::RecordTy)) {
    errormsg->Error(var_->pos_, "not a record type");
    return type::IntTy::Instance();
  }

  type::FieldList* fieldList = static_cast<type::RecordTy*>(varTy)->fields_;
  for (type::Field* field : fieldList->GetList()) {
    if (field->name_->Name() == sym_->Name()) {
      return field->ty_->ActualTy();
    }
  }

  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
  return type::IntTy::Instance();

}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty* varTy, * subscriptTy;

  varTy = var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*varTy) != typeid(type::ArrayTy)) {
    errormsg->Error(var_->pos_, "array type required");
    return type::IntTy::Instance();
  }

  subscriptTy = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*subscriptTy) != typeid(type::IntTy)) {
    errormsg->Error(subscript_->pos_, "ARRAY can only be subscripted by INT.");
    return type::IntTy::Instance();
  }

  return static_cast<type::ArrayTy*>(varTy)->ty_;
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  std::cout << "CallExp: " << func_->Name() << std::endl;

  env::EnvEntry* entry;
  env::FunEntry* funcEnt;
  type::TyList* formalList;

  entry = venv->Look(func_);
  if (!entry || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return type::VoidTy::Instance();
  }
  
  funcEnt = static_cast<env::FunEntry*>(entry);
  formalList = funcEnt->formals_;

  auto argIt = args_->GetList().begin();
  auto formalIt = formalList->GetList().begin();
  while (argIt != args_->GetList().end() && formalIt != formalList->GetList().end()) {
    type::Ty* argTy, * formalTy;
    formalTy = (*formalIt)->ActualTy();
    argTy = (*argIt)->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
    if (typeid(*argTy) != typeid(*formalTy)) {
      errormsg->Error((*argIt)->pos_, "para type mismatch");
      return type::VoidTy::Instance();
    }
    argIt++;
    formalIt++;
  }

  if (argIt != args_->GetList().end() || formalIt != formalList->GetList().end()) {
    int errorpos = argIt == args_->GetList().end() ? args_->GetList().back()->pos_ : (*argIt)->pos_;
    errormsg->Error(errorpos, "too many params in function %s", func_->Name().data());
    return type::VoidTy::Instance();
  }

  if (!funcEnt->result_)
    return type::VoidTy::Instance();

  return funcEnt->result_->ActualTy();
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *leftTy = left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *rightTy = right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (oper_ == absyn::PLUS_OP || oper_ == absyn::MINUS_OP 
    || oper_ == absyn::TIMES_OP || oper_==absyn::DIVIDE_OP) {
      if (typeid(*leftTy) != typeid(type::IntTy)) 
        errormsg->Error(left_->pos_, "integer required");
      if (typeid(*rightTy) != typeid(type::IntTy)) 
        errormsg->Error(left_->pos_, "integer required");
      return type::IntTy::Instance();
  } else {
    if (!leftTy->IsSameType(rightTy)) {
      errormsg->Error(left_->pos_, "same type required");
      return type::IntTy::Instance();
    }
    return type::IntTy::Instance();
  }
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty* ty = tenv->Look(typ_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return type::VoidTy::Instance();
  }
  return ty;
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty* ty;
  for (Exp* exp : seq_->GetList())
    ty = exp->SemAnalyze(venv, tenv, labelcount, errormsg);
  return ty;
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty* varTy = var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty* expTy = exp_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (!varTy->IsSameType(expTy)) {
    errormsg->Error(exp_->pos_, "unmatched assign exp");
    return type::VoidTy::Instance();
  }

  // Check if read-only.
  if (typeid(*var_) == typeid(SimpleVar)) {
    SimpleVar* simpVar = static_cast<SimpleVar*>(var_);
    if (venv->Look(simpVar->sym_)->readonly_) {
      errormsg->Error(var_->pos_, "loop variable can't be assigned");
      return type::VoidTy::Instance();
    }
  }

  return type::VoidTy::Instance();
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty* testTy, * thenTy, * elseTy;

  testTy = test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*testTy) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "integer required for if test condition");
    return type::VoidTy::Instance();
  }

  thenTy = then_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (!elsee_ && typeid(*thenTy) != typeid(type::VoidTy)) {
    errormsg->Error(then_->pos_, "if-then exp's body must produce no value");
    return type::VoidTy::Instance();
  }

  if (elsee_) {
    elseTy = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
    if (!thenTy->IsSameType(elseTy)) {
      errormsg->Error(elsee_->pos_, "then exp and else exp type mismatch");
      return thenTy;
    }
  }

  return thenTy;
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  venv->BeginScope();
  tenv->BeginScope();

  type::Ty* testTy, * bodyTy;

  testTy = test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*testTy) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "while test condition must produce INT value.");
    return type::VoidTy::Instance();
  }

  bodyTy = body_->SemAnalyze(venv, tenv, -1, errormsg)->ActualTy();
  if (typeid(*bodyTy) != typeid(type::VoidTy)) {
    errormsg->Error(body_->pos_, "while body must produce no value");
    return type::VoidTy::Instance();
  }

  venv->EndScope();
  tenv->EndScope();

  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty* loTy, * hiTy, * bodyTy;

  venv->BeginScope();
  tenv->BeginScope();

  venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true));

  loTy = lo_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*loTy) != typeid(type::IntTy)) {
    errormsg->Error(lo_->pos_, "for exp's range type is not integer");
    // venv->EndScope();
    // return type::VoidTy::Instance();
  }

  hiTy = hi_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*hiTy) != typeid(type::IntTy)) {
    errormsg->Error(hi_->pos_, "for exp's range type is not integer");
    // venv->EndScope();
    // return type::VoidTy::Instance();
  }

  bodyTy = body_->SemAnalyze(venv, tenv, -1, errormsg)->ActualTy();
  if (typeid(*bodyTy) != typeid(type::VoidTy)) {
    errormsg->Error(body_->pos_, "for body must produce no value.");
    venv->EndScope();
    return type::VoidTy::Instance();
  }

  venv->EndScope();
  tenv->EndScope();

  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  if (labelcount != -1)
    errormsg->Error(pos_, "break is not inside any loop");
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  venv->BeginScope();
  tenv->BeginScope();

  for (Dec* dec : decs_->GetList()) {
    std::cout << "LetExp: dec is " << typeid(*dec).name() << std::endl;
    dec->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  type::Ty *result = body_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  venv->EndScope();
  tenv->EndScope();

  return result;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty* arrayTy, * sizeTy, * initTy;
  // type::ArrayTy* castTy;

  arrayTy = tenv->Look(typ_);
  if (!arrayTy || typeid(*arrayTy->ActualTy()) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "no array type %s", typ_->Name().data());
    return type::IntTy::Instance();
  }

  // castTy = static_cast<type::ArrayTy*>(arrayTy);

  sizeTy = size_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*sizeTy) != typeid(type::IntTy)) {
    errormsg->Error(size_->pos_, "integer required for array size");
    return arrayTy;
  }

  initTy = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (initTy->ActualTy() != static_cast<type::ArrayTy*>(arrayTy->ActualTy())->ty_->ActualTy()) {
    errormsg->Error(init_->pos_, "type mismatch");
    return arrayTy;
  }
  
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty* resultTy, * bodyTy;
  type::TyList* formals;
  unordered_map<string, int> functionRecord;

  for (FunDec* function : functions_->GetList()) {
    if (functionRecord.count(function->name_->Name())) {
      errormsg->Error(function->pos_, "two functions have the same name");
      return;
    }
    functionRecord[function->name_->Name()] = 1;
    resultTy = tenv->Look(function->result_);
    formals = function->params_->MakeFormalTyList(tenv, errormsg);
    venv->Enter(function->name_, new env::FunEntry(formals, resultTy));
  }

  for (FunDec* function : functions_->GetList()) {
    resultTy = tenv->Look(function->result_);
    formals = function->params_->MakeFormalTyList(tenv, errormsg);
    
    venv->BeginScope();

    auto paramIt = function->params_->GetList().begin();
    auto formalIt = formals->GetList().begin();
    for (; formalIt != formals->GetList().end(); paramIt++, formalIt++) {
      venv->Enter((*paramIt)->name_, new env::VarEntry(*formalIt));
    }

    bodyTy = function->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!resultTy && typeid(*bodyTy) != typeid(type::VoidTy)) {
      errormsg->Error(function->body_->pos_, "procedure returns value");
    } else if (resultTy && !bodyTy->IsSameType(resultTy)) {
      errormsg->Error(function->body_->pos_, "");
    }

    venv->EndScope();
  }
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty* ty, * initTy;

  if (typ_) {
    std::cout << "VarDec: typ_ is " << typ_->Name() << std::endl;
    ty = tenv->Look(typ_);
    if (!ty) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
      return;
    }
  }

  initTy = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*initTy) == typeid(type::NilTy) && (!typ_ || typeid(*ty) != typeid(type::RecordTy))) {
    errormsg->Error(init_->pos_, "init should not be nil without type specified");
    return;
  } else if (typ_ && ty->ActualTy() != initTy->ActualTy()) {
    errormsg->Error(init_->pos_, "type mismatch");
    return;
  }

  venv->Enter(var_, new env::VarEntry(initTy));
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty* ty;
  type::NameTy* tenvTy;
  unordered_map<string, int> typeRecord;

  for (NameAndTy* nameAndTy : types_->GetList()) {
    // cout << "TypeDec 1: " << nameAndTy->name_->Name() << endl;
    if (typeRecord.count(nameAndTy->name_->Name())) {
      errormsg->Error(nameAndTy->ty_->pos_, "two types have the same name");
      return;
    }
    typeRecord[nameAndTy->name_->Name()] = 1;
    tenv->Enter(nameAndTy->name_, new type::NameTy(nameAndTy->name_, NULL));
  }

  for (NameAndTy* nameAndTy : types_->GetList()) {
    // cout << "TypeDec 2: " << nameAndTy->name_->Name() << endl;
    ty = tenv->Look(nameAndTy->name_);
    if (!ty) {
      errormsg->Error(nameAndTy->ty_->pos_, "undefined type %s", nameAndTy->name_->Name().data());
      return;
    }
    tenvTy = static_cast<type::NameTy*>(ty);
    tenvTy->ty_ = nameAndTy->ty_->SemAnalyze(tenv, errormsg);
    tenv->Set(nameAndTy->name_, tenvTy);
  }

  // Detect cycle
  for (NameAndTy* nameAndTy : types_->GetList()) {
    // cout << "TypeDec 3: " << nameAndTy->name_->Name() << endl;
    ty = static_cast<type::NameTy*>(tenv->Look(nameAndTy->name_))->ty_;
    while (typeid(*ty) == typeid(type::NameTy)) {
      tenvTy = static_cast<type::NameTy*>(ty);
      if (tenvTy->sym_->Name() == nameAndTy->name_->Name()) {
        errormsg->Error(nameAndTy->ty_->pos_, "illegal type cycle");
        return;
      }
      ty = tenvTy->ty_;
    }
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty* ty = tenv->Look(name_);
  return ty;
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::FieldList* fieldList = record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(fieldList);
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return new type::ArrayTy(tenv->Look(array_));
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

} // namespace tr
