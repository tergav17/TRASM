# TRASM
TRASM (Type-based Relocatable ASseMbler), as the number suggests, is an assembler for the Z80 processor. Additionally, utilities for manipulating object files created by the assembler are also included. The assembler syntax and related tool functions are based off of the toolchain found in Version 6 UNIX. By design, all parts of the toolchain are fairly lightweight. Certain features found in more modern toolchains are omitted. This makes it possible for the entire toolchain to be ported to run natively on the Z80 processor. 

Tools include:
- `as`: Assembler, core of the TRASM toolchain. It is capable of assembling all documented and undocumented Z80 instructions. Unlike most Z80 assemblers, the TRASM assembler does not allow an origin to be defined, and all addresses should be assumed to be ambiguous. Object data is divided up into text, data, and bss segments. No macros are implemented, but custom types can be defined which act as primitive structs. 
- `ld`: Link Editor, combines object files together. Text, data, and bss from all objects are relocated and spliced together. External symbols are resolved, and symbol tables are merged.
- `reloc`: Relocation Tool, moves object files around in memory. By default, all objects files have their origin set at 0x0000. This utility has the capacity to change that. Additionally, text/data or bss can be statically relocated to independent addresses. A final post-processing function exists which removes the header from the object file, creating a fully static binary.
- `nm`: Print name table, dumps all symbols in an object file. Symbols can be sorted in a number of different ways.
- `size`: Prints the size of the text, data, and bss segments
- `strip`: Removes the symbol table, can also be done by `ld` or `reloc`
