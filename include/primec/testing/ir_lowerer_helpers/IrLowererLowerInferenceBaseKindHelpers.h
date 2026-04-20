#pragma once

bool isMapTryAtCallName(const Expr &expr);
bool isMapContainsCallName(const Expr &expr);
bool inferMapTryAtResultValueKind(const Expr &expr,
                                  const LocalMap &localsIn,
                                  LocalInfo::ValueKind &kindOut);
bool inferMapContainsResultKind(const Expr &expr,
                                const LocalMap &localsIn,
                                LocalInfo::ValueKind &kindOut);
