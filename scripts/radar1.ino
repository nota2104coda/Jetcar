/*
 * 
 * Frame header: 4 bytes - 0xF4 0xF3 0xF2 0xF1 
 * Data length in frame: 2 bytes
 * In-frame data
 *  Target quantity: 1 byte
 *  Alarm information: 1 byte (target aproacing = 01)
 *  Angle in degrees = reported value - 0x80: 1 byte
 *  Distance in meters: 1 byte
 *  Speed direction (01 = approach, 00 = away): 1 byte
 *  Speed (in km/h, max 120): 1 byte
 *  Signal to noise ratio: 0-255: 1 byte
 *  
 *  In-frame data is sent for each target (max 5)
 *  
 *  End frame: 4 bytes - 0xF8 0xF7 0xF6 0xF5
 * 
*/

//#include<SoftwareSerial.h>
//SoftwareSerial Serial1(2,3);
int j=0;


void setup()
{
  Serial.begin(9600);
  delay(100);
  Serial.println("RadarTest");
  Serial1.begin(115200);
  Serial.println("Started dataread");
}
void readData() 
{
  int messageSize=0, i=0;
  byte dataSize[2];
  if(Serial1.available()) {
     Serial1.readBytes(dataSize, 2); //The next 2 bytes tells the size in bytes of the dataframe thata follows
  }
  Serial.print("Messagesize 1:"); //Debug
  Serial.print(dataSize[0]); //Debug
  Serial.print(" Messagesize 2:"); //Debug
  Serial.println(dataSize[1]); //Debug
  messageSize = word(dataSize[1], dataSize[0]);
  Serial.print("messageSize = "); //Debug
  Serial.println(messageSize); //Debug
  messageSize = messageSize+4; //Add the 4 "end of message" bytes to the total length
  byte data[messageSize];
  if(Serial1.available()) {
     Serial1.readBytes(data, messageSize); //Read all the data and the 4 endbytes
  }
  while((messageSize)>0) {  //Display all the data in hex format
      Serial.print((String)"Data["+i+"] = ");
      Serial.println(data[i], HEX); 
      i++;
      messageSize--;
  }
}

void loop(){
  byte response[] = {0xF4, 0xF3, 0xF2, 0xF1 }; //Expected response from radar.
  if(Serial1.available()) {
    int temp = Serial1.read();
    if(temp!=response[j]) { //Check if read data matches with expected response
      Serial.print("Read fail Data = "); //Debug //We did not get the response we expected in the order that we expected
      Serial.println(temp, HEX); //Debug
      Serial.print("J = "); //Debug
      Serial.println(j);  //Debug
      j=0; //Start over, looking for correct byte 0
    }
    else { 
      j++;
      Serial.print("Read ok, Data = "); //Debug
      Serial.println(temp, HEX); //Debug
      Serial.print("J = "); //Debug
      Serial.println(j); //Debug
    }
    if(j>3) {  //Now we have the 4 message startbytes.
      readData(); //Read dataframe
      j=0; //Start over looking for the 4 startbytes.
    }  
  }
}
