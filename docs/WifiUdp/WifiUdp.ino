/*
  UDPSendReceive.pde:
  This sketch receives UDP message strings, prints them to the serial port
  and sends an "acknowledge" string back to the sender

  A Processing sketch is included at the end of file that can be used to send
  and received messages for testing with a computer.

  created 21 Aug 2010
  by Michael Margolis

  This code is in the public domain.

  adapted from Ethernet library examples
*/


#include <WiFi.h>
#include <WiFiUdp.h>
#include <math.h>

#ifndef STASSID
#define STASSID "yourRouterName"
#define STAPSK "yourWifiPasswd"
#endif

IPAddress PC_IP(192, 168, 0, 20);           // <<< IMPORTANT: Replace with your PC's ACTUAL IP address (comma-separated)
const unsigned int PC_PORT = 12345;          // Port on your PC to listen for UDP data (must match Processing)
unsigned int localPort = 12345;  // local port to listen on

// --- Signal Configuration ---
const float FREQUENCY = 0.5;    // Frequency of the sine/cosine waves in Hz
const float AMPLITUDE = 100.0;  // Amplitude of the sine/cosine waves
const float OFFSET = 127.0;     // Offset to keep values positive

long startTime; // To track time for sine wave calculation
unsigned long counter = 0; // Simple counter for third signal
// NTP settings


// buffers for receiving and sending data
char packetBuffer[UDP_TX_PACKET_MAX_SIZE + 1];  // buffer to hold incoming packet,
char ReplyBuffer[] = "acknowledged\r\n";        // a string to send back

WiFiUDP Udp;

unsigned long previousMillis = 0;
const long interval = 10; // 10ms interval

struct __attribute__((__packed__)) SensorData {
    uint64_t timestamp_us; // Microseconds since epoch
    float velocity;
};

void setup() {
  Serial.begin(115200);
  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("UDP server on port %d\n", localPort);
  Udp.begin(localPort);
  delay(100); 
  startTime = millis(); // Record the start time
  Serial.println("Pico Sine Wave Generator Ready");
  // Initialize NTP client
  timeClient.begin();
  while(!timeClient.update()) { // Wait until initial sync
      Serial.println("Waiting for NTP sync...");
      delay(500);
  }
  Serial.println("NTP synchronized.");
}

void loop() {
  
   // Calc
  unsigned long currentMillis = millis();
  float elapsedTime = (millis() - startTime) / 1000.0;
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    // Calculate the sine wave value
    float sineValue = AMPLITUDE * sin(2 * PI * FREQUENCY * elapsedTime) + OFFSET;

    SensorData data;
    data.timestamp_us = timestamp_us;
    data.velocity = sineValue;

    // Convert float to a String
    String message = String(sineValue, 2); // Format to 2 decimal places
    //message += "\n"; // Add a newline character as a delimiter for Processing
    // Send the UDP packet
    Udp.beginPacket(PC_IP, PC_PORT); // Start sending to PC_IP on PC_PORT
    //Udp.print(message);              // Write the message string for simple sinewave
    Udp.write((uint8_t*)&data, sizeof(data));  //write the UDP message for time and sinewave
    Udp.endPacket();                 // Send the packet

    Serial.print("Sent: ");        // Uncomment for debugging on Serial Monitor
    Serial.print("time = ");
    Serial.print(timestamp_us);
    Serial.print(", sinevalue = ");
    Serial.println(data.velocity);

    delay(100); // Send data approximately 100 times per second (adjust as needed)
  }
}
/*
  test (shell/netcat):
  --------------------
    nc -u 192.168.pico.address 8888
*/
