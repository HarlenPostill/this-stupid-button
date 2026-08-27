#include <Arduino.h>
#include <limits.h>

const int GREEN_LED = 32;
const int RED_LED   = 18;

enum State {
  STATE_MENU,
  STATE_ASK_QUESTION,
  STATE_WAIT_ANSWER,
  STATE_NEXT_PROMPT
};

State currentState = STATE_MENU;
int selectedOp = 0;
int operandA = 0;
int operandB = 0;
int correctAnswer = 0;

const bool CLEAR_TERMINAL_BETWEEN_SCREENS = true;

 // SERIAL SCREEN CLEAR 
void clearSerialTerminal() {
  if (CLEAR_TERMINAL_BETWEEN_SCREENS) {
    Serial.print("\033[2J\033[H");
  }
}

// String.toInt() returns 0 for invalid text. Validating first prevents input
// such as "hello" from being accepted as the numerical answer 0.
bool parseInteger(const String &input, int &value) {
  if (input.length() == 0) return false;

  int index = 0;
  if (input[0] == '-' || input[0] == '+') {
    if (input.length() == 1) return false;
    index = 1;
  }

  for (; index < input.length(); index++) {
    if (!isDigit(input[index])) return false;
  }

  long parsed = input.toInt();
  if (parsed < INT_MIN || parsed > INT_MAX) return false;
  value = (int)parsed;
  return true;
}

void setLEDs(bool greenOn, bool redOn) {
  digitalWrite(GREEN_LED, greenOn ? HIGH : LOW);
  digitalWrite(RED_LED, redOn ? HIGH : LOW);
}

void showMenu() {
  clearSerialTerminal();
  setLEDs(false, false);
  Serial.println("\n=================================");
  Serial.println("         ESP32 MATHS QUIZ        ");
  Serial.println("=================================");
  Serial.println("Select an operation:");
  Serial.println("1: Addition       (+)");
  Serial.println("2: Subtraction    (-)");
  Serial.println("3: Multiplication (*)");
  Serial.println("4: Division       (/)");
  Serial.println("5: Modulo         (%)");
  Serial.println("6: Logical AND    (&&)");
  Serial.println("7: Logical OR     (||)");
  Serial.println("\nEnter 1-7:");
}

void generateQuestion() {
  clearSerialTerminal();
  setLEDs(false, false);
  switch (selectedOp) {
    case 1: // Addition
      operandA = random(1, 100);
      operandB = random(1, 100);
      correctAnswer = operandA + operandB;
      Serial.printf("\n---------------------------------\n%d + %d = ?\n", operandA, operandB);
      break;

    case 2: // Subtraction
      operandA = random(1, 100);
      operandB = random(1, 100);
      correctAnswer = operandA - operandB;
      Serial.printf("\n---------------------------------\n%d - %d = ?\n", operandA, operandB);
      break;

    case 3: // Multiplication
      operandA = random(1, 13);
      operandB = random(1, 13);
      correctAnswer = operandA * operandB;
      Serial.printf("\n---------------------------------\n%d * %d = ?\n", operandA, operandB);
      break;

    case 4: // Division (integer division, avoid 0 divisor)
      operandB = random(1, 13);
      correctAnswer = random(1, 13);
      operandA = operandB * correctAnswer;
      Serial.printf("\n---------------------------------\n%d / %d = ?\n", operandA, operandB);
      break;

    case 5: // Modulo (avoid 0)
      operandA = random(1, 50);
      operandB = random(1, 10);
      correctAnswer = operandA % operandB;
      Serial.printf("\n---------------------------------\n%d %% %d = ?\n", operandA, operandB);
      break;

    case 6: // Logical AND
      operandA = random(0, 2);
      operandB = random(0, 2);
      correctAnswer = (operandA && operandB) ? 1 : 0;
      Serial.printf("\n---------------------------------\n%d && %d = ?\n", operandA, operandB);
      break;

    case 7: // Logical OR
      operandA = random(0, 2);
      operandB = random(0, 2);
      correctAnswer = (operandA || operandB) ? 1 : 0;
      Serial.printf("\n---------------------------------\n%d || %d = ?\n", operandA, operandB);
      break;
  }
  Serial.println("Enter your answer:\n(Enter M to return to the menu)");
}

void setup() {
  Serial.begin(115200);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  setLEDs(false, false);

  randomSeed(analogRead(0));
  
  delay(1000); // Allow serial monitor connection to establish
  Serial.println("ESP32 Maths Quiz Started!");
  showMenu();
}

void loop() {
  if (!Serial.available()) return;

  String input = Serial.readStringUntil('\n');
  input.trim(); // Strips \r, \n, and spaces

  switch (currentState) {
    case STATE_MENU: {
      if (input.length() == 0) return;
      int choice;
      if (parseInteger(input, choice) && choice >= 1 && choice <= 7) {
        selectedOp = choice;
        currentState = STATE_ASK_QUESTION;
      } else {
        Serial.println("Invalid selection. Enter a number 1-7:");
      }
      break;
    }

    case STATE_WAIT_ANSWER: {
      if (input.length() == 0) return;

      if (input.equalsIgnoreCase("M")) {
        currentState = STATE_MENU;
        showMenu();
        return;
      }

      int userAnswer;
      if (!parseInteger(input, userAnswer)) {
        Serial.println("Invalid answer. Enter a whole number, or M for menu:");
        return;
      }

      Serial.printf("Your answer: %d\n", userAnswer);
      Serial.printf("Correct answer: %d\n", correctAnswer);

      if (userAnswer == correctAnswer) {
        Serial.println("CORRECT!");
        setLEDs(true, false);
      } else {
        Serial.println("WRONG!");
        setLEDs(false, true);
      }

      Serial.println("\nEnter Q for another question\nor M to return to the menu.");
      currentState = STATE_NEXT_PROMPT;
      break;
    }

    case STATE_NEXT_PROMPT: {
      if (input.equalsIgnoreCase("M")) {
        currentState = STATE_MENU;
        showMenu();
      } else if (input.equalsIgnoreCase("Q")) {
        currentState = STATE_ASK_QUESTION;
      } else {
        Serial.println("Enter Q for another question or M for the menu:");
      }
      break;
    }

    default:
      break;
  }

  if (currentState == STATE_ASK_QUESTION) {
    generateQuestion();
    currentState = STATE_WAIT_ANSWER;
  }
}
