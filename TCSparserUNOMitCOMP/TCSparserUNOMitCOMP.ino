#define outputPin 5

#define startBit 6//ms
#define oneBit 4//ms
#define zeroBit 2//ms

volatile uint32_t CMD = 0;
volatile uint8_t lengthCMD = 0;
volatile bool cmdReady;

ISR(ANALOG_COMP_vect ) {
  static uint32_t curCMD;
  static uint32_t usLast;
  static byte curCRC;
  static byte calCRC;
  static byte curLength;
  static byte cmdIntReady;
  static byte curPos;
  uint32_t usNow = micros();
  uint32_t timeInUS = usNow - usLast;
  usLast = usNow;
  byte curBit = 4;
  if (timeInUS < 1000) {
    curBit = 5; // invalid glitches typical 29ms
  } else if (timeInUS >= 1000 && timeInUS <= 2999) {
    curBit = 0;
  } else if (timeInUS >= 3000 && timeInUS <= 4999) {
    curBit = 1;
  } else if (timeInUS >= 5000 && timeInUS <= 6999) {
    curBit = 2;
  } else if (timeInUS >= 7000 && timeInUS <= 24000) {
    curBit = 3;
    curPos = 0;
  }

  if (curBit != 5) { // skip processing for glitches
    if (curPos == 0) {
      if (curBit == 2) {
        curPos++;
      }
      curCMD = 0;
      curCRC = 0;
      calCRC = 1;
      curLength = 0;
    } else if (curBit == 0 || curBit == 1) {
      if (curPos == 1) {
        curLength = curBit;
        curPos++;
      } else if (curPos >= 2 && curPos <= 17) {
        if (curBit)bitSet(curCMD, (curLength ? 33 : 17) - curPos);
        calCRC ^= curBit;
        curPos++;
      } else if (curPos == 18) {
        if (curLength) {
          if (curBit)bitSet(curCMD, 33 - curPos);
          calCRC ^= curBit;
          curPos++;
        } else {
          curCRC = curBit;
          cmdIntReady = 1;
        }
      } else if (curPos >= 19 && curPos <= 33) {
        if (curBit)bitSet(curCMD, 33 - curPos);
        calCRC ^= curBit;
        curPos++;
      } else if (curPos == 34) {
        curCRC = curBit;
        cmdIntReady = 1;
      }
    } else {
      curPos = 0;
    }
    if (cmdIntReady) {
      cmdIntReady = 0;
      curPos = 0;
      if (curCRC == calCRC) {
        cmdReady = 1;
        lengthCMD = curLength;
        CMD = curCMD;
      }
      curCMD = 0;
    }
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(outputPin, OUTPUT);
 ACSR =
   (0 << ACD) |    // Analog Comparator: Enabled
   (0 << ACBG) |   // Analog Comparator Bandgap Select: AIN0 is applied to the positive input
   (0 << ACO) |    // Analog Comparator Output: Off
   (1 << ACI) |    // Analog Comparator Interrupt Flag: Clear Pending Interrupt
   (1 << ACIE) |   // Analog Comparator Interrupt: Enabled
   (0 << ACIC) |   // Analog Comparator Input Capture: Disabled
   (0 << ACIS1) | (0 << ACIS0);   // Analog Comparator Interrupt Mode: Comparator Interrupt on changing Edge
}

String inData;
void loop()
{
  while (Serial.available() > 0)
  {
    char aChar = Serial.read();
    if (aChar == 0x04)
    {
      analyzeInString(inData);
    }
    else if (aChar == 0x01)
    {
      inData = "";
    } else {
      inData += aChar;
    }
  }
  if (cmdReady) {
    cmdReady = 0;
    Serial.write(0x01);
    Serial.print("$");
    printHEX(CMD);
    Serial.write(0x04);
    Serial.println();
  }
}

void analyzeInString(String inString) {
  if (inString.length() > 0) {
    if (inString == "(01") {
      SendeSerCMD("#");
    } else if (inString == ".3F") {
      SendeSerCMD(".222222222");
    } else if (inString.substring(0, 1) == " ") {
      sendeProtokollHEX((uint32_t)strtoul(&inString[1], NULL, 16));
    }
  }
}

void SendeSerCMD(String outString) {
  Serial.write(0x01);
  Serial.print(outString);
  Serial.write(0x04);
}

//it is better to also give an arg with the length because it 
//is possible that there is a 4Byte protokoll smaller than 0xFFFF
//something like void sendeProtokollHEX(uint32_t protokoll,byte firstBit) {
//  int length = 16;
//  byte checksm = 1;
//  if (firstBit) length = 32;
//and so on...
void sendeProtokollHEX(uint32_t protokoll) {
  int length = 16;
  byte checksm = 1;
  byte firstBit = 0;
  if (protokoll > 0xFFFF) {
    length = 32;
    firstBit = 1;
  }
  digitalWrite(outputPin, HIGH);
  delay(startBit);
  digitalWrite(outputPin, !digitalRead(outputPin));
  delay(firstBit ? oneBit : zeroBit);
  int curBit = 0;
  for (byte i = length; i > 0; i--) {
    curBit = bitRead(protokoll, i - 1);
    digitalWrite(outputPin, !digitalRead(outputPin));
    delay(curBit ? oneBit : zeroBit);
    checksm ^= curBit;
  }
  digitalWrite(outputPin, !digitalRead(outputPin));
  delay(checksm ? oneBit : zeroBit);
  digitalWrite(outputPin, LOW);
}

void printHEX(uint32_t DATA) {
  uint8_t numChars = lengthCMD ? 8 :4;
  uint32_t mask  = 0x0000000F;
  mask = mask << 4 * (numChars - 1);
  for (uint32_t i = numChars; i > 0; --i) {
    Serial.print(((DATA & mask) >> (i - 1) * 4), HEX);
    mask = mask >> 4;
  }
}
