struct Definition;
struct LowerSetupStageState;

struct LowerReturnEmitInlineContext;
struct LowerReturnEmitStageInput;
struct LowerReturnEmitStageState;

bool runLowerReturnEmitStage(const LowerReturnEmitStageInput &input,
                             LowerReturnEmitStageState &stateOut,
                             std::string &errorOut);
