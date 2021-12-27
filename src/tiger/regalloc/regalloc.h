#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

namespace ra {

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result() { delete coloring_; delete il_; };
};

class RegAllocator {
public:
  RegAllocator(frame::Frame *frame, std::unique_ptr<cg::AssemInstr> assem_instr);

  void RegAlloc();
  std::unique_ptr<Result> TransferResult();
  
private:
  frame::Frame *frame_;
  std::unique_ptr<cg::AssemInstr> assem_instr_;
  live::LiveGraphFactory *live_graph_factory_;
  fg::FlowGraphFactory *flow_graph_factory_;
  
  live::INodeList *precolored_;
  live::INodeList *initial_;

  live::INodeList *select_stack_;

  live::INodeList *simplify_worklist_;
  live::INodeList *freeze_worklist_;
  live::INodeList *spill_worklist_;

  live::INodeList *spilled_nodes_;
  live::INodeList *coalesced_nodes_;
  live::INodeList *colored_nodes_;

  live::MoveList *coalesced_moves_;
  live::MoveList *constrained_moves_;
  live::MoveList *frozen_moves_;
  live::MoveList *worklist_moves_;
  live::MoveList *active_moves_;

  std::unordered_map<live::INode*, live::INode*> alias_;
  std::unordered_map<live::INode*, int> color_;

  std::unique_ptr<Result> result_;

  // debug
  temp::Map *global_map_;

  void InitColor();
  void InitAlias();

  void MakeWorkList();
  live::INodeList *Adjacent(live::INode *n);
  live::MoveList *NodeMoves(live::INode *n);
  bool MoveRelated(live::INode *n);

  void Simplify();
  void DecrementDegree(live::INode *n);
  void EnableMoves(live::INodeList *nodes);

  void Coalesce();
  void AddWorkList(live::INode *u);
  bool OK(live::INode *t, live::INode *r);
  bool Conservative(live::INodeList *nodes);
  live::INode *GetAlias(live::INode *n);
  void Combine(live::INode *u, live::INode *v);
  bool George(live::INode *u, live::INode *v);
  bool Briggs(live::INode *u, live::INode *v);
  bool IsPrecolored(live::INode *n);
  bool AreAdj(live::INode *u, live::INode *v);

  void Freeze();
  void FreezeMoves(live::INode *u);

  void SelectSpill();
  live::INode *HeuristicSelect();
  void AssignColors();
  void RewriteProgram();

  void PrintMoveList();
  void PrintAlias();
  void PrintNodeList();
  
};

} // namespace ra

#endif