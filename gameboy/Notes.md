# Possible Code Refactoring

- Further breaking down and abstracting out parts of the emulator so it's not
  just one big humongous class

- Redundant number of comparisons made in conditional ranges

This chunk of code below can be modified to be more efficient by removing
redundant range comparisons

```cpp
if (address <= 0x1FFF) {
    ...
} else if (address >= 0x2000 && address <= 0x3FFF) {
    ...
}
```

To this:

```cpp
if (address <= 0x1FFF) {
    ...
} else if (address <= 0x3FFF) {
    ...
}
```
