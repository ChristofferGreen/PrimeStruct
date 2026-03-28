



using EmitExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using LowerExprEmitUnaryPassthroughCallFn =
    std::function<UnaryPassthroughCallResult(const Expr &, const LocalMap &, const EmitExprWithLocalsFn &, std::string &)>;
using LowerExprEmitMovePassthroughCallFn = LowerExprEmitUnaryPassthroughCallFn;
using LowerExprEmitUploadPassthroughCallFn = LowerExprEmitUnaryPassthroughCallFn;
using LowerExprEmitReadbackPassthroughCallFn = LowerExprEmitUnaryPassthroughCallFn;

struct LowerExprEmitSetupInput {};

bool runLowerExprEmitSetup(const LowerExprEmitSetupInput &input,
                           LowerExprEmitMovePassthroughCallFn &emitMovePassthroughCallOut,
                           LowerExprEmitUploadPassthroughCallFn &emitUploadPassthroughCallOut,
                           LowerExprEmitReadbackPassthroughCallFn &emitReadbackPassthroughCallOut,
                           std::string &errorOut);
UnaryPassthroughCallResult runLowerExprEmitMovePassthroughStep(
    const Expr &expr,
    const LocalMap &localsIn,
    const LowerExprEmitMovePassthroughCallFn &emitMovePassthroughCall,
    const EmitExprWithLocalsFn &emitExpr,
    std::string &errorOut);
UnaryPassthroughCallResult runLowerExprEmitUploadReadbackPassthroughStep(
    const Expr &expr,
    const LocalMap &localsIn,
    const LowerExprEmitUploadPassthroughCallFn &emitUploadPassthroughCall,
    const LowerExprEmitReadbackPassthroughCallFn &emitReadbackPassthroughCall,
    const EmitExprWithLocalsFn &emitExpr,
    std::string &errorOut);

