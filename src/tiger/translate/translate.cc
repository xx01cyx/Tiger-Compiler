#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/x64frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/frame.h"

#include <iostream>
#include <unordered_map>

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  /* TODO: Put your lab5 code here */
  frame::Frame *frame = level->frame_;
  frame::Access *access = frame->AllocLocal(escape);
  return new Access(level, access);
}

class Cx {
public:
  temp::Label **trues_;
  temp::Label **falses_;
  tree::Stm *stm_;

  Cx(temp::Label **trues, temp::Label **falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() const = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() const = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) const = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() const override { 
    /* TODO: Put your lab5 code here */
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    // if (typeid(*exp_) == typeid(tree::ConstExp)) {
    //   tree::ConstExp *cexp = static_cast<tree::ConstExp *>(exp_);
    //   if 
    // }
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    tree::Stm *stm = new tree::CjumpStm(tree::NE_OP, new tree::ConstExp(0), exp_, t, f);
    return Cx(&t, &f, stm);
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() const override { 
    /* TODO: Put your lab5 code here */
    return stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    errormsg->Error(errormsg->GetTokPos(), "No result cannot be cast to conditional.");
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(temp::Label** trues, temp::Label** falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}
  
  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    temp::Temp *reg = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();

    // Cx should jump to the new true label and false label instead of previous ones.
    *cx_.trues_ = t;
    *cx_.falses_ = f;

    return new tree::EseqExp(new tree::MoveStm(new tree::TempExp(reg), new tree::ConstExp(1)),
            new tree::EseqExp(cx_.stm_,
              new tree::EseqExp(new tree::LabelStm(f),
                new tree::EseqExp(new tree::MoveStm(new tree::TempExp(reg), new tree::ConstExp(0)),
                  new tree::EseqExp(new tree::LabelStm(t),
                    new tree::TempExp(reg))))));
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    return cx_.stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override { 
    /* TODO: Put your lab5 code here */
    return cx_;
  }
};

void ProgTr::Translate() {
  /* TODO: Put your lab5 code here */
  frame::Frame *new_frame = frame::NewFrame(temp::LabelFactory::NewLabel(), std::vector<bool>());
  Level *main_level = new Level(new_frame, outermost_level_.get());
  absyn_tree_->Translate(venv_.get(), tenv_.get(), main_level, nullptr, errormsg_.get());
}

// FIXME: formals?
void ProcEntryExit(Level *level, Exp *body) {
  frame::ProcFrag *frag = new frame::ProcFrag(body->UnNx(), level->frame_);
  frags->PushBack(frag);
}

} // namespace tr

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  env::EnvEntry *ent = venv->Look(sym_);
  if (!ent) {
    errormsg->Error(pos_, "variable %s not exist", sym_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance()); 
  }

  if (typeid(*ent) != typeid(env::VarEntry)) {
    errormsg->Error(pos_, "%s is not a variable", sym_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  env::VarEntry *var_ent = static_cast<env::VarEntry *>(ent);
  tr::Access *dec_acc = var_ent->access_;
  tr::Level *dec_level = dec_acc->level_;
  tr::Level *cur_level = level;
  tree::Exp *fp_exp = new tree::TempExp(cur_level->frame_->FramePointer());
  
  // follow the static links
  while (cur_level != dec_level) {
    fp_exp = frame::AccessExp(cur_level->frame_->formal_access_.front(), fp_exp);
    cur_level = cur_level->parent_;
  }

  // now at declare level
  tree::Exp *var_exp = frame::AccessExp(dec_acc->access_, fp_exp);
  return new tr::ExpAndTy(new tr::ExExp(var_exp), var_ent->ty_);

}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_expty = var_->Translate(venv, tenv, level, label, errormsg);
  tree::Exp *var_exp = var_expty->exp_->UnEx();
  type::Ty *var_ty = var_expty->ty_;

  if (typeid(*var_ty->ActualTy()) != typeid(type::RecordTy)) {
    errormsg->Error(var_->pos_, "not a record type");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  // record is bound to be in the frame
  type::RecordTy *rec = static_cast<type::RecordTy *>(var_ty);
  int k = 0;
  for (type::Field *field : rec->fields_->GetList()) {
    if (field->name_->Name() == sym_->Name()) {
      tree::Exp *exp = new tree::MemExp(
        new tree::BinopExp(tree::PLUS_OP, var_exp, 
          new tree::ConstExp(k * level->frame_->WordSize())));
      return new tr::ExpAndTy(new tr::ExExp(exp), field->ty_->ActualTy());
    }
    k++;
  }

  errormsg->Error(pos_, "no field named %s", sym_->Name().data());
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());

}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_expty = var_->Translate(venv, tenv, level, label, errormsg);
  tree::Exp *var_exp = var_expty->exp_->UnEx();
  type::Ty *var_ty = var_expty->ty_;

  if (typeid(*var_ty->ActualTy()) != typeid(type::ArrayTy)) {
    errormsg->Error(var_->pos_, "not an array type");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  tr::ExpAndTy *subscript_expty = subscript_->Translate(venv, tenv, level, label, errormsg);
  tree::Exp *subscript_exp = subscript_expty->exp_->UnEx();

  if (typeid(*(subscript_expty->ty_->ActualTy())) != typeid(type::IntTy)) {
    errormsg->Error(subscript_->pos_, "require integer array subsription");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  // array is bound to be in the frame
  type::ArrayTy *array = static_cast<type::ArrayTy *>(var_ty);
  tree::Exp *exp = new tree::MemExp(
    new tree::BinopExp(tree::PLUS_OP, var_exp, 
      new tree::BinopExp(tree::MUL_OP, subscript_exp, 
        new tree::ConstExp(level->frame_->WordSize()))));
  return new tr::ExpAndTy(new tr::ExExp(exp), array->ty_);

}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)), type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  temp::Label *str_label = temp::LabelFactory::NewLabel();
  frags->PushBack(new frame::StringFrag(str_label, str_));
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(str_label)), type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  env::EnvEntry *ent = venv->Look(func_);
  if (!ent || typeid(*ent) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  env::FunEntry *func_ent = static_cast<env::FunEntry *>(ent);
  tree::Exp *func_exp = new tree::NameExp(func_);
  tree::ExpList *args = new tree::ExpList();
  for (Exp *arg : args_->GetList()) {
    tree::Exp *arg_exp = arg->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx();
    args->Append(arg_exp);
  }
  tree::Exp *call_exp = new tree::CallExp(func_exp, args);
  return new tr::ExpAndTy(new tr::ExExp(call_exp), func_ent->result_);
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *left_expty = left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *right_expty = right_->Translate(venv, tenv, level, label, errormsg);
  tree::Exp *left_exp = left_expty->exp_->UnEx();
  tree::Exp *right_exp = right_expty->exp_->UnEx();
  tr::Exp *exp;
  tree::CjumpStm *cjump;

  if (typeid(*left_expty->ty_->ActualTy()) == typeid(type::IntTy)
      && typeid(*right_expty->ty_->ActualTy()) == typeid(type::IntTy)) {
        switch (oper_) {
        case absyn::PLUS_OP:
          exp = new tr::ExExp(new tree::BinopExp(tree::PLUS_OP, left_exp, right_exp));
          break;
        case absyn::MINUS_OP:
          exp = new tr::ExExp(new tree::BinopExp(tree::MINUS_OP, left_exp, right_exp));
          break;
        case absyn::TIMES_OP:
          exp = new tr::ExExp(new tree::BinopExp(tree::MUL_OP, left_exp, right_exp));
          break;
        case absyn::DIVIDE_OP:
          exp = new tr::ExExp(new tree::BinopExp(tree::DIV_OP, left_exp, right_exp));
          break;
        case absyn::EQ_OP:
          cjump = new tree::CjumpStm(tree::EQ_OP, left_exp, right_exp, nullptr, nullptr);
          exp = new tr::CxExp(&cjump->true_label_, &cjump->false_label_, cjump);
        case absyn::NEQ_OP:
          cjump = new tree::CjumpStm(tree::NE_OP, left_exp, right_exp, nullptr, nullptr);
          exp = new tr::CxExp(&cjump->true_label_, &cjump->false_label_, cjump);
        case absyn::GT_OP:
          cjump = new tree::CjumpStm(tree::GT_OP, left_exp, right_exp, nullptr, nullptr);
          exp = new tr::CxExp(&cjump->true_label_, &cjump->false_label_, cjump);
        case absyn::GE_OP:
          cjump = new tree::CjumpStm(tree::GE_OP, left_exp, right_exp, nullptr, nullptr);
          exp = new tr::CxExp(&cjump->true_label_, &cjump->false_label_, cjump);
        case absyn::LT_OP:
          cjump = new tree::CjumpStm(tree::LT_OP, left_exp, right_exp, nullptr, nullptr);
          exp = new tr::CxExp(&cjump->true_label_, &cjump->false_label_, cjump);
        case absyn::LE_OP:
          cjump = new tree::CjumpStm(tree::LE_OP, left_exp, right_exp, nullptr, nullptr);
          exp = new tr::CxExp(&cjump->true_label_, &cjump->false_label_, cjump);
        default:
          errormsg->Error(pos_, "unexpected binary token for integer");
          return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
        }
  } else if (typeid(*left_expty->ty_->ActualTy()) == typeid(type::StringTy)
      && typeid(*right_expty->ty_->ActualTy()) == typeid(type::StringTy)) {
        tree::ExpList *args = new tree::ExpList({left_exp, right_exp});
        std::string string_equal = "string_equal";

        switch (oper_) {
        case absyn::EQ_OP:
          exp = new tr::ExExp(frame::ExternalCall(string_equal, args));
        case absyn::NEQ_OP:
          exp = new tr::ExExp(new tree::BinopExp(tree::MINUS_OP, new tree::ConstExp(1), 
                  frame::ExternalCall(string_equal, args)));
        default:
          errormsg->Error(pos_, "unexpected binary token for string");
          return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
        }
  }

  if (!exp) {
    errormsg->Error(pos_, "binary operation type mismatch");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  return new tr::ExpAndTy(exp, type::IntTy::Instance());
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,      
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty* ty = tenv->Look(typ_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  } else if (typeid(*ty->ActualTy()) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "type %s is not a record", typ_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  type::RecordTy *rec_ty = static_cast<type::RecordTy *>(ty);
  auto fields = rec_ty->fields_->GetList();
  auto efields = fields_->GetList();
  int n = fields.size();
  tree::Exp *reg_exp = new tree::TempExp(temp::TempFactory::NewTemp());
  tree::ExpList *call_args = new tree::ExpList({new tree::ConstExp(n ? n : level->frame_->WordSize())});
  tree::Stm *malloc_stm = new tree::MoveStm(reg_exp, frame::ExternalCall("alloc_record", call_args));

  if (fields.empty() && efields.empty()) {  // record has no field
    return new tr::ExpAndTy(new tr::ExExp(new tree::EseqExp(malloc_stm, reg_exp)), rec_ty);
  } else if (fields.empty() || efields.empty()) {
    errormsg->Error(pos_, "field type mismatch");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  type::Field *last_field = fields.back();
  EField *last_efield = efields.back();
  tr::ExpAndTy *last_efield_expty = last_efield->exp_->Translate(venv, tenv, level, label, errormsg);
  if (typeid(last_field->ty_->ActualTy()) != typeid(last_efield_expty->ty_->ActualTy())) {
    errormsg->Error(pos_, "field type mismatch");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  tree::Stm *stm = new tree::MoveStm(new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, reg_exp, 
                    new tree::ConstExp((--n) * level->frame_->WordSize()))), last_efield_expty->exp_->UnEx());
  auto field_it = ++fields.rbegin();
  auto efield_it = ++efields.rbegin();
  for (; field_it != fields.rend() && efield_it != efields.rend(); field_it++, efield_it++) {
    tr::ExpAndTy *efield_expty = (*efield_it)->exp_->Translate(venv, tenv, level, label, errormsg);
    if (typeid(*efield_expty->ty_->ActualTy()) != typeid((*field_it)->ty_->ActualTy())) {
      errormsg->Error(pos_, "field type mismatch");
      return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
    }
    tree::Exp *efield_exp = efield_expty->exp_->UnEx();
    stm = new tree::SeqStm(new tree::MoveStm(new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, reg_exp,
            new tree::ConstExp((--n) * level->frame_->WordSize()))), efield_expty->exp_->UnEx()), stm);
  }

  if (field_it != fields.rend() || efield_it != efields.rend()) {
    errormsg->Error(pos_, "fields mismatch");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  stm = new tree::SeqStm(malloc_stm, stm);
  tree::Exp *res_exp = new tree::EseqExp(stm, reg_exp);

  return new tr::ExpAndTy(new tr::ExExp(res_exp), rec_ty);
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *last_expty = seq_->GetList().back()->Translate(venv, tenv, level, label, errormsg);
  tree::Exp *exp = last_expty->exp_->UnEx();

  for (auto e_it = ++seq_->GetList().rbegin(); e_it != seq_->GetList().rend(); ++e_it) {
    tree::Exp *e_exp = (*e_it)->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx();
    exp = new tree::EseqExp(new tree::ExpStm(e_exp), exp);
  }

  return new tr::ExpAndTy(new tr::ExExp(exp), last_expty->ty_);
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,                       
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_expty = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *exp_expty = exp_->Translate(venv, tenv, level, label, errormsg);
  if (typeid(*var_expty->ty_->ActualTy()) != typeid(exp_expty->ty_->ActualTy())) {
    errormsg->Error(exp_->pos_, "unmatched assign exp");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  tree::Exp *var_exp = var_expty->exp_->UnEx();
  tree::Exp *exp_exp = exp_expty->exp_->UnEx();
  tree::Stm *assign_stm = new tree::MoveStm(var_exp, exp_exp);
  return new tr::ExpAndTy(new tr::NxExp(assign_stm), var_expty->ty_);
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *test_expty = test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *then_expty = then_->Translate(venv, tenv, level, label, errormsg);
  tr::Cx test_cx = test_expty->exp_->UnCx(errormsg);

  temp::Label *true_label = temp::LabelFactory::NewLabel();
  temp::Label *false_label = temp::LabelFactory::NewLabel();

  if (!elsee_) {
    if (typeid(*then_expty->ty_) != typeid(type::VoidTy)) {
      errormsg->Error(then_->pos_, "if with no else must return no value");
      return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
    }

    if (typeid(*then_expty->ty_) == typeid(type::VoidTy)) {
      std::cout << "if without else returns no value" << std::endl;
      *test_cx.trues_ = true_label;
      *test_cx.falses_ = false_label;
      tree::Stm* stm = new tree::SeqStm(test_cx.stm_, 
                        new tree::SeqStm(new tree::LabelStm(*test_cx.trues_), 
                          new tree::SeqStm(then_expty->exp_->UnNx(), 
                            new tree::LabelStm(*test_cx.falses_))));
      return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());

    } else {
      errormsg->Error(then_->pos_, "if-then exp's body must produce no value");
      return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
    }

  } else {
    tr::ExpAndTy *else_expty = elsee_->Translate(venv, tenv, level, label, errormsg);
    if (typeid(*then_expty->ty_->ActualTy()) != typeid(*else_expty->ty_->ActualTy())) {
      errormsg->Error(elsee_->pos_, "then exp and else exp type mismatch");
      return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
    }

    *test_cx.trues_ = true_label;
    *test_cx.falses_ = false_label;
    
    if (typeid(*then_expty->ty_) == typeid(type::VoidTy)) {
      std::cout << "if with else returns no value" << std::endl;
      tree::Stm *stm = new tree::SeqStm(test_cx.stm_,
                          new tree::SeqStm(new tree::LabelStm(*test_cx.trues_), 
                            new tree::SeqStm(then_expty->exp_->UnNx(),
                              new tree::SeqStm(new tree::LabelStm(*test_cx.falses_),
                                else_expty->exp_->UnNx()))));
      return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());

    } else {
      std::cout << "if with else returns some value" << std::endl;
      temp::Temp *reg = temp::TempFactory::NewTemp();
      tree::Exp *reg_exp = new tree::TempExp(reg);
      *test_cx.trues_ = true_label;
      *test_cx.falses_ = false_label;
      tree::Exp *exp = new tree::EseqExp(test_cx.stm_,
                          new tree::EseqExp(new tree::LabelStm(*test_cx.trues_),
                            new tree::EseqExp(new tree::MoveStm(reg_exp, then_expty->exp_->UnEx()),
                              new tree::EseqExp(new tree::LabelStm(*test_cx.falses_),
                                new tree::EseqExp(new tree::MoveStm(reg_exp, else_expty->exp_->UnEx()), reg_exp)))));
      return new tr::ExpAndTy(new tr::ExExp(exp), then_expty->ty_);
    }
  }
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,            
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *test_expty = test_->Translate(venv, tenv, level, label, errormsg);
  tr::Cx test_cx = test_expty->exp_->UnCx(errormsg);

  temp::Label *test_label = temp::LabelFactory::NewLabel();
  temp::Label *body_label = temp::LabelFactory::NewLabel();
  temp::Label *done_label = temp::LabelFactory::NewLabel();
  tr::ExpAndTy *body_expty = body_->Translate(venv, tenv, level, done_label, errormsg);
  
  if (typeid(*body_expty->ty_) != typeid(type::VoidTy)) {
    errormsg->Error(body_->pos_, "while body must produce no value");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  *test_cx.trues_ = body_label;
  *test_cx.falses_ = done_label;
  std::vector<temp::Label *> *jumps = new std::vector<temp::Label *>{done_label};
  tree::Stm *jump_stm = new tree::JumpStm(new tree::NameExp(done_label), jumps);
  tree::Stm *while_stm = new tree::SeqStm(new tree::LabelStm(test_label),
                          new tree::SeqStm(test_cx.stm_,
                            new tree::SeqStm(new tree::LabelStm(body_label),
                              new tree::SeqStm(body_expty->exp_->UnNx(), 
                                new tree::SeqStm(jump_stm, new tree::LabelStm(done_label))))));
  return new tr::ExpAndTy(new tr::NxExp(while_stm), type::VoidTy::Instance());

}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  sym::Symbol *limit_sym = sym::Symbol::UniqueSymbol("limit");  // FIXME!
  sym::Symbol *int_sym = sym::Symbol::UniqueSymbol("int");

  DecList *decs = new DecList();
  decs->Prepend(new VarDec(hi_->pos_, limit_sym, int_sym, hi_));
  decs->Prepend(new VarDec(lo_->pos_, var_, int_sym, lo_));


  SimpleVar *it_var = new SimpleVar(pos_, var_);
  SimpleVar *limit_var = new SimpleVar(pos_, limit_sym);
  VarExp *limit_exp = new VarExp(pos_, limit_var);
  VarExp *it_exp = new VarExp(pos_, it_var);

  OpExp *test_exp = new OpExp(pos_, LE_OP, it_exp, limit_exp);
  ExpList *body_exps = new ExpList();
  body_exps->Prepend(new AssignExp(pos_, it_var, 
    new OpExp(pos_, PLUS_OP, it_exp, new IntExp(pos_, 1))));
  body_exps->Prepend(body_);
  SeqExp *seq_exp = new SeqExp(pos_, body_exps);

  WhileExp *while_exp = new WhileExp(pos_, test_exp, seq_exp);
  LetExp* let_exp = new LetExp(while_exp->pos_, decs, while_exp);

  return let_exp->Translate(venv, tenv, level, label, errormsg);

}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  std::vector<temp::Label *> *jumps = new std::vector<temp::Label *>{label};
  tree::Stm *jump_stm = new tree::JumpStm(new tree::NameExp(label), jumps);
  return new tr::ExpAndTy(new tr::NxExp(jump_stm), type::IntTy::Instance());
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  venv->BeginScope();
  tenv->BeginScope();

  auto dec_it = decs_->GetList().rbegin();
  if (dec_it == decs_->GetList().rend()) {  // declist is empty. (possible?)
    tr::ExpAndTy *body_expty = body_->Translate(venv, tenv, level, label, errormsg);
    if (typeid((body_expty->exp_)) == typeid(tr::NxExp)) {
      tree::Stm *body_stm = body_expty->exp_->UnNx();
      venv->EndScope();
      tenv->EndScope();
      return new tr::ExpAndTy(new tr::NxExp(body_stm), type::VoidTy::Instance());
    } else {
      tree::Exp *body_exp = body_expty->exp_->UnEx();
      venv->EndScope();
      tenv->EndScope();
      return new tr::ExpAndTy(new tr::ExExp(body_exp), body_expty->ty_);
    }
  }

  tree::Stm *dec_stm = (*dec_it)->Translate(venv, tenv, level, label, errormsg)->UnNx();
  dec_it++;
  for (; dec_it != decs_->GetList().rend(); dec_it++)
    dec_stm = new tree::SeqStm((*dec_it)->Translate(venv, tenv, level, label, errormsg)->UnNx(), dec_stm);
  
  tr::ExpAndTy *body_expty = body_->Translate(venv, tenv, level, label, errormsg);
  if (typeid((body_expty->exp_)) == typeid(tr::NxExp)) {
    tree::Stm *body_stm = body_expty->exp_->UnNx();
    tree::Stm *seq_stm = new tree::SeqStm(dec_stm, body_stm);
    venv->EndScope();
    tenv->EndScope();
    return new tr::ExpAndTy(new tr::NxExp(seq_stm), type::VoidTy::Instance());
  } else {
    tree::Exp *body_exp = body_expty->exp_->UnEx();
    tree::Exp *eseq_exp = new tree::EseqExp(dec_stm, body_exp);
    venv->EndScope();
    tenv->EndScope();
    return new tr::ExpAndTy(new tr::ExExp(eseq_exp), body_expty->ty_);
  }
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,                    
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty* ty = tenv->Look(typ_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  } else if (typeid(*ty->ActualTy()) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "type %s is not an array", typ_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  type::ArrayTy *arr_ty = static_cast<type::ArrayTy *>(ty);
  tr::ExpAndTy *size_expty = size_->Translate(venv, tenv, level, label, errormsg);
  if (typeid(*size_expty->ty_->ActualTy()) != typeid(type::IntTy)) {
    errormsg->Error(size_->pos_, "integer required for array size");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }
  tr::ExpAndTy *init_expty = init_->Translate(venv, tenv, level, label, errormsg);
  if (typeid(*init_expty->ty_->ActualTy()) != typeid(arr_ty->ty_->ActualTy())) {
    errormsg->Error(init_->pos_, "type mismatch");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  tree::Exp *reg_exp = new tree::TempExp(temp::TempFactory::NewTemp());
  tree::ExpList *call_args = new tree::ExpList({size_expty->exp_->UnEx(), init_expty->exp_->UnEx()});
  tree::Stm *init_stm = new tree::MoveStm(reg_exp, frame::ExternalCall("init_array", call_args));
  tree::Exp *res_exp = new tree::EseqExp(init_stm, reg_exp);
  return new tr::ExpAndTy(new tr::ExExp(res_exp), arr_ty);
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty* result_ty;
  type::TyList* formals;
  std::unordered_map<std::string, int> functionRecord;

  for (FunDec *function : functions_->GetList()) {
    if (functionRecord.count(function->name_->Name())) {
      errormsg->Error(function->pos_, "two functions have the same name");
      return new tr::ExExp(new tree::ConstExp(0));
    }
    functionRecord[function->name_->Name()] = 1;
    result_ty = tenv->Look(function->result_);
    formals = function->params_->MakeFormalTyList(tenv, errormsg);
    venv->Enter(function->name_, new env::FunEntry(level, label, formals, result_ty));
  }

  for (FunDec* function : functions_->GetList()) {
    result_ty = tenv->Look(function->result_);
    formals = function->params_->MakeFormalTyList(tenv, errormsg);
    
    venv->BeginScope();

    std::vector<bool> formal_escapes = std::vector<bool>{true};
    for (auto param : function->params_->GetList()) 
      formal_escapes.push_back(param->escape_);
    
    temp::Label *fun_label = temp::LabelFactory::NewLabel();
    frame::Frame* new_frame = frame::NewFrame(fun_label, formal_escapes);
    tr::Level *new_level = new tr::Level(new_frame, level);
    std::vector<frame::Access *> formal_access = new_frame->formal_access_;

    auto param_it = function->params_->GetList().cbegin();
    auto formal_it = formals->GetList().cbegin();
    auto acc_it = formal_access.cbegin() + 1;  // the first one goes to static link
    for (; formal_it != formals->GetList().cend(); param_it++, formal_it++, acc_it++)
      venv->Enter((*param_it)->name_, new env::VarEntry(new tr::Access(new_level, *acc_it), *formal_it));
    
    tr::ExpAndTy* body_expty = function->body_->Translate(venv, tenv, new_level, label, errormsg);
    if (!result_ty && typeid(*body_expty->ty_) != typeid(type::VoidTy)) {
      errormsg->Error(function->body_->pos_, "procedure returns value");
    } else if (result_ty && typeid(*body_expty->ty_->ActualTy()) != typeid(*result_ty->ActualTy())) {
      errormsg->Error(function->body_->pos_, "return type of function %s mismatch", 
                        function->name_->Name().data());
    }

    // FIXME!
    tree::Stm *body_stm = frame::ProcEntryExit1(new_frame, body_expty->exp_->UnNx());
    tr::ProcEntryExit(level, new tr::NxExp(body_stm));

    venv->EndScope();
  }

  return new tr::NxExp(new tree::ExpStm(new tree::ConstExp(0)));
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty* ty;
  if (typ_) {
    ty = tenv->Look(typ_);
    if (!ty) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
      return new tr::ExExp(new tree::ConstExp(0));
    }
  }

  tr::ExpAndTy *init_expty = init_->Translate(venv, tenv, level, label, errormsg);
  if (typeid(*init_expty->ty_) == typeid(type::NilTy) 
    && (!typ_ || typeid(*ty) != typeid(type::RecordTy))) {
      errormsg->Error(init_->pos_, "init should not be nil without type specified");
      return new tr::ExExp(new tree::ConstExp(0));
  } else if (typ_ && typeid(*init_expty->ty_) != typeid(*ty->ActualTy())) {
    errormsg->Error(init_->pos_, "type mismatch");
    return new tr::ExExp(new tree::ConstExp(0));
  }

  frame::Access *acc = level->frame_->AllocLocal(escape_);
  tr::Access *var_acc = new tr::Access(level, acc);
  env::EnvEntry *ent = new env::VarEntry(var_acc, init_expty->ty_);
  venv->Enter(var_, ent);

  tree::Exp *acc_exp = frame::AccessExp(acc, new tree::TempExp(level->frame_->FramePointer()));
  tree::Stm *dec_stm = new tree::MoveStm(acc_exp, init_expty->exp_->UnEx());

  return new tr::NxExp(dec_stm);
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty* ty;
  type::NameTy* tenvTy;
  std::unordered_map<std::string, int> typeRecord;

  for (NameAndTy* nameAndTy : types_->GetList()) {
    if (typeRecord.count(nameAndTy->name_->Name())) {
      errormsg->Error(nameAndTy->ty_->pos_, "two types have the same name");
      return new tr::ExExp(new tree::ConstExp(0));
    }
    typeRecord[nameAndTy->name_->Name()] = 1;
    tenv->Enter(nameAndTy->name_, new type::NameTy(nameAndTy->name_, NULL));
  }

  for (NameAndTy* nameAndTy : types_->GetList()) {
    ty = tenv->Look(nameAndTy->name_);
    if (!ty) {
      errormsg->Error(nameAndTy->ty_->pos_, "undefined type %s", nameAndTy->name_->Name().data());
      return new tr::ExExp(new tree::ConstExp(0));
    }
    tenvTy = static_cast<type::NameTy*>(ty);
    tenvTy->ty_ = nameAndTy->ty_->Translate(tenv, errormsg);
    tenv->Set(nameAndTy->name_, tenvTy);
  }

  // Detect cycle
  for (NameAndTy* nameAndTy : types_->GetList()) {
    ty = static_cast<type::NameTy*>(tenv->Look(nameAndTy->name_))->ty_;
    while (typeid(*ty) == typeid(type::NameTy)) {
      tenvTy = static_cast<type::NameTy*>(ty);
      if (tenvTy->sym_->Name() == nameAndTy->name_->Name()) {
        errormsg->Error(nameAndTy->ty_->pos_, "illegal type cycle");
        return new tr::ExExp(new tree::ConstExp(0));
      }
      ty = tenvTy->ty_;
    }
  }

  return new tr::NxExp(new tree::ExpStm(new tree::ConstExp(0)));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return tenv->Look(name_);
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::FieldList* fieldList = record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(fieldList);
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new type::ArrayTy(tenv->Look(array_));
}

} // namespace absyn
