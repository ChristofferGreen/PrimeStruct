using ::primec::Definition;
using ::primec::IrFunction;
using ::primec::Program;
using ::primec::SemanticProgram;

struct LowerSetupStageInput;
struct LowerSetupStageState;

bool runLowerSetupStage(const LowerSetupStageInput &input,
                        LowerSetupStageState &stateOut,
                        std::string &errorOut);
