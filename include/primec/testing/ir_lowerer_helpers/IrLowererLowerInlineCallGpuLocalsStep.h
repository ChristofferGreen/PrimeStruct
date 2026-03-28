



struct LowerInlineCallGpuLocalsStepInput {
  const LocalMap *callerLocals = nullptr;
  LocalMap *calleeLocals = nullptr;
};

bool runLowerInlineCallGpuLocalsStep(const LowerInlineCallGpuLocalsStepInput &input,
                                     std::string &errorOut);

