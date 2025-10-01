#ifndef LIMITLESS_MACHO_H
#define LIMITLESS_MACHO_H

/*
 * Mach-O (Mach Object) Loader
 * Supports macOS executables and dylibs (32-bit and 64-bit)
 */

#include "kernel.h"
#include "vmm.h"

/* Mach-O magic numbers */
#define MACHO_MAGIC_32      0xFEEDFACE  // 32-bit Mach-O
#define MACHO_MAGIC_64      0xFEEDFACF  // 64-bit Mach-O
#define MACHO_CIGAM_32      0xCEFAEDFE  // 32-bit reverse byte order
#define MACHO_CIGAM_64      0xCFFAEDFE  // 64-bit reverse byte order
#define MACHO_FAT_MAGIC     0xCAFEBABE  // Universal binary
#define MACHO_FAT_CIGAM     0xBEBAFECA  // Universal binary (reverse)

/* CPU types */
#define MACHO_CPU_TYPE_X86      7
#define MACHO_CPU_TYPE_X86_64   (MACHO_CPU_TYPE_X86 | 0x01000000)
#define MACHO_CPU_TYPE_ARM      12
#define MACHO_CPU_TYPE_ARM64    (MACHO_CPU_TYPE_ARM | 0x01000000)

/* CPU subtypes */
#define MACHO_CPU_SUBTYPE_X86_ALL    3
#define MACHO_CPU_SUBTYPE_X86_64_ALL 3
#define MACHO_CPU_SUBTYPE_ARM64_ALL  0

/* File types */
#define MACHO_TYPE_OBJECT     0x1  // Relocatable object file
#define MACHO_TYPE_EXECUTE    0x2  // Executable
#define MACHO_TYPE_FVMLIB     0x3  // Fixed VM shared library
#define MACHO_TYPE_CORE       0x4  // Core dump
#define MACHO_TYPE_PRELOAD    0x5  // Preloaded executable
#define MACHO_TYPE_DYLIB      0x6  // Dynamic library
#define MACHO_TYPE_DYLINKER   0x7  // Dynamic linker
#define MACHO_TYPE_BUNDLE     0x8  // Bundle
#define MACHO_TYPE_DSYM       0xA  // Debug symbols

/* Header flags */
#define MACHO_FLAG_NOUNDEFS            0x00000001  // No undefined refs
#define MACHO_FLAG_INCRLINK            0x00000002  // Incremental link
#define MACHO_FLAG_DYLDLINK            0x00000004  // Input for dyld
#define MACHO_FLAG_BINDATLOAD          0x00000008  // Bind at load
#define MACHO_FLAG_PREBOUND            0x00000010  // Prebound
#define MACHO_FLAG_SPLIT_SEGS          0x00000020  // Split segments
#define MACHO_FLAG_TWOLEVEL            0x00000080  // Two-level namespace
#define MACHO_FLAG_FORCE_FLAT          0x00000100  // Force flat namespace
#define MACHO_FLAG_NOMULTIDEFS         0x00000200  // No multiple defs
#define MACHO_FLAG_PIE                 0x00200000  // Position independent
#define MACHO_FLAG_NO_HEAP_EXECUTION   0x01000000  // Heap not executable

/* Mach-O Header (32-bit) */
typedef struct macho_header_32 {
    uint32_t magic;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
} PACKED macho_header_32_t;

/* Mach-O Header (64-bit) */
typedef struct macho_header_64 {
    uint32_t magic;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
    uint32_t reserved;
} PACKED macho_header_64_t;

/* Fat header (universal binary) */
typedef struct macho_fat_header {
    uint32_t magic;
    uint32_t nfat_arch;
} PACKED macho_fat_header_t;

/* Fat architecture */
typedef struct macho_fat_arch {
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t offset;
    uint32_t size;
    uint32_t align;
} PACKED macho_fat_arch_t;

/* Load command types */
#define LC_SEGMENT         0x01  // Segment of file to map
#define LC_SYMTAB          0x02  // Symbol table
#define LC_SYMSEG          0x03  // Symbol segment (obsolete)
#define LC_THREAD          0x04  // Thread
#define LC_UNIXTHREAD      0x05  // Unix thread (includes stack)
#define LC_LOADFVMLIB      0x06  // Load fixed VM library
#define LC_IDFVMLIB        0x07  // Fixed VM library ID
#define LC_IDENT           0x08  // Object identification (obsolete)
#define LC_FVMFILE         0x09  // Fixed VM file
#define LC_PREPAGE         0x0A  // Prepage command (obsolete)
#define LC_DYSYMTAB        0x0B  // Dynamic symbol table
#define LC_LOAD_DYLIB      0x0C  // Load dynamic library
#define LC_ID_DYLIB        0x0D  // Dynamic library ID
#define LC_LOAD_DYLINKER   0x0E  // Load dynamic linker
#define LC_ID_DYLINKER     0x0F  // Dynamic linker ID
#define LC_PREBOUND_DYLIB  0x10  // Prebound modules
#define LC_ROUTINES        0x11  // Image routines
#define LC_SUB_FRAMEWORK   0x12  // Sub framework
#define LC_SUB_UMBRELLA    0x13  // Sub umbrella
#define LC_SUB_CLIENT      0x14  // Sub client
#define LC_SUB_LIBRARY     0x15  // Sub library
#define LC_TWOLEVEL_HINTS  0x16  // Two-level namespace hints
#define LC_PREBIND_CKSUM   0x17  // Prebind checksum
#define LC_SEGMENT_64      0x19  // 64-bit segment
#define LC_ROUTINES_64     0x1A  // 64-bit image routines
#define LC_UUID            0x1B  // UUID
#define LC_RPATH           0x8000001C  // Runpath additions
#define LC_CODE_SIGNATURE  0x1D  // Code signature
#define LC_SEGMENT_SPLIT_INFO 0x1E  // Split segment info
#define LC_REEXPORT_DYLIB  0x8000001F  // Re-export dylib
#define LC_ENCRYPTION_INFO 0x21  // Encrypted segment
#define LC_DYLD_INFO       0x22  // Dyld info
#define LC_DYLD_INFO_ONLY  0x80000022  // Dyld info only
#define LC_VERSION_MIN_MACOSX   0x24  // Minimum macOS version
#define LC_VERSION_MIN_IPHONEOS 0x25  // Minimum iOS version
#define LC_FUNCTION_STARTS 0x26  // Function start addresses
#define LC_DYLD_ENVIRONMENT 0x27  // Dyld environment
#define LC_MAIN            0x80000028  // Entry point (replaces LC_UNIXTHREAD)
#define LC_DATA_IN_CODE    0x29  // Data in code
#define LC_SOURCE_VERSION  0x2A  // Source version
#define LC_DYLIB_CODE_SIGN_DRS 0x2B  // Code signing DRs
#define LC_ENCRYPTION_INFO_64 0x2C  // 64-bit encrypted segment
#define LC_BUILD_VERSION   0x32  // Build version

/* Load command */
typedef struct load_command {
    uint32_t cmd;
    uint32_t cmdsize;
} PACKED load_command_t;

/* Segment command (32-bit) */
typedef struct segment_command_32 {
    uint32_t cmd;
    uint32_t cmdsize;
    char segname[16];
    uint32_t vmaddr;
    uint32_t vmsize;
    uint32_t fileoff;
    uint32_t filesize;
    uint32_t maxprot;
    uint32_t initprot;
    uint32_t nsects;
    uint32_t flags;
} PACKED segment_command_32_t;

/* Segment command (64-bit) */
typedef struct segment_command_64 {
    uint32_t cmd;
    uint32_t cmdsize;
    char segname[16];
    uint64_t vmaddr;
    uint64_t vmsize;
    uint64_t fileoff;
    uint64_t filesize;
    uint32_t maxprot;
    uint32_t initprot;
    uint32_t nsects;
    uint32_t flags;
} PACKED segment_command_64_t;

/* Section (32-bit) */
typedef struct section_32 {
    char sectname[16];
    char segname[16];
    uint32_t addr;
    uint32_t size;
    uint32_t offset;
    uint32_t align;
    uint32_t reloff;
    uint32_t nreloc;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t reserved2;
} PACKED section_32_t;

/* Section (64-bit) */
typedef struct section_64 {
    char sectname[16];
    char segname[16];
    uint64_t addr;
    uint64_t size;
    uint32_t offset;
    uint32_t align;
    uint32_t reloff;
    uint32_t nreloc;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
} PACKED section_64_t;

/* VM protection flags */
#define VM_PROT_NONE    0x00
#define VM_PROT_READ    0x01
#define VM_PROT_WRITE   0x02
#define VM_PROT_EXECUTE 0x04

/* Dylib command */
typedef struct dylib_command {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t name_offset;  // Offset to library name
    uint32_t timestamp;
    uint32_t current_version;
    uint32_t compatibility_version;
} PACKED dylib_command_t;

/* Dylinker command */
typedef struct dylinker_command {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t name_offset;  // Offset to linker name
} PACKED dylinker_command_t;

/* Entry point command */
typedef struct entry_point_command {
    uint32_t cmd;
    uint32_t cmdsize;
    uint64_t entryoff;  // File offset of entry point
    uint64_t stacksize;
} PACKED entry_point_command_t;

/* UUID command */
typedef struct uuid_command {
    uint32_t cmd;
    uint32_t cmdsize;
    uint8_t uuid[16];
} PACKED uuid_command_t;

/* Symbol table command */
typedef struct symtab_command {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t symoff;
    uint32_t nsyms;
    uint32_t stroff;
    uint32_t strsize;
} PACKED symtab_command_t;

/* Dynamic symbol table command */
typedef struct dysymtab_command {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t ilocalsym;
    uint32_t nlocalsym;
    uint32_t iextdefsym;
    uint32_t nextdefsym;
    uint32_t iundefsym;
    uint32_t nundefsym;
    uint32_t tocoff;
    uint32_t ntoc;
    uint32_t modtaboff;
    uint32_t nmodtab;
    uint32_t extrefsymoff;
    uint32_t nextrefsyms;
    uint32_t indirectsymoff;
    uint32_t nindirectsyms;
    uint32_t extreloff;
    uint32_t nextrel;
    uint32_t locreloff;
    uint32_t nlocrel;
} PACKED dysymtab_command_t;

/* Mach-O Context (loader state) */
typedef struct macho_context {
    /* File data */
    const uint8_t* data;
    size_t size;

    /* Headers */
    union {
        macho_header_32_t* header32;
        macho_header_64_t* header64;
    };

    /* Loaded segments */
    uintptr_t base_address;
    size_t total_size;
    uintptr_t entry_point;

    /* Load commands */
    uint8_t* load_commands;
    uint32_t num_commands;

    /* Metadata */
    bool is_64bit;
    bool is_dylib;
    bool swap_bytes;  // Need byte swapping
    uint32_t file_type;

    /* Address space */
    struct address_space* aspace;
} macho_context_t;

/* Mach-O Loader API */
status_t macho_init(void);
status_t macho_validate(const uint8_t* data, size_t size);
status_t macho_load(const uint8_t* data, size_t size, struct address_space* aspace, macho_context_t** out_ctx);
status_t macho_load_segments(macho_context_t* ctx);
status_t macho_resolve_symbols(macho_context_t* ctx);
void macho_free_context(macho_context_t* ctx);

/* Helper functions */
load_command_t* macho_get_load_command(macho_context_t* ctx, uint32_t cmd_type);
const char* macho_get_dylib_name(macho_context_t* ctx, dylib_command_t* dylib_cmd);
const char* macho_get_dylinker_name(macho_context_t* ctx, dylinker_command_t* dylinker_cmd);

/* Byte swapping helpers */
static inline uint32_t macho_swap32(uint32_t val) {
    return ((val & 0xFF000000) >> 24) |
           ((val & 0x00FF0000) >> 8) |
           ((val & 0x0000FF00) << 8) |
           ((val & 0x000000FF) << 24);
}

static inline uint64_t macho_swap64(uint64_t val) {
    return ((val & 0xFF00000000000000ULL) >> 56) |
           ((val & 0x00FF000000000000ULL) >> 40) |
           ((val & 0x0000FF0000000000ULL) >> 24) |
           ((val & 0x000000FF00000000ULL) >> 8) |
           ((val & 0x00000000FF000000ULL) << 8) |
           ((val & 0x0000000000FF0000ULL) << 24) |
           ((val & 0x000000000000FF00ULL) << 40) |
           ((val & 0x00000000000000FFULL) << 56);
}

/* Statistics */
typedef struct macho_stats {
    uint64_t macho_loaded;
    uint64_t macho32_loaded;
    uint64_t macho64_loaded;
    uint64_t dylibs_loaded;
    uint64_t segments_loaded;
    uint64_t symbols_resolved;
} macho_stats_t;

status_t macho_get_stats(macho_stats_t* out_stats);

#endif /* LIMITLESS_MACHO_H */
