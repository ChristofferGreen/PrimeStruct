struct LowerSetupStageInput;
struct LowerSetupStageState;

bool runLowerSetupStage(const LowerSetupStageInput &input,
                        LowerSetupStageState &stateOut,
                        std::string &errorOut);
