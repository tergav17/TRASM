# Relocation Tool
Relocation tool for the TRASM toolchain. Used for object file post-processing.

## Usage
```
reloc [-vsd] [-n] [-b base] object.o base
```
| Option  | Description |
| ------- | ----------- |
| -v      | Verbose output, will display version information |
| -s      | Squash output, no symbol table will be emitted |
| -b base | Statically links bss segment to another location |
| -d      | Statically links text and data segments to base |
| -n      | Removes object header, relocation data, and symbol table |

## Description
The relocation tool is used to configure a final binary for execution, or to handle special cases in the linking process. Due to the fact that `as` does not allow for an assembly origin to be specified, `reloc` must be used to move segments to their correct location in memory. Bases can be passed in decimal, hex, or octal formats in line with those found in the assembly.

By default, `reloc` will place the text segment at the defined base, and all other segments will be placed consecutively after. Object headers will be maintained, so linking is still possible, thought the linker will relocate this segment back to the base of `0x0000` during the linking phase. If a segment is statically linked, all relocation references will be removed, and any symbol in that segment will be set to absolute. The two flags that have this effect, `-b` and `-d`, behave slightly differently.

The flag `-b` only moves the bss segment. The size of the bss segment is set to 0 in the object header, and bss relocations are removed, and bss symbols will be set to absolute. This is useful if the final executable will be burned to a ROM, and the program needs to make use of modifiable memory locations.

The flag `-d` moves all text information into the data segment, removes relocations for both, and sets the relevant symbols to absolute. The size of the text segment will be added to the data segment, then set to 0. This is useful if a portion of the final binary needs to be copied into a static location in memory during execution.

Finally, the flag `-n` can be used to generate a raw binary. The flags `-d` and `-s` are incompatible, as all segments are ultimately wiped away. The header will be removed, and the first non-header byte will be placed at the base. All symbols and relocations are removed, `-b` can still be used to relocate the bss if desired.
