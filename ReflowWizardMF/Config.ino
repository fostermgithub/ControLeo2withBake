// Setup menu
// Called from the main loop
// Allows the user to configure the outputs and maximum temperature

// Called when in setup mode
// Return false to exit this mode
boolean Config() {
  static int setupPhase = 0;
  static int output = 4;    // Start with output D4
  static int type = TYPE_TOP_ELEMENT;
  static boolean drawMenu = true;
  static int maxTemperature;
  static int servoDegrees;
  static int servoDegreesIncrement = 5;
  static int selectedServo = SETTING_SERVO_OPEN_DEGREES;
#ifdef BAKE
  static int bakeSetting = SETTING_BAKE_TEMP;
  static int bakeValue = BAKE_TEMP;
#endif //BAKE
  boolean continueWithSetup = true;

  switch (setupPhase) {
    case 0:  // Set up the output types
      if (drawMenu) {
        drawMenu = false;
        lcdPrintLine(0, "Dx is");
        lcd.setCursor(1, 0);
        lcd.print(output);
        type = getSetting(SETTING_D4_TYPE - 4 + output);
        lcdPrintLine(1, outputDescription[type]);
      }

      // Was a button pressed?
      switch (getButton()) {
        case CONTROLEO_BUTTON_TOP:
          // Move to the next type
          type = (type + 1) % NO_OF_TYPES;
          lcdPrintLine(1, outputDescription[type]);
          break;
        case CONTROLEO_BUTTON_BOTTOM:
          // Save the type for this output
          setSetting(SETTING_D4_TYPE - 4 + output, type);
          // Go to the next output
          output++;
          if (output != 8) {
            lcd.setCursor(1, 0);
            lcd.print(output);
            type = getSetting(SETTING_D4_TYPE - 4 + output);
            lcdPrintLine(1, outputDescription[type]);
            break;
          }

          // Go to the next phase.  Reset variables used in this phase
          setupPhase++;
          drawMenu = true;
          output = 4;
          break;
      }
      break;

    case 1:  // Get the maximum temperature
      if (drawMenu) {
        drawMenu = false;
        lcdPrintLine(0, "Max temperature");
        lcdPrintLine(1, "xxx\1C");
        maxTemperature = getSetting(SETTING_MAX_TEMPERATURE);
        displayMaxTemperature(maxTemperature);
      }

      // Was a button pressed?
      switch (getButton()) {
        case CONTROLEO_BUTTON_TOP:
          // Increase the temperature
          maxTemperature++;
          if (maxTemperature > 280)
            maxTemperature = 175;
          displayMaxTemperature(maxTemperature);
          break;
        case CONTROLEO_BUTTON_BOTTOM:
          // Save the temperature
          setSetting(SETTING_MAX_TEMPERATURE, maxTemperature);
          // Go to the next phase.  Reset variables used in this phase
          setupPhase++;
          drawMenu = true;
      }
      break;

    case 2:  // Get the servo open and closed settings
      if (drawMenu) {
        drawMenu = false;
        lcdPrintLine(0, "Door servo");
        lcdPrintLine(1, selectedServo == SETTING_SERVO_OPEN_DEGREES ? "open:" : "closed:");
        servoDegrees = getSetting(selectedServo);
        displayServoDegrees(servoDegrees);
        // Move the servo to the saved position
        setServoPosition(servoDegrees, 1000);
      }

      // Was a button pressed?
      switch (getButton()) {
        case CONTROLEO_BUTTON_TOP:
          // Should the servo increment change direction?
          if (servoDegrees >= 180)
            servoDegreesIncrement = -5;
          if (servoDegrees == 0)
            servoDegreesIncrement = 5;
          // Change the servo angle
          servoDegrees += servoDegreesIncrement;
          // Move the servo to this new position
          setServoPosition(servoDegrees, 200);
          displayServoDegrees(servoDegrees);
          break;
        case CONTROLEO_BUTTON_BOTTOM:
          // Save the servo position
          setSetting(selectedServo, servoDegrees);
          // Go to the next phase.  Reset variables used in this phase
          drawMenu = true;
          if (selectedServo == SETTING_SERVO_OPEN_DEGREES)
            selectedServo = SETTING_SERVO_CLOSED_DEGREES;
          else {
            selectedServo = SETTING_SERVO_OPEN_DEGREES;
            // Go to the next phase.  Reset variables used in this phase
            setupPhase++;
            drawMenu = true;
          }
      }
      break;
#ifdef BAKE
    case 3:  // Get the bake temperature (degrees C) and duration (10's of minutes)
      if (drawMenu) {
        drawMenu = false;
        bakeValue = getSetting(bakeSetting);
        lcdPrintLine(0, bakeSetting == SETTING_BAKE_TEMP ? "Bake temp (C)" : "Bake time");
        lcdPrintLine(1, "");
        if (bakeSetting == SETTING_BAKE_TEMP) {
          displayVal(0, bakeValue);
        } else {
          displayTime(0, (unsigned long) (bakeValue * SLOT_MINS) * 60); // convert to seconds
        }
      }


      // Was a button pressed?
      switch (getButton()) {
        case CONTROLEO_BUTTON_TOP:
          bakeValue++;
          if (bakeSetting == SETTING_BAKE_TEMP) {
            // adjust the temperature, wrap at max back to min temp
            if (bakeValue > BAKE_MAXTEMP)
              bakeValue = BAKE_MINTEMP;
            displayVal(0, bakeValue);
          } else {
            // adjust the time (minutes), wrap at max back to zero time
            if (bakeValue > (BAKE_DURATION * 3)) //  max is 3x the default duration
              bakeValue = 0;
            lcdPrintLine(1, "");
            displayTime(0, (unsigned long) (bakeValue * SLOT_MINS) * 60); // convert to seconds for displayTime
          }
          sprintf(debugBuffer, "Bake setting=%d value=%d", bakeSetting, bakeValue);
          Serial.println(debugBuffer);
          break;
        case CONTROLEO_BUTTON_BOTTOM:
          // Save the setting
          setSetting(bakeSetting, bakeValue);
          // Go to the next phase.  Reset variables used in this phase
          drawMenu = true;
          if (bakeSetting == SETTING_BAKE_TEMP)
            bakeSetting = SETTING_BAKE_DURATION;
          else {
            bakeSetting = SETTING_BAKE_TEMP;
            // Done with setup
            continueWithSetup = false;
          }
      }
      break;
#endif // BAKE      
  }

  // Is setup complete?
  if (!continueWithSetup)
    setupPhase = 0;
  return continueWithSetup;
}


void displayMaxTemperature(int maxTemperature) {
  lcd.setCursor(0, 1);
  lcd.print(maxTemperature);
}


void displayServoDegrees(int degrees) {
  lcd.setCursor(8, 1);
  lcd.print(degrees);
  lcd.print("\1 ");
}

#ifdef BAKE
void displayVal(int offset, unsigned long val) {
  lcd.setCursor(offset, 1);
  lcd.print(val);
  lcd.print("  ");
}
#endif BAKE





