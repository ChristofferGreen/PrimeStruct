#include "primec/Parser.h"

#include "ParserHelpers.h"

#include <sstream>

namespace primec {
using namespace parser;

namespace {

bool isLoopFormKeyword(const std::string &name) {
  return name == "loop" || name == "while" || name == "for";
}

bool hasSumTransform(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "sum") {
      return true;
    }
  }
  return false;
}

std::string sumPayloadTypeText(const Transform &transform) {
  if (transform.templateArgs.empty()) {
    return transform.name;
  }
  std::ostringstream out;
  out << transform.name << "<";
  for (size_t i = 0; i < transform.templateArgs.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << transform.templateArgs[i];
  }
  out << ">";
  return out.str();
}

void populateParsedSumVariants(Definition &def) {
  if (!hasSumTransform(def)) {
    return;
  }

  def.sumVariants.clear();
  def.sumVariants.reserve(def.statements.size());
  for (const auto &stmt : def.statements) {
    if (stmt.isBinding && stmt.transforms.size() == 1) {
      const Transform &payload = stmt.transforms.front();
      def.sumVariants.push_back(SumVariant{
          stmt.name,
          true,
          payload.name,
          payload.templateArgs,
          sumPayloadTypeText(payload),
          def.sumVariants.size(),
          stmt.sourceLine,
          stmt.sourceColumn,
          stmt.semanticNodeId,
      });
    } else if (stmt.kind == Expr::Kind::Name && !stmt.name.empty() &&
               stmt.args.empty() && stmt.argNames.empty() &&
               stmt.bodyArguments.empty() && !stmt.hasBodyArguments &&
               stmt.transforms.empty() && stmt.templateArgs.empty()) {
      def.sumVariants.push_back(SumVariant{
          stmt.name,
          false,
          {},
          {},
          {},
          def.sumVariants.size(),
          stmt.sourceLine,
          stmt.sourceColumn,
          stmt.semanticNodeId,
      });
    }
  }
}

} // namespace

bool Parser::parseDefinitionBody(Definition &def, bool allowNoReturn, std::vector<Definition> &defs) {
  bool returnsVoid = false;
  bool hasReturnTransform = false;
  bool allowNoReturnAuto = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1 &&
        transform.templateArgs.front() == "void") {
      returnsVoid = true;
    }
    if (transform.name == "return") {
      hasReturnTransform = true;
      if (transform.templateArgs.size() == 1 && transform.templateArgs.front() == "auto") {
        allowNoReturnAuto = true;
      }
    }
  }
  const bool allowImplicitVoidReturn = !hasReturnTransform && !allowNoReturn;
  bool foundReturn = false;
  struct ReturnContextGuard {
    Parser *parser;
    bool *prevTracker;
    bool prevReturnsVoid;
    bool prevAllowImplicitVoid;
    ReturnContextGuard(Parser *parserIn, bool *tracker, bool returnsVoid, bool allowImplicitVoid)
        : parser(parserIn),
          prevTracker(parserIn->returnTracker_),
          prevReturnsVoid(parserIn->returnsVoidContext_),
          prevAllowImplicitVoid(parserIn->allowImplicitVoidReturn_) {
      parser->returnTracker_ = tracker;
      parser->returnsVoidContext_ = returnsVoid;
      parser->allowImplicitVoidReturn_ = allowImplicitVoid;
    }
    ~ReturnContextGuard() {
      parser->returnTracker_ = prevTracker;
      parser->returnsVoidContext_ = prevReturnsVoid;
      parser->allowImplicitVoidReturn_ = prevAllowImplicitVoid;
    }
  } guard(this, &foundReturn, returnsVoid, allowImplicitVoidReturn);
  if (!expect(TokenKind::LBrace, "expected '{' to start body")) {
    return false;
  }
  while (!match(TokenKind::RBrace)) {
    if (match(TokenKind::End)) {
      return fail("unexpected end of file inside definition body");
    }
    if (match(TokenKind::Semicolon)) {
      expect(TokenKind::Semicolon, "expected ';'");
      continue;
    }
    if (match(TokenKind::LBracket)) {
      Expr lambdaExpr;
      bool parsedLambda = false;
      if (!tryParseLambdaExpr(lambdaExpr, def.namespacePrefix, parsedLambda)) {
        return false;
      }
      if (parsedLambda) {
        def.statements.push_back(std::move(lambdaExpr));
        continue;
      }
    }
    std::vector<Transform> statementTransforms;
    bool hasStatementTransforms = false;
    if (match(TokenKind::LBracket)) {
      if (!parseTransformList(statementTransforms)) {
        return false;
      }
      hasStatementTransforms = true;
    }
    if (match(TokenKind::Identifier) && tokens_[pos_].text == "return") {
      if (hasStatementTransforms) {
        return fail("return statement cannot have transforms");
      }
      Expr returnCall;
      if (!parseReturnStatement(returnCall, def.namespacePrefix)) {
        return false;
      }
      def.hasReturnStatement = true;
      if (!returnCall.args.empty()) {
        def.returnExpr = returnCall.args.front();
      }
      def.statements.push_back(std::move(returnCall));
      continue;
    }
    if (allowSurfaceSyntax_ && match(TokenKind::Identifier) &&
        (tokens_[pos_].text == "if" || tokens_[pos_].text == "match")) {
      if (hasStatementTransforms) {
        return fail(tokens_[pos_].text + " statement cannot have transforms");
      }
      Expr ifExpr;
      bool parsedIf = false;
      if (!tryParseIfStatementSugar(ifExpr, def.namespacePrefix, parsedIf, true)) {
        return false;
      }
      if (parsedIf) {
        def.statements.push_back(std::move(ifExpr));
        continue;
      }
    }

    bool parsedNestedDefinition = false;
    if (!tryParseNestedDefinition(defs, statementTransforms, def.fullPath, parsedNestedDefinition)) {
      return false;
    }
    if (parsedNestedDefinition) {
      continue;
    }

    if (!statementTransforms.empty()) {
      Token name = consume(TokenKind::Identifier, "expected statement");
      if (name.kind == TokenKind::End) {
        return false;
      }
      if (name.text == "if" || name.text == "match") {
        return fail(name.text + " statement cannot have transforms");
      }
      if (name.text == "return") {
        return fail("return statement cannot have transforms");
      }

      const bool bindingTransforms = isBindingTransformList(statementTransforms);
      if (allowSurfaceSyntax_ && isLoopFormKeyword(name.text)) {
        Expr loopExpr;
        bool parsedLoop = false;
        if (!tryParseLoopFormAfterName(loopExpr, def.namespacePrefix, name.text, statementTransforms,
                                       parsedLoop)) {
          return false;
        }
        if (parsedLoop) {
          def.statements.push_back(std::move(loopExpr));
          continue;
        }
      }
      std::string nameError;
      if (!validateIdentifierText(name.text, nameError)) {
        if (!isLoopFormKeyword(name.text) || (!match(TokenKind::LParen) && !match(TokenKind::LAngle))) {
          return fail(nameError);
        }
      }

      Expr callExpr;
      callExpr.kind = Expr::Kind::Call;
      callExpr.name = name.text;
      callExpr.namespacePrefix = def.namespacePrefix;
      callExpr.transforms = std::move(statementTransforms);
      bool hasCallSyntax = false;
      bool sawParen = false;
      if (match(TokenKind::LAngle)) {
        if (!parseTemplateList(callExpr.templateArgs)) {
          return false;
        }
      }
      if (match(TokenKind::LParen)) {
        BareBindingGuard bindingGuard(*this, true);
        expect(TokenKind::LParen, "expected '(' after identifier");
        hasCallSyntax = true;
        sawParen = true;
        if (!parseCallArgumentList(callExpr.args, callExpr.argNames, def.namespacePrefix)) {
          return false;
        }
        if (!validateNamedArgumentOrdering(callExpr.argNames)) {
          return false;
        }
        if (!expect(TokenKind::RParen, "expected ')' to close call")) {
          return false;
        }
      }
      if (match(TokenKind::LBrace)) {
        if (!sawParen) {
          if (!callExpr.templateArgs.empty()) {
            return fail("template arguments require a call");
          }
          callExpr.isBinding = true;
          if (!parseBindingInitializerList(callExpr.args, callExpr.argNames, def.namespacePrefix)) {
            return false;
          }
          if (!finalizeBindingInitializer(callExpr)) {
            return false;
          }
          def.statements.push_back(std::move(callExpr));
          continue;
        }
        if (!allowSurfaceSyntax_ &&
            (callExpr.name == "if" || callExpr.name == "loop" || callExpr.name == "while" ||
             callExpr.name == "for")) {
          return fail("control-flow body sugar requires canonical call form");
        }
        hasCallSyntax = true;
        callExpr.hasBodyArguments = true;
        if (callExpr.name == "block") {
          if (!parseValueBlock(callExpr.bodyArguments, def.namespacePrefix)) {
            return false;
          }
        } else {
          if (!parseBraceExprList(callExpr.bodyArguments, def.namespacePrefix, true)) {
            return false;
          }
        }
      }
      if (hasCallSyntax) {
        def.statements.push_back(std::move(callExpr));
        continue;
      }
      if (!callExpr.templateArgs.empty()) {
        return fail("template arguments require a call");
      }
      if (bindingTransforms) {
        callExpr.isBinding = true;
        def.statements.push_back(std::move(callExpr));
        continue;
      }
      return fail("transform-prefixed execution requires arguments");
    }

    Expr statementExpr;
    {
      BareBindingGuard bindingGuard(*this, true);
      SingleBranchIfGuard singleBranchGuard(*this, true);
      if (!parseExpr(statementExpr, def.namespacePrefix)) {
        return false;
      }
    }
    def.statements.push_back(std::move(statementExpr));
  }
  expect(TokenKind::RBrace, "expected '}' to close body");
  if (!foundReturn && !returnsVoid && !allowNoReturn && !allowNoReturnAuto) {
    if (!allowImplicitVoidReturn) {
      return fail("missing return statement in definition body");
    }
  }
  def.hasReturnStatement = foundReturn;
  populateParsedSumVariants(def);
  return true;
}

} // namespace primec
