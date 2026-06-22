#include <LittleFS.h>

void setup() {
  Serial.begin(115200);
  
  // Wait up to 5 seconds for the Serial Monitor to be opened
  while (!Serial && millis() < 5000); 
  
  Serial.println("\n--- Initializing LittleFS ---");

  // Mount the LittleFS file system
  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed! Check your Flash Size settings.");
    return;
  }

  Serial.println("LittleFS Mounted Successfully.");
  Serial.println("--- Listing Root Directory Files ---");

  // Open the root directory
  File root = LittleFS.open("/", "r");
  if (!root) {
    Serial.println("Failed to open root directory.");
    return;
  }

  // Iterate and print all files found
  File file = root.openNextFile();
  if (!file) {
    Serial.println("No files found. The directory is empty.");
  }

  while (file) {
    Serial.print("  File: ");
    Serial.print(file.name());
    Serial.print("\tSize: ");
    Serial.print(file.size());
    Serial.println(" bytes");
    
    file = root.openNextFile(); // Move to the next file
  }
  
  Serial.println("--- End of File List ---");
}

void loop() {
  // Empty loop
}