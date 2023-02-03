# Print Name List
Symbol dumper for the TRASM toolchain. Prints the name list of an object file

## Usage
```
nm [-prgvh] object.o
```
| Option  | Description |
| ------- | ----------- |
| -p      | Do not sort, symbols will print in the order they are defined in |
| -r      | Reverse sort order |
| -g      | Show only external symbols |
| -v      | Sort by value instead of symbol name |
| -h      | Supress printing of object header information |

## Description
Prints the name list of an object file. Be default, these symbols will be sorted by alphabetical order. This can be changed using argument switches. A header displaying object base, entry offset, and size will also be printed unless supressed.
