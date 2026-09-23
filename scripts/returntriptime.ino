#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "yourRouterName";
const char* password = "yourWifiPasswd";

WiFiUDP udp;
const char* pcIp = "192.168.0.20";  // Replace with your PC's IP
const int pcPort = 8888;
const int localPort = 9999;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("WiFi connected.");
  udp.begin(localPort);
}

void loop() {
  unsigned long sendTime = millis();
  udp.beginPacket(pcIp, pcPort);
  udp.write((uint8_t*)&sendTime, sizeof(sendTime));
  udp.endPacket();

  // Wait for response (timeout 100ms)
  unsigned long startWait = millis();
  unsigned long recvTime;
  unsigned long echoedTime;
  bool flag = 0;
  while (  !flag && ((millis() - startWait) < 100)) {
    delay(1);
    flag = udp.parsePacket();
    if (flag != 0){
      recvTime = millis();
      udp.read((char*)&echoedTime, sizeof(echoedTime));  
      Serial.println("Reply received. Echo - Send time = ");
      Serial.println(sendTime- echoedTime);
    }
  }
  if (flag != 0 ) {
    unsigned long rtt = recvTime - echoedTime;
    Serial.print("RTT: ");
    Serial.print(rtt);
    Serial.print(" ms | Latency: ");
    Serial.print(rtt / 2.0);
    Serial.println(" ms");
  }
  else{
    Serial.println("NO Reply received.");
  }
  delay(100);  // Wait 1 second before next ping
}
