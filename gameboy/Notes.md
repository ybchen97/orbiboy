# Possible Code Refactoring & Bug Locations

## Bug locations

#### Discrepancies to note:
- Missing MBC2 section in writeMem at address A000-A1FF
- MBC2 2000-3FFF ROM select value off?

#### Others
- Not completely sure on the exact workings of sprite priority in renderSprites(). Need to do more research
- Usage of bitwise not (~) on binary numbers. Not sure if cpp can differentiate context and difference between two's complement & unsigned integer. Appears in setLCDStatus() under the graphics section

## Code refactoring
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
# Useful links

Debugging possibly helpful
https://www.reddit.com/r/EmuDev/comments/a1pt87/need_help_with_gameboy_emulator_lcd_controller/

https://github.com/Gekkio/mooneye-gb/blob/master/docs/accuracy.markdown

getJoypadState() using my own understanding, when debugging check again

POP_rr might have scoping issues when checking if the reg is AF
    Use pointer address?? 

Check INC scoping &.

SBC ABC half carry flag not sure if correct. ABC should be correct, but SBC not confident. 
Gameboy manual should be wrong on the instructions based on Gamelad and a few other sources.

DAA chotto seh
