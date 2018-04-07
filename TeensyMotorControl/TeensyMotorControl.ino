//Declare pin functions for Teensy
#define stp 8
#define dir 9
#define EN  5
#define MS1 6
#define MS2 7
#define FLO 14
#define led 13

//Declare variables for functions
char user_input;
int x;
int y;
int i,j;
int state;
volatile int flowCount = 0;
 
// define packet parameters
const byte PACKET_START_BYTE = 0xAA;
const unsigned int PACKET_OVERHEAD_BYTES = 3;
const unsigned int PACKET_MIN_BYTES = PACKET_OVERHEAD_BYTES + 1;
const unsigned int PACKET_MAX_BYTES = 255;

void setup() {
  // put your setup code here, to run once:
  pinMode(stp, OUTPUT);
  pinMode(dir, OUTPUT);
  pinMode(MS1, OUTPUT);
  pinMode(MS2, OUTPUT);
  pinMode(EN, OUTPUT);
  pinMode(led, OUTPUT);
  pinMode(FLO, INPUT);
  attachInterrupt(digitalPinToInterrupt(FLO), CountFlow, FALLING);
  resetEDPins(); //Set step, direction, microstep and enable pins to default states
  Serial.begin(9600); //Open Serial connection for debugging
}


void loop() {
  // create the serial packet receive buffer
  static byte buffer[PACKET_MAX_BYTES];
  int count = 0;
  int packetSize = PACKET_MIN_BYTES;
  interrupts();
  // continuously check for received packets
  while(true)
  {
      // check to see if serial byte is available
    if(Serial.available())
    {
      // get the byte
      byte b = Serial.read();

      // handle the byte according to the current count
      if(count == 0 && b == PACKET_START_BYTE){
        // this byte signals the beginning of a new packet
        buffer[count] = b;
        count++;
        continue;
      }
      else if(count == 0){
        // the first byte is not valid, ignore it and continue
        continue;
      }
      else if(count == 1){
        // this byte contains the overall packet length
        buffer[count] = b;
        // reset the count if the packet length is not in range
        if(packetSize < PACKET_MIN_BYTES || packetSize > PACKET_MAX_BYTES){
          count = 0;
        }
        else{
          packetSize = b;
          count++;
        }
        continue;
      }
      else if(count < packetSize){
        // store the byte
        buffer[count] = b;
        count++;
      }
      // check to see if we have acquired enough bytes for a full packet
      if(count >= packetSize){
        // validate the packet
        if(validatePacket(packetSize, buffer)){
          if(buffer[2] == 'M' && packetSize == 7){
            digitalWrite(led, HIGH);
            delay(1000);
            digitalWrite(led, LOW);
            delay(500);
            digitalWrite(led, HIGH);
            delay(1000);
            digitalWrite(led, LOW);
            noInterrupts();
            TurnMotor(buffer[3], buffer[4], buffer[5]);
            sendPacket(packetSize - PACKET_OVERHEAD_BYTES, buffer + 2);
            interrupts();
          }
          else if(buffer[2] == 'F'){
            digitalWrite(led, HIGH);
            delay(200);
            digitalWrite(led, LOW);
            noInterrupts();
            SendFlow();
            interrupts();
          }
          else if(buffer[2] == 'T'){
            digitalWrite(led, HIGH);
            delay(1000);
            digitalWrite(led, LOW);
            noInterrupts();
            sendPacket(packetSize - PACKET_OVERHEAD_BYTES, buffer + 2);
            interrupts();
          }
        }
        // reset the count
        count = 0; 
      }
    }
  }
}

//Default microstep mode function 
void TurnMotor(char direc, int steps, int multi)
{
  digitalWrite(EN, LOW);
  if(direc == 'F'){
    digitalWrite(dir, LOW); //Pull direction pin low to move "forward"
  }
  else if(direc == 'B'){
    digitalWrite(dir, HIGH);
  }
  for(i = 0; i < multi; i++)  //Loop the forward stepping enough times for motion to be visible
  {
    for(j = 0; j < steps; j++)
    {
      digitalWrite(stp,HIGH); //Trigger one step forward
      delay(1);
      digitalWrite(stp,LOW); //Pull step pin low so it can be triggered again
      delay(1);
    }
  }
  flowCount = 0;
  resetEDPins();
}

boolean SendFlow()
{
  // the payload size will stay constant
  unsigned int packetSize = 2 + PACKET_OVERHEAD_BYTES;
  // create the serial packet transmit buffer
  static byte packet[PACKET_MAX_BYTES];
  // populate the overhead fields
  packet[0] = PACKET_START_BYTE;
  packet[1] = packetSize;
  byte checkSum = packet[0] ^ packet[1];
  // populate the packet payload while computing the checksum
  packet[2] = 'F';
  checkSum = checkSum ^ packet[2];
  packet[3] = flowCount;
  packet[4] = checkSum ^ packet[3];
  // send the packet
  Serial.write(packet, packetSize);
  Serial.flush();
  flowCount = 0;
  return true;
}

//Reset Easy Driver pins to default states
void resetEDPins()
{
  digitalWrite(stp, LOW);
  digitalWrite(dir, LOW);
  digitalWrite(MS1, LOW);
  digitalWrite(MS2, LOW);
  digitalWrite(EN, HIGH);
}

boolean validatePacket(unsigned int packetSize, byte *packet)
{
  // check the packet size
  if(packetSize < PACKET_MIN_BYTES || packetSize > PACKET_MAX_BYTES)
  {
      return false;
  }
  // check the start byte
  if(packet[0] != PACKET_START_BYTE)
  {
    return false;
  }
  // check the length byte
  if(packet[1] != packetSize)
  {
    return false;
  }
  // compute the checksum
  byte checksum = 0x00;
  for(int i = 0; i < packetSize - 1; i++)
  {
    checksum = checksum ^ packet[i];
  }
  // check to see if the computed checksum and packet checksum are equal
  if(packet[packetSize - 1] != checksum)
  {
    return false;
  }
  // all validation checks passed, the packet is valid
  return true;
}

boolean sendPacket(unsigned int payloadSize, byte *payload)
{
  // check for max payload size
  unsigned int packetSize = payloadSize + PACKET_OVERHEAD_BYTES;
  if(packetSize > PACKET_MAX_BYTES){
    return false;
  }
  // create the serial packet transmit buffer
  static byte packet[PACKET_MAX_BYTES];
  // populate the overhead fields
  packet[0] = PACKET_START_BYTE;
  packet[1] = packetSize;
  byte checkSum = packet[0] ^ packet[1];
  // populate the packet payload while computing the checksum
  for(int i = 0; i < payloadSize; i++){
    packet[i + 2] = payload[i];
    checkSum = checkSum ^ packet[i + 2];
  }
  // store the checksum
  packet[packetSize - 1] = checkSum;
  // send the packet
  Serial.write(packet, packetSize);
  Serial.flush();
  return true;
}

void CountFlow()
{
  digitalWrite(led, HIGH);
  delayMicroseconds(50000);
  digitalWrite(led, LOW);
  flowCount++;
}

