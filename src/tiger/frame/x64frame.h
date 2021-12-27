//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include <unordered_map>

#include "tiger/frame/frame.h"

#define X64_WORD_SIZE 8

namespace frame {
class X64RegManager : public RegManager {
public:
  X64RegManager();

  temp::TempList *Registers() override;
  temp::TempList *ArgRegs() override;
  temp::TempList *CallerSaves() override;
  temp::TempList *CalleeSaves() override;
  temp::TempList *ReturnSink() override;
  temp::Temp *FramePointer() override;
  temp::Temp *StackPointer() override;
  temp::Temp *ReturnValue() override;
  temp::Temp *ArithmeticAssistant() override;
  int WordSize() override;
  int RegCount() override;

  enum Register {
    RAX, RBX, RCX, RDX, RSI, RDI, RBP, R8, R9, R10, R11, R12, R13, R14, R15, RSP, REG_COUNT, 
  };

  static std::unordered_map<Register, std::string> reg_str;

private:
  static const int WORD_SIZE = X64_WORD_SIZE;

};

Frame *NewFrame(temp::Label *name, std::vector<bool> formals);
tree::Exp *AccessCurrentExp(Access *acc, Frame *frame);
tree::Exp *AccessExp(Access* acc, tree::Exp *fp);
tree::Exp *ExternalCall(std::string s, tree::ExpList *args);
tree::Stm *ProcEntryExit1(Frame *frame, tree::Stm *stm);
assem::InstrList *ProcEntryExit2(assem::InstrList *body);
assem::Proc *ProcEntryExit3(Frame *frame, assem::InstrList *body);

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
