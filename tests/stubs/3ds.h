#pragma once
#include "3ds/types.h"
#define BIT(n) (1u << (n))
#define KEY_B (1u<<1)
#define KEY_X (1u<<10)
#define KEY_L (1u<<9)
#define KEY_R (1u<<8)
bool aptMainLoop(void);
void hidScanInput(void);
u32 hidKeysDown(void);
u32 hidKeysHeld(void);
void gspWaitForVBlank(void);
