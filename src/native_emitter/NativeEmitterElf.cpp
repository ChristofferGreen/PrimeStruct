#include "NativeEmitterInternals.h" // alignTo, PageSize (arch-agnostic)
#include "NativeEmitterInternalsX64.h"

#include <cstring>

#if defined(__linux__)
#include <elf.h>
#endif

namespace primec::native_emitter {

#if defined(__linux__)

uint32_t computeElfCodeOffset() {
  return static_cast<uint32_t>(alignTo(sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr), 16));
}

bool buildElf(const std::vector<uint8_t> &code, std::vector<uint8_t> &image, std::string &error) {
  if (code.empty()) {
    error = "native backend requires non-empty code";
    return false;
  }

  const uint32_t codeOffset = computeElfCodeOffset();
  const uint64_t fileSize = static_cast<uint64_t>(codeOffset) + code.size();

  image.assign(static_cast<size_t>(fileSize), 0);

  Elf64_Ehdr header{};
  header.e_ident[EI_MAG0] = ELFMAG0;
  header.e_ident[EI_MAG1] = ELFMAG1;
  header.e_ident[EI_MAG2] = ELFMAG2;
  header.e_ident[EI_MAG3] = ELFMAG3;
  header.e_ident[EI_CLASS] = ELFCLASS64;
  header.e_ident[EI_DATA] = ELFDATA2LSB;
  header.e_ident[EI_VERSION] = EV_CURRENT;
  header.e_ident[EI_OSABI] = ELFOSABI_SYSV;
  header.e_ident[EI_ABIVERSION] = 0;
  header.e_type = ET_EXEC;
  header.e_machine = EM_X86_64;
  header.e_version = EV_CURRENT;
  header.e_entry = ElfLoadAddress + codeOffset;
  header.e_phoff = sizeof(Elf64_Ehdr);
  header.e_shoff = 0;
  header.e_flags = 0;
  header.e_ehsize = sizeof(Elf64_Ehdr);
  header.e_phentsize = sizeof(Elf64_Phdr);
  header.e_phnum = 1;
  header.e_shentsize = 0;
  header.e_shnum = 0;
  header.e_shstrndx = 0;

  Elf64_Phdr load{};
  load.p_type = PT_LOAD;
  load.p_flags = PF_R | PF_X;
  load.p_offset = 0;
  load.p_vaddr = ElfLoadAddress;
  load.p_paddr = ElfLoadAddress;
  load.p_filesz = fileSize;
  load.p_memsz = fileSize;
  load.p_align = PageSize;

  std::memcpy(image.data(), &header, sizeof(header));
  std::memcpy(image.data() + sizeof(header), &load, sizeof(load));
  std::memcpy(image.data() + codeOffset, code.data(), code.size());
  return true;
}

#endif

} // namespace primec::native_emitter
