struct Definition;
struct IrFunction;
struct Program;
struct SemanticProgram;

struct LowerStatementsCallsStageInput;

bool runLowerStatementsCallsStage(const LowerStatementsCallsStageInput &input,
                                  std::string &errorOut);
