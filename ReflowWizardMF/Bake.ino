// Bake - for prebaking of parts per mfg spec, to get specific moisture levels prior to reflow
// Called when in Bake mode from the main loop (20 times per second)
// Return false to exit this mode

// Derived from Testing and Reflow codes
//
// Unlike precision reflow profile, bake warms to a set temperature, maintains that temperature
// for a specified time period, then shuts off. Relies upon being able to open the door (servo)
// to modulate the temperature when it gets too warm.
//
// Much of this looks like the reflow code, so the heating elements are modulated at a similar
// duty cycle. We take similar phase stages, and use the "reflow" phase as the actual bake cycle.
// A key thing dropped is the learning and adaptation of the reflow wizard. Assume that the learning
// there is sufficient for the ramp and duty cycle in the bake mode

#ifdef BAKE
#include "ReflowWizard.h"

boolean Bake() {
  static int bakePhase = PHASE_INIT;
  static int outputType[4];
  static int maxTemperature;
  static boolean learningMode;
  static phaseData phase[PHASE_REFLOW + 1];
  static unsigned long phaseStartTime, reflowStartTime;
  static int elementDutyCounter[4];
  static int counter = 0;
  static boolean firstTimeInPhase = true;
  static boolean doorClosed = true;

  double currentTemperature;
  unsigned long currentTime = millis();
  int i, j;
  int elementDutyStart = 0;
  char msgbuf[17] = "                ";

  // check the temperature
  currentTemperature = getCurrentTemperature();
  if (checkTCFault(currentTemperature)) {
    // Abort the reflow
    Serial.println(F("Bake aborted because of thermocouple error!"));
    bakePhase = PHASE_ABORT_REFLOW;
  }

  // Abort if a button is pressed
  if (getButton() != CONTROLEO_BUTTON_NONE) {
    bakePhase = PHASE_ABORT_REFLOW;
    lcdPrintLine(0, "Aborting bake");
    lcdPrintLine(1, "Button pressed");
    Serial.println(F("Button pressed.  Aborting bake ..."));
  }

  switch (bakePhase) {
    case PHASE_INIT: // User has requested to start a bake

      // Make sure the oven is at or below the MAX bake temp.
      maxTemperature = getSetting(SETTING_BAKE_TEMP);

      if (currentTemperature > (maxTemperature - 5)) {
        lcdPrintLine(0, "TOO Warm to Bake");
        lcdPrintLine(1, "");
        Serial.println(F("Oven too hot to start bake.  Please wait ..."));
        delay(3000);
        // Abort the bake
        bakePhase = PHASE_ABORT_REFLOW;
        break;
      }
      // Get the types for the outputs (elements, fan or unused)
      for (i = 0; i < 4; i++)
        outputType[i] = getSetting(SETTING_D4_TYPE + i);

      // Let the user know if learning mode is on
      // although we don't do training for bake, detect if it's set, and abort bake if training is needed
      learningMode = getSetting(SETTING_LEARNING_MODE);
      if (learningMode) {
        lcdPrintLine(0, "Learning ON");
        lcdPrintLine(1, "Use Reflow mode");
        Serial.println(F("Learning mode is enabled.  Must use Reflow mode to do learning. Aborting bake."));
        delay(3000);
        bakePhase = PHASE_ABORT_REFLOW;
        break;
      }

      // Read all the settings
      for (i = PHASE_PRESOAK; i <= PHASE_REFLOW; i++) {
        for (j = 0; j < 4; j++)
          phase[i].elementDutyCycle[j] = (getSetting(SETTING_PRESOAK_D4_DUTY_CYCLE + ((i - PHASE_PRESOAK) * 4) + j)) / 2;
        // Time to peak temperature should be between 3.5 and 5.5 minutes.
        // While J-STD-20 gives exact phase temperatures, the reading depends very much on the thermocouple used
        // and its location.  Varying the phase temperatures as the max temperature changes allows for thermocouple
        // variation.
        // Keep in mind that there is a 2C error in the MAX31855, and typically a 3C error in the thermocouple.
        switch (i) {
          case PHASE_PRESOAK:
            phase[i].endTemperature = maxTemperature * 3 / 5; // J-STD-20 gives 150C
            phase[i].phaseMinDuration = 60;
            phase[i].phaseMaxDuration = 100;
            break;
          case PHASE_SOAK:
            phase[i].endTemperature = maxTemperature * 4 / 5; // J-STD-20 gives 200C
            phase[i].phaseMinDuration = 80;
            phase[i].phaseMaxDuration = 140;
            break;
          case PHASE_REFLOW:
            phase[i].endTemperature = maxTemperature;
            phase[i].phaseMinDuration = getSetting(SETTING_BAKE_DURATION) * SLOT_MINS * 60; //convert slot count to seconds
            phase[i].phaseMaxDuration = (getSetting(SETTING_BAKE_DURATION) * SLOT_MINS * 60) + 60; //max is just one minute more
            break;
        }
      }

      // Move to the next phase
      bakePhase = PHASE_PRESOAK;
      firstTimeInPhase = true;
      lcdPrintLine(0, bakephaseDescription[bakePhase]);
      lcdPrintLine(1, "");

      // Display information about this phase
      serialDisplayPhaseData(bakephaseDescription[bakePhase], &phase[bakePhase], outputType);

      // Stagger the element start cycle to avoid abrupt changes in current draw
      for (i = 0; i < 4; i++) {
        elementDutyCounter[i] = elementDutyStart;
        // Turn the next element on (elementDutyCounter[i+1] == 0) when this element turns off (elementDutyCounter[i] == phase[bakePhase].elementDutyCycle[i])
        // For example, assume two element both at 20% duty cycle.
        //   The counter for the first starts at 0
        //   The counter for the second should start at 80, because by the time counter 1 = 20 (1 turned off) counter 2 = 0 (counter 2 turned on)
        elementDutyStart = (100 + elementDutyStart - phase[bakePhase].elementDutyCycle[i]) % 100;
      }

      // Start the reflow and phase timers
      reflowStartTime = millis();
      phaseStartTime = reflowStartTime;
      break;

    case PHASE_PRESOAK:
    case PHASE_SOAK:
      // Has the ending temperature for this phase been reached?
      if (currentTemperature >= phase[bakePhase].endTemperature) {
        // The temperature is high enough to move to the next phase
        bakePhase++;
        firstTimeInPhase = true;
        lcdPrintLine(0, bakephaseDescription[bakePhase]);
        phaseStartTime = millis();
        // Stagger the element start cycle to avoid abrupt changes in current draw
        for (i = 0; i < 4; i++) {
          elementDutyCounter[i] = elementDutyStart;
          // Turn the next element on (elementDutyCounter[i+1] == 0) when this element turns off (elementDutyCounter[i] == phase[bakePhase].elementDutyCycle[i])
          // For example, assume two element both at 20% duty cycle.
          //   The counter for the first starts at 0
          //   The counter for the second should start at 80, because by the time counter 1 = 20 (1 turned off) counter 2 = 0 (counter 2 turned on)
          elementDutyStart = (100 + elementDutyStart - phase[bakePhase].elementDutyCycle[i]) % 100;
        }
        // Display information about this phase
        if (bakePhase <= PHASE_REFLOW)
          serialDisplayPhaseData(bakephaseDescription[bakePhase], &phase[bakePhase], outputType);
        break;
      }
      // Turn the output on or off based on its duty cycle
      for (i = 0; i < 4; i++) {
        // Skip unused elements
        if (outputType[i] == TYPE_UNUSED)
          continue;
        // Turn all the elements on at the start of the presoak
        if (bakePhase == PHASE_PRESOAK && currentTemperature < (phase[bakePhase].endTemperature * 3 / 5)) {
          digitalWrite(4 + i, HIGH);
          continue;
        }
        // Turn the output on at 0, and off at the duty cycle value
        if (elementDutyCounter[i] == 0)
          digitalWrite(4 + i, HIGH);
        if (elementDutyCounter[i] == phase[bakePhase].elementDutyCycle[i])
          digitalWrite(4 + i, LOW);
        // Increment the duty counter
        elementDutyCounter[i] = (elementDutyCounter[i] + 1) % 100;
      }

      // Has too much time been spent in this phase?
      if (currentTime - phaseStartTime > (unsigned long) (phase[bakePhase].phaseMaxDuration * MILLIS_TO_SECONDS)) {
        lcdPrintPhaseMessage(bakePhase, "Too slow");
        lcdPrintLine(1, "Aborting ...");
        bakePhase = PHASE_ABORT_REFLOW;
        Serial.println(F("Too Slow. Aborting bake.  Oven cannot reach required temperature!"));
      }

      // Don't consider the bake process started until the temperature passes min bake
      // some baking make require a lower temp
      if (currentTemperature < BAKE_MINTEMP)
        phaseStartTime = currentTime;

      // Update the displayed temperature roughly once per second
      if (counter++ % 20 == 0)
        displayReflowTemperature(currentTime, reflowStartTime, phaseStartTime, currentTemperature);
      break;

    case PHASE_REFLOW:
      // For bake: stay in this phase and hold the temperature
      // until the duration time is reached, then move to the cool phase

      if (currentTemperature >= phase[bakePhase].endTemperature) {
        if (firstTimeInPhase) {
          phaseStartTime = millis(); // don't start phase until we reach temp
          firstTimeInPhase = false;
        }
        // hold steady or cool
        if (currentTemperature > (phase[bakePhase].endTemperature + 0.5 )) { // accuracy is +-5C so this offset probably too low
          lcdPrintLine(0, "Bake Too Warm");

          // turn all the elements off (keep convection fans on)
          for (int i = 0; i < 4; i++) {
            if (outputType[i] != TYPE_CONVECTION_FAN)
              digitalWrite(i + 4, LOW);
          }
          if (doorClosed) {
            setServoPosition(getSetting(SETTING_SERVO_OPEN_DEGREES), 3000);
            doorClosed = false;
          }
        }
      } else {
        // Warm a cycle
        if (! firstTimeInPhase)
          lcdPrintLine(0, "Bake ON");
        if (! doorClosed && currentTemperature < (phase[bakePhase].endTemperature - 0.5 )) {
          setServoPosition(getSetting(SETTING_SERVO_CLOSED_DEGREES), 3000); // ensure door is closed
          doorClosed = true;
        }
        if (currentTemperature <= (phase[bakePhase].endTemperature - 3 )) { // go below margin before heating
          for (i = 0; i < 4; i++) {
            // Skip unused elements
            if (outputType[i] == TYPE_UNUSED)
              continue;
            // Turn the output on at 0, and off at the duty cycle value
            if (elementDutyCounter[i] == 0)
              digitalWrite(4 + i, HIGH);
            if (elementDutyCounter[i] == phase[bakePhase].elementDutyCycle[i])
              digitalWrite(4 + i, LOW);
            // Increment the duty counter
            elementDutyCounter[i] = (elementDutyCounter[i] + 1) % 100;
          }
        } else {
          for (int i = 0; i < 4; i++) {
            if (outputType[i] != TYPE_CONVECTION_FAN)
              digitalWrite(i + 4, LOW);
          }
        }
      }
      if (currentTime - phaseStartTime >= (unsigned long) (phase[bakePhase].phaseMinDuration * MILLIS_TO_SECONDS)) {
        // done baking
        lcdPrintLine(1, "Bake done.");
        Serial.println(F("Bake duration complete."));
        playTones(TUNE_REFLOW_DONE);
        bakePhase = PHASE_ABORT_REFLOW;
      }

      // Update the displayed temperature and minutes remaining roughly once per second
      if (counter++ % 20 == 0) {
        displayReflowTemperature(currentTime, reflowStartTime, phaseStartTime, currentTemperature);
        if (! firstTimeInPhase)
          displayTime(9, ((phase[bakePhase].phaseMinDuration * MILLIS_TO_SECONDS) - (currentTime - phaseStartTime)) / MILLIS_TO_SECONDS);
      }
      break;
      
/* these phases are not used, so omit them from the code
    case PHASE_WAITING:  // No need for wait in baking, Shut off elements, skip to cooling
      if (firstTimeInPhase) {
        firstTimeInPhase = false;
        Serial.println(F("******* Phase: Bake Waiting *******"));
        Serial.println(F("Turning all heating elements off ..."));
        // Make sure all the elements are off (keep convection fans on)
        for (int i = 0; i < 4; i++) {
          if (outputType[i] != TYPE_CONVECTION_FAN)
            digitalWrite(i + 4, LOW);
        }
        bakePhase = PHASE_COOLING_BOARDS_IN;
        firstTimeInPhase = true;
      }
      break;

    case PHASE_COOLING_BOARDS_IN: // Start cooling the oven.
      if (firstTimeInPhase) {
        firstTimeInPhase = false;
        // Update the display
        lcdPrintLine(0, "Cool - open door");
        Serial.println(F("******* Phase: Cooling *******"));
        Serial.println(F("Open the oven door ..."));

        // If a servo is attached, use it to open the door over 3 seconds
        setServoPosition(getSetting(SETTING_SERVO_OPEN_DEGREES), 3000);
        // Play a tune to let the user know the door should be opened
        playTones(TUNE_REFLOW_DONE);
      }
      // Update the temperature roughly once per second
      if (counter++ % 20 == 0)
        displayReflowTemperature(currentTime, reflowStartTime, phaseStartTime, currentTemperature);

      // stay in this phase until the oven is mostly cool
      if (currentTemperature < (BAKE_MINTEMP - 10.0)) {
        bakePhase = PHASE_COOLING_BOARDS_OUT;
      }
      break;

    case PHASE_COOLING_BOARDS_OUT: // just play tone and skip to next phase
      playTones(TUNE_REMOVE_BOARDS);
      bakePhase = PHASE_ABORT_REFLOW;
      break;
*/
    case PHASE_ABORT_REFLOW: // stop baking
      lcdPrintLine(0, "Bake is done");
      lcdPrintLine(1, "Remove parts.");
      Serial.println(F("Bake is done!"));
      // Turn all elements and fans off
      for (i = 4; i < 8; i++)
        digitalWrite(i, LOW);
      // Close the oven door now, over 3 seconds
      setServoPosition(getSetting(SETTING_SERVO_CLOSED_DEGREES), 3000);
      // Start next time with initialization
      bakePhase = PHASE_INIT;
      // Wait for a bit to allow the user to read the last message
      delay(3000);
      // Return to the main menu
      return false;
  }

  // Stay in this mode;
  return true;
}

void displayTime(int offset, unsigned long val) { // val in seconds;
  lcd.setCursor(offset, 1);
  if (val >= 60) {
    if (val >= (60 * 60)) { // hrs
      lcd.print(val / (60 * 60));
      lcd.print("h:");
      lcd.print((val / 60) % 60);
      lcd.print("m ");
    } else {
      lcd.print((val / 60) % 60);
      lcd.print("min");
    }
  } else {
    lcd.print(val);
    lcd.print("sec");
  }
}
#endif //BAKE



