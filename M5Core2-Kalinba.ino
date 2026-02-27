/**
 * @file M5Core2-Kalinba.ino
 * @brief M5Core2 Kalimba (Thumb Piano) Emulator using UNIT-SYNTH
 * @version 2.0
 * @date 2024
 *
 * A 17-key Kalimba emulator with touchscreen interface
 * Uses M5UnitSynth library for sound via UNIT-SYNTH (Serial2, pins 33, 32)
 *
 * Pococha Owa Higashi 尾和/東
 *
 * @Hardwares: M5Core2 + Unit Synth
 * @Platform Version: Arduino M5Stack Board Manager v2.1.0
 * @Dependent Library:
 * M5UnitSynth: https://github.com/m5stack/M5Unit-Synth
 * M5Core2: https://github.com/m5stack/M5Core2
 */

#include <M5Core2.h>

// for SD-Updater
#define SDU_ENABLE_GZ
#include <M5StackUpdater.h>

#include "M5UnitSynth.h"

// 尾和東@Pococha技術枠

// UNIT-SYNTH instance
M5UnitSynth synth;

// Kalimba tuning - 17 keys in C major
// Index = physical position (0=leftmost shortest, 8=center longest, 16=rightmost shortest)
// Center=C4, left1=D4, right1=E4, left2=F4, right2=G4, ...
// Longer tine = lower pitch (center is lowest)
uint8_t tine_notes[17] = {
    86,  // pos 0  (leftmost)  D6
    83,  // pos 1              B5
    79,  // pos 2              G5
    76,  // pos 3              E5
    72,  // pos 4              C5
    69,  // pos 5              A4
    65,  // pos 6              F4
    62,  // pos 7              D4
    60,  // pos 8  (center)    C4  ← longest tine, lowest note
    64,  // pos 9              E4
    67,  // pos 10             G4
    71,  // pos 11             B4
    74,  // pos 12             D5
    77,  // pos 13             F5
    81,  // pos 14             A5
    84,  // pos 15             C6
    88   // pos 16 (rightmost) E6
};

// Key status tracking (indexed by physical position 0-16)
bool key_pressed[17] = {false};
uint8_t active_note_pitch = 0;     // actual MIDI pitch being played
int8_t active_tine = -1;           // which tine is currently active (-1 = none)

// Display constants
const uint16_t DISPLAY_WIDTH = 320;
const uint16_t DISPLAY_HEIGHT = 240;

// Tine layout constants
const uint16_t TINE_WIDTH = 16;
const uint16_t TINE_GAP = 2;
const uint16_t TINE_PITCH = TINE_WIDTH + TINE_GAP;  // 18
const uint16_t TINE_START_X = (DISPLAY_WIDTH - (17 * TINE_PITCH - TINE_GAP)) / 2;  // 8
const uint16_t BRIDGE_Y = 22;
const uint16_t TINE_CENTER_HEIGHT = 150;
const uint16_t TINE_EDGE_STEP = 8;  // height decrease per step from center

// Colors
const uint16_t COL_WOOD = 0x4A08;       // dark brown: color565(75, 65, 8) - actual: ~(74,64,64)
const uint16_t COL_WOOD_LIGHT = 0x6B49;  // lighter brown
const uint16_t COL_TINE = 0xC618;        // silver: color565(192,192,192)
const uint16_t COL_TINE_EDGE = 0x9CD3;   // darker silver edge
const uint16_t COL_BRIDGE = 0xFFFF;      // white
const uint16_t COL_PRESSED = 0xFFE0;     // yellow
const uint16_t COL_HOLE = 0x2104;        // very dark brown

// Note names for display
const char* note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Get tine height for a given physical position (0-16, center=8)
uint16_t getTineHeight(uint8_t pos) {
    int dist = pos - 8;
    if (dist < 0) dist = -dist;
    return TINE_CENTER_HEIGHT - dist * TINE_EDGE_STEP;
}

// Get tine X position for physical position
uint16_t getTineX(uint8_t pos) {
    return TINE_START_X + pos * TINE_PITCH;
}

void setup() {
    M5.begin();

    // for SD-Updater
    checkSDUpdater( SD, MENU_BIN, 2000, TFCARD_CS_PIN );

    Serial.begin(115200);

    // Initialize UNIT-SYNTH on Serial2 with pins 33, 32
    synth.begin(&Serial2, UNIT_SYNTH_BAUD, 33, 32);

    // Set instrument to Celesta (kalimba-like sound)
    synth.setInstrument(0, 0, Celesta);

    Serial.println("M5Core2 Kalimba Emulator Started");

    drawKalimba();
}

void loop() {
    M5.update();

    // Determine semitone offset from buttons
    // BtnA (left) = flat (-1), BtnC (right) = sharp (+1)
    int8_t semitone_offset = 0;
    if (M5.BtnA.isPressed()) semitone_offset = -1;
    if (M5.BtnC.isPressed()) semitone_offset = 1;

    // Show current mode indicator
    drawModeIndicator(semitone_offset);

    if (M5.Touch.changed) {
        if (M5.Touch.points > 0) {
            TouchPoint_t tp = M5.Touch.point[0];
            int pos = getKeyAtPosition(tp.x, tp.y);

            if (pos >= 0 && !key_pressed[pos]) {
                // Release any previously active note first
                if (active_tine >= 0 && active_tine != pos) {
                    key_pressed[active_tine] = false;
                    synth.setNoteOff(0, active_note_pitch, 0);
                    drawTine(active_tine, false);
                }
                key_pressed[pos] = true;
                active_tine = pos;
                active_note_pitch = tine_notes[pos] + semitone_offset;
                synth.setNoteOn(0, active_note_pitch, 100);
                drawTine(pos, true);
            }
        }
    }

    // Check for touch release
    if (M5.Touch.points == 0) {
        for (int i = 0; i < 17; i++) {
            if (key_pressed[i]) {
                key_pressed[i] = false;
                synth.setNoteOff(0, active_note_pitch, 0);
                drawTine(i, false);
            }
        }
        active_tine = -1;
        active_note_pitch = 0;
    }

    delay(10);
}

/**
 * Draw mode indicator showing flat/natural/sharp state
 */
void drawModeIndicator(int8_t offset) {
    static int8_t prev_offset = 0;
    if (offset == prev_offset) return;
    prev_offset = offset;

    // Draw indicator area at top-right
    M5.Lcd.fillRect(DISPLAY_WIDTH - 30, 2, 28, 16, COL_WOOD);
    M5.Lcd.setTextSize(2);
    if (offset == -1) {
        M5.Lcd.setTextColor(0x07FF);  // cyan
        M5.Lcd.setCursor(DISPLAY_WIDTH - 24, 3);
        M5.Lcd.print("b");
    } else if (offset == 1) {
        M5.Lcd.setTextColor(0xF800);  // red
        M5.Lcd.setCursor(DISPLAY_WIDTH - 24, 3);
        M5.Lcd.print("#");
    }
}

/**
 * Find which tine is at the given touch position
 * Returns physical position (0-16) or -1
 */
int getKeyAtPosition(uint16_t x, uint16_t y) {
    if (y < BRIDGE_Y) return -1;

    for (uint8_t pos = 0; pos < 17; pos++) {
        uint16_t tx = getTineX(pos);
        uint16_t th = getTineHeight(pos);
        uint16_t ty_bottom = BRIDGE_Y + th;

        if (x >= tx && x < tx + TINE_WIDTH && y >= BRIDGE_Y && y <= ty_bottom) {
            return pos;
        }
    }
    return -1;
}

/**
 * Draw a single tine at physical position pos (0-16)
 */
void drawTine(uint8_t pos, bool pressed) {
    uint16_t x = getTineX(pos);
    uint16_t h = getTineHeight(pos);
    uint16_t color = pressed ? COL_PRESSED : COL_TINE;
    uint16_t edge_color = pressed ? 0xF700 : COL_TINE_EDGE;

    // Draw tine body
    M5.Lcd.fillRect(x, BRIDGE_Y, TINE_WIDTH, h, color);

    // Draw edges (left and right borders for 3D look)
    M5.Lcd.drawFastVLine(x, BRIDGE_Y, h, edge_color);
    M5.Lcd.drawFastVLine(x + TINE_WIDTH - 1, BRIDGE_Y, h, edge_color);

    // Rounded tip at bottom
    M5.Lcd.fillRoundRect(x, BRIDGE_Y + h - 6, TINE_WIDTH, 6, 3, color);
    M5.Lcd.drawRoundRect(x, BRIDGE_Y + h - 6, TINE_WIDTH, 6, 3, edge_color);

    // Note name at the tip
    uint8_t note = tine_notes[pos];
    uint8_t note_num = note % 12;

    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(pressed ? BLACK : 0x4208);
    M5.Lcd.setCursor(x + 2, BRIDGE_Y + h - 14);
    M5.Lcd.print(note_names[note_num]);
}

/**
 * Draw the full kalimba interface
 */
void drawKalimba() {
    // Wood body background
    M5.Lcd.fillScreen(COL_WOOD);

    // Lighter wood grain effect
    for (int y = 40; y < DISPLAY_HEIGHT; y += 8) {
        M5.Lcd.drawFastHLine(0, y, DISPLAY_WIDTH, COL_WOOD_LIGHT);
    }

    // Sound hole (dark circle in center-bottom area)
    uint16_t hole_cx = DISPLAY_WIDTH / 2;
    uint16_t hole_cy = DISPLAY_HEIGHT - 20;
    M5.Lcd.fillCircle(hole_cx, hole_cy, 18, COL_HOLE);
    M5.Lcd.drawCircle(hole_cx, hole_cy, 18, COL_WOOD_LIGHT);
    M5.Lcd.drawCircle(hole_cx, hole_cy, 15, COL_WOOD_LIGHT);

    // Title
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(DISPLAY_WIDTH / 2 - 48, 3);
    M5.Lcd.print("KALIMBA");

    // Bridge (horizontal bar at top)
    M5.Lcd.fillRect(TINE_START_X - 4, BRIDGE_Y - 3, 17 * TINE_PITCH + 6, 5, COL_BRIDGE);

    // Draw all 17 tines
    for (uint8_t pos = 0; pos < 17; pos++) {
        drawTine(pos, false);
    }
}
