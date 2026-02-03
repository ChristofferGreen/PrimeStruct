#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
struct Options {
  std::string emitKind;
  std::string inputPath;
  std::string outputPath;
  std::string entryPath = "/main";
};

bool parseArgs(int argc, char **argv, Options &out) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--emit=cpp" || arg == "--emit=exe") {
      out.emitKind = arg.substr(std::string("--emit=").size());
    } else if (arg == "-o" && i + 1 < argc) {
      out.outputPath = argv[++i];
    } else if (arg == "--entry" && i + 1 < argc) {
      out.entryPath = argv[++i];
    } else if (arg.rfind("--entry=", 0) == 0) {
      out.entryPath = arg.substr(std::string("--entry=").size());
    } else if (!arg.empty() && arg[0] == '-') {
      return false;
    } else {
      out.inputPath = arg;
    }
  }
  if (out.entryPath.empty()) {
    out.entryPath = "/main";
  } else if (out.entryPath[0] != '/') {
    out.entryPath = "/" + out.entryPath;
  }
  return !out.emitKind.empty() && !out.inputPath.empty() && !out.outputPath.empty();
}

bool readFile(const std::string &path, std::string &out) {
  std::ifstream file(path);
  if (!file) {
    return false;
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  out = buffer.str();
  return true;
}

bool writeFile(const std::string &path, const std::string &contents) {
  std::ofstream file(path);
  if (!file) {
    return false;
  }
  file << contents;
  return file.good();
}

enum class TokenKind {
  Identifier,
  Number,
  LBracket,
  RBracket,
  LParen,
  RParen,
  LBrace,
  RBrace,
  LAngle,
  RAngle,
  Comma,
  KeywordNamespace,
  End
};

struct Token {
  TokenKind kind;
  std::string text;
};

struct Transform {
  std::string name;
  std::optional<std::string> templateArg;
};

struct Expr {
  enum class Kind { Literal, Call, Name } kind = Kind::Literal;
  int literalValue = 0;
  std::string name;
  std::vector<Expr> args;
  std::string namespacePrefix;
};

struct Definition {
  std::string name;
  std::string fullPath;
  std::string namespacePrefix;
  std::vector<Transform> transforms;
  std::vector<std::string> parameters;
  Expr returnExpr;
};

struct Execution {
  std::string name;
  std::string fullPath;
  std::string namespacePrefix;
};


class Lexer {
public:
  explicit Lexer(const std::string &source) : source_(source) {}

  std::vector<Token> tokenize() {
    std::vector<Token> tokens;
    while (true) {
      skipWhitespace();
      if (pos_ >= source_.size()) {
        tokens.push_back({TokenKind::End, ""});
        break;
      }
      char c = source_[pos_];
      if (isIdentifierStart(c)) {
        tokens.push_back(readIdentifier());
      } else if (std::isdigit(static_cast<unsigned char>(c)) ||
                 (c == '-' && pos_ + 1 < source_.size() &&
                  std::isdigit(static_cast<unsigned char>(source_[pos_ + 1])))) {
        tokens.push_back(readNumber());
      } else {
        tokens.push_back(readPunct());
      }
    }
    return tokens;
  }

private:
  bool isIdentifierStart(char c) const {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '/';
  }

  bool isIdentifierBody(char c) const {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '/';
  }

  void skipWhitespace() {
    while (pos_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[pos_]))) {
      ++pos_;
    }
  }

  Token readIdentifier() {
    size_t start = pos_;
    while (pos_ < source_.size() && isIdentifierBody(source_[pos_])) {
      ++pos_;
    }
    std::string text = source_.substr(start, pos_ - start);
    if (text == "namespace") {
      return {TokenKind::KeywordNamespace, text};
    }
    return {TokenKind::Identifier, text};
  }

  Token readNumber() {
    size_t start = pos_;
    if (source_[pos_] == '-') {
      ++pos_;
    }
    while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
      ++pos_;
    }
    if (source_.compare(pos_, 3, "i32") == 0) {
      pos_ += 3;
    }
    return {TokenKind::Number, source_.substr(start, pos_ - start)};
  }

  Token readPunct() {
    char c = source_[pos_++];
    switch (c) {
    case '[':
      return {TokenKind::LBracket, "["};
    case ']':
      return {TokenKind::RBracket, "]"};
    case '(':
      return {TokenKind::LParen, "("};
    case ')':
      return {TokenKind::RParen, ")"};
    case '{':
      return {TokenKind::LBrace, "{"};
    case '}':
      return {TokenKind::RBrace, "}"};
    case '<':
      return {TokenKind::LAngle, "<"};
    case '>':
      return {TokenKind::RAngle, ">"};
    case ',':
      return {TokenKind::Comma, ","};
    default:
      return {TokenKind::End, ""};
    }
  }

  const std::string &source_;
  size_t pos_ = 0;
};

class Parser {
public:
  explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

  bool parse(std::vector<Definition> &defs, std::vector<Execution> &execs, std::string &error) {
    error_ = &error;
    while (!match(TokenKind::End)) {
      if (match(TokenKind::KeywordNamespace)) {
        if (!parseNamespace(defs, execs)) {
          return false;
        }
      } else {
        if (!parseDefinitionOrExecution(defs, execs)) {
          return false;
        }
      }
    }
    return true;
  }

private:
  bool parseNamespace(std::vector<Definition> &defs, std::vector<Execution> &execs) {
    if (!expect(TokenKind::KeywordNamespace, "expected 'namespace'")) {
      return false;
    }
    Token name = consume(TokenKind::Identifier, "expected namespace identifier");
    if (name.kind == TokenKind::End) {
      return false;
    }
    if (!expect(TokenKind::LBrace, "expected '{' after namespace")) {
      return false;
    }
    namespaceStack_.push_back(name.text);
    while (!match(TokenKind::RBrace)) {
      if (match(TokenKind::End)) {
        return fail("unexpected end of file inside namespace block");
      }
      if (match(TokenKind::KeywordNamespace)) {
        if (!parseNamespace(defs, execs)) {
          return false;
        }
      } else if (!parseDefinitionOrExecution(defs, execs)) {
        return false;
      }
    }
    expect(TokenKind::RBrace, "expected '}'");
    namespaceStack_.pop_back();
    return true;
  }

  bool parseDefinitionOrExecution(std::vector<Definition> &defs, std::vector<Execution> &execs) {
    std::vector<Transform> transforms;
    if (match(TokenKind::LBracket)) {
      if (!parseTransformList(transforms)) {
        return false;
      }
    }

    Token name = consume(TokenKind::Identifier, "expected identifier");
    if (name.kind == TokenKind::End) {
      return false;
    }

    if (!expect(TokenKind::LParen, "expected '(' after identifier")) {
      return false;
    }
    std::vector<std::string> parameters;
    if (!parseIdentifierList(parameters)) {
      return false;
    }
    if (!expect(TokenKind::RParen, "expected ')' after parameters")) {
      return false;
    }

    if (!match(TokenKind::LBrace)) {
      Execution exec;
      exec.name = name.text;
      exec.namespacePrefix = currentNamespacePrefix();
      exec.fullPath = makeFullPath(exec.name, exec.namespacePrefix);
      execs.push_back(std::move(exec));
      return true;
    }
    Definition def;
    def.name = name.text;
    def.namespacePrefix = currentNamespacePrefix();
    def.fullPath = makeFullPath(def.name, def.namespacePrefix);
    def.transforms = std::move(transforms);
    def.parameters = std::move(parameters);
    if (!parseDefinitionBody(def)) {
      return false;
    }
    defs.push_back(std::move(def));
    return true;
  }

  bool parseTransformList(std::vector<Transform> &out) {
    if (!expect(TokenKind::LBracket, "expected '['")) {
      return false;
    }
    while (!match(TokenKind::RBracket)) {
      Token name = consume(TokenKind::Identifier, "expected transform identifier");
      if (name.kind == TokenKind::End) {
        return false;
      }
      Transform transform;
      transform.name = name.text;
      if (match(TokenKind::LAngle)) {
        expect(TokenKind::LAngle, "expected '<'");
        Token arg = consume(TokenKind::Identifier, "expected template argument");
        if (arg.kind == TokenKind::End) {
          return false;
        }
        transform.templateArg = arg.text;
        if (!expect(TokenKind::RAngle, "expected '>'")) {
          return false;
        }
      }
      out.push_back(std::move(transform));
      if (match(TokenKind::Comma)) {
        expect(TokenKind::Comma, "expected ','");
      }
    }
    expect(TokenKind::RBracket, "expected ']'" );
    return true;
  }

  bool parseIdentifierList(std::vector<std::string> &out) {
    if (match(TokenKind::RParen)) {
      return true;
    }
    Token first = consume(TokenKind::Identifier, "expected parameter identifier");
    if (first.kind == TokenKind::End) {
      return false;
    }
    out.push_back(first.text);
    if (!match(TokenKind::RParen)) {
      while (match(TokenKind::Comma)) {
        expect(TokenKind::Comma, "expected ','");
        Token next = consume(TokenKind::Identifier, "expected parameter identifier");
        if (next.kind == TokenKind::End) {
          return false;
        }
        out.push_back(next.text);
      }
    }
    return true;
  }

  bool parseDefinitionBody(Definition &def) {
    if (!expect(TokenKind::LBrace, "expected '{' to start body")) {
      return false;
    }
    Token name = consume(TokenKind::Identifier, "expected statement");
    if (name.kind == TokenKind::End) {
      return false;
    }
    if (name.text != "return") {
      return fail("only return(...) is supported inside definitions");
    }
    Expr returnCall = {};
    returnCall.kind = Expr::Kind::Call;
    returnCall.name = name.text;
    returnCall.namespacePrefix = def.namespacePrefix;
    if (!expect(TokenKind::LParen, "expected '(' after return")) {
      return false;
    }
    Expr arg;
    if (!parseExpr(arg, def.namespacePrefix)) {
      return false;
    }
    returnCall.args.push_back(std::move(arg));
    if (!expect(TokenKind::RParen, "expected ')' after return argument")) {
      return false;
    }
    if (!expect(TokenKind::RBrace, "expected '}' to close body")) {
      return false;
    }
    def.returnExpr = returnCall.args.front();
    return true;
  }

  bool parseExpr(Expr &expr, const std::string &namespacePrefix) {
    if (match(TokenKind::Number)) {
      Token number = consume(TokenKind::Number, "expected number");
      if (number.kind == TokenKind::End) {
        return false;
      }
      expr.kind = Expr::Kind::Literal;
      expr.namespacePrefix = namespacePrefix;
      std::string text = number.text;
      if (text.size() >= 3 && text.compare(text.size() - 3, 3, "i32") == 0) {
        text = text.substr(0, text.size() - 3);
      }
      try {
        expr.literalValue = std::stoi(text);
      } catch (const std::exception &) {
        return fail("invalid integer literal");
      }
      return true;
    }
    if (match(TokenKind::Identifier)) {
      Token name = consume(TokenKind::Identifier, "expected identifier");
      if (name.kind == TokenKind::End) {
        return false;
      }
      if (match(TokenKind::LParen)) {
        expect(TokenKind::LParen, "expected '(' after identifier");
        Expr call;
        call.kind = Expr::Kind::Call;
        call.name = name.text;
        call.namespacePrefix = namespacePrefix;
        if (!match(TokenKind::RParen)) {
          while (true) {
            Expr arg;
            if (!parseExpr(arg, namespacePrefix)) {
              return false;
            }
            call.args.push_back(std::move(arg));
            if (match(TokenKind::Comma)) {
              expect(TokenKind::Comma, "expected ','");
            } else {
              break;
            }
          }
        }
        if (!expect(TokenKind::RParen, "expected ')' to close call")) {
          return false;
        }
        expr = std::move(call);
        return true;
      }

      expr.kind = Expr::Kind::Name;
      expr.name = name.text;
      expr.namespacePrefix = namespacePrefix;
      return true;
    }
    return fail("unexpected token in expression");
  }

  std::string currentNamespacePrefix() const {
    if (namespaceStack_.empty()) {
      return "";
    }
    std::ostringstream out;
    for (const auto &segment : namespaceStack_) {
      out << "/" << segment;
    }
    return out.str();
  }

  std::string makeFullPath(const std::string &name, const std::string &prefix) const {
    if (!name.empty() && name[0] == '/') {
      return name;
    }
    if (prefix.empty()) {
      return "/" + name;
    }
    return prefix + "/" + name;
  }

  bool match(TokenKind kind) const {
    return tokens_[pos_].kind == kind;
  }

  bool expect(TokenKind kind, const std::string &message) {
    if (!match(kind)) {
      return fail(message);
    }
    ++pos_;
    return true;
  }

  Token consume(TokenKind kind, const std::string &message) {
    if (!match(kind)) {
      fail(message);
      return {TokenKind::End, ""};
    }
    return tokens_[pos_++];
  }

  bool fail(const std::string &message) {
    if (error_) {
      *error_ = message;
    }
    return false;
  }

  std::vector<Token> tokens_;
  size_t pos_ = 0;
  std::vector<std::string> namespaceStack_;
  std::string *error_ = nullptr;
};

struct Program {
  std::vector<Definition> definitions;
  std::vector<Execution> executions;
};

bool validateProgram(const Program &program, const std::string &entryPath, std::string &error) {
  std::unordered_map<std::string, const Definition *> defMap;
  for (const auto &def : program.definitions) {
    if (defMap.count(def.fullPath) > 0) {
      error = "duplicate definition: " + def.fullPath;
      return false;
    }
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        if (!transform.templateArg || *transform.templateArg != "int") {
          error = "unsupported return type on " + def.fullPath;
          return false;
        }
      }
    }
    defMap[def.fullPath] = &def;
  }

  auto resolveCalleePath = [&](const Expr &expr) -> std::string {
    if (expr.name.empty()) {
      return "";
    }
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      return expr.namespacePrefix + "/" + expr.name;
    }
    return "/" + expr.name;
  };

  std::unordered_map<std::string, size_t> paramCounts;
  for (const auto &def : program.definitions) {
    paramCounts[def.fullPath] = def.parameters.size();
  }

  std::function<bool(const Definition &, const Expr &)> validateExpr =
      [&](const Definition &def, const Expr &expr) -> bool {
    if (expr.kind == Expr::Kind::Literal) {
      return true;
    }
    if (expr.kind == Expr::Kind::Name) {
      for (const auto &param : def.parameters) {
        if (param == expr.name) {
          return true;
        }
      }
      error = "unknown identifier: " + expr.name;
      return false;
    }
    if (expr.kind == Expr::Kind::Call) {
      std::string resolved = resolveCalleePath(expr);
      auto it = defMap.find(resolved);
      if (it == defMap.end()) {
        error = "unknown call target: " + expr.name;
        return false;
      }
      size_t expectedArgs = paramCounts[resolved];
      if (expr.args.size() != expectedArgs) {
        error = "argument count mismatch for " + resolved;
        return false;
      }
      for (const auto &arg : expr.args) {
        if (!validateExpr(def, arg)) {
          return false;
        }
      }
      return true;
    }
    return false;
  };

  for (const auto &def : program.definitions) {
    if (!validateExpr(def, def.returnExpr)) {
      return false;
    }
  }

  if (defMap.count(entryPath) == 0) {
    error = "missing entry definition " + entryPath;
    return false;
  }
  return true;
}

std::string toCppName(const std::string &fullPath) {
  std::string name = "ps";
  for (char c : fullPath) {
    if (c == '/') {
      name += "_";
    } else {
      name += c;
    }
  }
  return name;
}

std::string emitExpr(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap) {
  if (expr.kind == Expr::Kind::Literal) {
    return std::to_string(expr.literalValue);
  }
  if (expr.kind == Expr::Kind::Name) {
    return expr.name;
  }
  std::string full;
  if (!expr.name.empty() && expr.name[0] == '/') {
    full = expr.name;
  } else if (!expr.namespacePrefix.empty()) {
    full = expr.namespacePrefix + "/" + expr.name;
  } else {
    full = "/" + expr.name;
  }
  auto it = nameMap.find(full);
  if (it == nameMap.end()) {
    return "0";
  }
  std::ostringstream out;
  out << it->second << "(";
  for (size_t i = 0; i < expr.args.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << emitExpr(expr.args[i], nameMap);
  }
  out << ")";
  return out.str();
}

std::string emitCpp(const Program &program, const std::string &entryPath) {
  std::unordered_map<std::string, std::string> nameMap;
  for (const auto &def : program.definitions) {
    nameMap[def.fullPath] = toCppName(def.fullPath);
  }

  std::ostringstream out;
  out << "// Generated by PrimeStructc (minimal)\n";
  for (const auto &def : program.definitions) {
    out << "static int " << nameMap[def.fullPath] << "(";
    for (size_t i = 0; i < def.parameters.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      out << "int " << def.parameters[i];
    }
    out << ") { return " << emitExpr(def.returnExpr, nameMap) << "; }\n";
  }

  out << "int main() { return " << nameMap.at(entryPath) << "(); }\n";
  return out.str();
}

bool runCommand(const std::string &command) {
  return std::system(command.c_str()) == 0;
}

std::string quotePath(const std::filesystem::path &path) {
  std::string text = path.string();
  std::string quoted = "\"";
  for (char c : text) {
    if (c == '\"') {
      quoted += "\\\"";
    } else {
      quoted += c;
    }
  }
  quoted += "\"";
  return quoted;
}
} // namespace

int main(int argc, char **argv) {
  Options options;
  if (!parseArgs(argc, argv, options)) {
  std::cerr << "Usage: PrimeStructc --emit=cpp|exe <input.prime> -o <output> [--entry /path]\n";
  return 2;
}

  std::string source;
  if (!readFile(options.inputPath, source)) {
    std::cerr << "Failed to read input: " << options.inputPath << "\n";
    return 2;
  }

  Lexer lexer(source);
  Parser parser(lexer.tokenize());
  Program program;
  std::string error;
  if (!parser.parse(program.definitions, program.executions, error)) {
    std::cerr << "Parse error: " << error << "\n";
    return 2;
  }
  if (!validateProgram(program, options.entryPath, error)) {
    std::cerr << "Semantic error: " << error << "\n";
    return 2;
  }

  std::string cppSource = emitCpp(program, options.entryPath);

  if (options.emitKind == "cpp") {
    if (!writeFile(options.outputPath, cppSource)) {
      std::cerr << "Failed to write output: " << options.outputPath << "\n";
      return 2;
    }
    return 0;
  }

  std::filesystem::path outputPath(options.outputPath);
  std::filesystem::path cppPath = outputPath;
  cppPath.replace_extension(".cpp");
  if (!writeFile(cppPath.string(), cppSource)) {
    std::cerr << "Failed to write intermediate C++: " << cppPath.string() << "\n";
    return 2;
  }

  std::string command = "clang++ -std=c++23 -O2 " + quotePath(cppPath) + " -o " + quotePath(outputPath);
  if (!runCommand(command)) {
    std::cerr << "Failed to compile output executable\n";
    return 3;
  }

  return 0;
}
