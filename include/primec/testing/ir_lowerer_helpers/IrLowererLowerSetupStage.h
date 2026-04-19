struct Definition;
struct IrFunction;
struct Program;
struct SemanticProgram;

struct LowerSetupStageInput;
struct LowerSetupStageState;

bool runLowerSetupStage(const LowerSetupStageInput &input,
                        LowerSetupStageState &stateOut,
                        std::string &errorOut);
