// =====================
// Mode Buzzer Controller + 7-Segment Display + ON Indicator
// =====================

// -------------------- PIN ASSIGNMENTS --------------------
const int ledPin = 8;          // LED that flashes with buzzer
const int buzzerPin = 7;       // Buzzer output

// Mode buttons
const int button1Pin = 2;      // OFF
const int button2Pin = 3;      // Calm
const int button3Pin = 4;      // Medium
const int button4Pin = 5;      // Intense
const int button5Pin = 6;      // Escalate

// ON button + indicator LED
const int buttonOnPin = A0;    
const int onLedPin = A5;       
bool isOn = false;             

// 7-Segment display pins (no pin overlap!)
const int segA = 9;
const int segB = 10;
const int segC = 11;
const int segD = 12;
const int segE = 13;
const int segF = A1;
const int segG = A2;
const int segDP = A3;

// -------------------- STATE VARIABLES --------------------
int currentMode = 0;  
int targetMode = 0;   
bool isRamping = false;
bool buzState = false;

unsigned long lastToggle = 0;
unsigned long rampStartTime = 0;
int rampStartFreq = 0;
unsigned long rampStartPeriod = 0;

struct ModeProfile {
  int freq;
  unsigned long period;
};

ModeProfile modes[] = {
  {0, 0},        // OFF
  {600, 250},    // Calm
  {1000, 150},   // Medium
  {1800, 80}     // Intense
};

// 7-segment display animation value
float displayNumber = 9;     
float driftSpeed = 0.05;     

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(9600);
  Serial.println("=== Mode Buzzer + 7-Segment Started ===");

  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(onLedPin, OUTPUT);

  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);
  pinMode(button3Pin, INPUT_PULLUP);
  pinMode(button4Pin, INPUT_PULLUP);
  pinMode(button5Pin, INPUT_PULLUP);
  pinMode(buttonOnPin, INPUT_PULLUP);

  // 7-Segment pins
  pinMode(segA, OUTPUT); pinMode(segB, OUTPUT); pinMode(segC, OUTPUT);
  pinMode(segD, OUTPUT); pinMode(segE, OUTPUT); pinMode(segF, OUTPUT);
  pinMode(segG, OUTPUT); pinMode(segDP, OUTPUT);

  stopOutput();
  showNumberOn7SegmentBlank();
}

// -------------------- MAIN LOOP --------------------
void loop() {
  
// ------------------ TOGGLING ON/OFF BUTTON ------------------
static bool lastButtonOnState = HIGH;
static unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

bool reading = digitalRead(buttonOnPin);
if (reading != lastButtonOnState) {
  lastDebounceTime = millis();
}

if ((millis() - lastDebounceTime) > debounceDelay) {
  static bool buttonOnState = HIGH;
  if (reading == LOW && buttonOnState == HIGH) {
    // Toggle system state
    isOn = !isOn;
    if (isOn) {
      Serial.println("=== System Turned ON ===");
      digitalWrite(onLedPin, HIGH);
    } else {
      Serial.println("=== System Turned OFF ===");
      stopOutput();
      showNumberOn7SegmentBlank();
      digitalWrite(onLedPin, LOW);
    }
  }
  buttonOnState = reading;
}

lastButtonOnState = reading;

// If system is OFF, don't process anything else
if (!isOn) {
  stopOutput();
  showNumberOn7SegmentBlank();
  digitalWrite(onLedPin, LOW);
  return;
}

  // ------------------ MODE BUTTONS ------------------
  static bool lastButton1 = HIGH;
  static bool lastButton2 = HIGH;
  static bool lastButton3 = HIGH;
  static bool lastButton4 = HIGH;
  static bool lastButton5 = HIGH;

  bool b1 = digitalRead(button1Pin);
  bool b2 = digitalRead(button2Pin);
  bool b3 = digitalRead(button3Pin);
  bool b4 = digitalRead(button4Pin);
  bool b5 = digitalRead(button5Pin);

  if (b1 == LOW && lastButton1 == HIGH) { currentMode=0; targetMode=0; isRamping=false; lastToggle=millis(); stopOutput(); Serial.println("[Button 1] OFF"); }
  if (b2 == LOW && lastButton2 == HIGH) { currentMode=1; targetMode=1; isRamping=false; lastToggle=millis(); Serial.println("[Button 2] CALM"); }
  if (b3 == LOW && lastButton3 == HIGH) { currentMode=2; targetMode=2; isRamping=false; lastToggle=millis(); Serial.println("[Button 3] MEDIUM"); }
  if (b4 == LOW && lastButton4 == HIGH) { currentMode=3; targetMode=3; isRamping=false; lastToggle=millis(); Serial.println("[Button 4] INTENSE"); }
  if (b5 == LOW && lastButton5 == HIGH) {
    if(currentMode>0 && currentMode<3 && !isRamping){
      targetMode=currentMode+1; 
      rampStartTime=millis(); 
      isRamping=true;
      rampStartFreq=modes[currentMode].freq;
      rampStartPeriod=modes[currentMode].period;
      lastToggle=rampStartTime;
      Serial.print("[Button 5] ESCALATE "); Serial.print(currentMode); Serial.print(" → "); Serial.println(targetMode);
    }
  }

  lastButton1=b1; lastButton2=b2; lastButton3=b3; lastButton4=b4; lastButton5=b5;

  unsigned long now = millis();
  ModeProfile activeProfile;
  ModeProfile mTarget = modes[targetMode];

  if(isRamping){
    unsigned long elapsed = now - rampStartTime;
    float progress = constrain((float)elapsed/30000.0,0.0,1.0);
    float freqF = rampStartFreq + (mTarget.freq - rampStartFreq)*progress;
    float periodF = rampStartPeriod + ((float)mTarget.period - (float)rampStartPeriod)*progress;
    activeProfile.freq = (int)(freqF+0.5f);
    activeProfile.period = (unsigned long)(periodF+0.5f);
    if(progress>=1.0){ isRamping=false; currentMode=targetMode; lastToggle=millis(); }
  } else { activeProfile = modes[currentMode]; } 
  if(activeProfile.period<20 && currentMode!=0) activeProfile.period=20; 
 
  // ------------------ LED + BUZZER ------------------ 
  if(currentMode!=0 && now-lastToggle>=activeProfile.period){ 
    buzState=!buzState; lastToggle=now;
    if(buzState){ 
      if(activeProfile.freq>0) tone(buzzerPin,activeProfile.freq); 
      digitalWrite(ledPin,HIGH); 
    }
    else{ 
      noTone(buzzerPin); 
      digitalWrite(ledPin,LOW); 
    }
  }

  // ------------------ 7-SEGMENT DISPLAY ------------------
  updateDisplayNumber();
  showNumberOn7Segment((int)displayNumber);

  delay(20);
}

// -------------------- FUNCTIONS --------------------
void stopOutput(){
  noTone(buzzerPin); 
  digitalWrite(ledPin,LOW);
}

void updateDisplayNumber(){
  int minV,maxV,targetNum;
  switch(currentMode){
    case 1: minV=6; maxV=9; break;
    case 2: minV=3; maxV=6; break;
    case 3: minV=0; maxV=3; break;
    default: minV=0; maxV=0; break;
  }
  if(currentMode==0){ displayNumber=9; return; }
  targetNum=random(minV,maxV+1);
  int fluct=random(-1,2);
  delay(115);
  displayNumber+=fluct;
  if(displayNumber<minV) displayNumber=minV;
  if(displayNumber>maxV) displayNumber=maxV;
  if(displayNumber<targetNum) displayNumber+=driftSpeed;
  if(displayNumber>targetNum) displayNumber-=driftSpeed;
  displayNumber=constrain(displayNumber,minV,maxV);
}

// ===================== REPLACED WITH CALIBRATED VERSION =====================
void showNumberOn7Segment(int num) {
  static const byte segDigits[10] = {
    B10111111, // 0
    B00000110, // 1
    B01110011, // 2
    B01010111, // 3
    B01001110, // 4
    B11011101, // 5
    B11111101, // 6
    B00000111, // 7
    B01111111, // 8
    B11011111  // 9
  };

  byte bits = segDigits[num];

  // Correct bit-to-pin remapping for your display wiring
  const int bitToPin[7] = {
    segA, // bit0 -> A
    segB, // bit1 -> B
    segC, // bit2 -> C
    segF, // bit3 -> D (swapped to physical F)
    segE, // bit4 -> E
    segD, // bit5 -> F (swapped to physical D)
    segG  // bit6 -> G
  };

  for (int i = 0; i < 7; ++i) {
    bool bitOn = (bits & (1 << i)) != 0;
    digitalWrite(bitToPin[i], bitOn ? LOW : HIGH); // LOW = ON for common-anode
  }

  digitalWrite(segDP, HIGH); // Decimal point OFF
}

void showNumberOn7SegmentBlank() {
  digitalWrite(segA, HIGH);
  digitalWrite(segB, HIGH);
  digitalWrite(segC, HIGH);
  digitalWrite(segD, HIGH);
  digitalWrite(segE, HIGH);
  digitalWrite(segF, HIGH);
  digitalWrite(segG, HIGH);
  digitalWrite(segDP, HIGH);
}
