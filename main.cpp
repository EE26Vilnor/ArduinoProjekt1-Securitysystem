#include <Keypad.h>
#include <Adafruit_LiquidCrystal.h>
#include <string.h>
#include <Servo.h>

Adafruit_LiquidCrystal lcd(0);
Servo servo;

const byte ROWS = 4, COLS = 4;
const char keys[ROWS][COLS] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

const byte pingPin = 10;
const byte servoPin = 11;
const byte greenPin = 12;
const byte redPin = 13;
const byte buzzerPin = A0;
byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};


unsigned long cooldownUntil = 0;

const unsigned long BASE_COOLDOWN = 2000;
const byte MAX_FAILED_ATTEMPTS = 5;
const byte MAX_PIN_LENGTH = 16;
const unsigned long MAX_DISTANCE_DURATION = 19000;

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

char pin[MAX_PIN_LENGTH + 1] = "";
byte pinLength = 0;
char correctPin[17];

bool unlocked = false;
unsigned long unlockTime = 0;
const unsigned long UNLOCK_DURATION = 5000;
byte failedAttempts = 0;

bool wasInsideZone = false, showingMessage = false;
unsigned long messageStart = 0;

bool changingPin = false;

unsigned long readDistance() {
    digitalWrite(pingPin, LOW);
    delayMicroseconds(2);

    digitalWrite(pingPin, HIGH);
    delayMicroseconds(10);

    digitalWrite(pingPin, LOW);
    pinMode(pingPin, INPUT);

    unsigned long duration = pulseIn(pingPin, HIGH, 25000);
    pinMode(pingPin, OUTPUT);
    return duration;
}

void clearPin() {
    pinLength = 0;
    pin[0] = '\0';
}

void showPrompt() {
    if (!unlocked) digitalWrite(redPin, LOW);

    lcd.clear();
    lcd.print("Enter PIN:");
    lcd.setCursor(0, 1);
}

void changePin(const char* newPin) {
    int len = strlen(newPin);

    if (len < 4 || len > 16) {
        lcd.clear();
        lcd.print("Invalid PIN length!");
        return;
    }
    strcpy(correctPin, newPin);

    lcd.clear();
    lcd.print("PIN changed!");

    Serial.print("New PIN: ");
    Serial.println(correctPin);
}

void lock() {
    servo.write(0);
    unlocked = false;
    changingPin = false;

    digitalWrite(greenPin, LOW);

    clearPin();

    lcd.clear();
    lcd.print("Locked!");

    showingMessage = true;
    messageStart = millis();
}

void handleKey(char key) {
    if (key == '*' && !changingPin && unlocked) {
        changingPin = true;
        clearPin();

        lcd.clear();
        lcd.print("Enter new PIN:");
        lcd.setCursor(0, 1);
        return;
    }

    if (key >= '0' && key <= '9' && pinLength < MAX_PIN_LENGTH) {
        pin[pinLength++] = key;
        pin[pinLength] = '\0';
        lcd.setCursor(pinLength - 1, 1);
        lcd.print('*');
    }

    else if(key == '#' && changingPin) {
        changePin(pin);
        changingPin = false;
        clearPin();

        delay(1500);
        showPrompt();
    }

    else if (key == '#') {
        lcd.clear();
        lcd.setCursor(0, 0);

        if (strcmp(pin, correctPin) == 0) {
            lcd.print("Correct!");
            failedAttempts = 0;
            unlocked = true;
            unlockTime = millis();
          	servo.write(90);
            digitalWrite(greenPin, HIGH);
        } else {
            failedAttempts++;
            lcd.print("Wrong pin.");
          
            digitalWrite(redPin, HIGH);

            Serial.print("Failed attempts: ");
            Serial.println(failedAttempts);

            if (failedAttempts >= MAX_FAILED_ATTEMPTS) {
                /*
                "tone()" funktionen gav mig "invalid header" error.
                Efter flera felsökningförsök, så har jag inte lyckats ännu.
                */
                // tone(buzzerPin, 1000, 3000);
                
                failedAttempts = 0;

                cooldownUntil = millis() + 10000;

            } else {

                unsigned long cooldown = BASE_COOLDOWN * (1UL << (failedAttempts - 1));

                cooldownUntil = millis() + cooldown;

                Serial.print("Cooldown: ");
                Serial.print(cooldown / 1000);
                Serial.println(" seconds");
            }
        }

        clearPin();
        showingMessage = true;
        messageStart = millis();
    }

    if (key == 'D' && unlocked && !changingPin) {
        lock();
        return;
    }
}

void setup() {
    Serial.begin(9600);
    strcpy(correctPin, "1234");
  	
    Serial.print("Current PIN: ");
    Serial.println(correctPin);
  
    servo.attach(servoPin);
    servo.write(0);
    pinMode(pingPin, OUTPUT);
    digitalWrite(pingPin, LOW);

    pinMode(greenPin, OUTPUT);
    digitalWrite(greenPin, LOW);
    pinMode(redPin, OUTPUT);
    digitalWrite(redPin, LOW);
  
    pinMode(buzzerPin, OUTPUT);

    lcd.begin(16, 2);
    showPrompt();
}

void loop() {
    unsigned long duration = readDistance();
    bool insideZone = duration > 0 && duration <= MAX_DISTANCE_DURATION;

    if (insideZone != wasInsideZone) {
        wasInsideZone = insideZone;
        if (insideZone) showPrompt();
    else {
        lcd.clear();
        clearPin();
        showingMessage = false;
        }
    }

    if (!insideZone) return;

    if (unlocked && !changingPin &&
        millis() - unlockTime >= UNLOCK_DURATION) {
        lock();
    }

    if (showingMessage) {
        if (millis() - messageStart >= 1500) {
            showingMessage = false;

            if (unlocked) {
                lcd.clear();
                lcd.print("Unlocked!");
                lcd.setCursor(0, 1);
                lcd.print("* = Change PIN");
            } else {
                showPrompt();
            }
        } else {
            return;
        }
    }
  	if (millis() < cooldownUntil) {
    	return;
    }

    char key = keypad.getKey();
    if (key) handleKey(key);
}
