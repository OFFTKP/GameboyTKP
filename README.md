# GameboyTKP
Gameboy emulator written in C++ for [TKPEmu](https://github.com/OFFTKP/TKPEmu).

This emulator can be integrated into other projects. Check `example_main.cpp` for an example.

## Features with TKPEmu
 - Customizable palette
 - Customizable controls
 - Disassembler/Debugger
 - Advanced breakpoints
 - Tracelogger
 - Save states 
 - Rewind functionality (WIP)

## Images
![Legend of Zelda color](./Images/zd_clr.bmp)
![Yugioh](./Images/yugi.bmp)
![GTA](./Images/gta.bmp)
![Pokemon Gold](./Images/gold.bmp)
<br/>
![Legend of Zelda](./Images/zd.bmp)
![Kirby](./Images/krb.bmp)
![Donkey Kong](./Images/dk.bmp)
![Pokemon Red](./Images/red.bmp)

## Resources
This emulator would not be possible without the use of these resources, as acquiring a DMG Gameboy     
becomes harder and harder by the day.

**[PanDocs](https://gbdev.io/pandocs/):**    
Very detailed document explaining tons of gameboy behavior. The best document to get started.

**[TCAGBD by AntonioND](https://github.com/AntonioND/giibiiadvance/blob/master/docs/TCAGBD.pdf):**    
Great and very detailed timer explanation.    

**[Blargg tests](https://github.com/retrio/gb-test-roms):**    
Very good testing suites. cpu_instrs is a great start for emulator development.    

**[Mooneye tests](https://github.com/Gekkio/mooneye-test-suite/):**    
Also very good testing suites. Tests some advanced mid-instruction timing, and has      
advanced timer/ppu behavior tests. Also very useful mbc tests.    

**[mattcurie's dmg_acid2 test](https://github.com/mattcurrie/dmg-acid2):**    
Best test to get started on correct ppu output

## Tests

See generated test results [here](./TEST_RESULTS.md).    
Always generate new test results before pushing a commit when changing the code.    
You can generate all tests with `./generate_test_results.sh` script on linux.

**Graphics tests:**
| Test            | GameboyTKP                                              |
| --------------- | ------------------------------------------------------- |
| dmg-acid2       | ![dmg-acid2](./Images/dmg-acid2_result.bmp)             |
| cgb-acid2       | ![cgb-acid2](./Images/acid.bmp)                         |
| sprite_priority | ![sprite_priority](./Images/sprite_priority_result.bmp) |

## License
TKPEmu is licensed under the MIT license    
Copyright (C) 2021-2022 Paris Oplopoios
