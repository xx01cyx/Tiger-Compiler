#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>
#include <iostream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;
constexpr int wordsize = 8;


} // namespace

namespace cg {

void CodeGen::Codegen() {
  /* TODO: Put your lab5 code here */
  tree::StmList *stm_list = traces_.get()->GetStmList();
  assem::InstrList *instr_list = new assem::InstrList();
  for (auto stm : stm_list->GetList())
    stm->Munch(*instr_list, fs_);
  // Pseudo instruction regarding return sink should be appended here
  // to inform the compiler of the sinking registers.
  instr_list = frame::ProcEntryExit2(instr_list);
  assem_instr_ = std::make_unique<AssemInstr>(instr_list);
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {
/* TODO: Put your lab5 code here */
static assem::MemFetch *MunchMem(tree::Exp* mem_exp, int ordinal, 
                                 assem::InstrList &instr_list, std::string_view fs) {
  tree::Exp *exp = static_cast<tree::MemExp *>(mem_exp)->exp_;
  std::stringstream mem_ss;
  if (typeid(*exp) == typeid(tree::BinopExp) 
      && static_cast<tree::BinopExp *>(exp)->op_ == tree::PLUS_OP) {
    tree::BinopExp *bin_exp = static_cast<tree::BinopExp *>(exp);
    if (typeid(*bin_exp->right_) == typeid(tree::ConstExp)) {
      tree::ConstExp *offset_exp = static_cast<tree::ConstExp *>(bin_exp->right_);
      temp::Temp *base_reg = bin_exp->left_->Munch(instr_list, fs);
      if (offset_exp->consti_ == 0)
        mem_ss << "(`s" << ordinal << ")";
      else
        mem_ss << offset_exp->consti_ <<  "(`s" << ordinal << ")";
      return new assem::MemFetch(mem_ss.str(), new temp::TempList(base_reg));

    } else if (typeid(*bin_exp->left_) == typeid(tree::ConstExp)) {
      tree::ConstExp *offset_exp = static_cast<tree::ConstExp *>(bin_exp->left_);
      temp::Temp *base_reg = bin_exp->right_->Munch(instr_list, fs);
      if (offset_exp->consti_ == 0)
        mem_ss << "(`s" << ordinal << ")";
      else
        mem_ss << offset_exp->consti_ <<  "(`s" << ordinal << ")";
      return new assem::MemFetch(mem_ss.str(), new temp::TempList(base_reg));

    } else {
      temp::Temp* mem_reg = exp->Munch(instr_list, fs);
      mem_ss << "(`s" << ordinal << ")";
      return new assem::MemFetch(mem_ss.str(), new temp::TempList(mem_reg));
    }

 } else {
   temp::Temp* mem_reg = exp->Munch(instr_list, fs);
    mem_ss << "(`s" << ordinal << ")";
    return new assem::MemFetch(mem_ss.str(), new temp::TempList(mem_reg));
  }
}

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::LabelInstr(label_->Name(), label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  std::string instr_str = "jmp " + exp_->name_->Name();
  instr_list.Append(new assem::OperInstr(instr_str, nullptr, nullptr, 
                      new assem::Targets(jumps_)));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  /* No two immediates appear at the same time. The immediate is always
     the first operand for cmpq. */
  std::stringstream instr_ss;

  if (typeid(*right_) == typeid(tree::ConstExp)) {
    tree::ConstExp *right_const = static_cast<tree::ConstExp *>(right_);
    temp::Temp *left_reg = left_->Munch(instr_list, fs);
    instr_ss << "cmpq $" << right_const->consti_ << ", `s0";
    instr_list.Append(new assem::OperInstr(instr_ss.str(), nullptr, 
                        new temp::TempList(left_reg), nullptr));

  } else {
    temp::Temp *left_reg = left_->Munch(instr_list, fs);
    temp::Temp *right_reg = right_->Munch(instr_list, fs);
    instr_list.Append(new assem::OperInstr("cmpq `s0, `s1", nullptr,
                        new temp::TempList({right_reg, left_reg}), nullptr));
  }

  instr_ss.str("");

  switch (op_) {
  case EQ_OP:
    instr_ss << "je " << true_label_->Name(); break;
  case NE_OP:
    instr_ss << "jne " << true_label_->Name(); break;
  case LT_OP:
    instr_ss << "jl " << true_label_->Name(); break;
  case GT_OP:
    instr_ss << "jg " << true_label_->Name(); break;
  case LE_OP:
    instr_ss << "jle " << true_label_->Name(); break;
  case GE_OP:
    instr_ss << "jge " << true_label_->Name(); break;
  default:
    return;  // error
  }
  instr_list.Append(new assem::OperInstr(instr_ss.str(), nullptr, nullptr,
                      new assem::Targets(new std::vector<temp::Label *>{true_label_})));
                      
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  std::stringstream instr_ss;

  if (typeid(*dst_) == typeid(tree::MemExp)) {  // movq r, M
    temp::Temp *src_reg = src_->Munch(instr_list, fs);
    assem::MemFetch *fetch = MunchMem(dst_, 1, instr_list, fs);
    instr_ss << "movq `s0, " << fetch->fetch_;
    temp::TempList *src_regs = new temp::TempList(src_reg);
    src_regs->CatList(fetch->regs_);
    instr_list.Append(new assem::OperInstr(instr_ss.str(), 
                        nullptr, src_regs, nullptr));

  } else {
    if (typeid(*src_) == typeid(tree::MemExp)) {  // movq M, r
      assem::MemFetch *fetch = MunchMem(src_, 0, instr_list, fs);
      temp::Temp *dst_reg = dst_->Munch(instr_list, fs);
      instr_ss << "movq " << fetch->fetch_ << ", `d0";
      instr_list.Append(new assem::OperInstr(instr_ss.str(),
                          new temp::TempList(dst_reg),
                          fetch->regs_, nullptr));

    } else if (typeid(*src_) == typeid(tree::ConstExp)) {  // movq $imm, r
      temp::Temp *dst_reg = dst_->Munch(instr_list, fs);
      instr_ss << "movq $" << static_cast<tree::ConstExp *>(src_)->consti_
               << ", `d0";
      instr_list.Append(new assem::OperInstr(instr_ss.str(), 
                          new temp::TempList(dst_reg),
                          nullptr, nullptr));

    } else {  // movq r1, r2
      temp::Temp *src_reg = src_->Munch(instr_list, fs);
      temp::Temp *dst_reg = dst_->Munch(instr_list, fs);
      instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                          new temp::TempList(dst_reg),
                          new temp::TempList(src_reg)));
    }
  }
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  std::string assem_instr;
  std::stringstream instr_ss;

  // leaq fs(r1), r2
  // frame pointer
  if (op_ == PLUS_OP && (typeid(*right_) == typeid(tree::NameExp))
      && static_cast<tree::NameExp *>(right_)->name_->Name() == fs) {
      temp::Temp *left_reg = left_->Munch(instr_list, fs);
      temp::Temp *res_reg = temp::TempFactory::NewTemp();
      instr_ss << "leaq " << fs << "(`s0), `d0";
      instr_list.Append(new assem::OperInstr(instr_ss.str(), 
                          new temp::TempList(res_reg), 
                          new temp::TempList(left_reg), 
                          nullptr));
      return res_reg;
  }

  if (op_ == PLUS_OP || op_ == MINUS_OP) {

    if (op_ == PLUS_OP)
      assem_instr = "addq";
    else
      assem_instr = "subq";
  
    temp::Temp *left_reg = left_->Munch(instr_list, fs);
    temp::Temp* res_reg = temp::TempFactory::NewTemp();

    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                        new temp::TempList(res_reg),
                        new temp::TempList(left_reg)));

    if (typeid(*right_) == typeid(tree::ConstExp)) {  // instr $imm, r
      tree::ConstExp *right_const = static_cast<tree::ConstExp *>(right_);
      instr_ss << assem_instr << " $" << right_const->consti_ << ", `d0";
      instr_list.Append(new assem::OperInstr(instr_ss.str(),
                          new temp::TempList({res_reg}),
                          new temp::TempList(res_reg),
                          nullptr));
      return res_reg;

    } else {  // instr r1, r2
      temp::Temp *right_reg = right_->Munch(instr_list, fs);
      instr_ss << assem_instr << " `s1, `d0";
      instr_list.Append(new assem::OperInstr(instr_ss.str(),
                          new temp::TempList({res_reg}),
                          new temp::TempList({res_reg, right_reg}),
                          nullptr));
      return res_reg;
    }

  } else if (op_ == MUL_OP || op_ == DIV_OP) {

    if (op_ == MUL_OP)
      assem_instr = "imulq";
    else
      assem_instr = "idivq";

    temp::Temp *rax = reg_manager->ReturnValue();
    temp::Temp *rdx = reg_manager->ArithmeticAssistant();
    temp::Temp *rax_saver = temp::TempFactory::NewTemp();
    temp::Temp *rdx_saver = temp::TempFactory::NewTemp();

    // Save %rax and %rdx
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                        new temp::TempList(rax_saver), 
                        new temp::TempList(rax)));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                        new temp::TempList(rdx_saver), 
                        new temp::TempList(rdx)));

    if (typeid(*left_) == typeid(tree::ConstExp)) {  // movq $imm, %rax
      tree::ConstExp *left_const = static_cast<tree::ConstExp *>(left_);
      instr_ss << "movq $" << left_const->consti_ << ", `d0";
      instr_list.Append(new assem::OperInstr(instr_ss.str(),
                          new temp::TempList(rax), 
                          nullptr, nullptr));

    } else if (typeid(*left_) == typeid(tree::MemExp)) {  // movq M, %rax
      assem::MemFetch *fetch = MunchMem(left_, 0, instr_list, fs);
      instr_ss << "movq " << fetch->fetch_ << ", `d0";
      instr_list.Append(new assem::OperInstr(instr_ss.str(),
                          new temp::TempList(rax),
                          fetch->regs_, nullptr));

    } else {  // movq r, %rax
      temp::Temp *left_reg = left_->Munch(instr_list, fs);
      instr_ss << "movq `s0, `d0";
      instr_list.Append(new assem::MoveInstr(instr_ss.str(),
                          new temp::TempList(rax), 
                          new temp::TempList(left_reg)));
    }

    instr_ss.str("");

    if (op_ == DIV_OP)
      instr_list.Append(new assem::OperInstr("cqto", 
                          new temp::TempList({rdx, rax, rax_saver, rdx_saver}),
                          new temp::TempList(rax),
                          nullptr));
    
    temp::Temp *right_reg = right_->Munch(instr_list, fs);
    instr_ss << assem_instr << " `s2";
    instr_list.Append(new assem::OperInstr(instr_ss.str(),
                        new temp::TempList({rdx, rax, rax_saver, rdx_saver}),
                        new temp::TempList({rdx, rax, right_reg}),
                        nullptr));

    // Move the result to a new register.
    temp::Temp *res_reg = temp::TempFactory::NewTemp();
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                        new temp::TempList(res_reg), 
                        new temp::TempList(rax)));

    // Restore %rax and %rdx
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                        new temp::TempList(rax), 
                        new temp::TempList(rax_saver)));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                        new temp::TempList(rdx), 
                        new temp::TempList(rdx_saver)));

    return res_reg;
  }

  return temp::TempFactory::NewTemp();  // error

}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *reg = temp::TempFactory::NewTemp();
  assem::MemFetch* fetch = MunchMem(this, 0, instr_list, fs);
  std::stringstream instr_ss;
  instr_ss << "movq " << fetch->fetch_ << ", `d0";
  instr_list.Append(new assem::OperInstr(instr_ss.str(), 
                      new temp::TempList(reg), 
                      fetch->regs_, nullptr));
  return reg;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  return temp_;
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  stm_->Munch(instr_list, fs);
  return exp_->Munch(instr_list, fs);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *reg = temp::TempFactory::NewTemp();
  std::stringstream instr_ss;
  // load address
  instr_ss << "leaq " << name_->Name() << "(%rip), `d0";
  assem::Instr *instr = new assem::OperInstr(instr_ss.str(), 
                          new temp::TempList(reg), 
                          nullptr, nullptr);
  instr_list.Append(instr);
  return reg;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *reg = temp::TempFactory::NewTemp();
  std::stringstream instr_ss;
  instr_ss << "movq $" << consti_ << ", `d0";
  assem::Instr *instr = new assem::OperInstr(instr_ss.str(), 
                          new temp::TempList(reg), 
                          nullptr, nullptr);
  instr_list.Append(instr);
  return reg;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *rax = reg_manager->ReturnValue();
  std::stringstream instr_ss;

  if (typeid(*fun_) != typeid(tree::NameExp))  // error
    return rax; 

  temp::TempList *arg_list = args_->MunchArgs(instr_list, fs);
  temp::TempList *calldefs = reg_manager->CallerSaves();
  calldefs->Append(reg_manager->ReturnValue());

  instr_ss << "callq " << static_cast<tree::NameExp *>(fun_)->name_->Name();
  instr_list.Append(new assem::OperInstr(instr_ss.str(), 
                      calldefs, arg_list, nullptr));
  return rax;
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::TempList *arg_list = new temp::TempList();
  std::stringstream instr_ss;
  int arg_reg_count = reg_manager->ArgRegs()->GetList().size();
  int i = 0;

  for (tree::Exp *arg : this->GetList()) {  // static link is included
    if (i < arg_reg_count) {
      temp::Temp *dst_reg = reg_manager->ArgRegs()->NthTemp(i);
      if (typeid(*arg) == typeid(tree::ConstExp)) {
        tree::ConstExp *const_exp = static_cast<tree::ConstExp *>(arg);
        instr_ss << "movq $" << const_exp->consti_ << ", `d0";
        instr_list.Append(new assem::OperInstr(instr_ss.str(), 
                            new temp::TempList(dst_reg), nullptr, nullptr));
      } else {
        temp::Temp *src_reg = arg->Munch(instr_list, fs);
        instr_ss << "movq `s0, `d0";
        instr_list.Append(new assem::MoveInstr(instr_ss.str(), 
                            new temp::TempList(dst_reg), 
                            new temp::TempList(src_reg)));
      }
      arg_list->Append(dst_reg);

    } else {  // the first one goes to (%rsp)
      if (typeid(*arg) == typeid(tree::ConstExp)) {
        tree::ConstExp *const_exp = static_cast<tree::ConstExp *>(arg);
        instr_ss << "movq $" << const_exp->consti_ << ", ";
        if (i != arg_reg_count)
          instr_ss << (i - arg_reg_count) * wordsize;
        instr_ss << "(" << *reg_manager->temp_map_->Look(reg_manager->StackPointer())
                 << ")";
        instr_list.Append(new assem::OperInstr(instr_ss.str(), 
                            nullptr, new temp::TempList(reg_manager->StackPointer()), nullptr));
      } else {
        temp::Temp *src_reg = arg->Munch(instr_list, fs);
        instr_ss << "movq `s0, ";
        if (i != arg_reg_count)
          instr_ss << (i - arg_reg_count) * wordsize;
        instr_ss << "(" << *reg_manager->temp_map_->Look(reg_manager->StackPointer())
                 << ")";
        instr_list.Append(new assem::OperInstr(instr_ss.str(), 
                            nullptr,
                            new temp::TempList({src_reg, reg_manager->StackPointer()}), 
                            nullptr));
      }
    }
    instr_ss.str("");
    ++i;
  }

  if (i > arg_reg_count)
    arg_list->Append(reg_manager->StackPointer());

  return arg_list;
}



} // namespace tree
