#ifndef TIGER_TRANSLATE_TRANSLATE_H_
#define TIGER_TRANSLATE_TRANSLATE_H_

#include <list>
#include <memory>

#include "tiger/absyn/absyn.h"
#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/semant/types.h"

namespace tr {

class Exp;
class ExpAndTy;
class Level;

class Access {
public:
  Level *level_;
  frame::Access *access_;

  Access(Level *level, frame::Access *access)
      : level_(level), access_(access) {}
  static Access *AllocLocal(Level *level, bool escape);
};

class Level {
public:
  frame::Frame *frame_;
  Level *parent_;

  /* TODO: Put your lab5 code here */
  Level() : frame_(nullptr), parent_(nullptr) {}
  explicit Level(frame::Frame *frame) : frame_(frame), parent_(nullptr) {}
  Level(frame::Frame *frame, Level *parent) : frame_(frame), parent_(parent) {}
};

class ProgTr {
public:
  // TODO: Put your lab5 code here */
  ProgTr(std::unique_ptr<absyn::AbsynTree> absyn_tree, 
    std::unique_ptr<err::ErrorMsg> errormsg) {
      absyn_tree_ = std::move(absyn_tree);
      errormsg_ = std::move(errormsg);
      outermost_level_ = std::make_unique<Level>();
      FillBaseVEnv();
      FillBaseTEnv();
    }

  /**
   * Translate IR tree
   */
  void Translate();

  /**
   * Transfer the ownership of errormsg to outer scope
   * @return unique pointer to errormsg
   */
  std::unique_ptr<err::ErrorMsg> TransferErrormsg() {
    return std::move(errormsg_);
  }


private:
  std::unique_ptr<absyn::AbsynTree> absyn_tree_;
  std::unique_ptr<err::ErrorMsg> errormsg_;
  std::unique_ptr<Level> outermost_level_;
  std::unique_ptr<env::TEnv> tenv_;
  std::unique_ptr<env::VEnv> venv_;

  // Fill base symbol for var env and type env
  void FillBaseVEnv();
  void FillBaseTEnv();
};

void ProcEntryExit(Level *level, Exp *body);

} // namespace tr

#endif
