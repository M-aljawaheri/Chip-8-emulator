#pragma once
/** @file main.cpp
 *  @brief Main File for the emulator
 *
 *  This project follows the specification as laid out by
 *  Thomas P.Greene over at http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
 *  Big shoutout to Mr.Thomas and everyone who worked hard to compile
 *  this document
 *
 *  @author Mohammed Al-Jawaheri (mobj)
 *  @bug No known bugs
 */

#include <stdint.h>
#include <vector>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <SDL.h>
#include <string>
#include <unordered_map>
#include <random>

 /** Memory starts at 0x200 (address 512) since 0 through 511 are reserved for CHIP-8 Interpreter */
#define MEMSTART 0
#define FONT_ADDRESS 2
static constexpr const uint8_t SCALEFACTOR = 10u;
static constexpr const uint8_t W = 64u, H = 32u;

/** Types */
typedef uint16_t uregister16;
typedef uint8_t uregister8;
typedef uint16_t instruction;
typedef int8_t byte;
typedef uint8_t ubyte;
typedef unsigned int uint;

/** All instructions are 2 bytes long and are stored most-significant-byte first (big endian) */
enum instructions {
    CLS = (uint16_t)0xE0,  // Clear display
    RET = (uint16_t)0xEE,  // Return from subroutine
};
enum keyState { UP, DOWN };

/** Globals */
SDL_Renderer* gRenderer = nullptr;

struct Chip_8 {

    // Initialize memory. Memory spans addresses 0x000 through 0xFFF (4095)
    std::vector<ubyte> mem{ 0x12, 0x00 };  // Memory emulator, initialized with jump to program
    std::vector<uint8_t> V;                // 16 8 bit registers (reserved for interpreter purposes)
    std::vector<keyState> keys{ UP };      // keys[i] indicates whether key i is pressed or not
    uint16_t VI = { 0 };                   // 16 bit register to store memory addresses (uses only rightmost 12 bits)
    uint8_t soundTimer = { 0 };
    uint8_t delayTimer = { 0 };
    uint16_t PC = { 0 };           //  16 bit Program counter
    uint8_t SP = { 0 };           // 8 bit stack pointer (value is index of topmost level of stack)
    std::vector<int16_t> callstack;   // stack is array of 16 16-bits
    std::uniform_int_distribution<uint16_t> randomSeed;    // Generator and Seed for randomness
    std::default_random_engine randGen;                    // Adapted from stack overflow

    // Graphics output
    SDL_Window* mWindow = nullptr;
    SDL_Texture* texture = nullptr;
    std::vector<int8_t> pixels{ 0 };    // size [W*H], 0 is for white, 1 is for black

 
    Chip_8(uint _FPS, SDL_Window* window);
    void ExecuteInst();
    void RunCycle();
    // Read the program to be executed from a file
    void LoadProgram(const char* filename, uint pos = 0x200);

    
    
};

/*
 * Some documentation on best cycle speed for specific games
 * MISSILES -- 100
 *
 * */
