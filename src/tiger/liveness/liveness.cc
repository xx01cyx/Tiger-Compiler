#include "tiger/liveness/liveness.h"

#include <iostream>
#include <limits>

extern frame::RegManager *reg_manager;

namespace graph {

Node<temp::Temp> *IGraph::NewNode(temp::Temp *info) {
  auto n = new Node<temp::Temp>();
  n->my_graph_ = this;
  n->my_key_ = nodecount_++;

  my_nodes_->Append(n);

  n->succs_ = new NodeList<temp::Temp>();
  n->preds_ = new NodeList<temp::Temp>();
  n->info_ = info;

  degree_[n] = 0;

  return n;
}

bool IGraph::IAdj(Node<temp::Temp> *n, Node<temp::Temp> *m) {
  return adj_set_.find(std::make_pair(n, m)) != adj_set_.end();
}

void IGraph::AddEdge(Node<temp::Temp> *from, Node<temp::Temp> *to) {
  assert(from);
  assert(to);
  assert(from->my_graph_ == this);
  assert(to->my_graph_ == this);

  if (!IAdj(from, to) && from != to) {
    // Add to adjacent set
    adj_set_.emplace(from, to);
    adj_set_.emplace(to, from);

    // Add to adjacent list
    if (!precolored_->Contain(from->NodeInfo())) {
      live::INodeList *single_to = new live::INodeList(to);
      from->succs_ = from->succs_->Union(single_to);
      from->AddOneIDegree();
    }
    if (!precolored_->Contain(to->NodeInfo())) {
      live::INodeList *single_from = new live::INodeList(from);
      to->preds_ = to->preds_->Union(single_from);
      to->AddOneIDegree();
    }
  } 
}

void IGraph::SetDegree(Node<temp::Temp> *n, int d) {
  assert(n->my_graph_ == this);
  degree_.at(n) = d;
}

int IGraph::GetDegree(Node<temp::Temp> *n) {
  assert(n->my_graph_ == this);
  return degree_.at(n);
}

void IGraph::ClearEdge() {
  adj_set_.clear();
  for (Node<temp::Temp> *n : my_nodes_->GetList()) {
    degree_.at(n) = 0;
    n->preds_->Clear();
    n->succs_->Clear();
  }
}

void IGraph::AddOneDegree(Node<temp::Temp> *n) {
  assert(n->my_graph_ == this);
  degree_.at(n)++;
}

void IGraph::MinusOneDegree(Node<temp::Temp> *n) {
  assert(n->my_graph_ == this);
  degree_.at(n)--;
}

} // namespace graph

namespace temp {
  bool TempList::Contain(Temp *t) const {
    for (auto temp : temp_list_) {
      if (temp == t)
        return true;
    }
    return false;
  }

  void TempList::CatList(const TempList *tl) {
    if (!tl || tl->temp_list_.empty()) 
      return;
    temp_list_.insert(temp_list_.end(), tl->temp_list_.begin(),
                      tl->temp_list_.end());
  }

  TempList *TempList::Union(const TempList *tl) const {
    TempList *res = new TempList();
    res->CatList(this);
    for (auto temp : tl->GetList()) {
      if (!res->Contain(temp))
        res->Append(temp);
    }
    return res;
  }

  TempList *TempList::Diff(const TempList *tl) const {
    TempList *res = new TempList();
    for (auto temp : temp_list_) {
      if (!tl->Contain(temp))
        res->Append(temp);
    }
    return res;
  }

  bool TempList::IdentitalTo(const TempList *tl) const {
    TempList *diff1 = this->Diff(tl);
    TempList *diff2 = tl->Diff(this);
    return diff1->GetList().empty() && diff2->GetList().empty();
  }

  std::list<Temp *>::const_iterator TempList::Replace(
    std::list<Temp *>::const_iterator pos, Temp *temp) {
    temp_list_.insert(pos, temp);
    pos = temp_list_.erase(pos);
    pos--;  // points to the replaced temp
    return pos;
  }
} // namespace temp

namespace live {

LiveGraphFactory::LiveGraphFactory() 
  : live_graph_(new IGraph(reg_manager->Registers()), new MoveList()),
    in_(std::make_unique<graph::Table<assem::Instr, temp::TempList>>()),
    out_(std::make_unique<graph::Table<assem::Instr, temp::TempList>>()),
    temp_node_map_(new tab::Table<temp::Temp, INode>()),
    node_instr_map_(std::make_shared<NodeInstrMap>()) {}

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_)
    res->move_list_.push_back(move);
  for (auto move : list->GetList()) {
    if (!Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Diff(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    if (!list->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

void LiveGraphFactory::LiveMap(fg::FGraphPtr flowgraph) {

  for (fg::FNode *fnode : flowgraph->Nodes()->GetList()) {
    in_.get()->Enter(fnode, new temp::TempList());
    out_.get()->Enter(fnode, new temp::TempList());
  }

  int finished = 0;
  int i = 0;

  while (finished != flowgraph->nodecount_) {

    finished = 0;

    for (auto fnode_it = flowgraph->Nodes()->GetList().rbegin();
         fnode_it != flowgraph->Nodes()->GetList().rend(); fnode_it++) {
          // Compute out 
          temp::TempList *out_n = new temp::TempList();
          for (fg::FNode *succ_fnode : (*fnode_it)->Succ()->GetList())
            out_n = out_n->Union(in_.get()->Look(succ_fnode));

          // Compute in
          temp::TempList *in_n = (*fnode_it)->NodeInfo()->Use();
          in_n = in_n->Union(out_n->Diff((*fnode_it)->NodeInfo()->Def()));

          if (out_n->IdentitalTo(out_.get()->Look(*fnode_it)) 
              && in_n->IdentitalTo(in_.get()->Look(*fnode_it))) {
            finished++;
          } else {
            out_.get()->Enter((*fnode_it), out_n);
            in_.get()->Enter(*fnode_it, in_n);
          }
    } 
  }
}

void LiveGraphFactory::InterfGraph(fg::FGraphPtr flowgraph, MoveList **worklist_moves) {

  for (fg::FNode *fnode : flowgraph->Nodes()->GetList()) {
    assem::Instr *instr = fnode->NodeInfo();
    temp::TempList *live = out_.get()->Look(fnode);

    if (typeid(*instr) == typeid(assem::MoveInstr)) {  // move instruction
      assert(instr->Def()->GetList().size() == 1);
      assert(instr->Use()->GetList().size() == 1);

      live = live->Diff(instr->Use());

      temp::Temp *def_reg = instr->Def()->GetList().front();
      temp::Temp *use_reg = instr->Use()->GetList().front();
      INode *def_n = temp_node_map_->Look(def_reg);
      INode *use_n = temp_node_map_->Look(use_reg);
      MoveList *single_move = new MoveList(Move(use_n, def_n));

      temp::TempList *defs_and_uses = instr->Def()->Union(instr->Use());
      for (temp::Temp *reg : defs_and_uses->GetList()) {
        INode *n = temp_node_map_->Look(reg);
        MoveList *new_moves = live_graph_.move_list->Look(n)->Union(single_move);;
        live_graph_.move_list->Set(n, new_moves);
      }

      *worklist_moves = (*worklist_moves)->Union(single_move);
    }

    live = live->Union(instr->Def());

    // Add inteference edges
    temp::TempList *defs = instr->Def();
    for (temp::Temp *def_reg : defs->GetList()) {
      INode *def_n = temp_node_map_->Look(def_reg);
      for (temp::Temp *live_reg : live->GetList()) {
        INode *live_n = temp_node_map_->Look(live_reg);
        live_graph_.interf_graph->AddEdge(live_n, def_n);
      }
    }

    live = instr->Use()->Union(live->Diff(instr->Def())); 
  }
}

void LiveGraphFactory::Liveness(fg::FGraphPtr flowgraph, MoveList **worklist_moves) {
  LiveMap(flowgraph);
  InterfGraph(flowgraph, worklist_moves);
}

void LiveGraphFactory::BuildIGraph(assem::InstrList *instr_list) {
  // Add precolored registers as nodes to interference graph
  // Precolored registers will never be spilled
  for (temp::Temp *reg : reg_manager->Registers()->GetList()) {
    if (temp_node_map_->Look(reg) == nullptr) {
      INode *n = live_graph_.interf_graph->NewNode(reg);
      live_graph_.interf_graph->SetDegree(n, std::numeric_limits<int>::max());
      live_graph_.move_list->Enter(n, new MoveList());
      temp_node_map_->Enter(reg, n);
      node_instr_map_.get()->insert(std::make_pair(n, new std::vector<InstrPos>()));
    }
  }

  // Add temporaries as nodes to interference graph
  for (auto instr_it = instr_list->GetList().cbegin();
        instr_it != instr_list->GetList().cend(); instr_it++) {
    INode *n;
    temp::TempList *defs_and_uses = (*instr_it)->Def()->Union((*instr_it)->Use());
    for (temp::Temp *reg : defs_and_uses->GetList()) {
      if ((n = temp_node_map_->Look(reg)) == nullptr) {
        n = live_graph_.interf_graph->NewNode(reg);
        live_graph_.move_list->Enter(n, new MoveList());
        temp_node_map_->Enter(reg, n); 
        node_instr_map_.get()->insert(std::make_pair(n, new std::vector<InstrPos>{instr_it}));
      } else {
        node_instr_map_.get()->at(n)->push_back(instr_it);
      }
    }
  }
}

} // namespace live