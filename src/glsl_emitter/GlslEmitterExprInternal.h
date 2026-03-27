#pragma once

#include "GlslEmitterInternal.h"

namespace primec::glsl_emitter {

std::string callLeafName(const std::string &name);
int vectorDimension(GlslType type);
int matrixDimension(GlslType type);
bool vectorFieldComponentIndex(const std::string &name, int dimension, int &componentOut);
bool matrixFieldRowColumn(const std::string &name, int dimension, int &rowOut, int &columnOut);

bool emitQuaternionHelperCall(const Expr &expr,
                              const std::string &leafName,
                              EmitState &state,
                              std::string &error,
                              ExprResult &out);
bool emitVectorArithmetic(const Expr &expr,
                          const std::string &name,
                          EmitState &state,
                          std::string &error,
                          ExprResult &out);
bool emitQuaternionArithmetic(const Expr &expr,
                              const std::string &name,
                              EmitState &state,
                              std::string &error,
                              ExprResult &out);
bool emitVectorConstructor(const Expr &expr,
                           const std::string &name,
                           GlslType type,
                           EmitState &state,
                           std::string &error,
                           ExprResult &out);
bool emitMatrixConstructor(const Expr &expr,
                           const std::string &name,
                           GlslType type,
                           EmitState &state,
                           std::string &error,
                           ExprResult &out);
bool emitMatrixArithmetic(const Expr &expr,
                          const std::string &name,
                          EmitState &state,
                          std::string &error,
                          ExprResult &out);
bool tryEmitMathBuiltinCall(const Expr &expr,
                            const std::string &mathName,
                            EmitState &state,
                            std::string &error,
                            ExprResult &out);

} // namespace primec::glsl_emitter
