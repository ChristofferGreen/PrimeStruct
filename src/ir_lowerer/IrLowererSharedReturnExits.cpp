#include "primec/ir_lowerer/IrLowererSharedReturnExits.h"

#include <algorithm>

namespace primec::ir_lowerer {

void SharedReturnExits::begin(bool enabled) {
  enabled_ = enabled;
  exits_.clear();
  nodes_.clear();
  childByEntry_.clear();
  rootByOp_.clear();
}

bool SharedReturnExits::shouldShare(const std::vector<int32_t> &liveEntries) const {
  return enabled_ && std::any_of(liveEntries.begin(), liveEntries.end(), [](int32_t entry) {
           return entry < 0;
         });
}

void SharedReturnExits::emitExit(std::vector<IrInstruction> &instructions,
                                 IrOpcode returnOp,
                                 const std::vector<int32_t> &liveEntries,
                                 const std::function<int32_t()> &allocLocal) {
  auto rootIt = rootByOp_.find(returnOp);
  if (rootIt == rootByOp_.end()) {
    Node root;
    root.returnOp = returnOp;
    if (returnOp != IrOpcode::ReturnVoid) {
      root.returnLocal = allocLocal();
    }
    nodes_.push_back(root);
    rootIt = rootByOp_.emplace(returnOp, nodes_.size() - 1).first;
  }
  size_t node = rootIt->second;
  const int32_t returnLocal = nodes_[node].returnLocal;
  for (const int32_t entry : liveEntries) {
    const auto key = std::make_pair(node, entry);
    auto childIt = childByEntry_.find(key);
    if (childIt == childByEntry_.end()) {
      Node child;
      child.returnOp = returnOp;
      child.parent = node;
      child.entry = entry;
      child.isRoot = false;
      child.returnLocal = returnLocal;
      nodes_.push_back(child);
      childIt = childByEntry_.emplace(key, nodes_.size() - 1).first;
    }
    node = childIt->second;
  }
  if (returnOp != IrOpcode::ReturnVoid) {
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(returnLocal)});
  }
  Exit exit;
  exit.jumpIndex = instructions.size();
  exit.node = node;
  instructions.push_back({IrOpcode::Jump, 0});
  exits_.push_back(exit);
}

bool SharedReturnExits::endsWithSharedExit(size_t instructionCount) const {
  return !exits_.empty() && exits_.back().jumpIndex + 1 == instructionCount;
}

bool SharedReturnExits::finish(std::vector<IrInstruction> &instructions,
                               const EmitEntryCleanupFn &emitEntryCleanup) {
  // A child comes after its parent in `nodes_`, so emitting the last node first puts a parent right after its child
  // along a chain; a node jumps to its parent only when the parent is not the node emitted next.
  std::vector<size_t> start(nodes_.size(), 0);
  std::vector<size_t> jumpToParent(nodes_.size(), 0);
  std::vector<bool> hasJump(nodes_.size(), false);
  for (size_t index = nodes_.size(); index-- > 0;) {
    const Node &node = nodes_[index];
    start[index] = instructions.size();
    if (node.isRoot) {
      if (node.returnOp != IrOpcode::ReturnVoid) {
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(node.returnLocal)});
      }
      instructions.push_back({node.returnOp, 0});
      continue;
    }
    if (!emitEntryCleanup(node.entry)) {
      return false;
    }
    if (node.parent + 1 != index) {
      jumpToParent[index] = instructions.size();
      hasJump[index] = true;
      instructions.push_back({IrOpcode::Jump, 0});
    }
  }
  for (size_t index = 0; index < nodes_.size(); ++index) {
    if (hasJump[index]) {
      instructions[jumpToParent[index]].imm = static_cast<uint64_t>(start[nodes_[index].parent]);
    }
  }
  for (const Exit &exit : exits_) {
    instructions[exit.jumpIndex].imm = static_cast<uint64_t>(start[exit.node]);
  }
  return true;
}

} // namespace primec::ir_lowerer
