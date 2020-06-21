#pragma once

/* All instructions are 2 bytes long and are stored most-significant-byte first (big endian) */
enum instructions {
	CLS = (uint16_t)0xE0,  // Clear display
	RET = (uint16_t)0xEE,  // Return from subroutine
};