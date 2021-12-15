#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

#include <sstream>

extern frame::RegManager *reg_manager;
namespace ra {
/* TODO: Put your lab6 code here */

RegAllocator::RegAllocator(frame::Frame *frame, std::unique_ptr<cg::AssemInstr> assem_instr)
  : frame_(frame), assem_instr_(std::move(assem_instr)) {

  global_map_ = temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name());

  precolored_ = new live::INodeList();
  
  simplify_worklist_ = new live::INodeList();
  freeze_worklist_ = new live::INodeList();
  spill_worklist_ = new live::INodeList();

  spilled_nodes_ = new live::INodeList();
  coalesced_nodes_ = new live::INodeList();
  colored_nodes_ = new live::INodeList();

  select_stack_ = new live::INodeList();

  coalesced_moves_ = new live::MoveList();
  constrained_moves_ = new live::MoveList();
  frozen_moves_ = new live::MoveList();
  worklist_moves_ = new live::MoveList();
  active_moves_ = new live::MoveList();

}

void RegAllocator::RegAlloc() {

  flow_graph_factory_ = new fg::FlowGraphFactory();
  live_graph_factory_ = new live::LiveGraphFactory();

  // Add all inodes.
  live_graph_factory_->BuildIGraph(assem_instr_.get()->GetInstrList());

  // Construct flow graph.
  flow_graph_factory_->AssemFlowGraph(assem_instr_.get()->GetInstrList());

  // Analyze liveness.
  live_graph_factory_->Liveness(flow_graph_factory_->GetFlowGraph(), &worklist_moves_);

  // Do other initializations according to inodes.
  InitColor();
  InitAlias();
  initial_ = live_graph_factory_->GetLiveGraph().interf_graph->Nodes()->Diff(precolored_);

  MakeWorkList();

  do {
    // PrintMoveList();
    // PrintNodeList();

    if (!simplify_worklist_->GetList().empty())
      Simplify();
    else if (!worklist_moves_->GetList().empty())
      Coalesce();
    else if (!freeze_worklist_->GetList().empty())
      Freeze();
    else if (!spill_worklist_->GetList().empty())
      SelectSpill();
  } while (!(simplify_worklist_->GetList().empty()
           && worklist_moves_->GetList().empty()
           && freeze_worklist_->GetList().empty()
           && spill_worklist_->GetList().empty()));
  
  AssignColors();

  if (!spilled_nodes_->GetList().empty()) {
    RewriteProgram();
    RegAlloc();

  } else {
    // Remove moves of same source and destination
    assem::InstrList *instr_list = assem_instr_.get()->GetInstrList();
    std::vector<live::InstrPos> delete_moves;
    for (auto instr_it = instr_list->GetList().begin(); 
         instr_it != instr_list->GetList().end(); instr_it++) {

      if (typeid(**instr_it) == typeid(assem::MoveInstr)) {
        assem::MoveInstr *move_instr = static_cast<assem::MoveInstr*>(*instr_it);
        temp::Temp *src_reg = move_instr->src_->GetList().front();
        temp::Temp *dst_reg = move_instr->dst_->GetList().front();
        live::INode *src_n = live_graph_factory_->GetTempNodeMap()->Look(src_reg);
        live::INode *dst_n = live_graph_factory_->GetTempNodeMap()->Look(dst_reg);
        if (color_.at(src_n) == color_.at(dst_n))
          delete_moves.push_back(instr_it);
      }
    }

    for (auto it : delete_moves)
      instr_list->Erase(it);
  }
    
}

std::unique_ptr<Result> RegAllocator::TransferResult() {
  temp::Map *coloring = temp::Map::Empty();
  for (auto node_color : color_) {
    temp::Temp *reg = node_color.first->NodeInfo();
    int c = node_color.second;
    std::string *str = global_map_->Look(reg_manager->Registers()->NthTemp(c));
    coloring->Enter(reg, str);
  }
  result_ = std::make_unique<Result>(coloring, assem_instr_.get()->GetInstrList());

  return std::move(result_);
}

void RegAllocator::MakeWorkList() {
  for (live::INode *n : initial_->GetList()) {
    live::INodeList *single_n = new live::INodeList(n);
    if (n->IDegree() >= reg_manager->RegCount())
      spill_worklist_ = spill_worklist_->Union(single_n);
    else if (MoveRelated(n))
      freeze_worklist_ = freeze_worklist_->Union(single_n);
    else
      simplify_worklist_ = simplify_worklist_->Union(single_n);
  }
}

live::INodeList *RegAllocator::Adjacent(live::INode *n) {
  return n->AdjList()->Diff(select_stack_->Union(coalesced_nodes_));
}

live::MoveList *RegAllocator::NodeMoves(live::INode *n) {
  live::MoveList *node_moves = live_graph_factory_->GetLiveGraph().move_list->Look(n);
  return node_moves->Intersect(active_moves_->Union(worklist_moves_));
}

bool RegAllocator::MoveRelated(live::INode *n) {
  live::MoveList *node_moves = NodeMoves(n);
  return !node_moves->GetList().empty();
}

void RegAllocator::Simplify() {
  live::INode *n = simplify_worklist_->GetList().front();
  simplify_worklist_->DeleteNode(n);
  select_stack_->Prepend(n);
  live::INodeList *adj_nodes = Adjacent(n);
  for (live::INode *m : adj_nodes->GetList())
    DecrementDegree(m);
}

void RegAllocator::DecrementDegree(live::INode *m) {
  if (precolored_->Contain(m))
    return;
  int d = m->IDegree();
  m->MinusOneIDegree();
  if (d == reg_manager->RegCount()) {
    live::INodeList *single_m = new live::INodeList(m);
    EnableMoves(single_m->Union(Adjacent(m)));
    spill_worklist_ = spill_worklist_->Diff(single_m);
    if (MoveRelated(m))
      freeze_worklist_ = freeze_worklist_->Union(single_m);
    else
      simplify_worklist_ = simplify_worklist_->Union(single_m);
  }
}

void RegAllocator::EnableMoves(live::INodeList *nodes) {
  for (live::INode *n : nodes->GetList()) {
    live::MoveList *moves = NodeMoves(n);
    for (live::Move m : moves->GetList()) {
      if (active_moves_->Contain(m.first, m.second)) {
        live::MoveList *single_move = new live::MoveList(m);
        active_moves_ = active_moves_->Diff(single_move);
        worklist_moves_ = worklist_moves_->Union(single_move);
        // PrintMoveList();
      }
    }
  }
}

void RegAllocator::Coalesce() {

  live::Move m = worklist_moves_->GetList().front();
  live::MoveList *single_move = new live::MoveList(m);
  live::INode *x = GetAlias(m.first);
  live::INode *y = GetAlias(m.second);
  live::INode *u, *v;

  if (IsPrecolored(y)) {
    u = y;
    v = x;
  } else {
    u = x;
    v = y;
  }

  worklist_moves_ = worklist_moves_->Diff(single_move);

  if (u == v) {
    coalesced_moves_ = coalesced_moves_->Union(single_move);
    AddWorkList(u);

  } else if (IsPrecolored(v) || AreAdj(u, v)) {
    constrained_moves_ = constrained_moves_->Union(single_move);
    AddWorkList(u);
    AddWorkList(v);

  } else if (George(u, v) || Briggs(u, v)) {
    coalesced_moves_ = coalesced_moves_->Union(single_move);
    Combine(u, v);
    AddWorkList(u);

  } else {
    active_moves_ = active_moves_->Union(single_move);
  }

  // PrintMoveList();

}

void RegAllocator::AddWorkList(live::INode *u) {
  if (!IsPrecolored(u) && !MoveRelated(u) && u->IDegree() < reg_manager->RegCount()) {
    live::INodeList *single_u = new live::INodeList(u);
    freeze_worklist_ = freeze_worklist_->Diff(single_u);
    simplify_worklist_ = simplify_worklist_->Union(single_u);
  }
}

bool RegAllocator::OK(live::INode *t, live::INode *r) {
  return r->IDegree() < reg_manager->RegCount() || IsPrecolored(t) || AreAdj(t, r);
}

bool RegAllocator::Conservative(live::INodeList *nodes) {
  int k = 0;
  for (live::INode *n : nodes->GetList()) {
    if (n->IDegree() >= reg_manager->RegCount())
      k++;
  }
  return k < reg_manager->RegCount();
}

live::INode *RegAllocator::GetAlias(live::INode *n) {
  if (coalesced_nodes_->Contain(n))
    return GetAlias(alias_.at(n));
  else
    return n;
}

void RegAllocator::Combine(live::INode *u, live::INode *v) {
  live::INodeList *single_v = new live::INodeList(v);

  if (freeze_worklist_->Contain(v))
    freeze_worklist_ = freeze_worklist_->Diff(single_v);
  else 
    spill_worklist_ = spill_worklist_->Diff(single_v);
  
  coalesced_nodes_ = coalesced_nodes_->Union(single_v);
  alias_[v] = u;
  // PrintAlias();

  live::MoveList *u_moves = live_graph_factory_->GetLiveGraph().move_list->Look(u);
  live::MoveList *v_moves = live_graph_factory_->GetLiveGraph().move_list->Look(v);
  live_graph_factory_->GetLiveGraph().move_list->Enter(u, u_moves->Union(v_moves));
  EnableMoves(single_v);

  live::INodeList *adj_nodes = Adjacent(v);
  for (live::INode *t : adj_nodes->GetList()) {
    live_graph_factory_->GetLiveGraph().interf_graph->AddEdge(t, u);
    DecrementDegree(t);
  }

  if (u->IDegree() >= reg_manager->RegCount() && freeze_worklist_->Contain(u)) {
    live::INodeList *single_u = new live::INodeList(u);
    freeze_worklist_ = freeze_worklist_->Diff(single_u);
    spill_worklist_ = spill_worklist_->Union(single_u);
  }

}

bool RegAllocator::George(live::INode *u, live::INode *v) {
  if (!IsPrecolored(u))
    return false;

  live::INodeList *adj_nodes = Adjacent(v);
  for (live::INode *t : adj_nodes->GetList())
    if (!OK(t, u))
      return false;

  return true;
}

bool RegAllocator::Briggs(live::INode *u, live::INode *v) {
  if (IsPrecolored(u))
    return false;

  live::INodeList *nodes = Adjacent(u);
  nodes = nodes->Union(Adjacent(v));
  return Conservative(nodes);
}

bool RegAllocator::IsPrecolored(live::INode *n) {
  return precolored_->Contain(n);
}

bool RegAllocator::AreAdj(live::INode *u, live::INode *v) {
  return live_graph_factory_->GetLiveGraph().interf_graph->IAdj(u, v);
}

void RegAllocator::Freeze() {
  live::INode *u = freeze_worklist_->GetList().front();
  live::INodeList *single_u = new live::INodeList(u);
  freeze_worklist_ = freeze_worklist_->Diff(single_u);
  simplify_worklist_ = simplify_worklist_->Union(single_u);
  FreezeMoves(u);
}

void RegAllocator::FreezeMoves(live::INode *u) {
  live::MoveList *u_moves = NodeMoves(u);
  live::INode *v;

  for (live::Move m : u_moves->GetList()) {
    if (GetAlias(m.second) == GetAlias(u))
      v = GetAlias(m.first);
    else
      v =GetAlias(m.second);

    live::MoveList *single_m = new live::MoveList(m);
    active_moves_ = active_moves_->Diff(single_m);
    frozen_moves_ = frozen_moves_->Union(single_m);

    // PrintMoveList();

    if (NodeMoves(v)->GetList().empty() && v->IDegree() < reg_manager->RegCount()) {
      live::INodeList *single_v = new live::INodeList(v);
      freeze_worklist_ = freeze_worklist_->Diff(single_v);
      simplify_worklist_ = simplify_worklist_->Union(single_v);
    }
  }
}

void RegAllocator::SelectSpill() {
  assert(!spill_worklist_->GetList().empty());
  live::INode *m = HeuristicSelect();

  live::INodeList *single_m = new live::INodeList(m);
  spill_worklist_ = spill_worklist_->Diff(single_m);
  simplify_worklist_ = simplify_worklist_->Union(single_m);
  FreezeMoves(m);
}

/* Implement furthest-next-use algorithm to heuristically find a node to spill */
live::INode *RegAllocator::HeuristicSelect() {
  live::INode *res;
  int max_distance = 0;
  assem::InstrList *instr_list = (*assem_instr_).GetInstrList();

  for (live::INode *n : spill_worklist_->GetList()) {
    int pos = 0;
    int start;
    int distance = -1;
    for (assem::Instr *instr : instr_list->GetList()) {
      if (instr->Def()->Contain(n->NodeInfo())) {
        start = pos;
      }
      if (instr->Use()->Contain(n->NodeInfo())) {
        distance = pos - start;
        if (distance > max_distance) {
          max_distance = distance;
          res = n;
        }
      }
      pos++;
    }

    // Defined but never used
    if (distance == -1)
      return n;
  }

  return res;
}

void RegAllocator::AssignColors() {

  while (!select_stack_->GetList().empty()) {
    live::INode *n = select_stack_->GetList().front();
    select_stack_->DeleteNode(n);

    std::set<int> ok_colors = std::set<int>();
    for (int c = 0; c < reg_manager->RegCount(); ++c)
      ok_colors.emplace(c);
    
    live::INodeList *adj_list = n->AdjList();
    for (live::INode *w : adj_list->GetList()) {
      live::INode *alias = GetAlias(w);
      if (colored_nodes_->Union(precolored_)->Contain(alias))
        ok_colors.erase(color_.at(alias));
    }

    live::INodeList *single_n = new live::INodeList(n);
    if (ok_colors.empty()) {
      spilled_nodes_ = spilled_nodes_->Union(single_n);
    } else {
      colored_nodes_ = colored_nodes_->Union(single_n);
      int c = *(ok_colors.begin());
      color_[n] = c;
    }
  }

  for (live::INode *n : coalesced_nodes_->GetList())
    color_[n] = color_[GetAlias(n)];

}

void RegAllocator::RewriteProgram() {

  live::NodeInstrMap *node_instr_map = live_graph_factory_->GetNodeInstrMap().get();
  
  for (live::INode *v : spilled_nodes_->GetList()) { 
    // Allocate memory for the spilled node
    frame::Access *acc = frame_->AllocLocal(true);
    std::string mem_pos = acc->MunchAccess(frame_);

    // Get all instructions regarding the spilled temporary
    auto node_instrs = node_instr_map->at(v);

    for (auto instr_it = node_instrs->begin(); instr_it != node_instrs->end(); instr_it++) {
      std::stringstream instr_ss;
      auto instr_pos = *instr_it;
      assem::Instr *instr = *instr_pos;

      // Create a new temporary for each definition and use
      temp::Temp *new_reg = temp::TempFactory::NewTemp();

      // The spilled temporary is a use
      if (instr->Use()->Contain(v->NodeInfo())) {

        // Replace the spilled temporary with the new temporary
        for (auto temp_it = instr->Use()->GetList().begin();
             temp_it != instr->Use()->GetList().end(); temp_it++) {
          if (*temp_it == v->NodeInfo()) {
            instr->Use()->Replace(temp_it, new_reg);
            break;
          }
        }

        // Insert a fetch before use of the new temporary
        instr_ss << "movq " << mem_pos << ", `d0";
        assem::Instr *fetch_instr = new assem::OperInstr(instr_ss.str(), 
                                      new temp::TempList(new_reg), 
                                      new temp::TempList(reg_manager->StackPointer()), 
                                      nullptr);
        assem_instr_.get()->GetInstrList()->Insert(instr_pos, fetch_instr);

        // Restore instruction position iterator
        // instr_pos++;

        instr_ss.str("");
      }

      // The spilled temporary is a definition
      if (instr->Def()->Contain(v->NodeInfo())) {

        // Replace the spilled temporary with the new temporary
        for (auto temp_it = instr->Def()->GetList().begin();
             temp_it != instr->Def()->GetList().end(); temp_it++) {
          if (*temp_it == v->NodeInfo()) {
            instr->Def()->Replace(temp_it, new_reg);
            break;
          }
        }

        // Insert a store after definition of the new temporary
        instr_ss << "movq `s0, " << mem_pos;
        assem::Instr *store_instr = new assem::OperInstr(instr_ss.str(), 
                                      nullptr, 
                                      new temp::TempList({new_reg, reg_manager->StackPointer()}), 
                                      nullptr);
        assem_instr_.get()->GetInstrList()->Insert(++instr_pos, store_instr);
      }
    }
  }

  spilled_nodes_->Clear();
  colored_nodes_->Clear();
  coalesced_nodes_->Clear();

  coalesced_moves_->Clear();
  constrained_moves_->Clear();
  frozen_moves_->Clear();
  worklist_moves_->Clear();
  active_moves_->Clear();

  color_.clear();
  alias_.clear();

  delete flow_graph_factory_;
  delete live_graph_factory_;

}

void RegAllocator::InitColor() {
  auto tn_map = live_graph_factory_->GetTempNodeMap();
  int c = 0;
  for (temp::Temp *reg : reg_manager->Registers()->GetList()) {
    live::INode *node = tn_map->Look(reg);
    precolored_->Append(node);
    color_[node] = c++;
  }
}

void RegAllocator::InitAlias() {
  live::INodeList *all_nodes = live_graph_factory_->GetLiveGraph().interf_graph->Nodes();
  for (live::INode *n : all_nodes->GetList()) 
    alias_[n] = n;
}

void RegAllocator::PrintMoveList() {

  std::cout << "worklist_moves_: ";
  for (live::Move m : worklist_moves_->GetList()) {
    std::cout <<  *global_map_->Look(m.first->NodeInfo()) << "->" 
              << *global_map_->Look(m.second->NodeInfo()) << " ";
  }
  std::cout << std::endl;

  std::cout << "coalesced_moves_: ";
  for (live::Move m : coalesced_moves_->GetList()) {
    std::cout <<  *global_map_->Look(m.first->NodeInfo()) << "->" 
              << *global_map_->Look(m.second->NodeInfo()) << " ";
  }
  std::cout << std::endl;

  std::cout << "constrained_moves_: ";
  for (live::Move m : constrained_moves_->GetList()) {
    std::cout <<  *global_map_->Look(m.first->NodeInfo()) << "->" 
              << *global_map_->Look(m.second->NodeInfo()) << " ";
  }
  std::cout << std::endl;

  std::cout << "frozen_moves_: ";
  for (live::Move m : frozen_moves_->GetList()) {
    std::cout <<  *global_map_->Look(m.first->NodeInfo()) << "->" 
              << *global_map_->Look(m.second->NodeInfo()) << " ";
  }
  std::cout << std::endl;

  std::cout << "active_moves_: ";
  for (live::Move m : active_moves_->GetList()) {
    std::cout <<  *global_map_->Look(m.first->NodeInfo()) << "->" 
              << *global_map_->Look(m.second->NodeInfo()) << " ";
  }
  std::cout << std::endl;
}

void RegAllocator::PrintAlias() {
  std::cout << "PrintAlias: ";
  auto all_nodes = live_graph_factory_->GetLiveGraph().interf_graph->Nodes();
  for (auto n : all_nodes->GetList())
    std::cout << *global_map_->Look(n->NodeInfo()) << '-'
              << *global_map_->Look(alias_[n]->NodeInfo()) << ' ';
  std::cout << std::endl;
}

void RegAllocator::PrintNodeList() {
  std::cout << "spilled_nodes_: ";
  for (auto n : spilled_nodes_->GetList()) {
    std::cout << *global_map_->Look(n->NodeInfo()) << ' ';
  }
  std::cout << std::endl;

  std::cout << "coalesced_nodes_: ";
  for (auto n : coalesced_nodes_->GetList()) {
    std::cout << *global_map_->Look(n->NodeInfo()) << ' ';
  }
  std::cout << std::endl;

  std::cout << "colored_nodes_: ";
  for (auto n : colored_nodes_->GetList()) {
    std::cout << *global_map_->Look(n->NodeInfo()) << ' ';
  }
  std::cout << std::endl;

  std::cout << "select_stack_: ";
  for (auto n : select_stack_->GetList()) {
    std::cout << *global_map_->Look(n->NodeInfo()) << ' ';
  }
  std::cout << std::endl;
}


} // namespace ra