//Declare pin functions for Teensy
#define stp 3
#define dir 4
#define EN  6
#define MS1 9
#define MS2 10
#define led 13

//Declare variables for functions
char user_input;
int x;
int y;
int state;

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
  resetEDPins(); //Set step, direction, microstep and enable pins to default states
  Serial.begin(9600); //Open Serial connection for debugging
  Serial.println("Begin motor control");
  Serial.println();
  //Print function list for user selection
  Serial.println("Enter number for control option:");
  Serial.println("1. Turn at default microstep mode.");
  Serial.println("2. Reverse direction at default microstep mode.");
  Serial.println("3. Turn at 1/8th microstep mode.");
  Serial.println("4. Step forward and reverse directions.");
  Serial.println();
}


void loop() {
  // create the serial packet receive buffer
  static byte buffer[PACKET_MAX_BYTES];
  int count = 0;
  int packetSize = PACKET_MIN_BYTES;

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
          if(buffer[2] == 'M'){
            digitalWrite(led, HIGH);
            delay(1000);
            digitalWrite(led, LOW);
            delay(500);
            digitalWrite(led, HIGH);
            delay(1000);
            digitalWrite(led, LOW);
            sendPacket(packetSize - PACKET_OVERHEAD_BYTES, buffer + 2);
          }
          else{
            digitalWrite(led, HIGH);
            delay(200);
            digitalWrite(led, LOW);
            sendPacket(packetSize - PACKET_OVERHEAD_BYTES, buffer + 2);  
          }
        }
        // reset the count
        count = 0;
      }
    }
  }
}

//Default microstep mode function
void StepForwardDefault()
{
  Serial.println("Moving forward at default step mode.");
  digitalWrite(dir, LOW); //Pull direction pin low to move "forward"
  for(x = 0; x < 1000; x++)  //Loop the forward stepping enough times for motion to be visible
  {
    digitalWrite(stp,HIGH); //Trigger one step forward
    delay(1);
    digitalWrite(stp,LOW); //Pull step pin low so it can be triggered again
    delay(1);
  }
  Serial.println("Enter new option");
  Serial.println();
}

//Reverse default microstep mode function
void ReverseStepDefault()
{
  Serial.println("Moving in reverse at default step mode.");
  digitalWrite(dir, HIGH); //Pull direction pin high to move in "reverse"
  for(x = 0; x < 1000; x++)  //Loop the stepping enough times for motion to be visible
  {
    digitalWrite(stp,HIGH); //Trigger one step
    delay(1);
    digitalWrite(stp,LOW); //Pull step pin low so it can be triggered again
    delay(1);
  }
  Serial.println("Enter new option");
  Serial.println();
}

// 1/8th microstep foward mode function
void SmallStepMode()
{
  Serial.println("Stepping at 1/8th microstep mode.");
  digitalWrite(dir, LOW); //Pull direction pin low to move "forward"
  digitalWrite(MS1, HIGH); //Pull MS1, and MS2 high to set logic to 1/8th microstep resolution
  digitalWrite(MS2, HIGH);
  for(x = 0; x < 1000; x++)  //Loop the forward stepping enough times for motion to be visible
  {
    digitalWrite(stp,HIGH); //Trigger one step forward
    delay(1);
    digitalWrite(stp,LOW); //Pull step pin low so it can be triggered again
    delay(1);
  }
  Serial.println("Enter new option");
  Serial.println();
}

//Forward/reverse stepping function
void ForwardBackwardStep()
{
  Serial.println("Alternate between stepping forward and reverse.");
  for(x = 0; x < 5; x++)  //Loop the forward stepping enough times for motion to be visible
  {
    //Read direction pin state and change it
    state=digitalRead(dir);
    if(state == HIGH)
    {
      digitalWrite(dir, LOW);
    }
    else if(state ==LOW)
    {
      digitalWrite(dir,HIGH);
    }
    for(y = 0; y < 1000; y++)
    {
      digitalWrite(stp,HIGH); //Trigger one step
      delay(1);
      digitalWrite(stp,LOW); //Pull step pin low so it can be triggered again
      delay(1);
    }
  }
  Serial.println("Enter new option:");
  Serial.println();
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
