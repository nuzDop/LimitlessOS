#ifndef LIMITLESS_PE_H
#define LIMITLESS_PE_H

/*
 * PE (Portable Executable) Loader
 * Supports Windows executables (PE32, PE32+)
 */

#include "kernel.h"
#include "vmm.h"

/* DOS Header */
#define PE_DOS_MAGIC 0x5A4D  // "MZ"

typedef struct pe_dos_header {
    uint16_t e_magic;      // Magic number
    uint16_t e_cblp;       // Bytes on last page
    uint16_t e_cp;         // Pages in file
    uint16_t e_crlc;       // Relocations
    uint16_t e_cparhdr;    // Size of header in paragraphs
    uint16_t e_minalloc;   // Minimum extra paragraphs
    uint16_t e_maxalloc;   // Maximum extra paragraphs
    uint16_t e_ss;         // Initial SS value
    uint16_t e_sp;         // Initial SP value
    uint16_t e_csum;       // Checksum
    uint16_t e_ip;         // Initial IP value
    uint16_t e_cs;         // Initial CS value
    uint16_t e_lfarlc;     // File address of relocation table
    uint16_t e_ovno;       // Overlay number
    uint16_t e_res[4];     // Reserved
    uint16_t e_oemid;      // OEM identifier
    uint16_t e_oeminfo;    // OEM information
    uint16_t e_res2[10];   // Reserved
    int32_t e_lfanew;      // File address of PE header
} PACKED pe_dos_header_t;

/* PE Signature */
#define PE_NT_MAGIC 0x00004550  // "PE\0\0"

/* Machine types */
#define PE_MACHINE_I386  0x014C  // Intel 386
#define PE_MACHINE_AMD64 0x8664  // x64

/* Characteristics */
#define PE_CHAR_EXECUTABLE        BIT(1)
#define PE_CHAR_LARGE_ADDRESS     BIT(5)
#define PE_CHAR_32BIT_MACHINE     BIT(8)
#define PE_CHAR_SYSTEM            BIT(12)
#define PE_CHAR_DLL               BIT(13)

/* COFF File Header */
typedef struct pe_coff_header {
    uint16_t machine;
    uint16_t num_sections;
    uint32_t timestamp;
    uint32_t symbol_table_offset;
    uint32_t num_symbols;
    uint16_t optional_header_size;
    uint16_t characteristics;
} PACKED pe_coff_header_t;

/* Optional Header Magic */
#define PE_OPTIONAL_MAGIC_PE32     0x010B
#define PE_OPTIONAL_MAGIC_PE32_PLUS 0x020B

/* Subsystems */
#define PE_SUBSYSTEM_NATIVE           1
#define PE_SUBSYSTEM_WINDOWS_GUI      2
#define PE_SUBSYSTEM_WINDOWS_CUI      3
#define PE_SUBSYSTEM_POSIX_CUI        7
#define PE_SUBSYSTEM_WINDOWS_CE_GUI   9
#define PE_SUBSYSTEM_EFI_APPLICATION  10

/* DLL Characteristics */
#define PE_DLL_CHAR_DYNAMIC_BASE      BIT(6)
#define PE_DLL_CHAR_NX_COMPAT         BIT(8)
#define PE_DLL_CHAR_NO_SEH            BIT(10)
#define PE_DLL_CHAR_TERMINAL_SERVER   BIT(15)

/* Data Directory indices */
#define PE_DIR_EXPORT          0
#define PE_DIR_IMPORT          1
#define PE_DIR_RESOURCE        2
#define PE_DIR_EXCEPTION       3
#define PE_DIR_SECURITY        4
#define PE_DIR_BASERELOC       5
#define PE_DIR_DEBUG           6
#define PE_DIR_ARCHITECTURE    7
#define PE_DIR_GLOBALPTR       8
#define PE_DIR_TLS             9
#define PE_DIR_LOAD_CONFIG    10
#define PE_DIR_BOUND_IMPORT   11
#define PE_DIR_IAT            12
#define PE_DIR_DELAY_IMPORT   13
#define PE_DIR_CLR_RUNTIME    14
#define PE_NUM_DIRECTORIES    16

/* Data Directory */
typedef struct pe_data_directory {
    uint32_t virtual_address;
    uint32_t size;
} PACKED pe_data_directory_t;

/* Optional Header (PE32+) */
typedef struct pe_optional_header64 {
    uint16_t magic;
    uint8_t major_linker_version;
    uint8_t minor_linker_version;
    uint32_t code_size;
    uint32_t initialized_data_size;
    uint32_t uninitialized_data_size;
    uint32_t entry_point;
    uint32_t code_base;
    uint64_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t major_os_version;
    uint16_t minor_os_version;
    uint16_t major_image_version;
    uint16_t minor_image_version;
    uint16_t major_subsystem_version;
    uint16_t minor_subsystem_version;
    uint32_t win32_version;
    uint32_t image_size;
    uint32_t headers_size;
    uint32_t checksum;
    uint16_t subsystem;
    uint16_t dll_characteristics;
    uint64_t stack_reserve;
    uint64_t stack_commit;
    uint64_t heap_reserve;
    uint64_t heap_commit;
    uint32_t loader_flags;
    uint32_t num_data_directories;
    pe_data_directory_t data_directories[PE_NUM_DIRECTORIES];
} PACKED pe_optional_header64_t;

/* Optional Header (PE32) */
typedef struct pe_optional_header32 {
    uint16_t magic;
    uint8_t major_linker_version;
    uint8_t minor_linker_version;
    uint32_t code_size;
    uint32_t initialized_data_size;
    uint32_t uninitialized_data_size;
    uint32_t entry_point;
    uint32_t code_base;
    uint32_t data_base;
    uint32_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t major_os_version;
    uint16_t minor_os_version;
    uint16_t major_image_version;
    uint16_t minor_image_version;
    uint16_t major_subsystem_version;
    uint16_t minor_subsystem_version;
    uint32_t win32_version;
    uint32_t image_size;
    uint32_t headers_size;
    uint32_t checksum;
    uint16_t subsystem;
    uint16_t dll_characteristics;
    uint32_t stack_reserve;
    uint32_t stack_commit;
    uint32_t heap_reserve;
    uint32_t heap_commit;
    uint32_t loader_flags;
    uint32_t num_data_directories;
    pe_data_directory_t data_directories[PE_NUM_DIRECTORIES];
} PACKED pe_optional_header32_t;

/* Section Header */
#define PE_SECTION_NAME_SIZE 8

#define PE_SCN_CNT_CODE               BIT(5)
#define PE_SCN_CNT_INITIALIZED_DATA   BIT(6)
#define PE_SCN_CNT_UNINITIALIZED_DATA BIT(7)
#define PE_SCN_MEM_EXECUTE            BIT(29)
#define PE_SCN_MEM_READ               BIT(30)
#define PE_SCN_MEM_WRITE              BIT(31)

typedef struct pe_section_header {
    char name[PE_SECTION_NAME_SIZE];
    uint32_t virtual_size;
    uint32_t virtual_address;
    uint32_t raw_data_size;
    uint32_t raw_data_offset;
    uint32_t relocations_offset;
    uint32_t line_numbers_offset;
    uint16_t num_relocations;
    uint16_t num_line_numbers;
    uint32_t characteristics;
} PACKED pe_section_header_t;

/* Import Directory Entry */
typedef struct pe_import_descriptor {
    uint32_t import_lookup_table_rva;
    uint32_t timestamp;
    uint32_t forwarder_chain;
    uint32_t name_rva;
    uint32_t import_address_table_rva;
} PACKED pe_import_descriptor_t;

/* Export Directory */
typedef struct pe_export_directory {
    uint32_t characteristics;
    uint32_t timestamp;
    uint16_t major_version;
    uint16_t minor_version;
    uint32_t name_rva;
    uint32_t ordinal_base;
    uint32_t num_functions;
    uint32_t num_names;
    uint32_t functions_rva;
    uint32_t names_rva;
    uint32_t name_ordinals_rva;
} PACKED pe_export_directory_t;

/* Base Relocation Block */
typedef struct pe_base_relocation {
    uint32_t virtual_address;
    uint32_t size;
} PACKED pe_base_relocation_t;

/* Relocation types */
#define PE_REL_BASED_ABSOLUTE  0
#define PE_REL_BASED_HIGH      1
#define PE_REL_BASED_LOW       2
#define PE_REL_BASED_HIGHLOW   3
#define PE_REL_BASED_HIGHADJ   4
#define PE_REL_BASED_DIR64     10

/* PE Context (loader state) */
typedef struct pe_context {
    /* File data */
    const uint8_t* data;
    size_t size;

    /* Headers */
    pe_dos_header_t* dos_header;
    pe_coff_header_t* coff_header;
    union {
        pe_optional_header32_t* opt32;
        pe_optional_header64_t* opt64;
    } optional_header;
    pe_section_header_t* sections;

    /* Loaded image */
    uintptr_t image_base;
    size_t image_size;
    uintptr_t entry_point;

    /* Metadata */
    bool is_pe32_plus;
    bool is_dll;
    uint16_t subsystem;

    /* Address space */
    struct address_space* aspace;
} pe_context_t;

/* PE Loader API */
status_t pe_init(void);
status_t pe_validate(const uint8_t* data, size_t size);
status_t pe_load(const uint8_t* data, size_t size, struct address_space* aspace, pe_context_t** out_ctx);
status_t pe_load_sections(pe_context_t* ctx);
status_t pe_apply_relocations(pe_context_t* ctx, uintptr_t new_base);
status_t pe_resolve_imports(pe_context_t* ctx);
void pe_free_context(pe_context_t* ctx);

/* Helper functions */
status_t pe_section_by_rva(pe_context_t* ctx, uint32_t rva, pe_section_header_t** out_section);
uintptr_t pe_rva_to_va(pe_context_t* ctx, uint32_t rva);
const void* pe_rva_to_file_offset(pe_context_t* ctx, uint32_t rva);

/* Statistics */
typedef struct pe_stats {
    uint64_t pe_loaded;
    uint64_t pe32_loaded;
    uint64_t pe32_plus_loaded;
    uint64_t dlls_loaded;
    uint64_t relocations_applied;
    uint64_t imports_resolved;
} pe_stats_t;

status_t pe_get_stats(pe_stats_t* out_stats);

#endif /* LIMITLESS_PE_H */
