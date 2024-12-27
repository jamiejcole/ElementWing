import sys
import time
import select
from machine import Pin

rows = [Pin(pin, Pin.OUT) for pin in (3, 4, 5)]  # Set rows as outputs
cols = [Pin(pin, Pin.IN, Pin.PULL_DOWN) for pin in (7, 8, 9)]  # Set columns as inputs with pull-down resistors

# EOS
PING_INTERVAL = 2.5
TIMEOUT_INTERVAL = 5

# State tracking
lastMessageTime = 0
timeoutPingSent = False
connectedToConsole = False

# Define OSC commands for each button
button_to_osc = {
    (0, 0): "/eos/key/go",
    (0, 1): "/eos/key/stop",
    (0, 2): "/eos/key/record",
    (1, 0): "/eos/key/cue",
    (1, 1): "/eos/key/next",
    (1, 2): "/eos/key/prev",
    (2, 0): "/eos/key/clear",
    (2, 1): "/eos/key/update",
    (2, 2): "/eos/key/save",
}

# Function to send an OSC message via USB serial
def send_osc_message(address):
    osc_message = address + "\0\0\0"
    sys.stdout.write(osc_message)  # Send OSC message via USB serial
    print(f"Sent OSC: {address}")

# Scan the button matrix and return the (row, col) of pressed buttons
def scan_matrix():
    pressed = []
    for row_idx, row in enumerate(rows):
        row.value(1)  # Activate the current row
        for col_idx, col in enumerate(cols):
            if col.value():  # Check if the column is active
                pressed.append((row_idx, col_idx))  # Record the pressed button
        row.value(0)  # Deactivate the row
    return pressed

def process_message(message):
    global lastMessageTime, timeoutPingSent, connectedToConsole

    print(f"Received: {message}")  # Debug log
    if "ETCOSC?" in message:
        send_osc_message("OK")
        connectedToConsole = True
        print("Handshake completed")
    elif "/eos/out/ping" in message:
        lastMessageTime = time.time()  # Update last message time
        timeoutPingSent = False  # Reset timeout ping flag

def handle_keepalive():
    global lastMessageTime, timeoutPingSent, connectedToConsole
    
    currentTime = time.time()

    if connectedToConsole:
        # timeout
        if lastMessageTime > 0 and (currentTime - lastMessageTime) > TIMEOUT_INTERVAL:
            print("Connection timeout!")
            connectedToConsole = False
            lastMessageTime = 0
            timeoutPingSent = False
        # send ping if idle
        if not timeoutPingSent and (currentTime - lastMessageTime) > PING_INTERVAL:
            send_osc_message("/eos/ping ElementWing_hello")
            timeoutPingSent = True
    
# Main loop
print("Starting OSC Button Matrix")
send_osc_message("/eos/filter/add /eos/out/ping")

poll = select.poll()
poll.register(sys.stdin, select.POLLIN)

while True:
    pressed_buttons = scan_matrix()
    for button in pressed_buttons:
        if button in button_to_osc:
            send_osc_message(button_to_osc[button])

    # Check for incoming OSC messages
    
    if poll.poll(0):
        try:
            incomingMessage = sys.stdin.read(1)
            if incomingMessage:
                process_message(incomingMessage.strip())
        except Exception as e:
            print(f"Error: {e}")
    
    handle_keepalive()
                
    time.sleep(0.2)  # Small delay to debounce
