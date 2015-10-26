/*
 * Arduino library that serves as the driver for GhostLab42's Reboot
 * dual-display board set
 *
 * See README.md and LICENSE for more information
 */

#include <Arduino.h>
#include <Wire.h>
#include "GhostLab42Reboot.h"

// Each I2C has a unique bus address
#define IS31FL3730_DIGIT_4_I2C_ADDRESS 0x63 // 4 digit IS31FL3730 display
#define IS31FL3730_DIGIT_6_I2C_ADDRESS 0x60 // 6 digit IS31FL3730 display

// "Matrix 1 Data Register" index in the IS31FL3730
// 8 bit value to define which segments are lit. 0 == off, 1 == on
// This is the starting index.  Sequential bytes will go to the next
// register index.
// Works great for data, but stop after the matrix data is updated and then
// end the command.
const byte IS31FL3730_Data_Registers = 0x01;

// "Update Column Register" index in the IS31FL3730
// The data sent to the Data Registers will be stored in temporary registers
// A write operation of any 8-bit value to the Update Column Register is
// required to update the Data Registers (01h~0Bh, 0Eh~18h)
const byte IS31FL3730_Update_Column_Register = 0x0C;

// "Lighting Effect Register" index in the IS31FL3730
const byte IS31FL3730_Lighting_Effect_Register = 0x0D;

// "PWM Register" index in the IS31FL3730
// The PWM Register can modulate LED light with 128 different items --
// When the D7 set to “1”, the PWM is the 128 item
// When the D7 set to “0”, D6:D0 set the PWM from the 0 item to the 127 item
const byte IS31FL3730_PWM_Register = 0x19;

// "Reset Register" index in the IS31FL3730
// Once user writes any 8-bit data to the Reset Register, IS31FL3730 will reset
// all registers to default value
// On  initial power-up, the IS31FL3730 registers are reset to their default
// values for a blank display.
const byte IS31FL3730_Reset_Register = 0xFF;

/******************************************************************************
 *                                Constructor                                 *
 ******************************************************************************/

GhostLab42Reboot::GhostLab42Reboot(){}

/*
 * Acts as the Constructor
 *
 * Would have liked to just use the constructor, but you can't call
 * Wire.begin there :-/
 */
void GhostLab42Reboot::begin()
{
    Wire.begin();

    // Set the maximum display power for both displays
    setDisplayPowerMax(4);
    setDisplayPowerMax(6);
}

/******************************************************************************
 *                              Public Functions                              *
 ******************************************************************************/

/*
 * Writes the characters to the selected display. The only characters allowed
 * are numbers 0-9 and letters A, b, C, d, E, and F
 */
void GhostLab42Reboot::write(int digits, char value[])
{
  // User can technically give us any digit, so we have to do a nice
  // conversion with that data so that we can use it for proper iteration
  if (digits != 4) digits = 6;

  // Write the display data in the temporary registers
  setupWireTransmission(digits);
  Wire.write(IS31FL3730_Data_Registers);

  // Iterate over the print value and print out the individual characters
  // Any string that goes over the number of digits gets cut off
  // Any string that goes under the number of digits has blank spaces in
  // in the remaining spots
  for (int i = 0; i < digits; i++)
  {
    Wire.write(charToDisplayByte(value[i]));
  }

  // End the temporary register transmission
  Wire.endTransmission();

  // Transfer the display data from the temporary registers to the display
  setupWireTransmission(digits);

  // Write to the Update Column Register to let the board know we want to
  // update the display
  Wire.write(IS31FL3730_Update_Column_Register);

  // Send any value to initate the display (value ignored)
  Wire.write(0x00);

  // End the Updte Column Register Transmission
  Wire.endTransmission();
}

/*
 * Writes the characters to the selected display. The only characters allowed
 * are numbers 0-9 and letters A, b, C, d, E, and F
 */
void GhostLab42Reboot::writeRandom(int digits)
{
  // User can technically give us any digit, so we have to do a nice
  // conversion with that data so that we can use it for proper iteration
  if (digits != 4) digits = 6;

  char randomString[digits];
  getRandomString(digits, randomString);
  write(digits, randomString);
}


/*
 * Resets the display and sets the current to the maximum (5mA per segment)
 *
 * Parameters:
 * digits Selects the display to set the current, 4 = 4 digit display and all
 *        others = 6 digit display
 */
void GhostLab42Reboot::resetDisplay(int digits)
{
  setupWireTransmission(digits);

  // Reset the lighting effects (dim, bright, etc.)
  // Send any value to reset the lighting (value ignored)
  Wire.write(IS31FL3730_Reset_Register);
  Wire.write(0x00);
  Wire.endTransmission();

  // Reset the current
  setDisplayPowerMax(digits);
}

/**
 * Dims the display
 *
 * Parameters:
 * digits     Selects the display to set the current, 4 = 4 digit display and all
 *            others = 6 digit display
 * brightness The dimming level percentage as an int 0 - 100.
 */
void GhostLab42Reboot::setDisplayBrightness (int digits, int brightness)
{
  // Setup brightness variable
  byte PWM;

  // Convert brightness percent to PWM (0x80 is max, 128 settings total)
  if (brightness <= 0)
  {
    // User gave us a setting of less than or equal to 0%, just give them 0%
    PWM = 0x00;
  }
  else if (brightness >= 100)
  {
    // User gave us a setting of more than or equal to 100%, just give them 100%
    PWM = 0x80;
  }
  else
  {
    // User gave us something that we actually have to convert
    // Have to include a cast to float, otherwise the whole thing gets
    // converted to 0.00
    float pwmPercent = ((float)brightness / 100) *  128;
    PWM = pwmPercent; // Convert the float to byte

    // Just another check to make sure the PWM doesn't exceed the max allowed
    // A higher PWM could be very bad
    if (PWM > 0x80) PWM = 0x80;
    else if (PWM < 0x00) PWM = 0x00;
  }

  // Begin dimming the display
  setupWireTransmission(digits);
  Wire.write(IS31FL3730_PWM_Register); // Lighting Effect Register
  Wire.write(PWM); // write the value that was requested
  Wire.endTransmission();
}

/******************************************************************************
 *                             Private Functions                              *
 ******************************************************************************/

/*
 * Sets the current to the minimum (5mA per segment)
 *
 * Parameters:
 * digits Selects the display to set the current, 4 = 4 digit display and all
 *        others = 6 digit display
 */
void GhostLab42Reboot::setDisplayPowerMin(int digits)
{
  setupWireTransmission(digits);

  Wire.write(IS31FL3730_Lighting_Effect_Register);
  Wire.write(0x08); // Lowest level, 10mA
  Wire.endTransmission();
}

/*
 * Sets the current to the maximum allowed for these displays (0mA per segment)
 *
 * Parameters:
 * digits Selects the display to set the current, 4 = 4 digit display and all
 *        others = 6 digit display
 */
void GhostLab42Reboot::setDisplayPowerMax(int digits)
{
  // The display driver does allow currents greater than the displays should
  // take - do not allow anything over 20mA!
  // The display driver resets to 40mA per segment which is too much.  Somehow
  // we need to make sure that is never violated but the part is write-only, so
  // we need to keep track of the setting somewhere or just always force to the
  // max current.

  setupWireTransmission(digits);

  Wire.write(IS31FL3730_Lighting_Effect_Register);
  Wire.write(0x0B); // Highest level, 20mA
  Wire.endTransmission();
}

/*
 * Set up the Wire transmission depending on the display being used
 *
 * Parameters:
 * digits Selects the display to set the current, 4 = 4 digit display and all
 *        others = 6 digit display
 */
void GhostLab42Reboot::setupWireTransmission(int digits)
{
  if (digits == 4)
  {
    // Use the four digit display
    Wire.beginTransmission(IS31FL3730_DIGIT_4_I2C_ADDRESS);
  }
  else
  {
    // Use the six digit display
    Wire.beginTransmission(IS31FL3730_DIGIT_6_I2C_ADDRESS);
  }
}

/*
 * Gets a random number in the form of a character array for use on the
 * display
 *
 * Parameters:
 * digits       Selects the display to set the current, 4 = 4 digit display and
 *              all others = 6 digit display
 * randomString String pointer where the random integer will be displayed
 *
 */
void GhostLab42Reboot::getRandomString(int digits, char randomString[])
{
  String str = String(random(100000, 999999));
  str.toCharArray(randomString, digits + 1);
}

/*
 * Converts characters into the appropriate bytes for display (gfedcba format)
 *
 * Parameters:
 * displayCharacter The character to be converted into a byte for the display
 */
byte GhostLab42Reboot::charToDisplayByte(char displayCharacter)
{
  // Although this isn't very time efficient (linear time) it's apparently a
  // very bad idea to use hashmaps on Arduino because they're resource
  // intensive :-(

  // Numbers
  if      (displayCharacter == '0') return 0x3F;
  else if (displayCharacter == '1') return 0x06;
  else if (displayCharacter == '2') return 0x5B;
  else if (displayCharacter == '3') return 0x4F;
  else if (displayCharacter == '4') return 0x66;
  else if (displayCharacter == '5') return 0x6D;
  else if (displayCharacter == '6') return 0x7D;
  else if (displayCharacter == '7') return 0x07;
  else if (displayCharacter == '8') return 0x7F;
  else if (displayCharacter == '9') return 0x6F;

  // Lettering is incredibly hard because there isn't a good way to do
  // many letters (k, w, v, x, etc) and because of case-sensitivity.
  // Will need to do some thinking on how this should be accomplished
  // in the future

  // Anything else turns into a blank for that character space
  else { return NULL; }
}
