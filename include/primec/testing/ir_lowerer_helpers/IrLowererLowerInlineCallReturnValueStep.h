



struct LowerInlineCallReturnValueStepInput {
  IrFunction *function = nullptr;
  bool returnsVoid = true;
  int32_t returnLocal = -1;
  bool structDefinition = false;
  bool requireValue = false;
};

bool runLowerInlineCallReturnValueStep(const LowerInlineCallReturnValueStepInput &input,
                                       std::string &errorOut);
