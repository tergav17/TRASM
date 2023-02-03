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
| -h      | Suppress printing of object header information |

## Description
Prints the name list of an object file. By default, these symbols will be sorted by alphabetical order. This can be changed using argument switches. A header displaying object base, entry offset, and size will also be printed unless suppressed. A symbol record consists of its value, type, and then name. The type codes are as follows:

| Code | Name |
| ---- | ---- |
| u    | Undefined |
| t    | Text |
| d    | Data |
| b    | Bss |
| a    | Absolute |
| x    | External |
