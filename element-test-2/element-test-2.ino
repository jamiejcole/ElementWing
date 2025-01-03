#include <Wire.h>
#include "Adafruit_MCP23017.h"
#include "Rotary.h"
#include "RotaryEncOverMCP.h"

// Function Declarations
void RotaryEncoderChanged(bool clockwise, int id);

// MCP23017 definitions
#define MCP_SDA 20
#define MCP_SCL 5

#define MCP_EC1_A 4
#define MCP_EC1_B 5
#define MCP_EC2_A 6
#define MCP_EC2_B 7
#define MCP_EC3_A 1
#define MCP_EC3_B 0
#define MCP_EC4_A 3
#define MCP_EC4_B 2

#define NUM_ENCODER_BUTTONS 4
const int encoderButtons[NUM_ENCODER_BUTTONS] = {9, 8, 11, 10};
bool encoderButtonState[NUM_ENCODER_BUTTONS];
unsigned long lastDebounceTime[NUM_ENCODER_BUTTONS];
const unsigned long debounceDelay = 80; // debounce delay

Adafruit_MCP23017 mcp;

RotaryEncOverMCP rotaryEncoders[] = {
  RotaryEncOverMCP(&mcp, MCP_EC1_A, MCP_EC1_B, &RotaryEncoderChanged, 1),
  RotaryEncOverMCP(&mcp, MCP_EC2_A, MCP_EC2_B, &RotaryEncoderChanged, 2),
  RotaryEncOverMCP(&mcp, MCP_EC3_A, MCP_EC3_B, &RotaryEncoderChanged, 3),
  RotaryEncOverMCP(&mcp, MCP_EC4_A, MCP_EC4_B, &RotaryEncoderChanged, 4),
};

constexpr int numEncoders = (int)(sizeof(rotaryEncoders) / sizeof(*rotaryEncoders));

// Key switch matrix definitions
const byte ROWS = 8;
const byte COLS = 13;

const char* keys[ROWS][COLS] = {
  {"displays", "address", "open_ml_controls", "cia_softkey1", "cia_softkey2", "cia_softkey3", "cia_softkey4", "cia_softkey5", "cia_softkey6", "more_softkeys", "live", "blind", "staging_mode"},
  {"macro", "help", "learn", "query", "copy_to", "recall_from", "label", "macro_1", "about", "park", "escape", "page_up", "select"},
  {"delete", "path", "effect", "gotocue", "block", "assert", "undo", "highlight", "fan_", "capture", "page_left", "page_down", "page_right"},
  {"part", "cue", "record", "+", "thru", "-", "\\", "mark", "sneak", "select_last"},
  {"intensity_palette", "focus_palette", "record_only", "7", "8", "9", "rem_dim", "+%", "home", "select_manual"},
  {"color_palette", "beam_palette", "update", "4", "5", "6", "out", "-%", "trace", "select_active"},
  {"preset", "sub", "group", "1", "2", "3", "full", "level", "cueonlytrack", "last", "", "stopback"},
  {"shift", "delay", "time", "clear", "0", ".", "at", "enter", "", "next", "", "go_0"}
};


byte rowPins[ROWS] = {17, 18, 19, 21, 22, 23, 24, 25};
byte colPins[COLS] = {3, 4, 27, 28, 7, 8, 9, 10, 12, 11, 13, 14, 16};

bool keyState[ROWS][COLS];

void RotaryEncoderChanged(bool clockwise, int id) {
  Serial.println(
    "Encoder " + String(id) + ": "
    + (clockwise ? String("clockwise") : String("counter-clock-wise"))
  );
  // sendEncoderMovement(currentPage, id - 1, (clockwise ? -TICK_AMOUNT : TICK_AMOUNT));
}

void pollAllEncoders() {
  uint16_t gpioAB = mcp.readGPIOAB();
  for (int i = 0; i < numEncoders; i++) {
    rotaryEncoders[i].feedInput(gpioAB);
  }
}

void pollEncoderButtons() {
  for (int i = 0; i < NUM_ENCODER_BUTTONS; i++) {
    bool isPressed = !mcp.digitalRead(encoderButtons[i]);
    unsigned long currentTime = millis();

    // Check for debounce
    if (isPressed != encoderButtonState[i] && (currentTime - lastDebounceTime[i] > debounceDelay)) {
      lastDebounceTime[i] = currentTime; // Update debounce timer

      if (isPressed) {
        Serial.print("Encoder Button ");
        Serial.print(i + 1);
        Serial.println(" pressed");
      }
      encoderButtonState[i] = isPressed;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Manual Matrix Scan Test");

  Wire.setSDA(MCP_SDA);
  Wire.setSCL(MCP_SCL);
  Wire.begin();
  Wire.setClock(400000);
  mcp.begin();

  // Initalize encoders
  for (int i = 0; i < numEncoders; i++) {
    rotaryEncoders[i].init();
  }

  // Initialize row pins to be pulled up
  for (int i = 0; i < ROWS; i++) {
    pinMode(rowPins[i], INPUT_PULLUP);
  }

  // Initialize encoder button pins with pull-ups
  for (int i = 0; i < NUM_ENCODER_BUTTONS; i++) {
    mcp.pinMode(encoderButtons[i], INPUT);
    mcp.pullUp(encoderButtons[i], HIGH);
    encoderButtonState[i] = false;
    lastDebounceTime[i] = 0;
  }

  // Initialize all key states to "released"
  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLS; col++) {
      keyState[row][col] = false;
    }
  }
}

void loop() {
  pollAllEncoders();
  pollEncoderButtons();

  for (int row = 0; row < ROWS; row++) {
    // Activate the current row
    pinMode(rowPins[row], OUTPUT);
    digitalWrite(rowPins[row], LOW);

    for (int col = 0; col < COLS; col++) {
      // Set column pins as inputs with pullups
      pinMode(colPins[col], INPUT_PULLUP);

      // Read the state of the key
      bool isPressed = (digitalRead(colPins[col]) == LOW);

      // Check for state changes
      if (isPressed && !keyState[row][col]) {
        // Key just pressed
        keyState[row][col] = true;
        Serial.print("Key ");
        Serial.print(keys[row][col]);
        Serial.print(" pressed at row ");
        Serial.print(row);
        Serial.print(", column ");
        Serial.println(col);
      } else if (!isPressed && keyState[row][col]) {
        // Key just released
        keyState[row][col] = false;
        Serial.print("Key ");
        Serial.print(keys[row][col]);
        Serial.print(" released at row ");
        Serial.print(row);
        Serial.print(", column ");
        Serial.println(col);
      }
    }

    // Deactivate the current row
    pinMode(rowPins[row], INPUT_PULLUP);
  }

  // delay(10); // Small delay for stability
}
