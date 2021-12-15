#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

#include <sstream>

extern frame::RegManager *reg_manager;
namespace ra {
/* TODO: Put your lab6 code here */

RegAllocator::RegAllocator(frame::Frame *frame, std::unique_ptr<cg::AssemInstr> assem_instr)
  : frame_(frame), assem_instr_(std::move(assem_instr)) {

  global_map_ = temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name());
  temp_map_ = temp::Map::Name();

  flow_graph_factory_ = new fg::FlowGraphFactory();
  live_graph_factory_ = new live::LiveGraphFactory();
  live_graph_factory_->BuildIGraph(assem_instr_.get()->GetInstrList());

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

  auto tn_map = live_graph_factory_->GetTempNodeMap();
  int c = 0;
  for (temp::Temp *reg : reg_manager->Registers()->GetList()) {
    live::INode *node = tn_map->Look(reg);
    precolored_->Append(node);
    color_[node] = c++;
  }

  InitAlias();

  initial_ = live_graph_factory_->GetLiveGraph().interf_graph->Nodes()->Diff(precolored_);
  std::cout << "size of initial_ is " << initial_->GetList().size() << std::endl;

}

void RegAllocator::RegAlloc() {

  std::cout << std::endl << "========RegAlloc========" << std::endl;

  // live_graph_factory_ = new live::LiveGraphFactory();
  // live_graph_factory_->BuildIGraph(assem_instr_.get()->GetInstrList());

  flow_graph_factory_->AssemFlowGraph(assem_instr_.get()->GetInstrList());
  live_graph_factory_->Liveness(flow_graph_factory_->GetFlowGraph(), &worklist_moves_);

  move_count = worklist_moves_->GetList().size();
  // initial_ = initial_->Union(live_graph_factory_->GetLiveGraph().interf_graph->Nodes()->Diff(precolored_));

  MakeWorkList();

  do {

    std::cout << "simplify worklist contents: ";
    for (live::INode *t : simplify_worklist_->GetList()) {
      std::cout << *global_map_->Look(t->NodeInfo()) << '-' << t->IDegree() << ' ';
    }
    std::cout << std::endl;

    std::cout << "freeze worklist contents: ";
    for (live::INode *t : freeze_worklist_->GetList()) {
      std::cout << *global_map_->Look(t->NodeInfo()) << '-' << t->IDegree() << ' ';
    }
    std::cout << std::endl;

    std::cout << "spill worklist contents: ";
    for (live::INode *t : spill_worklist_->GetList()) {
      std::cout << *global_map_->Look(t->NodeInfo()) << '-' << t->IDegree() << ' ';
    }
    std::cout << std::endl;

    PrintMoveList();
    PrintNodeList();

    std::cout << std::endl << "### ";

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

    // for (auto it : delete_moves)
    //   instr_list->Erase(it);
  }
    
}

std::unique_ptr<Result> RegAllocator::TransferResult() {

  std::cout << "TransferResult" << std::endl;

  // temp::Map *coloring = temp::Map::LayerMap(temp::Map::Empty(), temp::Map::Name());
  temp::Map *coloring = temp::Map::Empty();
  for (auto node_color : color_) {
    temp::Temp *reg = node_color.first->NodeInfo();
    int c = node_color.second;
    std::string *str = global_map_->Look(reg_manager->Registers()->NthTemp(c));
    std::cout << *global_map_->Look(reg) << ' ' << *str << std::endl;
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
  node_count = initial_->GetList().size();
  initial_->Clear();
}

live::INodeList *RegAllocator::Adjacent(live::INode *n) {
  live::INodeList *nl = n->AdjList()->Diff(select_stack_->Union(coalesced_nodes_));
  std::cout << "Adjacent of " << *global_map_->Look(n->NodeInfo()) << ": ";
  for (live::INode *m : nl->GetList()) {
    std::cout << *global_map_->Look(m->NodeInfo()) << '-' << m->IDegree() << ' ';
  }
  std::cout << std::endl;
  return nl;
}

live::MoveList *RegAllocator::NodeMoves(live::INode *n) {
  live::MoveList *node_moves = live_graph_factory_->GetLiveGraph().move_list->Look(n);
  // std::cout << "size of move list of " << *temp::Map::Name()->Look(n->NodeInfo()) 
  //           << " is " << node_moves->GetList().size() << std::endl;
  return node_moves->Intersect(active_moves_->Union(worklist_moves_));
}

bool RegAllocator::MoveRelated(live::INode *n) {
  live::MoveList *node_moves = NodeMoves(n);
  return !node_moves->GetList().empty();
}

void RegAllocator::Simplify() {
  live::INode *n = simplify_worklist_->GetList().front();
  std::cout << "Simplify " << *global_map_->Look(n->NodeInfo())
            << " with a degree of " << n->IDegree() << std::endl;
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
        PrintMoveList();
      }
    }
  }
}

void RegAllocator::Coalesce() {

  std::cout << "Coalesce" << std::endl;

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
    std::cout << "$$$ coalesce move " << *global_map_->Look(m.first->NodeInfo()) 
              << "->" << *global_map_->Look(m.second->NodeInfo()) 
              << " since src and dst are same" << std::endl;
    coalesced_moves_ = coalesced_moves_->Union(single_move);
    AddWorkList(u);

  } else if (IsPrecolored(v) || AreAdj(u, v)) {
    std::cout << "$$$ constrain move " << *global_map_->Look(m.first->NodeInfo()) 
              << "->" << *global_map_->Look(m.second->NodeInfo()) << std::endl;
    constrained_moves_ = constrained_moves_->Union(single_move);
    AddWorkList(u);
    AddWorkList(v);

  } else if (George(u, v) || Briggs(u, v)) {
    std::cout << "$$$ coalesce move " << *global_map_->Look(m.first->NodeInfo()) 
              << "->" << *global_map_->Look(m.second->NodeInfo()) 
              << " since the move satisfy George or Briggs" << std::endl;
    coalesced_moves_ = coalesced_moves_->Union(single_move);
    Combine(u, v);
    AddWorkList(u);

  } else {
    std::cout << "$$$ active move " << *global_map_->Look(m.first->NodeInfo()) 
              << "->" << *global_map_->Look(m.second->NodeInfo());
    active_moves_ = active_moves_->Union(single_move);
  }

  PrintMoveList();

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
  PrintAlias();

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

  std::cout << "Freeze" << std::endl;

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

    PrintMoveList();

    if (NodeMoves(v)->GetList().empty() && v->IDegree() < reg_manager->RegCount()) {
      live::INodeList *single_v = new live::INodeList(v);
      freeze_worklist_ = freeze_worklist_->Diff(single_v);
      simplify_worklist_ = simplify_worklist_->Union(single_v);
    }
  }
}

void RegAllocator::SelectSpill() {

  // FIXME: heuristic algorithm
  live::INode *m = spill_worklist_->GetList().front();

  std::cout << "SelectSpill " << *global_map_->Look(m->NodeInfo()) << std::endl;

  live::INodeList *single_m = new live::INodeList(m);
  spill_worklist_ = spill_worklist_->Diff(single_m);
  simplify_worklist_ = simplify_worklist_->Union(single_m);
  FreezeMoves(m);
}

void RegAllocator::AssignColors() {

  std::cout << "AssignColors" << std::endl;

  while (!select_stack_->GetList().empty()) {
    live::INode *n = select_stack_->GetList().front();
    select_stack_->DeleteNode(n);

    std::cout << "pop " << *global_map_->Look(n->NodeInfo()) << " out of stack" << std::endl;

    std::set<int> ok_colors = std::set<int>();
    for (int c = 0; c < reg_manager->RegCount(); ++c)
      ok_colors.emplace(c);
    
    live::INodeList *adj_list = n->AdjList();
    for (live::INode *w : adj_list->GetList()) {
      live::INode *alias = GetAlias(w);
      if (colored_nodes_->Union(precolored_)->Contain(alias)) {
        // std::cout << "color " << color_.at(GetAlias(w)) << " is used by "
        //           << *temp::Map::Name()->Look(w->NodeInfo()) << ", cannot assign to "
        //           << *temp::Map::Name()->Look(n->NodeInfo()) << std::endl;
        ok_colors.erase(color_.at(alias));
      }
    }

    live::INodeList *single_n = new live::INodeList(n);
    if (ok_colors.empty()) {
      spilled_nodes_ = spilled_nodes_->Union(single_n);
      std::cout << *global_map_->Look(n->NodeInfo()) << " is spilled to memory" << std::endl;
    } else {
      colored_nodes_ = colored_nodes_->Union(single_n);
      int c = *(ok_colors.begin());
      color_[n] = c;
      std::cout << "color " << c << " is selected for " << *global_map_->Look(n->NodeInfo()) << std::endl;
    }
  }

  for (live::INode *n : coalesced_nodes_->GetList()) {
    if (!color_.count(GetAlias(n))) {
      std::cout << "error! alias of " << *global_map_->Look(n->NodeInfo()) << " is " 
                << *global_map_->Look(GetAlias(n)->NodeInfo()) << ", which has no color" << std::endl;
    } else {
      std::cout << "color " << color_[n] << " is assigned for " << *global_map_->Look(n->NodeInfo()) << std::endl;
      color_[n] = color_[GetAlias(n)];
    }
  }
}

void RegAllocator::RewriteProgram() {

  std::cout << "RewriteProgram" << std::endl;

  live::INodeList *new_temps = new live::INodeList();
  live::NodeInstrMap *node_instr_map = live_graph_factory_->GetNodeInstrMap().get();
  
  for (live::INode *v : spilled_nodes_->GetList()) { 
    // Allocate memory for the spilled node
    frame::Access *acc = frame_->AllocLocal(true);
    std::string mem_pos = acc->MunchAccess(frame_);

    std::cout << "allocate " << mem_pos << " for "
              << *global_map_->Look(v->NodeInfo()) << std::endl;

    // Get all instructions regarding the spilled temporary
    // auto node_instrs = node_instr_map->equal_range(v);
    auto node_instrs = node_instr_map->at(v);

    // for (auto node_instr_it = node_instrs.first;
    //      node_instr_it != node_instrs.second; node_instr_it++) {
    for (auto instr_it = node_instrs->begin(); instr_it != node_instrs->end(); instr_it++) {

      std::cout << node_instrs->size() << " instructions regarding " 
                << *global_map_->Look(v->NodeInfo()) << std::endl;

      std::stringstream instr_ss;
      // live::INode *n = node_instr_it->first;
      // auto instr_pos = node_instr_it->second;
      auto instr_pos = *instr_it;
      assem::Instr *instr = *instr_pos;

      std::cout << "instrution regarding " << *global_map_->Look(v->NodeInfo()) << ": ";
      instr->Print(stdout, global_map_);

      // Create a new temporary for each definition and use
      temp::Temp *new_reg = temp::TempFactory::NewTemp();
      live::INode *new_node = live_graph_factory_->GetLiveGraph().interf_graph->NewNode(new_reg);
      live_graph_factory_->GetLiveGraph().move_list->Enter(new_node, new live::MoveList());
      live_graph_factory_->GetTempNodeMap()->Enter(new_reg, new_node);
      // live_graph_factory_->GetNodeInstrMap()->insert(std::make_pair(new_node, instr_pos));
      // node_instr_map.get()->at(new_node) = new std::vector<live::InstrPos>{instr_pos};
      node_instr_map->insert(std::make_pair(new_node, new std::vector<live::InstrPos>{instr_pos}));
      new_temps->Append(new_node);
      alias_[new_node] = new_node;

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
                                      nullptr, nullptr);
        assem_instr_.get()->GetInstrList()->Insert(instr_pos, fetch_instr);

        // Add the new instruction to the node-instruction map
        // live_graph_factory_->GetNodeInstrMap()->insert(std::make_pair(new_node, --instr_pos));
        node_instr_map->at(new_node)->push_back(--instr_pos);
        
        // Restore instruction position iterator
        instr_pos++;

        instr_ss.str("");

        std::cout << "insert fetch: ";  
        fetch_instr->Print(stdout, global_map_);
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
        assem::Instr *store_instr = new assem::OperInstr(instr_ss.str(), nullptr, 
                                      new temp::TempList(new_reg), nullptr);
        assem_instr_.get()->GetInstrList()->Insert(++instr_pos, store_instr);

        // Add the new instruction to the node-instruction map
        // live_graph_factory_->GetNodeInstrMap()->insert(std::make_pair(new_node, --instr_pos));
        node_instr_map->at(new_node)->push_back(--instr_pos);
        
        std::cout << "insert store: ";  
        store_instr->Print(stdout, global_map_);
      }
    }
  }

  initial_ = colored_nodes_->Union(coalesced_nodes_->Union(new_temps));

  int cur_node_count = spilled_nodes_->GetList().size()
                      + coalesced_nodes_->GetList().size()
                      + colored_nodes_->GetList().size(); 
  if (cur_node_count != node_count) {
    std::cout << "another error!!! node count should be " << node_count 
              << " but is " << cur_node_count << std::endl;   
  }

  spilled_nodes_ = new live::INodeList();
  colored_nodes_ = new live::INodeList();
  coalesced_nodes_ = new live::INodeList();

  coalesced_moves_ = new live::MoveList();
  constrained_moves_ = new live::MoveList();
  frozen_moves_ = new live::MoveList();
  worklist_moves_ = new live::MoveList();
  active_moves_ = new live::MoveList();

  InitColor();
  InitAlias();

  // Output the new instruction list
  for (assem::Instr *instr : assem_instr_.get()->GetInstrList()->GetList()) {
    instr->Print(stdout, global_map_);
  }

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


  if (coalesced_moves_->GetList().size() + constrained_moves_->GetList().size()
      + frozen_moves_->GetList().size() + worklist_moves_->GetList().size()
      + active_moves_->GetList().size() != move_count) {
    std::cout << "error!!!" << std::endl;
  }
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

void RegAllocator::InitColor() {
  color_.clear();
  auto tn_map = live_graph_factory_->GetTempNodeMap();
  int c = 0;
  for (temp::Temp *reg : reg_manager->Registers()->GetList()) {
    live::INode *node = tn_map->Look(reg);
    color_[node] = c++;
  }
}

void RegAllocator::InitAlias() {
  alias_.clear();
  live::INodeList *all_nodes = live_graph_factory_->GetLiveGraph().interf_graph->Nodes();
  for (live::INode *n : all_nodes->GetList()) 
    alias_[n] = n;
}


} // namespace ra