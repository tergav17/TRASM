# TRASM
TRASM (Type-based Relocatable ASseMbler), as the number suggests, is an assembler for the Z80 processor. Additionally, utilities for manipulating object files created by the assembler are also included. The assembler syntax and related tool functions are based off of the toolchain found in Version 6 UNIX. By design, all parts of the toolchain are fairly lightweight. Certain features found in more modern toolchains are omitted. This makes it possible for the entire toolchain to be ported to run natively on the Z80 processor. 

Tools include:
- `as`: Assembler, core of the TRASM toolchain. It is capable of assembling all documented and undocumented Z80 instructions. Unlike most Z80 assemblers, the TRASM assembler does not allow an origin to be defined, and all addresses should be assumed to be ambiguous. Object data is divided up into text, data, and bss segments. No macros are implemented, but custom types can be defined which act as primitive structs. 
- `ld`: Link Editor, combines object files together. Text, data, and bss from all objects are relocated and spliced together. External symbols are resolved, and symbol tables are merged.
- `reloc`: Relocation Tool, moves object files around in memory. By default, all objects files have their origin set at 0x0000. This utility has the capacity to change that. Additionally, text/data or bss can be statically relocated to independent addresses. A final post-processing function exists which removes the header from the object file, creating a fully static binary.
- `nm`: Print name table, dumps all symbols in an object file. Symbols can be sorted in a number of different ways.
- `size`: Prints the size of the text, data, and bss segments.
- `strip`: Removes the symbol table, can also be done by `ld` or `reloc`.

# Object Files
All toolchain utilities generate or utilize object files in some way. TRASM object files are loosely based off of the original `a.out` format. A few changes have been made in respect to what goals this toolset aims to achieve. An object file is separated into 3 different segments:

- Binary Segment: Includes header and all the text/data.
- Relocation Segment: Includes segment and external relocation information.
- Symbol Table: Contains symbols records for global and external symbols.

## Binary Segment
This segment is where all actual instruction and data information resides. When assembling, bytes emitted are sorted into either the text or data segments. The exception to this is the header, which occupies the first 16 bytes of the text segment. Despite being automatically generated, it is treated as part of the text segment. This header contains information critical for manipulating and executing object files. Fields are as follows:

| Field Name | Addresses Occupied | Description |
| ---------- | ------------------ | ----------- |
| H_MAGIG    | 0x0 - 0x1          | Magic number which identifies the file as an object file. Coincidentally, it is also a Z80 JR instruction which points to directly after the header |
| H_INFO     | 0x2                | Information byte, contains state information object the object |
| H_ORG      | 0x3 - 0x4          | Text origin. This is the base address that the object expects to be executed at |
| H_SYS      | 0x5 - 0x7          | Syscall thunk. This is used to quickly link a system entry vector to an executing binary |
| H_ENTRY    | 0x8 - 0x9          | Entry offset. Not currently used, but used to defined where the PC should start |
| H_TEXTT    | 0xA - 0xB          | Top of text segment in the binary. Points to the data segment base |
| H_DATAT    | 0xC - 0xD          | Top of data segment in the binary. Points to the relocation segment base |
| H_BSST     | 0xE - 0xF          | Top of bss segment. Can be read as the total amount of bytes that must be allocated |
