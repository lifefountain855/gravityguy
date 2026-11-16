#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Matches the 16x2 I2C LCD and button wiring used in byteClicker2.
// blac-grd wh-5v, pur-a5, gry-a4
LiquidCrystal_I2C lcd(0x27, 16, 2);

const uint8_t BUTTON_PIN = 11;
const uint8_t PLAYER_COLUMN = 5;
const uint8_t PLAYER_TOP_GLYPH = 0;
const uint8_t PLAYER_BOTTOM_GLYPH = 1;
const uint8_t BOTTOM_SPIKE_GLYPH = 2;
const uint8_t TOP_SPIKE_GLYPH = 3;
const uint8_t LCD_COLUMNS = 16;
const unsigned long DEBOUNCE_MS = 30;
const unsigned long HIT_FLASH_MS = 500;
const unsigned long SCORE_INTERVAL_MS = 3000;
const unsigned long START_MOVE_INTERVAL_MS = 300;
const unsigned long MIN_MOVE_INTERVAL_MS = 92;
const unsigned long SPEEDUP_PER_SCORE_MS = 5;
const unsigned long MIN_SPAWN_GAP_MS = 250;
const unsigned long MAX_SPAWN_GAP_MS = 1300;
const unsigned long RENDER_INTERVAL_MS = 50;
const unsigned long SECOND_OBSTACLE_MIN_DELAY_MS = 500;
const unsigned long SECOND_OBSTACLE_MAX_DELAY_MS = 1000;
const uint8_t SECOND_OBSTACLE_LEVEL = 10;
const uint8_t SECOND_OBSTACLE_CHANCE_DENOMINATOR = 3;
const uint8_t MAX_OBSTACLES = 2;

enum Lane {
  TOP_LANE,
  BOTTOM_LANE
};

struct Obstacle {
  bool active;
  bool pending;
  Lane lane;
  int8_t column;
  unsigned long lastMoveMs;
  unsigned long spawnAtMs;
};

byte playerIconUpsideDown[8] = {
  B10001,
  B01010,
  B00100,
  B10101,
  B01110,
  B00100,
  B01110,
  B00100
};

byte playerIcon[8] = {
  B00100,
  B01110,
  B00100,
  B01110,
  B10101,
  B00100,
  B01010,
  B10001
};

byte obstacleIcon[8] = {
  B00000,
  B00100,
  B01110,
  B01110,
  B11111,
  B11111,
  B01110,
  B00000
};

byte topSpikeIcon[8] = {
  B00000,
  B01110,
  B11111,
  B11111,
  B01110,
  B01110,
  B00100,
  B00000
};

unsigned long score = 0;
unsigned long highScore = 0;
unsigned long lastButtonChangeMs = 0;
unsigned long lastMoveMs = 0;
unsigned long lastRenderMs = 0;
unsigned long lastScoreTickMs = 0;
unsigned long hitFlashStartedMs = 0;
unsigned long lastObstacleEndedMs = 0;
unsigned long spawnGapMs = 0;
bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
bool playerOnTop = false;
bool obstacleWaveActive = false;
bool showHitFlash = false;
Obstacle obstacles[MAX_OBSTACLES] = {};

unsigned long moveIntervalMs() {
  const unsigned long speedupLimit =
      START_MOVE_INTERVAL_MS - MIN_MOVE_INTERVAL_MS;
  const unsigned long scoresToMinimum =
      (speedupLimit + SPEEDUP_PER_SCORE_MS - 1) / SPEEDUP_PER_SCORE_MS;
  if (score >= scoresToMinimum) {
    return MIN_MOVE_INTERVAL_MS;
  }
  return START_MOVE_INTERVAL_MS - score * SPEEDUP_PER_SCORE_MS;
}

void readButton(unsigned long now) {
  const bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastButtonReading = reading;
    lastButtonChangeMs = now;
  }

  if (now - lastButtonChangeMs >= DEBOUNCE_MS &&
      reading != stableButtonState) {
    stableButtonState = reading;
    if (stableButtonState == LOW) {
      playerOnTop = !playerOnTop;
    }
  }
}

void scheduleNextWave(unsigned long now) {
  obstacleWaveActive = false;
  for (uint8_t i = 0; i < MAX_OBSTACLES; ++i) {
    obstacles[i].active = false;
    obstacles[i].pending = false;
  }
  lastObstacleEndedMs = now;
  spawnGapMs = random(MIN_SPAWN_GAP_MS, MAX_SPAWN_GAP_MS);
}

Lane randomLane() {
  return random(0, 2) == 0 ? TOP_LANE : BOTTOM_LANE;
}

void activateObstacle(uint8_t index, Lane lane, unsigned long now) {
  obstacles[index].active = true;
  obstacles[index].pending = false;
  obstacles[index].lane = lane;
  obstacles[index].column = LCD_COLUMNS - 1;
  obstacles[index].lastMoveMs = now;
}

void startObstacleWave(unsigned long now) {
  for (uint8_t i = 0; i < MAX_OBSTACLES; ++i) {
    obstacles[i].active = false;
    obstacles[i].pending = false;
  }

  obstacleWaveActive = true;
  activateObstacle(0, randomLane(), now);

  if (score > SECOND_OBSTACLE_LEVEL &&
      random(0, SECOND_OBSTACLE_CHANCE_DENOMINATOR) == 0) {
    obstacles[1].pending = true;
    obstacles[1].lane = randomLane();
    obstacles[1].spawnAtMs =
        now + random(SECOND_OBSTACLE_MIN_DELAY_MS,
                     SECOND_OBSTACLE_MAX_DELAY_MS);
  }
}

bool playerCollidesWith(const Obstacle& obstacle) {
  return (obstacle.lane == TOP_LANE && playerOnTop) ||
         (obstacle.lane == BOTTOM_LANE && !playerOnTop);
}

void handleCollision(unsigned long now) {
  score = 0;
  lastScoreTickMs = now;
  showHitFlash = true;
  hitFlashStartedMs = now;
  scheduleNextWave(now);
}

void updateObstacleWave(unsigned long now) {
  if (!obstacleWaveActive) {
    if (now - lastObstacleEndedMs >= spawnGapMs) {
      startObstacleWave(now);
    }
    return;
  }

  for (uint8_t i = 0; i < MAX_OBSTACLES; ++i) {
    Obstacle& obstacle = obstacles[i];
    if (obstacle.pending) {
      if (static_cast<long>(now - obstacle.spawnAtMs) >= 0) {
        activateObstacle(i, obstacle.lane, now);
      } else {
        continue;
      }
    }
    if (!obstacle.active) {
      continue;
    }

    if (now - obstacle.lastMoveMs < moveIntervalMs()) {
      continue;
    }
    obstacle.lastMoveMs = now;
    --obstacle.column;

    if (obstacle.column == PLAYER_COLUMN &&
        playerCollidesWith(obstacle)) {
      handleCollision(now);
      return;
    }
    if (obstacle.column < 0) {
      obstacle.active = false;
    }
  }

  for (uint8_t i = 0; i < MAX_OBSTACLES; ++i) {
    if (obstacles[i].active || obstacles[i].pending) {
      return;
    }
  }
  scheduleNextWave(now);
}

void updateGame(unsigned long now) {
  const unsigned long elapsedScoreTime = now - lastScoreTickMs;
  if (elapsedScoreTime >= SCORE_INTERVAL_MS) {
    const unsigned long pointsEarned = elapsedScoreTime / SCORE_INTERVAL_MS;
    score += pointsEarned;
    lastScoreTickMs += pointsEarned * SCORE_INTERVAL_MS;
    if (score > highScore) {
      highScore = score;
    }
  }

  updateObstacleWave(now);
}

void putNumberOnRow(byte row[LCD_COLUMNS], unsigned long value,
                    uint8_t firstColumn) {
  char digits[10];
  uint8_t digitCount = 0;

  do {
    digits[digitCount++] = '0' + (value % 10);
    value /= 10;
  } while (value > 0 && digitCount < sizeof(digits));

  for (uint8_t i = 0; i < digitCount; ++i) {
    if (firstColumn + i < LCD_COLUMNS) {
      row[firstColumn + i] = digits[digitCount - i - 1];
    }
  }
}

void drawGame(unsigned long now) {
  byte topRow[LCD_COLUMNS];
  byte bottomRow[LCD_COLUMNS];
  for (uint8_t column = 0; column < LCD_COLUMNS; ++column) {
    topRow[column] = ' ';
    bottomRow[column] = ' ';
  }

  if (showHitFlash && now - hitFlashStartedMs < HIT_FLASH_MS) {
    topRow[0] = '!';
  } else {
    showHitFlash = false;
  }

  if (playerOnTop) {
    topRow[PLAYER_COLUMN] = PLAYER_TOP_GLYPH;
  } else {
    bottomRow[PLAYER_COLUMN] = PLAYER_BOTTOM_GLYPH;
  }

  for (uint8_t i = 0; i < MAX_OBSTACLES; ++i) {
    const Obstacle& obstacle = obstacles[i];
    if (obstacle.active && obstacle.column >= 0 &&
        obstacle.column < LCD_COLUMNS) {
      if (obstacle.lane == TOP_LANE) {
        topRow[obstacle.column] = TOP_SPIKE_GLYPH;
      } else {
        bottomRow[obstacle.column] = BOTTOM_SPIKE_GLYPH;
      }
    }
  }
  bottomRow[0] = 'H';
  putNumberOnRow(topRow, score, 0);
  putNumberOnRow(bottomRow, highScore, 1);

  lcd.setCursor(0, 0);
  for (uint8_t column = 0; column < LCD_COLUMNS; ++column) {
    lcd.write(topRow[column]);
  }
  lcd.setCursor(0, 1);
  for (uint8_t column = 0; column < LCD_COLUMNS; ++column) {
    lcd.write(bottomRow[column]);
  }
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  randomSeed(analogRead(A0));
  lcd.init();
  lcd.backlight();
  lcd.createChar(PLAYER_TOP_GLYPH, playerIconUpsideDown);
  lcd.createChar(PLAYER_BOTTOM_GLYPH, playerIcon);
  lcd.createChar(BOTTOM_SPIKE_GLYPH, obstacleIcon);
  lcd.createChar(TOP_SPIKE_GLYPH, topSpikeIcon);

  const unsigned long now = millis();
  lastObstacleEndedMs = now;
  lastScoreTickMs = now;
  startObstacleWave(now);
  lastRenderMs = now - RENDER_INTERVAL_MS;
}

void loop() {
  const unsigned long now = millis();
  readButton(now);
  updateGame(now);

  if (now - lastRenderMs >= RENDER_INTERVAL_MS) {
    lastRenderMs = now;
    drawGame(now);
  }
}
