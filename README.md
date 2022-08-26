# Space Invaders Emulator

![Space Invaders Gameplay](gameplay.gif)

My space invaders emulator, written in C++ and using SDL2 for display and control. It includes a complete emulator of the Intel 8080 CPU, which has been tested against [kpmiller's reference implementation](https://github.com/kpmiller/emulator101/blob/master/CocoaPart7-Threading/8080emu.c)

## References used in development

- Intel 8080 Assembly Language Programming Manual
- [ComputerArcheology.com](https://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html)
- [Emulator101.com](http://www.emulator101.com/)

## Requirements

- For copyright reasons I can not provide the ROM file for the space invaders game. You need to find and download this file yourself and place it in the same directory as the executable. The ROM can either be provided as one file ("*invaders.bin*") or as four separate files ("*invaders.e*", "*invaders.f*", "*invaders.g*" and "*invaders.h*").
- g++/gcc
- SDL 2

## Usage

Once you have made sure that you have ROM file and SDL2, type `make run` into your favorite console to compile and run.

## Controls

Player 1 plays with the arrow keys and Player 2 with WASD.

- Enter: Put a coin into the arcade machine
- 1: Select 1 Player mode
- 2: Select 2 Player mode

- Up    arrow: Player 1 fire
- Left  arrow: Player 1 left
- Right arrow: Player 1 right

- W: Player 2 fire
- A: Player 2 left
- D: Player 2 right