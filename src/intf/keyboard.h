#pragma once

void keyboard_init(void);
// Blocks (halting the CPU) until a key is available.
char keyboard_getchar(void);
