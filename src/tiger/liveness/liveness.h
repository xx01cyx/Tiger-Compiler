#ifndef TIGER_LIVENESS_LIVENESS_H_
#define TIGER_LIVENESS_LIVENESS_H_

#include "tiger/codegen/assem.h"
#include "tiger/frame/x64frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/flowgraph.h"
#include "tiger/util/graph.h"

namespace live {

using INode = graph::Node<temp::Temp>;
using INodePtr = graph::Node<temp::Temp>*;
using INodeList = graph::NodeList<temp::Temp>;
using INodeListPtr = graph::NodeList<temp::Temp>*;
using IGraph = graph::IGraph;
using IGraphPtr = graph::IGraph*;
using Move = std::pair<INodePtr, INodePtr>;
using InstrPos = std::list<assem::Instr *>::const_iterator;
using NodeInstrMap = std::unordered_map<INode*, std::vector<InstrPos>*>;

class MoveList {
public:
  MoveList() = default;
  explicit MoveList(Move m) : move_list_({m}) {}

  [[nodiscard]] const std::list<Move> &GetList() const { return move_list_; }
  void Append(INodePtr src, INodePtr dst) { move_list_.emplace_back(src, dst); }
  bool Contain(INodePtr src, INodePtr dst);
  void Delete(INodePtr src, INodePtr dst);
  void Clear() { move_list_.clear(); }
  void Prepend(INodePtr src, INodePtr dst) {
    move_list_.emplace_front(src, dst);
  }
  MoveList *Union(MoveList *list);
  MoveList *Intersect(MoveList *list);
  MoveList *Diff(MoveList *list);

private:
  std::list<Move> move_list_;
};

struct LiveGraph {
  IGraphPtr interf_graph;
  tab::Table<INode, MoveList> *move_list;

  LiveGraph(IGraphPtr interf_graph, MoveList *moves)
      : interf_graph(interf_graph), 
        move_list(new tab::Table<INode, MoveList>()) {}
};

class LiveGraphFactory {
public:
  LiveGraphFactory();
        
  void Liveness(fg::FGraphPtr flowgraph, MoveList **worklist_moves);
  void BuildIGraph(assem::InstrList *instr_list);
  LiveGraph GetLiveGraph() { return live_graph_; }
  std::shared_ptr<NodeInstrMap> GetNodeInstrMap() { return node_instr_map_; }
  tab::Table<temp::Temp, INode> *GetTempNodeMap() { return temp_node_map_; }

private:
  LiveGraph live_graph_;

  std::unique_ptr<graph::Table<assem::Instr, temp::TempList>> in_;
  std::unique_ptr<graph::Table<assem::Instr, temp::TempList>> out_;
  tab::Table<temp::Temp, INode> *temp_node_map_;
  std::shared_ptr<NodeInstrMap> node_instr_map_;

  void LiveMap(fg::FGraphPtr flowgraph);
  void InterfGraph(fg::FGraphPtr flowgraph, MoveList **worklist_moves);
};

} // namespace live

#endif