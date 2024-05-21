#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <SD.h>

#define TFT_RST  5
#define TFT_CS   6
#define TFT_DC   7
#define next_button   2
#define prev_button   3
#define auto_button   4

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

bool autoMode = false;
unsigned long lastStepTime = 0;
unsigned long autoStepDelay = 1000;

File root;
File entry;
File prevEntry;

void setup(void) {
  Serial.begin(9600);
  
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(next_button, INPUT_PULLUP);
  pinMode(prev_button, INPUT_PULLUP);
  pinMode(auto_button, INPUT_PULLUP);
  
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST7735_BLUE);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(3);

  Serial.print("Initializing SD card...");
  if (!SD.begin()) {
    Serial.println("failed!");
    while(1);
  }
  Serial.println("OK!");


  root = SD.open("/");
  entry = root.openNextFile(); // Open the first file in the root directory
  printDirectory(root, 0);
  Serial.println(String(entry.name()));
  if (entry) {
    displayImage(entry); // Display the first image
  }
  root.close();
}

void loop() {
  if (!root) {
    root = SD.open("/");
    root.rewindDirectory();
    entry = root.openNextFile();
  }

  if (digitalRead(next_button) == LOW) {
    delay(200);
    if (entry) entry.close();
    entry = root.openNextFile();
    if (!entry) {
      root.rewindDirectory();
      entry = root.openNextFile();
    }
    Serial.println("next image is: " + String(entry.name()));
    displayImage(entry);
    
  }

  if (digitalRead(prev_button) == LOW) {
    delay(200);
    if (entry) entry.close();
    entry = findPreviousFile(entry);
    Serial.println("previous image is: " + String(entry.name()));
    displayImage(entry);
  }
  

  if (digitalRead(auto_button) == LOW) {
    delay(200);
    autoMode = !autoMode;
  }

  if (autoMode && millis() - lastStepTime >= autoStepDelay) {
    if (entry) entry.close();
    entry = root.openNextFile();
    if (!entry) {
      root.rewindDirectory();
      entry = root.openNextFile();
    }
    Serial.println("auto next image is: " + String(entry.name()));
    displayImage(entry);
    lastStepTime = millis();
  }

  delay(50);
}

void displayImage(File entry) {
  if (!entry) return;
  uint8_t nameSize = String(entry.name()).length();
  String str1 = String(entry.name()).substring(nameSize - 4);
  char filename[13];

  strcpy(filename, entry.name());
  if (str1.equalsIgnoreCase(".bmp") )
    bmpDraw(filename, 0, 0);
}
#define BUFFPIXEL 20
void bmpDraw(char *filename, uint8_t x, uint16_t y) {
  File bmpFile;
  int bmpWidth, bmpHeight;
  uint8_t bmpDepth;
  uint32_t bmpImageoffset;
  uint32_t rowSize;
  uint8_t sdbuffer[3*BUFFPIXEL];
  uint8_t buffidx = sizeof(sdbuffer);
  boolean goodBmp = false;
  boolean flip = true;
  int w, h, row, col;
  uint8_t r, g, b;
  uint32_t pos = 0, startTime = millis();

  if((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');

  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print(F("File not found"));
    return;
  }

  if(read16(bmpFile) == 0x4D42) {
    Serial.print(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile);
    bmpImageoffset = read32(bmpFile);
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) {
      bmpDepth = read16(bmpFile);
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) {
        goodBmp = true;
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);
        rowSize = (bmpWidth * 3 + 3) & ~3;
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip = false;
        }
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;
        tft.startWrite();
        tft.setAddrWindow(x, y, w, h);
        for (row=0; row<h; row++) {
          if(flip) pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) {
            tft.endWrite();
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer);
          }
          for (col=0; col<w; col++) {
            if (buffidx >= sizeof(sdbuffer)) {
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0;
              tft.startWrite();
            }
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.Color565(r,g,b));
          }
        }
        tft.endWrite();
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      }
    }
  }

  bmpFile.close();
  if(!goodBmp) Serial.println(F("BMP format not recognized."));
}

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read();
  return result;
}

void printDirectory(File dir, int numTabs) {
  while (true) {
    File entry =  dir.openNextFile();
    if (! entry) {
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

File findPreviousFile(File currentFile) {
  File previous;
  File root = SD.open("/");
  root.rewindDirectory(); // Start from the beginning of the directory

  File entry = root;
  Serial.println("current file is:" + String(currentFile.name()));
  while (true) {
    
    entry = root.openNextFile();
    if (!entry) {
      Serial.println("prev Image not found");
      // No more files
      break;
    }
    if (String(entry.name()) == String(currentFile.name())) {
      // Found the current file, return previous file
      Serial.println("prev Image  found1");
      return previous;
    }
    
    previous = entry; // Update previous file
    Serial.println("previous file is: " + String(previous.name()));
  }
  return File(); // Return empty File if previous file not found
}

