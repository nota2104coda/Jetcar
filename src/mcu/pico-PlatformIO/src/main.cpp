#include <ld06.h>


LD06 lidar(Serial1);
void setup() {
  Serial.begin(115200); // Debug output 
  lidar.init(); // Initialize UART at 230400 baud 
  lidar.enableCRC(); // Enable checksum validation 
  lidar.enableFullScan(); // Detect full 360° scans 
}

void loop() {
  if (lidar.readScan()) { // Returns true when a full 360° scan is ready 
   lidar.printScanCSV(Serial);
  } 
}