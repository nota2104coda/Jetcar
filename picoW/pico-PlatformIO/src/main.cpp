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

// buffers for receiving and sending data
char packetBuffer[UDP_TX_PACKET_MAX_SIZE + 1];  // buffer to hold incoming packet,
char ReplyBuffer[] = "acknowledged\r\n";        // a string to send back

WiFiUDP Udp;

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
}

void loop() {
  // // if there's data available, read a packet
  // int packetSize = Udp.parsePacket();
  // if (packetSize) {
  //   Serial.printf("Received packet of size %d from %s:%d\n    (to %s:%d)\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort(), Udp.destinationIP().toString().c_str(), Udp.localPort());

  //   // read the packet into packetBufffer
  //   int n = Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
  //   packetBuffer[n] = 0;
  //   Serial.println("Contents:");
  //   Serial.println(packetBuffer);

  //   // send a reply, to the IP address and port that sent us the packet we received
  //   Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  //   Udp.write(ReplyBuffer);
  //   Udp.endPacket();
  // }
   // Calculate elapsed time in seconds
  float elapsedTime = (millis() - startTime) / 1000.0;
  // Calculate the sine wave value
  float sineValue = AMPLITUDE * sin(2 * PI * FREQUENCY * elapsedTime) + OFFSET;

  // Convert float to a String
  String message = String(sineValue, 2); // Format to 2 decimal places
  //message += "\n"; // Add a newline character as a delimiter for Processing
  // Send the UDP packet
  Udp.beginPacket(PC_IP, PC_PORT); // Start sending to PC_IP on PC_PORT
  Udp.print(message);              // Write the message string
  Udp.endPacket();                 // Send the packet

  Serial.print("Sent: ");        // Uncomment for debugging on Serial Monitor
  Serial.println(message);

  delay(100); // Send data approximately 100 times per second (adjust as needed)
}
