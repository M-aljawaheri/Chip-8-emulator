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
#include "Instructions.h"
#include <SDL.h>
#include <string>
#include <unordered_map>

 /** Memory starts at 0x200 (address 512) since 0 through 511 are reserved for CHIP-8 Interpreter */
#define MEMSTART 0
#define NEXT_INSTRUCTION { PC += 2; break; }  // Increments program counter and breaks switch
static constexpr const unsigned SCALEFACTOR = 10;
static constexpr const unsigned W = 64, H = 32;

typedef uint16_t uregister16;
typedef uint8_t uregister8;
typedef uint16_t instruction;
typedef int8_t byte;
typedef uint8_t ubyte;
typedef unsigned int uint;
enum keyState {UP, DOWN};

/** Globals */
SDL_Renderer* gRenderer = nullptr;

/** Helper functions */
static inline ubyte GetfirstBit(ubyte bits) { return (bits >> 7) & 0x1; }

struct Chip_8 {

    // Initialize memory. Memory spans addresses 0x000 through 0xFFF (4095)
    std::vector<ubyte> mem{ 0x12, 0x00 };  // Memory emulator, initialized with jump to program
    std::vector<uint8_t> V;                // 16 8 bit registers (reserved for interpreter purposes)
    std::vector<keyState> keys{ UP };      // keys[i] indicates whether key i is pressed or not
    uint16_t VI = { 0 };                   // 16 bit register to store memory addresses (uses only rightmost 12 bits)
    uint8_t soundTimer = { 0 };
    uint8_t delayTimer = { 0 };
    uint16_t PC = { 0 };           //  16 bit Program counter
    uint8_t SP  = { 0 };           // 8 bit stack pointer (value is index of topmost level of stack)
    std::vector<int16_t> callstack;   // stack is array of 16 16-bits

    // Graphics output
    //void* pixelState[W * H] = { 0 };
    SDL_Window* mWindow = nullptr;
    SDL_Texture* texture = nullptr;
    std::vector<int8_t> pixels{ 0 };    // size [W*H], 0 is for white, 1 is for black
    int texturePitch = 0;   // texture width in memory

    Chip_8(uint _FPS, SDL_Window* window) {
        uint FPS = _FPS;                      // How many frames per second should programs run on
        mem.reserve(4096); mem.resize(4096);  // Our addresses -> 0x000-0xFFF
        keys.reserve(16); keys.resize(16);    // keyboard has 16 keys
        V.reserve(16); V.resize(16);          // 16 general-purpose registers V0 through VF
        callstack.reserve(16); callstack.resize(16);  // Note: 16 bits mean we can only have 16 nested subroutines in a given program
        const ubyte font[80] = {                      // Note: Each digit is 5 bytes stacked on top of each other
            0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0       // E.g: ****       11110000
            0x20, 0x60, 0x20, 0x20, 0x70,  // 1       //      *  *       10010000
            0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2       //      *  *  -->  10010000
            0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3       //      *  *       10010000
            0x90, 0x90, 0xF0, 0x10, 0x10,  // 4       //      ****       11110000
            0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
            0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
            0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
            0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
            0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
            0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
            0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
            0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
            0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
            0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
            0xF0, 0x80, 0xF0, 0x80, 0x80,  // F
        };
        for (int i = 2; i < 82; ++i) {
            mem[i] = font[i-2];
        }
        pixels.reserve(W * H); pixels.resize(W * H);
        SDL_Texture* texture = SDL_CreateTexture(gRenderer, SDL_GetWindowPixelFormat(window), SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, W * SCALEFACTOR, H * SCALEFACTOR);
        if (!texture) {
            printf("Unable to create blank texture! SDL Error: %s\n", SDL_GetError());
            exit(1);
        }
        mWindow = window;
        // texture modification test

    }                                                 


    void ExecuteInst() {
        instruction opcode = mem[PC];
        opcode <<= 8; opcode |= mem[PC + 1];

        // get the variables and start matching
        uint16_t nnn = opcode & 0xFFF;
        uint8_t n = opcode & 0xF;
        uint8_t x = mem[PC] & 0xF;           // lower half of first byte   (nibble 2)
        uint8_t y = mem[PC + 1] >> 4 & 0xF;  // upper half of second byte  (nibble 3)
        int8_t kk = mem[PC + 1];
        ubyte u = (mem[PC] >> 4) & 0xF;      // first nibble
        ubyte v = (mem[PC + 1] ) & 0xF;      // last nibble

        if (opcode == CLS) {  // clear display
            //Clear screen TODO
            for (int i = 0; i < W*H; ++i) {
                pixels[i] = 0;
            }
            PC += 2;
            return;
        }
        else if (opcode == RET) {  // return from subroutine
            PC = callstack[SP--];
            return;
        }

        switch (u) {
            case 1: {  // [0x1nnn]
                if (nnn >= mem.size()) {  // All instructions start at even indices
                    std::cerr << "Incorrect jump address\n"; exit(1);
                }
                PC = nnn;
                break;
            }

            case 2: {  // [0x2nnn]
                callstack[++SP] = PC;
                PC = nnn;
                break;
            }

            case 3: { // [0x3xkk] Skip next instruction
                if (V[x] == kk)
                    PC += 4;
                else
                    PC += 2;
                break;
            }

            case 4: { // [4xkk] Skip next instruction if NE
                if (V[x] != kk)
                    PC += 4;
                else
                    PC += 2;
                break;
            }

            case 5: {  // [5xy0] Skip next instruction if E
                if (V[x] == V[y])
                    PC += 4;
                else
                    PC += 2;
                break;
            }

            case 6: {   // [6xkk] Load
                V[x] = kk;
                PC += 2;
                break;
            }

            case 7: {  // [7xkk]
                V[x] += kk;
                PC += 2;
                break;
            }
            
            case 8: {
                switch (v) {
                    case 0: {  // [8xy0] - LD Vx, Vy
                        V[x] = V[y];
                        break;
                    }
                    case 1: {  // [8xy1] - OR Vx, Vy
                        V[x] |= V[y];
                        break;
                    }
                    case 2: {  // [8xy2] - AND Vx, Vy
                        V[x] &= V[y];
                        break;
                    }
                    case 3: {  // [8xy3] - XOR Vx, Vy
                        V[x] ^= V[y];
                        break;
                    }
                    case 4: {  // [8xy4] - ADD
                        uint16_t result = V[x] + V[y];
                        result > 255 ? V[15] = 1 : V[15] = 0;
                        V[x] = result & 0xFF;
                        break;
                    }
                    case 5: {  // [8xy5] - SUB
                        uint16_t result = V[x] - V[y];
                        V[x] > V[y] ? V[15] = 1 : V[15] = 0;
                        V[x] = V[x] - V[y];
                        break;
                    }
                    case 6: {  // [8xy6] - SHR - Raises flag if num is odd
                        if (V[x] % 2 == 1) V[15] = 1;
                        else               V[15] = 0;
                        V[x] >>= 1;   // Divide by 2
                        break;
                    }
                    case 7: {  // [8xy7] SUBN Vx, Vy
                        
                        if (V[y] > V[x]) { V[15] = 1; }
                        else             { V[15] = 0; }
                        V[x] = V[y] - V[x];
                        break;
                    }
                    case 0xE: { // [8xyE] SHL (multiply by two)
                        if (GetfirstBit(V[x]) == 1) { V[15] = 1; }
                        else                        { V[15] = 0; }

                        V[x] <<= 1;
                        break;
                    }
                    default:
                        std::cout << "Invalid Opcode\n"; exit(1);
                    }
                PC += 2;
                break;
            }
            
            case 9: {  // [9xy0] - SNE Vx, Vy
                if (V[x] != V[y])
                    PC += 4;
                else
                    PC += 2;
                break;
            }

            case 0xA: {  // [Annn] - Load I at address nnn
                VI = nnn;
                PC += 2;
                break;
            }

            case 0xB: {  // [Bnnn] - Jump to address nnn + v[0]
                PC = nnn + V[0];
                break;
            }

            case 0xC: {  // [Cxkk] - Vx = rand & kk
                V[x] = (int8_t)(rand() % 255) & kk;
                PC += 2;
                break;
            }

                    // TODO
            case 0xD: {  // [Dxyn] - DRW Vx, Vy, nibble
                ubyte nextSpriteLine = 0;
                for (uint spriteRow = 0; spriteRow < n; ++spriteRow) {
                    // Get the n-byte sprite
                    nextSpriteLine = mem[VI & 0xFFF + spriteRow];
                    // Go through each bit of the byte
                    for (uint bit = 0; bit < 8; ++bit) {
                        // spriterow + vertOffset is current row, horizontal offset is current col
                        uint8_t horizontalOffset = bit + V[x];
                        uint8_t verticalOffset = spriteRow + V[y];
                        uint8_t drawBit = (nextSpriteLine >> (7 - bit)) & 0x1;
                        // Set the drawflag for collision
                        if (pixels[W * ((spriteRow + verticalOffset) % H) + (horizontalOffset % W)] == 1) {
                            V[15] = 1;
                        }
                        // draw the drawBit
                        pixels[W * ((spriteRow + verticalOffset) % H) + (horizontalOffset % W)] ^= drawBit;
                    }
                }
                PC += 2;
                break;
            }

            case 0xE: {  // [Ex9E] Skip if Vx is pressed
                if (mem[PC + 1] == 0x9E) {
                    if (V[x] > 15) { std::cout << "Invalid code\n"; exit(1); }
                    if (keys[V[x]] == DOWN)
                        PC += 2;
                    
                } else if (mem[PC + 1] == 0xA1) {
                    if (keys[V[x]] == UP)
                        PC += 2;
                } else {
                    std::cout << "Invalid Opcode\n"; exit(1);
                    return;
                }
                PC += 2;
                break;
            }

            case 0xF: {
                switch (mem[PC+1]) {
                    /* Note: no need to advance PC in this switch case. PC is incremented at end of parent switch */
                    case 0x07: {  // [Fx07]
                        V[x] = delayTimer;
                        break;
                    }

                    case 0x0A: {  // [Fx0A]
                        // TODO : wait for key press
                        uint8_t keypressed = 5;
                        V[x] = keypressed;
                        break;
                    }

                    case 0x15: {  // [Fx15]
                        delayTimer = V[x];
                        break;
                    }

                    case 0x18: {  // [Fx18]
                        soundTimer = V[x];
                        break;
                    }

                    case 0x1E: {  // [Fx1E]
                        VI += V[x];
                        break;
                    }

                    // TODO
                    case 0x29: {  // [Fx29]
                        VI = V[x] * 5 + 2;
                        break;
                    }

                    case 0x33: {  // [Fx33]
                        uint16_t value = V[x];
                        mem[VI + 2] = value % 100;  // ones digits
                        value /= 10;  // get rid of ones digits
                        mem[VI + 1] = value % 10;
                        value /= 10;  // get rid of tens digits
                        mem[VI] = (ubyte)value;
                        break;
                    }

                    case 0x55: {  // [Fx55]
                        for (uint_fast8_t i = 0; i <= x; ++i) {
                            mem[VI + i] = V[i];
                        }
                        break;
                    }

                    case 0x65: {  // [Fx65]
                        for (uint_fast8_t i = 0; i <= x; ++i) {
                            V[i] = mem[VI + i];
                        }
                        break;
                    }

                    default:
                        std::cout << "Invalid Opcode inside F case\n"; exit(1);
                        return;
                    }
                PC += 2;
                break;
            } // End of case 0xF 

            default: {
                std::cout << "Invalid opcode from start\n"; exit(1);
                return;
            } 
        
        }   // End of switch case
        std::cout << "Instruction is over\n";
        return;
    }

    void RunCycle() {
        uint maxInstructionsPerFrame = 50;
        for (uint i = 0; i < maxInstructionsPerFrame; ++i) {
            this->ExecuteInst();
        }
    }

    // Read the program to be executed from a file
    void LoadProgram(const char* filename, uint pos = 0x200) {
        for (std::ifstream file(filename, std::ios::binary); file.good(); ) {
            mem[pos++] = file.get();
        }
    }
};


/** Simple SDL memory cleanup */
void WindowCleanup(SDL_Window* f_window, SDL_Texture* f_texture) {
    //SDL_FreeSurface(f_surface);
    SDL_DestroyWindow(f_window);
    SDL_DestroyTexture(f_texture);
    SDL_Quit();
}


void drawPixels(std::vector<int8_t> pixels) {
    //SDL_RenderDrawPoint(gRenderer, row, col);
    for (uint row = 0; row < H; ++row) {
        for (uint col = 0; col < W; ++col) {
            // go through each pixel in the 64*32 array then scale by scaleFactor
            if (pixels[row * 64 + col] == 1) {
                SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
                //SDL_RenderDrawLine(gRenderer, col * SCALEFACTOR, row * SCALEFACTOR, (row + 1) * SCALEFACTOR, col * SCALEFACTOR);
                for (int i = 0; i < SCALEFACTOR; ++i) {
                    SDL_RenderDrawLine(gRenderer, col * SCALEFACTOR, row * SCALEFACTOR + i, (col + 1) * SCALEFACTOR, row * SCALEFACTOR + i);
                }
            }
            else {
                SDL_SetRenderDrawColor(gRenderer, 255, 255, 255, 255);
                //SDL_RenderDrawLine(gRenderer, col * SCALEFACTOR, row * SCALEFACTOR, (col + 1) * SCALEFACTOR, row * SCALEFACTOR);
                for (int i = 0; i < SCALEFACTOR; ++i) {
                    SDL_RenderDrawLine(gRenderer, col * SCALEFACTOR, row * SCALEFACTOR + i, (col + 1) * SCALEFACTOR, row * SCALEFACTOR + i);
                }
            }

        }
    }
}

/**
 * Key binding :
 * |1|2|3|C|            |1|2|3|4|
 * |4|5|6|D|   ----->   |Q|W|E|R|
 * |7|8|9|E|            |A|S|D|F|
 * |A|0|B|F|            |Z|X|C|V|
 */
using keyDictionary = std::unordered_map<SDL_Keycode, int>;
keyDictionary KeybindsInitialize() {
    std::unordered_map<SDL_Keycode, int> keybindMap = {
        {SDLK_1, 1},  {SDLK_2, 2}, {SDLK_3, 3},  {SDLK_4, 12},
        {SDLK_q, 4},  {SDLK_w, 5}, {SDLK_e, 6},  {SDLK_c, 13},
        {SDLK_a, 7},  {SDLK_s, 8}, {SDLK_d, 9},  {SDLK_e, 14},
        {SDLK_z, 10}, {SDLK_x, 0}, {SDLK_c, 11}, {SDLK_v, 15},
    };
    return keybindMap;
}


/* Main execution point */
int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_EVENTS) == 0 ? std::cout << "Success!\n" : std::cerr << "SDL FAILED" << std::endl;
    std::string title("Chip-8 Emulator");
    SDL_Window* window = SDL_CreateWindow(title.c_str(),
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               W*SCALEFACTOR, H*SCALEFACTOR, 0);
    if (window == nullptr) { std::cerr << "Window initialized failed"; }

    // Create renderer and texture that we will edit
    gRenderer = SDL_CreateRenderer(window, -1, SDL_RendererFlags::SDL_RENDERER_ACCELERATED);
    if (!gRenderer) {
        printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        exit(1);
    }
    SDL_SetRenderDrawColor(gRenderer, 255, 255, 240, 0);
    SDL_RenderClear(gRenderer);
    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 0);

    // Initialize the CPU
    keyDictionary keybinds = KeybindsInitialize();
    std::vector<ubyte> testProgram{ 0xF2, 0x15 };
    Chip_8* CPU = new Chip_8(60, window);
    CPU->LoadProgram("..\\Programs\\hanoi.bin");
    
    

    // while program is not over run cpu for one cycle
    // executing max-instructions-per-frame number of ExecuteInsts
    SDL_Event e;
    bool quit = false;
    // Main program loop
    while (!quit) {
        // process pending input
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                quit = true;
            else if (e.type == SDL_KEYDOWN)
                CPU->keys[keybinds[e.key.keysym.sym]] = DOWN;
            else if (e.type == SDL_KEYUP)
                CPU->keys[keybinds[e.key.keysym.sym]] = UP;
        }
        // Run CPU
        
        // Assuming CPU clock speed of 10 mhz (10,000,000 hertz frequency) and all operations take 2 cycles to complete
        const int clockSpeed = 500; // Assume clock speed of 500 hertz (500 cycles per second, each instruction takes 2 cycles)
        int clockTimer = 0;
        // Run clockSpeed / 2 instructions (500 cycles) 
        for (int i = 0; i < clockSpeed; i+=2) {
            CPU->ExecuteInst();
        }
        
        //Render texture to screen
        //SDL_RenderCopy(gRenderer, CPU->texture, NULL, NULL);
        //Update screen
        drawPixels(CPU->pixels);
        SDL_RenderPresent(gRenderer);
        /* DEPRECATED
        //SDL_BlitSurface(windowSurface, NULL, windowSurface, NULL);
        //SDL_UpdateWindowSurface(window); */
    }
    // poll events and pass them to the chip8 to let it know what is being pressed

    // program is over do cleanup
    WindowCleanup(window, CPU->texture);
    return 0;
}


class mainTester {
public:
    mainTester() { };

    void runTests() {
        //std::vector<ubyte> testProgram{ 0xF2, 0x15 };
        //Chip_8* CPU = new Chip_8(60);
    }

};