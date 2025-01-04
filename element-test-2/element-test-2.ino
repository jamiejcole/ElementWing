#include <Wire.h>
#include "Adafruit_MCP23017.h"
#include "Rotary.h"
#include "RotaryEncOverMCP.h"
#include <OSCBoards.h>
#include <OSCBundle.h>
#include <OSCData.h>
#include <OSCMatch.h>
#include <OSCMessage.h>
#include <OSCTiming.h>

// EOS OSC Serial
#ifdef BOARD_HAS_USB_SERIAL
  #include <SLIPEncodedUSBSerial.h>
    SLIPEncodedUSBSerial SLIPSerial(thisBoardsSerialUSB);
  #else
  #include <SLIPEncodedSerial.h>
    SLIPEncodedSerial SLIPSerial(Serial);
#endif

enum ConsoleType
{
  ConsoleNone,
  ConsoleEos,
};

const String HANDSHAKE_QUERY = "ETCOSC?";
const String HANDSHAKE_REPLY = "OK";

ConsoleType connectedToConsole = ConsoleNone;
unsigned long lastMessageRxTime = 0;
bool timeoutPingSent = false;

// Function Declarations
void RotaryEncoderChanged(bool clockwise, int id);
void sendOSCMessage(const String &address, float value);
void sendKeyPress(const String &key, int shiftMod = -1);

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
#define OSC_BUF_MAX_SIZE 512
#define PING_AFTER_IDLE_INTERVAL 2500
#define TIMEOUT_AFTER_IDLE_INTERVAL 5000

const int encoderButtons[NUM_ENCODER_BUTTONS] = {9, 8, 11, 10};
bool encoderButtonState[NUM_ENCODER_BUTTONS];
unsigned long lastDebounceTime[NUM_ENCODER_BUTTONS];
const unsigned long debounceDelay = 80; // debounce delay
int shiftState = 0;

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
  {"displays", "address", "open_ml_controls", "softkey_1", "softkey_2", "softkey_3", "softkey_4", "softkey_5", "softkey_6", "more_softkeys", "live", "blind", "staging_mode"},
  {"macro", "help", "learn", "query", "copy_to", "recall_from", "label", "macro_1", "about", "park", "escape", "page_up", "select"},
  {"delete", "path", "effect", "go_to_cue", "block", "assert", "undo", "highlight", "fan_", "capture", "page_left", "page_down", "page_right"},
  {"part", "cue", "record", "+", "thru", "-", "\\", "mark", "sneak", "select_last"},
  {"intensity_palette", "focus_palette", "record_only", "7", "8", "9", "rem_dim", "+%", "home", "select_manual"},
  {"color_palette", "beam_palette", "update", "4", "5", "6", "out", "-%", "trace", "select_active"},
  {"preset", "sub", "group", "1", "2", "3", "full", "level", "cueonlytrack", "last", "", "stop"},
  {"shift", "delay", "time", "clear_cmd", "0", ".", "at", "enter", "", "next", "", "go_0"}
};


byte rowPins[ROWS] = {17, 18, 19, 21, 22, 23, 24, 25};
byte colPins[COLS] = {3, 4, 27, 28, 7, 8, 9, 10, 12, 11, 13, 14, 16};

bool keyState[ROWS][COLS];

void sendOscMessage(const String &address, float value) {
  OSCMessage msg(address.c_str());
  msg.add(value);
  SLIPSerial.beginPacket();
  msg.send(SLIPSerial);
  SLIPSerial.endPacket();
}

void issueEosSubscribes() {
  // Adding a filter to only listen for pings
  OSCMessage filter("/eos/filter/add");
  // filter.add("/eos/out/param/*");
  filter.add("/eos/out/ping");

  SLIPSerial.beginPacket();
  filter.send(SLIPSerial);
  SLIPSerial.endPacket();
}

void parseOscMessage(String &msg) {
  // If it's a handshake string, reply
  if (msg.indexOf(HANDSHAKE_QUERY) != -1) {
    SLIPSerial.beginPacket();
    SLIPSerial.write((const uint8_t*)HANDSHAKE_REPLY.c_str(), (size_t)HANDSHAKE_REPLY.length());
    SLIPSerial.endPacket();

    // Subscribe to ping message
    issueEosSubscribes();
  }
}

void sendOSCMessage(const String &address, float value) {
  OSCMessage msg(address.c_str());
  msg.add(value);
  SLIPSerial.beginPacket();
  msg.send(SLIPSerial);
  SLIPSerial.endPacket();
}

void sendKeyPress(const String &key, int shiftMod) {
  String msg = "/eos/key/" + key;
  if (shiftMod == 1) {
    sendOSCMessage(msg, 1.0);
    shiftState = 1;
  }
  else if (shiftMod == 0) {
    sendOSCMessage(msg, 0.0);
    shiftState = 0;
  }
  else {
    OSCMessage keyMsg(msg.c_str());
    SLIPSerial.beginPacket();
    keyMsg.send(SLIPSerial);
    SLIPSerial.endPacket();
  }
}

void RotaryEncoderChanged(bool clockwise, int id) {
  Serial.println(
    "Encoder " + String(id) + ": "
    + (clockwise ? String("clockwise") : String("counter-clock-wise"))
  );
  // sendEncoderMovement(currentPage, id - 1, (clockwise ? -TICK_AMOUNT : TICK_AMOUNT));
}

void pollKeyMatrix() {
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
        keyState[row][col] = true;

        // Serial.print(keys[row][col]); pressed

        if (keys[row][col] == "shift") {
          sendKeyPress(keys[row][col], 1);
        }
        else {
          sendKeyPress(keys[row][col]);
        }
      } else if (!isPressed && keyState[row][col]) {
        keyState[row][col] = false;

        // Serial.print(keys[row][col]); released
        if (keys[row][col] == "shift") {
          sendKeyPress(keys[row][col], 0);
        }
      }
    }

    // Deactivate the current row
    pinMode(rowPins[row], INPUT_PULLUP);
  }
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
  // Serial.begin(115200);
  // Serial.println("Manual Matrix Scan Test");

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

  delay(500);

  SLIPSerial.begin(115200);
  while (!Serial) {
    delay(10);
    continue;
  }

  SLIPSerial.beginPacket();
  SLIPSerial.write((const uint8_t*)HANDSHAKE_REPLY.c_str(), (size_t)HANDSHAKE_REPLY.length());
  SLIPSerial.endPacket();

  issueEosSubscribes();

  delay(100);
}

void loop() {
  pollAllEncoders();
  pollEncoderButtons();
  pollKeyMatrix();

  // Checking if OSC commands have come from EOS
  static String curMsg;
  int size;
  size = SLIPSerial.available();
  if (size > 0)
  {
    while (size--)
      curMsg += (char)(SLIPSerial.read());
  }
  if (SLIPSerial.endofPacket())
  {
    parseOscMessage(curMsg);
    lastMessageRxTime = millis();
    timeoutPingSent = false;
    curMsg = String();
  }

  // Checking for timeouts
  if (lastMessageRxTime > 0)
  {
    unsigned long diff = millis() - lastMessageRxTime;
    if (diff > TIMEOUT_AFTER_IDLE_INTERVAL)
    {
      connectedToConsole = ConsoleNone;
      lastMessageRxTime = 0;
      timeoutPingSent = false;
    }

    // Send a ping after 2.5s
    if (!timeoutPingSent && diff > PING_AFTER_IDLE_INTERVAL)
    {
      OSCMessage ping("/eos/ping");
      ping.add("ElementWing_hello");
      SLIPSerial.beginPacket();
      ping.send(SLIPSerial);
      SLIPSerial.endPacket();
      timeoutPingSent = true;
    }
  }
}
