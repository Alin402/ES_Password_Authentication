#include <Wire.h>
#include <string.h>

// For LCD control
// This is the I2C address of the LCD display
#define LCD_ADDRESS 0x27
// Bit for the backlight control
#define BACKLIGHT 0x08
// Enable bit
#define ENABLE 0x04
// Read/Write Bit
#define READ_WRITE 0x02
// Register select bit
#define REGISTER_SELECT 0x01
// Clear display
#define CLEAR_DISPLAY 0x01

// For LEDs control
// The 7th pin on the B port
#define GREEN_LED_EN  0x80
// The 5th pin on the B port 
#define RED_LED_EN  0x20

// Rows and Columns of the matrix keypad
const byte ROW_COUNT = 4; 
const byte COL_COUNT = 4; 
// Mapping a row and column combination to a character to display
char hexaKeys[ROW_COUNT][COL_COUNT] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Rows are assigned to pins 9, 8, 7, 6
byte rows[] = { DDH6, DDH5, DDH4, DDH3 };
// Columns are assigned to bits 5, 4, 3, 2
byte columns[] = { DDE3, DDG5, DDE5, DDE4 };

// This keeps track of the number of characters currently inputted
// in order to add characters to the inputted string buffer
// and to set the cursor of the display accordingly
unsigned k = 0;
// This keeps track if the device is locked or unlocked
unsigned locked_state = 1;
// This keeps of the number of tries the user has left to input the correct password
unsigned no_of_tries = 3;
// Buffer to check the inputted password by the user 
char inputtedPassword[17];

// The correct password 
const char* password = "2586A";

// Generates a pulse on the enable pin of the LCD
// in order to latch to the lcd
void pulseEnable(uint8_t data) {
  // Initiate communication with the I2C device
  // located at the specified address
  Wire.beginTransmission(LCD_ADDRESS);
  // Sets the enable bit high along with the backlight
  // and the provided data
  Wire.write(data | ENABLE | BACKLIGHT);
  // Ends the transmission
  Wire.endTransmission();
  // Small delay
  delayMicroseconds(1);

  Wire.beginTransmission(LCD_ADDRESS);
  Wire.write((data & ~ENABLE) | BACKLIGHT);
  Wire.endTransmission();
  delayMicroseconds(50);
}

// Used to write a 4 bit nibble to the lcd
void write4Bits(uint8_t data) {
  // Initiates the communication
  Wire.beginTransmission(LCD_ADDRESS);
  // Writes the data to the lcd and ensures that the backlight is on
  Wire.write(data | BACKLIGHT);
  // Ends communication
  Wire.endTransmission();
  // A pulse is sent in order to latch the nibble to the lcd
  pulseEnable(data);
}

// Sends an 8 bit value to the lcd
// It send the data and a command
// The command can either be REGISTER_SELECT for data or 0 for commands
void send(uint8_t value, uint8_t mode) {
  // The data is split into the first 4 bits and the last 4 bits 
  uint8_t highNibble = value & 0xF0;
  uint8_t lowNibble = (value << 4) & 0xF0;

  // Write the 2 nibbles
  write4Bits(highNibble | mode);
  write4Bits(lowNibble | mode);
}

void lcdCommand(uint8_t command) {
  // Sets the mode to 0 and sends a command
  send(command, 0);
}

void lcdData(uint8_t data) {
  // Sets the mode to REGISTER SELECT and sends data to the lcd
  send(data, REGISTER_SELECT);
}

void lcdInit() {
  // Initiate I2C communication
  Wire.begin();
  // Delay ensures that the display has enough time to power up properly
  delay(50);
  // Initial command to put the display into 8 bit mode
  write4Bits(0x30);
  // Delay to let the lcd process the command properly
  delay(5);
  // As recommended in the data sheet, another command to ensure 
  // that the display is in 8 bit mode
  write4Bits(0x30);
  delayMicroseconds(150);
  // And yet another one
  write4Bits(0x30);
  delayMicroseconds(150);
  // Finally we put the display into 4 bit mode
  // Because this is how the commands are going to be sent
  // To put the display in 8 bit mode before switching to 4 bit
  // is a procedure recommended in the datasheet
  // To establish reliable communication with the lcd
  write4Bits(0x20);
  delayMicroseconds(150);
    
  // Function set: 4 bit, number of display line: 1 line, 5x8 dots
  lcdCommand(0x28);
  // Display control: display off, cursor off, blink off
  lcdCommand(0x08);
  // Clear the display
  lcdCommand(CLEAR_DISPLAY);
  // Delay to ensure the commands are processed properly
  delay(2);
  // Entry mode set: increment cursor, no shift
  lcdCommand(0x06);
  // Display control: display on, cursor off, blink off
  lcdCommand(0x0C);
}

// Used to set the cursor to a row and column
void lcdSetCursor(uint8_t col, uint8_t row) {
    const uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    if (row > 1) row = 1; 
    lcdCommand(0x80 | (col + row_offsets[row]));
}

void lcdPrintString(const char* str) {
  // Iterates through each character of the string
  // and sends it to the display
  while (*str) {
    lcdData(*str++);
  }
}

void lcdPrintCharacter(char ch) {
  // Sends one character to the lcd
  lcdData(ch);
}

void initialize_leds() {
  // Set the data direction for the led and green LEDs as output
  DDRB |= (GREEN_LED_EN | RED_LED_EN);

  PORTB &= ~(GREEN_LED_EN);
  PORTB |= (RED_LED_EN);
}

void initialize_keypad() {
    // Set each row pin as input with pull-up
    for (int i = 0; i < ROW_COUNT; i++) {
        DDRH &= ~(1 << rows[i]);
        PORTH |= (1 << rows[i]);
    }
    // Set each column pin as input with pull-up initially
    for (int i = 0; i < COL_COUNT; i++) {
      // There are two cases because one of the column pins is on a different PORT
        if (i == 1) {
            DDRG &= ~(1 << columns[i]);
            PORTG |= (1 << columns[i]);
        } else {
            DDRE &= ~(1 << columns[i]);
            PORTE |= (1 << columns[i]);
        }
    }
}

void initialize_lcd(const char *initialMessage) {
  lcdInit();
  lcdSetCursor(0, 0);
  lcdPrintString(initialMessage);
}

// Sends command to clear the lcd display
void clearLcdDisplay() {
  lcdCommand(CLEAR_DISPLAY);
}

void setup() {
  // Initialize serial monitor for debugging purposes
	Serial.begin(9600);
  // Initialize LEDs
  initialize_leds();
  // Initialize keypad
  initialize_keypad();
  // Initialize LCD
  initialize_lcd("Enter password..");
}

void read_matrix_keypad_key() {
    // Iterate through the columns
    for (int colIndex = 0; colIndex < COL_COUNT && locked_state; colIndex++) {
        byte curCol = columns[colIndex];

        // Set the current column to LOW
        // There are 2 cases, because one of the column pins is from a different PORT
        if (colIndex == 1) {
            // Set as output
            DDRG |= (1 << curCol);
            // Set to LOW
            PORTG &= ~(1 << curCol);
        } else {
            // Set as output
            DDRE |= (1 << curCol);
            // Set to LOW
            PORTE &= ~(1 << curCol);
        }

        // Iterate through the rows
        for (int rowIndex = 0; rowIndex < ROW_COUNT && locked_state; rowIndex++) {
            byte curRow = rows[rowIndex];
            // Read the current row
            // If the row pin is set on low, means that the corresponding button is pressed
            if (!(PINH & (1 << curRow))) {
                char keyChar = hexaKeys[rowIndex][colIndex];
                if (keyChar == 'D') {
                    // If the users presses the 'D' key, the contents of the display are cleared
                    // and the display is re-initialized
                    clearLcdDisplay();
                    // Initial message on the lcd
                    initialize_lcd("Enter password..");
                    k = 0;
                } else if (keyChar == '#') {
                    // If the user presses # key, the password is being checked
                    if (strcmp(inputtedPassword, password) == 0) {
                      // If the password, matches we clear the display
                      // light up the green led and set the board into unlocked state
                      clearLcdDisplay();
                      initialize_lcd("Good password!");
                      PORTB &= ~(RED_LED_EN);
                      PORTB |= (GREEN_LED_EN);
                      locked_state = 0;
                      delay(300);
                      break;
                    }
                    else {
                      // Is the password is not correct, substract one try from the tries counter
                      no_of_tries--;
                      Serial.println(no_of_tries);
                      // If there are no tries left, the user has to wait 10s to try again
                      if (no_of_tries == 0) {
                        initialize_lcd("10s to retry");
                        delay(1000);
                        wait_n_seconds(10);
                        no_of_tries = 3;
                      } else {
                        // If the try number is not 0, the the user can try again
                        // And the number of tries remaining is being displayed
                        char buffer[20];
                        snprintf(buffer, sizeof(buffer), "%d tries left", no_of_tries);
                        clearLcdDisplay();
                        initialize_lcd(buffer);
                      }
                      // Clear the inputted password, and set the cursor count on 0
                      memset(inputtedPassword, '\0', sizeof(inputtedPassword));
                      k = 0;
                      delay(500);
                    }
                } else {
                  // Add character to password buffer
                  // And set the cursor accordingly
                  lcdSetCursor(k, 1);
                  lcdPrintCharacter(keyChar);
                  inputtedPassword[k] = keyChar;
                  delay(300);
                  k++;
                }
            }
        }
        // Reinitialize current column
        // There are two cases because one of the column pins is set on a different port
        if (colIndex == 1) {
            DDRG &= ~(1 << curCol);  // Set as input
            PORTG |= (1 << curCol);  // Enable pull-up resistor
        } else {
            DDRE &= ~(1 << curCol);  // Set as input
            PORTE |= (1 << curCol);  // Enable pull-up resistor
        }
    }
}

// Used to wait n seconds and display the progress on the lcd
void wait_n_seconds(unsigned n) {
    for (int i = n; i >= 0; i--) {
      // Delay one second at each iteration
      delay(1000);
      char buffer[20];
      // Concatenate string with seconds remaining to display on the lcd
      snprintf(buffer, sizeof(buffer), "Wait %d seconds", i);
      // Clear the lcd and reinitialize it with current progress
      clearLcdDisplay();
      initialize_lcd(buffer);
    }
    // Reinitialize the display and let the user input the password again
    clearLcdDisplay();
    initialize_lcd("Enter password..");
}

void loop() {
  // Read keys and check password loop, runs when in locked state
  if (locked_state) {
    read_matrix_keypad_key();
  }
}