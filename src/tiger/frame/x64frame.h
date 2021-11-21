//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

#define X64_WORD_SIZE 8

namespace frame {
class X64RegManager : public RegManager {
  /* TODO: Put your lab5 code here */
public:
  X64RegManager();

  temp::TempList *Registers() override;
  temp::TempList *ArgRegs() override;
  temp::TempList *CallerSaves() override;
  temp::TempList *CalleeSaves() override;
  temp::TempList *ReturnSink() override;
  int WordSize() override;
  temp::Temp *FramePointer() override;
  temp::Temp *StackPointer() override;
  temp::Temp *ReturnValue() override;

  static const int RAX = 0;
  static const int RBX = 1;
  static const int RCX = 2;
  static const int RDX = 3;
  static const int RSI = 4;
  static const int RDI = 5;
  static const int RBP = 6;
  static const int RSP = 7;
  static const int R8 = 8;
  static const int R9 = 9;
  static const int R10 = 10;
  static const int R11 = 11;
  static const int R12 = 12;
  static const int R13 = 13;
  static const int R14 = 14;
  static const int R15 = 15;

private:
  static const int WORD_SIZE = X64_WORD_SIZE;
  static const int REG_COUNT = 16;

};

Frame *NewFrame(temp::Label *name, std::vector<bool> formals);
tree::Exp *AccessExp(Access *acc, tree::Exp *fp);
tree::Exp *ExternalCall(std::string s, tree::ExpList *args);
tree::Stm *ProcEntryExit1(Frame *frame, tree::Stm *stm);

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
