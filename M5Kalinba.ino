/**
 * @file kalimba.ino
 * @brief Electronic Kalimba with 17 bars for M5Stack Core2 + UNIT-Synth
 * @date 2024-12-11
 *
 * @Hardware: M5Stack Core2 + Unit Synth
 * @Libraries:
 *   - M5Core2
 *   - M5UnitSynth
 */

#include <M5Core2.h>
#include "M5UnitSynth.h"

// for SD-Updater
#define SDU_ENABLE_GZ
#include <M5StackUpdater.h>


// SYNTH RELATED
M5UnitSynth synth;

// 17音の音階（例：C4～E6）
// ユーザ指定のノート配列（元コードより）
uint8_t notes[17] = {
  NOTE_D5, NOTE_B4, NOTE_G4, NOTE_E4, NOTE_C4, NOTE_A3, NOTE_F3, NOTE_D3, NOTE_C2,
  NOTE_E3, NOTE_G3, NOTE_B3, NOTE_D4, NOTE_F4, NOTE_A4, NOTE_C5, NOTE_E5
};

// 上記noteに対応するラベル（ユーザ指定の配列を使用）
const char* noteNames[17] = {
  "D4", "B4", "G4", "E4", "C4", "A3", "F3", "D3", "C2",
  "E5", "G3", "B3", "D4", "F4", "A4", "C5", "E5"
};

// バー(鍵盤)描画関連設定
int barCount = 17;
int barWidth = 320 / barCount; // 横幅320pxを17等分
int maxHeight = 200;          // 中央バーの最大高さ
int stepHeight = 6;           // 中心から1つ外れる毎に高さを短くする分
int centerIndex = barCount / 2; // 中央インデックス(17なら8)
int barY = 20;                // 全てのバーの上端位置は一定
uint32_t barColor = TFT_WHITE;
uint32_t highlightColor = 0xFFE0; // 薄い黄色系

int selectedBar = -1; 
int lastPressedBar = -1; // 最後にタッチされていたバー
bool wasPressed = false; // 前フレームでタッチされていたかどうか

void setup() {
  M5.begin();
  Serial.begin(115200);

  // for SD-Updater
  checkSDUpdater( SD, MENU_BIN, 5000 );

  Serial.println("Unit Synth Kalimba");

  // Synth初期化 (PORTC: TX33, RX32)
  synth.begin(&Serial2, UNIT_SYNTH_BAUD, 33, 32); 
  // 音色をMusicBoxに設定（元コード通り）
  synth.setInstrument(0, 0, MusicBox);

  // 画面初期化
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, barY + maxHeight + 30);
  M5.Lcd.print("Electronic Kalimba - Press, slide and release!");

  // バー描画
  drawBars();
}

void loop() {
  M5.update();
  
  bool isPressed = M5.Touch.ispressed();

  if (isPressed) {
    // タッチ中
    TouchPoint_t touchPoint = M5.Touch.getPressPoint();
    int x = touchPoint.x;
    int y = touchPoint.y;
    int barIndex = x / barWidth;
    if (barIndex >= 0 && barIndex < barCount) {
      int barH = getBarHeight(barIndex);
      if (y >= barY && y <= barY + barH) {
        // バー上に指がある
        if (selectedBar != barIndex) {
          // ハイライトを更新(別のバーへ移動したタイミング)
          if (selectedBar >= 0 && selectedBar < barCount) {
            drawSingleBar(selectedBar, barColor);
          }
          highlightBar(barIndex);
          // スライド時にも音をならす：短い音を"はじく"
          playNoteShort(barIndex);
        }
        lastPressedBar = barIndex;
      } else {
        // バー外をタッチしている場合はハイライト解除
        if (selectedBar != -1) {
          drawSingleBar(selectedBar, barColor);
          selectedBar = -1;
        }
      }
    } else {
      // バー範囲外をタッチしている場合もハイライト解除
      if (selectedBar != -1) {
        drawSingleBar(selectedBar, barColor);
        selectedBar = -1;
      }
    }
  } else {
    // タッチが離れた瞬間
    if (wasPressed) {
      // 最後に押されていたバーがあれば、そのバーの音を鳴らす
      if (lastPressedBar != -1) {
        // 選択中だったバーを元に戻す
        if (selectedBar != -1) {
          drawSingleBar(selectedBar, barColor);
          selectedBar = -1;
        }
        // 離したときにも音を鳴らす(元コードの動作に準拠)
        playNote(lastPressedBar, true);
        // 短い遅延をおいて音をオフにしても良いが、ここではonのみ
        // もし音を切るなら、後でNoteOffを呼ぶかdelay後に呼ぶ実装を追加
        lastPressedBar = -1;
      }
    }
  }

  wasPressed = isPressed;
}

void drawBars() {
  for (int i = 0; i < barCount; i++) {
    drawSingleBar(i, barColor);
  }
}

void drawSingleBar(int index, uint32_t color) {
  int barH = getBarHeight(index);
  int x = index * barWidth;
  M5.Lcd.fillRect(x, barY, barWidth - 1, barH, color);

  // 音名を表示：バー幅中央揃え
  int textX = x + barWidth / 2;
  int textY = barY + barH - 60;
  M5.Lcd.setTextColor(TFT_BLACK, color); // バー上に表示するので背景はバー色
  M5.Lcd.setTextSize(1);
  int16_t textWidth = strlen(noteNames[index]) * 6;
  M5.Lcd.setCursor(textX - textWidth / 2, textY);
  M5.Lcd.print(noteNames[index]);
}

int getBarHeight(int index) {
  int dist = abs(index - centerIndex);
  int height = maxHeight - dist * stepHeight;
  return height;
}

void highlightBar(int index) {
  drawSingleBar(index, highlightColor);
  selectedBar = index;
}

// バー移動時に短い音を出す関数
void playNoteShort(int index) {
  if (index >= 0 && index < barCount) {
    uint8_t note = notes[index];
    synth.setNoteOn(0, note, 127);
    delay(5); // 短い音
    synth.setNoteOff(0, note, 127);
  }
}

void playNote(int index, bool onoff) {
  if (index >= 0 && index < barCount) {
    uint8_t note = notes[index];
    if(onoff == true) synth.setNoteOn(0, note, 127);
    else synth.setNoteOff(0, note, 127);
  }
}
