#pragma once

bool isMapTryAtCallName(const Expr &expr);
bool isMapContainsCallName(const Expr &expr);
bool inferMapTryAtResultValueKind(const Expr &expr,
                                  const LocalMap &localsIn,
                                  LocalInfo::ValueKind &kindOut,
                                  const SemanticProgram *semanticProgram = nullptr,
                                  const SemanticProductIndex *semanticIndex = nullptr);
bool inferMapContainsResultKind(const Expr &expr,
                                const LocalMap &localsIn,
                                LocalInfo::ValueKind &kindOut,
                                const SemanticProgram *semanticProgram = nullptr,
                                const SemanticProductIndex *semanticIndex = nullptr);
