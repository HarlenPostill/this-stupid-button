#include <Arduino.h>

const int GREEN_LED = 18;
const int RED_LED   = 19;

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

void setLEDs(bool greenOn, bool redOn) {
  digitalWrite(GREEN_LED, greenOn ? HIGH : LOW);
  digitalWrite(RED_LED, redOn ? HIGH : LOW);
}

void showMenu() {
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

    case 5: // Modulo (avoid 0 divisor)
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
      int choice = input.toInt();
      if (choice >= 1 && choice <= 7) {
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

      int userAnswer = input.toInt();
      Serial.printf("Your answer: %d\n", userAnswer);
      Serial.printf("Correct answer: %d\n", correctAnswer);

      if (userAnswer == correctAnswer) {
        Serial.println("CORRECT!");
        setLEDs(true, false);
      } else {
        Serial.println("WRONG!");
        setLEDs(false, true);
      }

      Serial.println("\nPress 'Q') for another question\nor enter M to return to the menu.");
      currentState = STATE_NEXT_PROMPT;
      break;
    }

    case STATE_NEXT_PROMPT: {
      if (input.equalsIgnoreCase("M")) {
        currentState = STATE_MENU;
        showMenu();
      } else {
        // Any character, space, or newline advances to the next question
        currentState = STATE_ASK_QUESTION;
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