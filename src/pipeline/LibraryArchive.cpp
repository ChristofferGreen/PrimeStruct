#include "primec/pipeline/LibraryArchive.h"

#include "primec/ir/IrSerializer.h"
#include "primec/support/Sha256.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string_view>

namespace primec {
namespace {

// "PSLIBAR" and a line feed.
constexpr char Magic[8] = {'P', 'S', 'L', 'I', 'B', 'A', 'R', '\n'};
constexpr std::string_view ManifestHeader = "primestruct-library 1";
constexpr size_t HashHexSize = 64;
constexpr const char *RebuildHint = "; rebuild the library";

struct Section {
  std::string name;
  std::string bytes;
};

void appendU32(std::string &out, uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<char>((value >> shift) & 0xFFu));
  }
}

void appendU64(std::string &out, uint64_t value) {
  for (int shift = 0; shift < 64; shift += 8) {
    out.push_back(static_cast<char>((value >> shift) & 0xFFu));
  }
}

// A reader over bytes that fails instead of running past the end.
class ByteReader {
public:
  explicit ByteReader(std::string_view bytes) : bytes_(bytes) {
  }

  bool readU32(uint32_t &value) {
    if (bytes_.size() - offset_ < 4) {
      return false;
    }
    value = 0;
    for (int i = 0; i < 4; ++i) {
      value |= static_cast<uint32_t>(
                   static_cast<unsigned char>(bytes_[offset_ + static_cast<size_t>(i)]))
               << (8 * i);
    }
    offset_ += 4;
    return true;
  }

  bool readU64(uint64_t &value) {
    if (bytes_.size() - offset_ < 8) {
      return false;
    }
    value = 0;
    for (int i = 0; i < 8; ++i) {
      value |= static_cast<uint64_t>(
                   static_cast<unsigned char>(bytes_[offset_ + static_cast<size_t>(i)]))
               << (8 * i);
    }
    offset_ += 8;
    return true;
  }

  bool readBytes(uint64_t size, std::string_view &out) {
    if (size > bytes_.size() - offset_) {
      return false;
    }
    out = bytes_.substr(offset_, static_cast<size_t>(size));
    offset_ += static_cast<size_t>(size);
    return true;
  }

  bool atEnd() const {
    return offset_ == bytes_.size();
  }

private:
  std::string_view bytes_;
  size_t offset_ = 0;
};

bool isWord(const std::string &text) {
  if (text.empty()) {
    return false;
  }
  return std::none_of(text.begin(), text.end(), [](char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
  });
}

std::string manifestText(const LibraryManifest &manifest) {
  std::string out(ManifestHeader);
  out += "\nname " + manifest.name + "\n";
  for (const std::string &root : manifest.roots) {
    out += "root " + root + "\n";
  }
  for (const LibraryManifestStdlibModule &module : manifest.stdlibModules) {
    out += "stdlib " + module.file + "\n";
    for (const std::string &import : module.imports) {
      out += "stdimport " + module.file + " " + import + "\n";
    }
  }
  out += "compiler " + manifest.compilerBuildId + "\n";
  out += "irschema " + std::to_string(manifest.irSchemaVersion) + "\n";
  out += "hash " + manifest.hashScheme + "\n";
  out += "interface " + manifest.interfaceHash + "\n";
  for (const LibraryManifestDependency &library : manifest.libraries) {
    out += "library " + library.name + " " + library.interfaceHash + "\n";
  }
  return out;
}

std::string sourcesBytes(const std::vector<LibraryArchiveSource> &sources) {
  std::string out;
  appendU32(out, static_cast<uint32_t>(sources.size()));
  for (const LibraryArchiveSource &source : sources) {
    appendU32(out, static_cast<uint32_t>(source.path.size()));
    out += source.path;
    appendU64(out, source.text.size());
    out += source.text;
  }
  return out;
}

bool parseSources(std::string_view bytes, std::vector<LibraryArchiveSource> &out) {
  ByteReader reader(bytes);
  uint32_t count = 0;
  if (!reader.readU32(count)) {
    return false;
  }
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t pathSize = 0;
    std::string_view path;
    uint64_t textSize = 0;
    std::string_view text;
    if (!reader.readU32(pathSize) || !reader.readBytes(pathSize, path) ||
        !reader.readU64(textSize) || !reader.readBytes(textSize, text)) {
      return false;
    }
    out.push_back({std::string(path), std::string(text)});
  }
  return reader.atEnd();
}

std::string shortId(const std::string &id) {
  return id.size() > 12 ? id.substr(0, 12) : id;
}

bool damaged(std::string &error, const std::string &what) {
  error = "library archive is damaged (" + what + ")" + RebuildHint;
  return false;
}

// Splits the manifest into its lines, checking the header.
bool parseManifest(std::string_view text, LibraryManifest &out, std::string &error) {
  std::vector<std::string> lines;
  size_t start = 0;
  while (start < text.size()) {
    const size_t end = text.find('\n', start);
    if (end == std::string_view::npos) {
      return damaged(error, "manifest line without an end");
    }
    lines.emplace_back(text.substr(start, end - start));
    start = end + 1;
  }
  if (lines.empty() || lines.front() != ManifestHeader) {
    return damaged(error, "manifest has no header");
  }
  auto words = [](const std::string &line) {
    std::vector<std::string> parts;
    size_t from = 0;
    while (from <= line.size()) {
      const size_t space = line.find(' ', from);
      const size_t end = space == std::string::npos ? line.size() : space;
      parts.push_back(line.substr(from, end - from));
      from = end + 1;
    }
    return parts;
  };
  bool sawName = false;
  bool sawCompiler = false;
  bool sawSchema = false;
  bool sawHash = false;
  bool sawInterface = false;
  for (size_t i = 1; i < lines.size(); ++i) {
    const std::vector<std::string> parts = words(lines[i]);
    const std::string &key = parts.front();
    const bool one = parts.size() == 2 && !parts[1].empty();
    if (key == "name" && one && !sawName) {
      out.name = parts[1];
      sawName = true;
    } else if (key == "root" && one) {
      out.roots.push_back(parts[1]);
    } else if (key == "stdlib" && one) {
      out.stdlibModules.push_back({parts[1], {}});
    } else if (key == "stdimport" && parts.size() == 3 && !parts[1].empty() && !parts[2].empty()) {
      const auto module = std::find_if(
          out.stdlibModules.begin(),
          out.stdlibModules.end(),
          [&](const LibraryManifestStdlibModule &candidate) { return candidate.file == parts[1]; });
      if (module == out.stdlibModules.end()) {
        return damaged(error, "manifest imports for a stdlib module it does not list");
      }
      module->imports.push_back(parts[2]);
    } else if (key == "compiler" && one && !sawCompiler) {
      out.compilerBuildId = parts[1];
      sawCompiler = true;
    } else if (key == "irschema" && one && !sawSchema) {
      uint64_t version = 0;
      for (char c : parts[1]) {
        if (c < '0' || c > '9' || version > 0xFFFFFFFFull) {
          return damaged(error, "manifest has a bad IR schema version");
        }
        version = version * 10 + static_cast<uint64_t>(c - '0');
      }
      out.irSchemaVersion = static_cast<uint32_t>(version);
      sawSchema = true;
    } else if (key == "hash" && one && !sawHash) {
      out.hashScheme = parts[1];
      sawHash = true;
    } else if (key == "interface" && one && !sawInterface) {
      out.interfaceHash = parts[1];
      sawInterface = true;
    } else if (key == "library" && parts.size() == 3 && !parts[1].empty() && !parts[2].empty()) {
      out.libraries.push_back({parts[1], parts[2]});
    } else {
      return damaged(error, "manifest line `" + lines[i] + "` is not understood");
    }
  }
  if (!sawName || !sawCompiler || !sawSchema || !sawHash || !sawInterface) {
    return damaged(error, "manifest lacks a field");
  }
  return true;
}

bool fileBytes(const std::string &path, std::vector<uint8_t> &out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return !in.bad();
}

} // namespace

bool serializeLibraryArchive(const LibraryArchiveContent &content,
                             std::vector<uint8_t> &out,
                             std::string &error) {
  LibraryManifest manifest = content.manifest;
  if (!isWord(manifest.name) || !isWord(manifest.compilerBuildId) || !isWord(manifest.hashScheme) ||
      !isWord(manifest.interfaceHash)) {
    error = "library manifest has an empty field or one with white space";
    return false;
  }
  for (const std::string &root : manifest.roots) {
    if (!isWord(root)) {
      error = "library root `" + root + "` is empty or has white space";
      return false;
    }
  }
  for (const LibraryManifestStdlibModule &module : manifest.stdlibModules) {
    bool ok = isWord(module.file);
    for (const std::string &import : module.imports) {
      ok = ok && isWord(import);
    }
    if (!ok) {
      error =
          "stdlib module `" + module.file + "` of the library has an empty field or white space";
      return false;
    }
  }
  for (const LibraryManifestDependency &library : manifest.libraries) {
    if (!isWord(library.name) || !isWord(library.interfaceHash)) {
      error = "linked library `" + library.name + "` has an empty field or one with white space";
      return false;
    }
  }
  std::sort(manifest.roots.begin(), manifest.roots.end());
  std::sort(manifest.stdlibModules.begin(),
            manifest.stdlibModules.end(),
            [](const LibraryManifestStdlibModule &left, const LibraryManifestStdlibModule &right) {
              return left.file < right.file;
            });
  for (LibraryManifestStdlibModule &module : manifest.stdlibModules) {
    std::sort(module.imports.begin(), module.imports.end());
  }
  std::sort(manifest.libraries.begin(),
            manifest.libraries.end(),
            [](const LibraryManifestDependency &left, const LibraryManifestDependency &right) {
              return left.name < right.name;
            });

  std::vector<uint8_t> codeBytes;
  if (!serializeIr(content.code, codeBytes, error)) {
    return false;
  }
  std::vector<Section> sections;
  sections.push_back({"code", std::string(codeBytes.begin(), codeBytes.end())});
  sections.push_back({"interface", content.interfaceText});
  sections.push_back({"manifest", manifestText(manifest)});
  sections.push_back({"symbols", content.symbolsText});
  if (content.hasSources) {
    std::vector<LibraryArchiveSource> sources = content.sources;
    std::sort(sources.begin(),
              sources.end(),
              [](const LibraryArchiveSource &left, const LibraryArchiveSource &right) {
                return left.path < right.path;
              });
    sections.push_back({"sources", sourcesBytes(sources)});
  }
  std::sort(sections.begin(), sections.end(), [](const Section &left, const Section &right) {
    return left.name < right.name;
  });

  std::string bytes(Magic, sizeof(Magic));
  appendU32(bytes, LibraryArchiveFormatVersion);
  appendU32(bytes, static_cast<uint32_t>(sections.size()));
  for (const Section &section : sections) {
    appendU32(bytes, static_cast<uint32_t>(section.name.size()));
    bytes += section.name;
    appendU64(bytes, section.bytes.size());
    bytes += sha256Hex(section.bytes);
  }
  for (const Section &section : sections) {
    bytes += section.bytes;
  }
  out.assign(bytes.begin(), bytes.end());
  return true;
}

bool deserializeLibraryArchive(const std::vector<uint8_t> &bytes,
                               const LibraryArchiveExpectations &expected,
                               LibraryArchiveContent &out,
                               std::string &error) {
  out = {};
  const std::string_view view(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  if (view.size() < sizeof(Magic) || std::memcmp(view.data(), Magic, sizeof(Magic)) != 0) {
    error = "not a library archive (it does not start with the PSLIBAR mark)";
    return false;
  }
  ByteReader reader(view.substr(sizeof(Magic)));
  uint32_t formatVersion = 0;
  uint32_t sectionCount = 0;
  if (!reader.readU32(formatVersion) || !reader.readU32(sectionCount)) {
    return damaged(error, "the header is cut short");
  }
  if (formatVersion != LibraryArchiveFormatVersion) {
    error = "library archive format version " + std::to_string(formatVersion) +
            " is not the version this compiler reads (" +
            std::to_string(LibraryArchiveFormatVersion) + ")" + RebuildHint;
    return false;
  }
  struct Entry {
    std::string name;
    uint64_t size = 0;
    std::string hash;
  };
  std::vector<Entry> entries;
  for (uint32_t i = 0; i < sectionCount; ++i) {
    uint32_t nameSize = 0;
    std::string_view name;
    Entry entry;
    std::string_view hash;
    if (!reader.readU32(nameSize) || !reader.readBytes(nameSize, name) ||
        !reader.readU64(entry.size) || !reader.readBytes(HashHexSize, hash)) {
      return damaged(error, "the section table is cut short");
    }
    entry.name = std::string(name);
    entry.hash = std::string(hash);
    if (!entries.empty() && !(entries.back().name < entry.name)) {
      return damaged(error, "the sections are not in name order");
    }
    entries.push_back(std::move(entry));
  }
  std::vector<Section> sections;
  for (const Entry &entry : entries) {
    std::string_view payload;
    if (!reader.readBytes(entry.size, payload)) {
      return damaged(error, "section `" + entry.name + "` is cut short");
    }
    sections.push_back({entry.name, std::string(payload)});
  }
  if (!reader.atEnd()) {
    return damaged(error, "bytes follow the last section");
  }
  for (size_t i = 0; i < sections.size(); ++i) {
    if (sha256Hex(sections[i].bytes) != entries[i].hash) {
      error = "library section `" + sections[i].name +
              "` changed since the library was built (its hash differs)" + RebuildHint;
      return false;
    }
  }
  auto find = [&](const char *name) -> const Section * {
    for (const Section &section : sections) {
      if (section.name == name) {
        return &section;
      }
    }
    return nullptr;
  };
  for (const char *required : {"code", "interface", "manifest", "symbols"}) {
    if (find(required) == nullptr) {
      return damaged(error, std::string("section `") + required + "` is missing");
    }
  }

  if (!parseManifest(find("manifest")->bytes, out.manifest, error)) {
    return false;
  }
  if (out.manifest.compilerBuildId != expected.compilerBuildId) {
    error = "library was built by another compiler build (" +
            shortId(out.manifest.compilerBuildId) + ", this compiler is " +
            shortId(expected.compilerBuildId) + ")" + RebuildHint;
    return false;
  }
  if (out.manifest.irSchemaVersion != expected.irSchemaVersion) {
    error = "library was built with IR schema version " +
            std::to_string(out.manifest.irSchemaVersion) + ", this compiler uses version " +
            std::to_string(expected.irSchemaVersion) + RebuildHint;
    return false;
  }
  if (out.manifest.hashScheme != "sha256") {
    error = "library uses the hash scheme `" + out.manifest.hashScheme +
            "`, which this compiler does not know" + RebuildHint;
    return false;
  }
  out.interfaceText = find("interface")->bytes;
  out.symbolsText = find("symbols")->bytes;
  const std::string tablePrefix = "{\"interfaceHash\":\"" + out.manifest.interfaceHash + "\"";
  if (out.symbolsText.compare(0, tablePrefix.size(), tablePrefix) != 0) {
    error = "library manifest and symbol table name different interface hashes" +
            std::string(RebuildHint);
    return false;
  }
  const std::string &code = find("code")->bytes;
  const std::vector<uint8_t> codeBytes(code.begin(), code.end());
  std::string irError;
  if (!deserializeIr(codeBytes, out.code, irError)) {
    return damaged(error, "its code does not load: " + irError);
  }
  if (out.code.entryIndex != -1) {
    return damaged(error, "its code has an entry point");
  }
  if (const Section *sources = find("sources")) {
    out.hasSources = true;
    if (!parseSources(sources->bytes, out.sources)) {
      return damaged(error, "its sources section is cut short");
    }
  }
  return true;
}

bool writeLibraryArchiveFile(const std::string &path,
                             const LibraryArchiveContent &content,
                             std::string &error) {
  std::vector<uint8_t> bytes;
  if (!serializeLibraryArchive(content, bytes, error)) {
    return false;
  }
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) {
    error = "failed to write output: " + path;
    return false;
  }
  file.write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  file.close();
  if (!file) {
    error = "failed to write output: " + path;
    return false;
  }
  return true;
}

bool readLibraryArchiveFile(const std::string &path,
                            const LibraryArchiveExpectations &expected,
                            LibraryArchiveContent &out,
                            std::string &error) {
  std::vector<uint8_t> bytes;
  if (!fileBytes(path, bytes)) {
    error = "cannot read library archive: " + path;
    return false;
  }
  return deserializeLibraryArchive(bytes, expected, out, error);
}

} // namespace primec
