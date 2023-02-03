# Link Editor
Linker for the TRASM toolchain. Takes in multiple object files, resolves external references, and outputs a single executable.

## Usage
```
ld [-vs] [-r] object.o ...
```
| Option | Description |
| ------ | ----------- |
| -v     | Verbose output, will display version information, and information regarding object relocation |
| -s     | Squash output, no symbol table will be emitted |
| -r     | Pass unresolved externals into the final object final, allowing another round of linking. Incompatible with `-s` |

## Description
When linking, the entry object file will be the first one passed in the arguments. The link editor will process object files on their own, or ones from a `.a` archive file. All independent object files will automatically be checked into the linker, but archived object files must be referenced first before they will be checked in. This makes linking with a large archive files slower, as the symbol records will need to be checked recursively until all external references have been resolved. Order is not critical after the first object argument, but ordering objects so that external references come after global symbol declaration can speed things up.

The output object file will always start at the base address of `0x0000`. Source object files can have any base, and will be automatically relocated as needed.
