#ifndef LIMITLESS_ELF_H
#define LIMITLESS_ELF_H

/*
 * ELF (Executable and Linkable Format) Loader
 * Support for ELF64 binaries on x86_64
 */

#include "kernel.h"

/* ELF identification */
#define ELF_MAGIC 0x464C457F  // "\x7FELF"

/* ELF class */
#define ELFCLASS32 1
#define ELFCLASS64 2

/* ELF data encoding */
#define ELFDATA2LSB 1  // Little endian
#define ELFDATA2MSB 2  // Big endian

/* ELF version */
#define EV_CURRENT 1

/* ELF machine types */
#define EM_NONE     0
#define EM_386      3   // Intel x86
#define EM_X86_64   62  // AMD x86-64

/* ELF file types */
#define ET_NONE   0  // No file type
#define ET_REL    1  // Relocatable file
#define ET_EXEC   2  // Executable file
#define ET_DYN    3  // Shared object file
#define ET_CORE   4  // Core file

/* Program header types */
#define PT_NULL     0  // Unused entry
#define PT_LOAD     1  // Loadable segment
#define PT_DYNAMIC  2  // Dynamic linking information
#define PT_INTERP   3  // Interpreter path
#define PT_NOTE     4  // Auxiliary information
#define PT_SHLIB    5  // Reserved
#define PT_PHDR     6  // Program header table
#define PT_TLS      7  // Thread-local storage

/* Program header flags */
#define PF_X 0x1  // Execute
#define PF_W 0x2  // Write
#define PF_R 0x4  // Read

/* Section header types */
#define SHT_NULL     0   // Unused section
#define SHT_PROGBITS 1   // Program data
#define SHT_SYMTAB   2   // Symbol table
#define SHT_STRTAB   3   // String table
#define SHT_RELA     4   // Relocation entries with addends
#define SHT_HASH     5   // Symbol hash table
#define SHT_DYNAMIC  6   // Dynamic linking information
#define SHT_NOTE     7   // Notes
#define SHT_NOBITS   8   // Program space with no data (bss)
#define SHT_REL      9   // Relocation entries, no addends
#define SHT_DYNSYM   11  // Dynamic linker symbol table

/* Section header flags */
#define SHF_WRITE     0x1  // Writable
#define SHF_ALLOC     0x2  // Occupies memory during execution
#define SHF_EXECINSTR 0x4  // Executable

/* ELF64 types */
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

/* ELF64 header */
typedef struct {
    unsigned char e_ident[16];  // Magic number and other info
    Elf64_Half    e_type;       // Object file type
    Elf64_Half    e_machine;    // Architecture
    Elf64_Word    e_version;    // Object file version
    Elf64_Addr    e_entry;      // Entry point virtual address
    Elf64_Off     e_phoff;      // Program header table file offset
    Elf64_Off     e_shoff;      // Section header table file offset
    Elf64_Word    e_flags;      // Processor-specific flags
    Elf64_Half    e_ehsize;     // ELF header size in bytes
    Elf64_Half    e_phentsize;  // Program header table entry size
    Elf64_Half    e_phnum;      // Program header table entry count
    Elf64_Half    e_shentsize;  // Section header table entry size
    Elf64_Half    e_shnum;      // Section header table entry count
    Elf64_Half    e_shstrndx;   // Section header string table index
} PACKED Elf64_Ehdr;

/* ELF64 program header */
typedef struct {
    Elf64_Word  p_type;    // Segment type
    Elf64_Word  p_flags;   // Segment flags
    Elf64_Off   p_offset;  // Segment file offset
    Elf64_Addr  p_vaddr;   // Segment virtual address
    Elf64_Addr  p_paddr;   // Segment physical address
    Elf64_Xword p_filesz;  // Segment size in file
    Elf64_Xword p_memsz;   // Segment size in memory
    Elf64_Xword p_align;   // Segment alignment
} PACKED Elf64_Phdr;

/* ELF64 section header */
typedef struct {
    Elf64_Word  sh_name;      // Section name (string tbl index)
    Elf64_Word  sh_type;      // Section type
    Elf64_Xword sh_flags;     // Section flags
    Elf64_Addr  sh_addr;      // Section virtual addr at execution
    Elf64_Off   sh_offset;    // Section file offset
    Elf64_Xword sh_size;      // Section size in bytes
    Elf64_Word  sh_link;      // Link to another section
    Elf64_Word  sh_info;      // Additional section information
    Elf64_Xword sh_addralign; // Section alignment
    Elf64_Xword sh_entsize;   // Entry size if section holds table
} PACKED Elf64_Shdr;

/* ELF64 symbol table entry */
typedef struct {
    Elf64_Word    st_name;  // Symbol name (string tbl index)
    unsigned char st_info;  // Symbol type and binding
    unsigned char st_other; // Symbol visibility
    Elf64_Half    st_shndx; // Section index
    Elf64_Addr    st_value; // Symbol value
    Elf64_Xword   st_size;  // Symbol size
} PACKED Elf64_Sym;

/* ELF64 relocation */
typedef struct {
    Elf64_Addr  r_offset; // Address
    Elf64_Xword r_info;   // Relocation type and symbol index
} PACKED Elf64_Rel;

typedef struct {
    Elf64_Addr   r_offset; // Address
    Elf64_Xword  r_info;   // Relocation type and symbol index
    Elf64_Sxword r_addend; // Addend
} PACKED Elf64_Rela;

/* Dynamic section entry */
typedef struct {
    Elf64_Sxword d_tag;  // Dynamic entry type
    union {
        Elf64_Xword d_val; // Integer value
        Elf64_Addr  d_ptr; // Address value
    } d_un;
} PACKED Elf64_Dyn;

/* Dynamic table tags */
#define DT_NULL     0  // Marks end of dynamic section
#define DT_NEEDED   1  // Name of needed library
#define DT_HASH     4  // Address of symbol hash table
#define DT_STRTAB   5  // Address of string table
#define DT_SYMTAB   6  // Address of symbol table
#define DT_RELA     7  // Address of Rela relocs
#define DT_RELASZ   8  // Total size of Rela relocs
#define DT_RELAENT  9  // Size of one Rela reloc
#define DT_STRSZ    10 // Size of string table
#define DT_SYMENT   11 // Size of one symbol table entry
#define DT_INIT     12 // Address of init function
#define DT_FINI     13 // Address of termination function

/* Relocation macros */
#define ELF64_R_SYM(i)    ((i) >> 32)
#define ELF64_R_TYPE(i)   ((i) & 0xffffffffL)
#define ELF64_R_INFO(s,t) (((s) << 32) + ((t) & 0xffffffffL))

/* x86_64 relocation types */
#define R_X86_64_NONE       0  // No relocation
#define R_X86_64_64         1  // Direct 64 bit
#define R_X86_64_PC32       2  // PC relative 32 bit signed
#define R_X86_64_GOT32      3  // 32 bit GOT entry
#define R_X86_64_PLT32      4  // 32 bit PLT address
#define R_X86_64_COPY       5  // Copy symbol at runtime
#define R_X86_64_GLOB_DAT   6  // Create GOT entry
#define R_X86_64_JUMP_SLOT  7  // Create PLT entry
#define R_X86_64_RELATIVE   8  // Adjust by program base

/* ELF loading context */
typedef struct elf_context {
    /* File data */
    uint8_t* data;
    size_t size;

    /* Headers */
    Elf64_Ehdr* ehdr;
    Elf64_Phdr* phdr;
    Elf64_Shdr* shdr;

    /* Loaded segments */
    vaddr_t base_addr;
    vaddr_t entry_point;
    size_t total_size;

    /* Dynamic linking */
    bool is_dynamic;
    char interpreter[256];
    Elf64_Dyn* dynamic;
    size_t dynamic_count;

    /* Memory mapping */
    struct address_space* aspace;
} elf_context_t;

/* ELF loader API */
status_t elf_load(const uint8_t* data, size_t size, struct address_space* aspace, elf_context_t** out_ctx);
status_t elf_validate(const uint8_t* data, size_t size);
status_t elf_parse_headers(elf_context_t* ctx);
status_t elf_load_segments(elf_context_t* ctx);
status_t elf_apply_relocations(elf_context_t* ctx);
void elf_free_context(elf_context_t* ctx);

/* Utility functions */
const char* elf_get_section_name(elf_context_t* ctx, Elf64_Shdr* shdr);
Elf64_Shdr* elf_find_section(elf_context_t* ctx, const char* name);
status_t elf_read_string(elf_context_t* ctx, Elf64_Off offset, char* buffer, size_t max_len);

#endif /* LIMITLESS_ELF_H */
