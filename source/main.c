#include <nds.h>
#include <stdio.h>
#include <math.h>

#define COLOR(r, g, b) (((r) & 31) | (((g) & 31) << 5) | (((b) & 31) << 10) | (1 << 15))

// Game variables
int scorePlayer = 0;
int scoreEnemy = 0;
int winner = 0;    // 1 for Player, 2 for Enemy
int winTimer = 0;

float puckX = 128.0f, puckY = 192.0f;
float puckVX = 0.0f, puckVY = 0.0f;

float playerX = 128.0f, playerY = 320.0f;
float enemyX = 128.0f, enemyY = 64.0f;
float playerVX = 0.0f, playerVY = 0.0f;

// Graphics memory and bg IDs
u16* gfxPuck;
u16* gfxPuckSub;
u16* gfxEnemy;
u16* gfxPlayer;
int bgMainId;
int bgSubId;

// Sound buffers for procedurally generated sounds
#define BEEP_SAMPLES 4000
s8 bounceSnd[BEEP_SAMPLES];
s8 goalSnd[30000];

// Generate basic PSG-like sounds in RAM to avoid external files
void generateSounds() {
    for (int i = 0; i < BEEP_SAMPLES; i++) {
        int v = (i / 10) % 2 == 0 ? 60 : -60;
        bounceSnd[i] = v * (BEEP_SAMPLES - i) / BEEP_SAMPLES;
    }
    for (int i = 0; i < 30000; i++) {
        int v = ((i / (30 + (i % 20))) % 2 == 0 ? 80 : -80);
        goalSnd[i] = v * (30000 - i) / 30000;
    }
    DC_FlushRange(bounceSnd, BEEP_SAMPLES);
    DC_FlushRange(goalSnd, 30000);
}

// Generate sprite circle directly into mapped 1D tile VRAM (256 color format)
void drawSpriteCircle(u8* gfx, int size, int radius, u8 colorIndex) {
    for (int i = 0; i < size * size; i++) gfx[i] = 0;
    
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float cx = x - size / 2 + 0.5f;
            float cy = y - size / 2 + 0.5f;
            if (cx * cx + cy * cy <= radius * radius) {
                int tileX = x / 8;
                int tileY = y / 8;
                int tilesPerRow = size / 8;
                int tileIndex = tileY * tilesPerRow + tileX;
                int pixelIndex = (y % 8) * 8 + (x % 8);
                gfx[tileIndex * 64 + pixelIndex] = colorIndex;
            }
        }
    }
}

void drawRect(u16* bg, int x, int y, int w, int h, u16 color) {
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            if (x + i >= 0 && x + i < 256 && y + j >= 0 && y + j < 192) {
                bg[(y + j) * 256 + (x + i)] = color;
            }
        }
    }
}

void drawCircleOutline(u16* bg, int xc, int yc, int r, int thickness, u16 color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            int distSq = x * x + y * y;
            if (distSq <= r * r && distSq >= (r - thickness) * (r - thickness)) {
                int px = xc + x;
                int py = yc + y;
                if (px >= 0 && px < 256 && py >= 0 && py < 192) {
                    bg[py * 256 + px] = color;
                }
            }
        }
    }
}

// A simple minimalist 5x3 font for score display
const u8 font5x3[10][15] = {
    {1,1,1, 1,0,1, 1,0,1, 1,0,1, 1,1,1},
    {0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0},
    {1,1,1, 0,0,1, 1,1,1, 1,0,0, 1,1,1},
    {1,1,1, 0,0,1, 1,1,1, 0,0,1, 1,1,1},
    {1,0,1, 1,0,1, 1,1,1, 0,0,1, 0,0,1},
    {1,1,1, 1,0,0, 1,1,1, 0,0,1, 1,1,1},
    {1,1,1, 1,0,0, 1,1,1, 1,0,1, 1,1,1},
    {1,1,1, 0,0,1, 0,0,1, 0,0,1, 0,0,1},
    {1,1,1, 1,0,1, 1,1,1, 1,0,1, 1,1,1},
    {1,1,1, 1,0,1, 1,1,1, 0,0,1, 1,1,1}
};

void drawDigit(u16* bg, int x, int y, int digit, u16 color, int scale) {
    if (digit < 0 || digit > 9) return;
    for (int dy = 0; dy < 5; dy++) {
        for (int dx = 0; dx < 3; dx++) {
            if (font5x3[digit][dy * 3 + dx]) {
                drawRect(bg, x + dx * scale, y + dy * scale, scale, scale, color);
            }
        }
    }
}

void drawScore(u16* bg, int x, int y, int score, u16 color) {
    int tens = score / 10;
    int units = score % 10;
    if (tens > 0) drawDigit(bg, x, y, tens, color, 4);
    drawDigit(bg, x + 16, y, units, color, 4);
}

// Setup background
void drawField() {
    u16* bgTop = bgGetGfxPtr(bgMainId);
    u16* bgBot = bgGetGfxPtr(bgSubId);
    
    // Clear out screen
    for (int i = 0; i < 256 * 256; i++) {
        bgTop[i] = COLOR(2, 2, 4);
        bgBot[i] = COLOR(2, 2, 4);
    }

    // Goal outlines & center lines
    drawCircleOutline(bgTop, 128, 0, 40, 5, COLOR(31, 2, 2));
    drawCircleOutline(bgTop, 128, 192, 30, 2, COLOR(2, 2, 31));

    drawCircleOutline(bgBot, 128, 192, 40, 5, COLOR(2, 2, 31));
    drawCircleOutline(bgBot, 128, 0, 30, 2, COLOR(2, 2, 31));

    drawRect(bgTop, 0, 190, 256, 2, COLOR(2, 2, 31));
    drawRect(bgBot, 0, 0, 256, 2, COLOR(2, 2, 31));
}

void updateScoreDisplay() {
    u16* bgTop = bgGetGfxPtr(bgMainId);
    u16* bgBot = bgGetGfxPtr(bgSubId);

    // Erase old scores
    drawRect(bgTop, 200, 10, 50, 20, COLOR(2, 2, 4));
    drawRect(bgBot, 200, 162, 50, 20, COLOR(2, 2, 4));

    // Draw new scores
    drawScore(bgTop, 200, 10, scoreEnemy, COLOR(31, 2, 2));
    drawScore(bgBot, 200, 162, scorePlayer, COLOR(15, 15, 31));
}

void checkPaddleCollision(float px, float py, float pvx, float pvy, float radius) {
    float dx = puckX - px;
    float dy = puckY - py;
    float distSq = dx * dx + dy * dy;
    float r = 8 + radius;
    
    if (distSq < r * r && distSq > 0) {
        float dist = sqrtf(distSq);
        float nx = dx / dist;
        float ny = dy / dist;
        
        // Push puck out of the collision
        puckX = px + nx * r;
        puckY = py + ny * r;
        
        float rvx = puckVX - pvx;
        float rvy = puckVY - pvy;
        float velAlongNormal = rvx * nx + rvy * ny;
        if (velAlongNormal > 0) return; // Discard moving away
        
        float restitution = 0.8f;
        float j = -(1 + restitution) * velAlongNormal;
        
        puckVX += j * nx * 1.5f;
        puckVY += j * ny * 1.5f;
        
        // Allow adding some spin/push velocity
        puckVX += pvx * 0.5f;
        puckVY += pvy * 0.5f;
        
        soundPlaySample(bounceSnd, SoundFormat_8Bit, BEEP_SAMPLES, 22050, 127, 64, false, 0);
    }
}

void clampVelocity(float* vx, float* vy, float maxSpeed) {
    float speedSq = (*vx) * (*vx) + (*vy) * (*vy);
    if (speedSq > maxSpeed * maxSpeed && speedSq > 0) {
        float speed = sqrtf(speedSq);
        *vx = (*vx / speed) * maxSpeed;
        *vy = (*vy / speed) * maxSpeed;
    }
}

int main(void) {
    // 16-bit 256x256 backgrounds on both screens
    videoSetMode(MODE_3_2D);
    videoSetModeSub(MODE_3_2D);

    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE);
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_SUB_SPRITE);

    oamInit(&oamMain, SpriteMapping_1D_32, false);
    oamInit(&oamSub, SpriteMapping_1D_32, false);

    bgMainId = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    bgSubId = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    gfxPuck = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    gfxPuckSub = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_256Color);
    gfxEnemy = oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_256Color);
    gfxPlayer = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_256Color);

    SPRITE_PALETTE[1] = RGB15(31, 31, 31);
    SPRITE_PALETTE[2] = RGB15(31, 2, 2);
    SPRITE_PALETTE[3] = RGB15(15, 15, 31);
    
    SPRITE_PALETTE_SUB[1] = RGB15(31, 31, 31);
    SPRITE_PALETTE_SUB[2] = RGB15(31, 2, 2);
    SPRITE_PALETTE_SUB[3] = RGB15(15, 15, 31);

    drawSpriteCircle((u8*)gfxPuck, 16, 8, 1);
    drawSpriteCircle((u8*)gfxPuckSub, 16, 8, 1);
    drawSpriteCircle((u8*)gfxEnemy, 32, 16, 2);
    drawSpriteCircle((u8*)gfxPlayer, 32, 16, 3);

    drawField();
    updateScoreDisplay();

    soundEnable();
    generateSounds();

    while (1) {
        swiWaitForVBlank();
        scanKeys();
        
        touchPosition touch;
        // Game Loop / Win State
        if (winner != 0) {
            winTimer--;
            if ((winTimer / 10) % 2 == 0) {
                u16* bgTop = bgGetGfxPtr(bgMainId);
                u16* bgBot = bgGetGfxPtr(bgSubId);
                u16 c = (winner == 1) ? COLOR(15, 15, 31) : COLOR(31, 2, 2);
                for (int i = 0; i < 256 * 256; i++) {
                    bgTop[i] = c;
                    bgBot[i] = c;
                }
            } else {
                drawField();
                updateScoreDisplay();
            }
            
            if (winTimer <= 0) {
                winner = 0;
                scorePlayer = 0;
                scoreEnemy = 0;
                drawField();
                updateScoreDisplay();
                puckX = 128; puckY = 192;
                puckVX = 0; puckVY = 0;
            }
            continue;
        }

        // --- Physics & Input ---
        touchRead(&touch);
        float oldPx = playerX;
        float oldPy = playerY;
        
        // Touch Input logic
        if (keysHeld() & KEY_TOUCH) {
            playerX = touch.px;
            playerY = touch.py + 192; // Remap offset to global coordinates
        }
        
        // Player Bounds
        if (playerY < 192 + 16) playerY = 192 + 16;
        if (playerY > 384 - 16) playerY = 384 - 16;
        if (playerX < 16) playerX = 16;
        if (playerX > 256 - 16) playerX = 256 - 16;
        
        playerVX = playerX - oldPx;
        playerVY = playerY - oldPy;
        if (fabs(playerVX) > 20) playerVX = (playerVX > 0) ? 20 : -20;
        if (fabs(playerVY) > 20) playerVY = (playerVY > 0) ? 20 : -20;

        // --- AI Logic (Enemy) ---
        float targetX = 128.0f;
        float targetY = 64.0f;

        if (puckY < 192 + 20) {
            // Puck is on AI's side or close
            if (puckY > enemyY + 8) {
                // We are somewhat behind it -> SMASH!
                targetX = puckX;
                targetY = puckY + 15; 
            } else {
                // Puck is somehow behind us -> RUN BACK
                targetX = 128.0f;
                targetY = 20.0f; 
            }
        } else {
            // Anticipate! Target prediction
            targetX = puckX + (puckVX * 10);
            targetY = 40.0f;
            if (targetX < 32) targetX = 32;
            if (targetX > 256 - 32) targetX = 256 - 32;
        }

        float oldEx = enemyX;
        float oldEy = enemyY;
        enemyX += (targetX - enemyX) * 0.15f;
        enemyY += (targetY - enemyY) * 0.15f;
        
        if (enemyX < 16) enemyX = 16;
        if (enemyX > 256 - 16) enemyX = 256 - 16;
        if (enemyY < 16) enemyY = 16;
        if (enemyY > 192 - 16) enemyY = 192 - 16;
        
        float enemyVelX = enemyX - oldEx;
        float enemyVelY = enemyY - oldEy;

        // --- Puck Update ---
        puckX += puckVX;
        puckY += puckVY;
        
        puckVX *= 0.99f;
        puckVY *= 0.99f;
        
        // Wall Bounce
        if (puckX <= 8) {
            puckX = 8; puckVX = -puckVX;
            soundPlaySample(bounceSnd, SoundFormat_8Bit, BEEP_SAMPLES, 11025, 127, 64, false, 0);
        }
        if (puckX >= 256 - 8) {
            puckX = 256 - 8; puckVX = -puckVX;
            soundPlaySample(bounceSnd, SoundFormat_8Bit, BEEP_SAMPLES, 11025, 127, 64, false, 0);
        }

        // Goals Logic
        bool isGoal = false;
        
        // Top Goal
        if (puckY <= 8) {
            if (puckX > 128 - 40 && puckX < 128 + 40) {
                scorePlayer++;
                isGoal = true;
            } else {
                puckY = 8; puckVY = -puckVY;
                soundPlaySample(bounceSnd, SoundFormat_8Bit, BEEP_SAMPLES, 11025, 127, 64, false, 0);
            }
        }
        
        // Bottom Goal
        if (puckY >= 384 - 8) {
            if (puckX > 128 - 40 && puckX < 128 + 40) {
                scoreEnemy++;
                isGoal = true;
            } else {
                puckY = 384 - 8; puckVY = -puckVY;
                soundPlaySample(bounceSnd, SoundFormat_8Bit, BEEP_SAMPLES, 11025, 127, 64, false, 0);
            }
        }

        if (isGoal) {
            updateScoreDisplay();
            soundPlaySample(goalSnd, SoundFormat_8Bit, 30000, 11025, 127, 64, false, 0);
            puckX = 128; puckY = 192;
            puckVX = 0; puckVY = 0;
            
            if (scorePlayer >= 10) { winner = 1; winTimer = 180; }
            if (scoreEnemy >= 10) { winner = 2; winTimer = 180; }
        }

        // Paddle vs Puck Collisions
        checkPaddleCollision(playerX, playerY, playerVX, playerVY, 16);
        checkPaddleCollision(enemyX, enemyY, enemyVelX, enemyVelY, 16);
        
        clampVelocity(&puckVX, &puckVY, 15.0f);

        // --- Render ---
        oamClear(&oamMain, 0, 0);
        oamClear(&oamSub, 0, 0);

        if (enemyY < 192 + 16) {
            oamSet(&oamMain, 0, (int)enemyX - 16, (int)enemyY - 16, 0, 2, SpriteSize_32x32, SpriteColorFormat_256Color, gfxEnemy, -1, false, false, false, false, false);
        }
        if (playerY > 192 - 16) {
            // Translate subscreen Y position
            oamSet(&oamSub, 0, (int)playerX - 16, (int)playerY - 192 - 16, 0, 3, SpriteSize_32x32, SpriteColorFormat_256Color, gfxPlayer, -1, false, false, false, false, false);
        }

        long px = (long)puckX - 8;
        long py = (long)puckY - 8;
        if (py < 192 + 8) {
            oamSet(&oamMain, 1, (int)px, (int)py, 0, 1, SpriteSize_16x16, SpriteColorFormat_256Color, gfxPuck, -1, false, false, false, false, false);
        }
        if (py > 192 - 8) {
            oamSet(&oamSub, 1, (int)px, (int)py - 192, 0, 1, SpriteSize_16x16, SpriteColorFormat_256Color, gfxPuckSub, -1, false, false, false, false, false);
        }

        oamUpdate(&oamMain);
        oamUpdate(&oamSub);
    }

    return 0;
}
