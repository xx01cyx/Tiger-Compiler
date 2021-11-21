#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */

/* TODO: Put your lab5 code here */
X64RegManager::X64RegManager() {
  for (int i = 0; i < REG_COUNT; ++i) {
    regs_.push_back(temp::TempFactory::NewTemp());
  }
}

temp::TempList *X64RegManager::Registers() {
  temp::TempList *tempList = new temp::TempList();
  for (int reg = 0; reg < REG_COUNT; ++reg) {
    if (reg != RSI)
      tempList->Append(regs_.at(reg));
  }
  return tempList;
}

temp::TempList *X64RegManager::ArgRegs() {
  temp::TempList *tempList = new temp::TempList({
    regs_.at(RDI),
    regs_.at(RSI),
    regs_.at(RDX),
    regs_.at(RCX),
    regs_.at(R8),
    regs_.at(R9),
  });
  return tempList;
}

temp::TempList *X64RegManager::CallerSaves() {
  temp::TempList *tempList = new temp::TempList({
    regs_.at(RAX),
    regs_.at(RSP),
    regs_.at(RDI),
    regs_.at(RSI),
    regs_.at(RDX),
    regs_.at(RCX),
    regs_.at(R8),
    regs_.at(R9),
    regs_.at(R10),
    regs_.at(R11),
  });
  return tempList;
}

temp::TempList *X64RegManager::CalleeSaves() {
  temp::TempList *tempList = new temp::TempList({
    regs_.at(RBX), 
    regs_.at(RBP), 
    regs_.at(R12),
    regs_.at(R13), 
    regs_.at(R14), 
    regs_.at(R15),
  });
  return tempList;
}

temp::TempList *X64RegManager::ReturnSink() {
  return new temp::TempList();
}

int X64RegManager::WordSize() {
  return WORD_SIZE;
}

temp::Temp *X64RegManager::FramePointer() {
  return regs_.at(RBP);
}

temp::Temp *X64RegManager::StackPointer() {
  return regs_.at(RSP);
}

temp::Temp *X64RegManager::ReturnValue() {
  return regs_.at(RAX);
}

class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  /* TODO: Put your lab5 code here */
};


class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  /* TODO: Put your lab5 code here */
};

class X64Frame : public Frame {
  /* TODO: Put your lab5 code here */
public:
  X64Frame(temp::Label *name) : Frame(name) {
    word_size_ = X64_WORD_SIZE;
  }

  Access *AllocLocal(bool escape) override;
  temp::Temp *FramePointer() const override;
  int WordSize() const override;

};

Access *X64Frame::AllocLocal(bool escape) {
  Access *access;
  if (escape) {
    local_count_++;
    access = new InFrameAccess(local_count_ * word_size_);
  } else {
    access = new InRegAccess(temp::TempFactory::NewTemp());
  }
  local_access_.push_back(access);
  return access;
}

temp::Temp *X64Frame::FramePointer() const {
  return nullptr;
}

int X64Frame::WordSize() const {
  return word_size_;
}

Frame *NewFrame(temp::Label *name, std::vector<bool> formals) {
  Frame* frame = new X64Frame(name);
  int offset = 0;
  for (bool escape : formals) {  // the first one goes to static link
    if (escape) {
      frame->formal_access_.push_back(new InFrameAccess(offset));
      offset += frame->WordSize();
    } else {
      frame->formal_access_.push_back(new InRegAccess(temp::TempFactory::NewTemp()));
    }
  }
  return frame;
}

tree::Exp *AccessExp(Access *acc, tree::Exp *fp) {
  if (typeid(*acc) == typeid(InFrameAccess)) {
    InFrameAccess *frame_acc = static_cast<InFrameAccess *>(acc);
    return new tree::MemExp(
      new tree::BinopExp(tree::PLUS_OP, fp, new tree::ConstExp(frame_acc->offset)));
  } else {
    InRegAccess *reg_acc = static_cast<InRegAccess *>(reg_acc);
    return new tree::TempExp(reg_acc->reg);
  }
}

tree::Exp *ExternalCall(std::string s, tree::ExpList *args) {
  return new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(s)), args);
}

tree::Stm *ProcEntryExit1(Frame *frame, tree::Stm *stm) {
  return stm;
}

} // namespace frame