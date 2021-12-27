#include "straightline/slp.h"

#include <iostream>

namespace A {
int A::CompoundStm::MaxArgs() const {
    int maxArgs1 = stm1->MaxArgs();
    int maxArgs2 = stm2->MaxArgs();
    return maxArgs1 > maxArgs2 ? maxArgs1: maxArgs2;
}

Table *A::CompoundStm::Interp(Table *t) const {
  return stm2->Interp(stm1->Interp(t));
}

int A::AssignStm::MaxArgs() const {
  return exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
  return t->Update(id, exp->Interp(t)->i);
}

int A::PrintStm::MaxArgs() const {
  int numExps = exps->NumExps();
  int maxArgs = exps->MaxArgs();
  return numExps > maxArgs ? numExps : maxArgs;
}

Table *A::PrintStm::Interp(Table *t) const {
  if (exps->NumExps() != 1) {
      PairExpList* pairExpList = (PairExpList*)exps;
      std::cout << pairExpList->exp->Interp(t)->i << ' ';
      PrintStm* newPrintStm = new PrintStm(pairExpList->tail);
      return newPrintStm->Interp(t);
  } else {
      std::cout << exps->Interp(t)->i << std::endl;
      return t;
  }
}

int A::IdExp::MaxArgs() const {
    return 0;
}

IntAndTable* A::IdExp::Interp(Table* t) const {
    return new IntAndTable(t->Lookup(id), t);
}

int A::NumExp::MaxArgs() const {
    return 0;
}

IntAndTable* A::NumExp::Interp(Table* t) const {
    return new IntAndTable(num, t);
}

int A::OpExp::MaxArgs() const {
    int leftArgs = left->MaxArgs();
    int rightArgs = right->MaxArgs();
    return leftArgs > rightArgs ? leftArgs : rightArgs;
}

IntAndTable* A::OpExp::Interp(Table* t) const {
    int leftValue = left->Interp(t)->i;
    int rightValue = right->Interp(t)->i;
    switch(oper) {
        case PLUS: return new IntAndTable(leftValue + rightValue, t);
        case MINUS: return new IntAndTable(leftValue - rightValue, t);
        case TIMES: return new IntAndTable(leftValue * rightValue, t);
        case DIV: return new IntAndTable(leftValue / rightValue, t);
    }
}

int A::EseqExp::MaxArgs() const {
    int stmArgs = stm->MaxArgs();
    int expArgs = exp->MaxArgs();
    return stmArgs > expArgs ? stmArgs : expArgs;
}

IntAndTable* A::EseqExp::Interp(Table* t) const {
    Table* newTable = stm->Interp(t);
    return new IntAndTable(exp->Interp(newTable)->i, newTable);
}


int A::PairExpList::MaxArgs() const {
    int expArgs = exp->MaxArgs();
    int tailArgs = tail->MaxArgs();
    return expArgs > tailArgs ? expArgs : tailArgs;
}

int A::PairExpList::NumExps() const {
    return tail->NumExps() + 1;
}

IntAndTable* A::PairExpList::Interp(Table* t) const {
    return new IntAndTable(exp->Interp(t)->i, t);
}

int A::LastExpList::MaxArgs() const {
    return exp->MaxArgs();
}

int A::LastExpList::NumExps() const {
    return 1;
}

IntAndTable* A::LastExpList::Interp(Table* t) const {
    return new IntAndTable(exp->Interp(t)->i, t);
}


int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}
}  // namespace A
