#include "tiger/liveness/flowgraph.h"

namespace fg {

void FlowGraphFactory::AssemFlowGraph(assem::InstrList *instr_list) {
  /* TODO: Put your lab6 code here */

  // Construct the graph by adding all instructions as nodes without adding edges.
  for (assem::Instr *instr : instr_list->GetList()) {
    FNode *node = flowgraph_->NewNode(instr);
    if (typeid(*instr) == typeid(assem::LabelInstr)) {
      assem::LabelInstr *label_instr = static_cast<assem::LabelInstr *>(instr);
      label_map_.get()->Enter(label_instr->label_, node);
    }
  }

  // Add the edges to the graph.
  auto node_it = flowgraph_->Nodes()->GetList().begin();
  auto last_node_it = --flowgraph_->Nodes()->GetList().end();
  while (node_it != last_node_it) {
    FNode *node = *node_it;
    assem::Instr *instr = node->NodeInfo();
    
    if (typeid(*instr) == typeid(assem::OperInstr)) {
      assem::OperInstr *oper_instr = static_cast<assem::OperInstr *>(instr);

      // Jump instruction jumps to a label and may fall through
      if (oper_instr->jumps_ != nullptr) {
        for (temp::Label *label : *oper_instr->jumps_->labels_) {
          flowgraph_->AddEdge(node, label_map_.get()->Look(label)); 
        }

        // Conditional jump falls through
        if (oper_instr->assem_.find("jmp") == std::string::npos)
          flowgraph_->AddEdge(node, *(++node_it));
        else
          ++node_it; 

      // Other instructions just fall through
      } else {
        flowgraph_->AddEdge(node, *(++node_it));
      }

    } else {
      flowgraph_->AddEdge(node, *(++node_it));
    }
  }
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return new temp::TempList();
}

temp::TempList *MoveInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_ == nullptr ? new temp::TempList() : dst_;
}

temp::TempList *OperInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_ == nullptr ? new temp::TempList() : dst_;
}

temp::TempList *LabelInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return new temp::TempList();
}

temp::TempList *MoveInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_ == nullptr ? new temp::TempList() : src_;
}

temp::TempList *OperInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_ == nullptr ? new temp::TempList() : src_;
}
} // namespace assem
