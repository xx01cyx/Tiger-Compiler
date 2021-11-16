#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */
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
};

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

} // namespace frame