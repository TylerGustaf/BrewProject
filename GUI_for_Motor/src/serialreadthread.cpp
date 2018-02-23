#include <glib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "global.h"

#define READ_THREAD_SLEEP_DURATION_US 100000	//!< Duration to sleep while waiting for input
#define BETWEEN_CHARACTERS_TIMEOUT_US 1000		//!< Timeout time between character input
#define FLOW_COMMAND 'F'	//!< The command that indicates the following data in the packet is flowmeter data

// define packet parameters
#define PACKET_START_BYTE  0xAA	//!< The start of each packet
#define PACKET_OVERHEAD_BYTES  3	//!< The amount of overhead bytes for each packet
#define PACKET_MIN_BYTES  PACKET_OVERHEAD_BYTES + 1	//!< The minimum possible amount of bytes per packet
#define PACKET_MAX_BYTES  255	//!< Max amount of bytes per packet


bool validatePacket(unsigned int packetSize, unsigned char *packet);

/*!
 * \brief Reads packages recieved from the Teensy.
 * \details Reads in one byte at a time and once it recieves a full package
 * it validates it and if the first byte of the payload is a P then it converts the two
 * potentiometer bytes into a single value and calculates the voltage then sets it to a variable for printing
 */
gpointer Serial_Read_Thread()
{
    ssize_t r_res;
    char ob[50];					//Holds the bytes of the package sent by the Teensy
    unsigned int count=0;		// Counts how many bytes of the current package have been recieved
    static unsigned char buffer[PACKET_MAX_BYTES];	//Holds the full package recieved from the Teensy
    unsigned int packetSize = PACKET_MIN_BYTES;		//Holds the size of the packet recieved from the Teensy
    double flow_rate;		//Holds the calculated flow rate from the flowmeter
    //int pulses;			//Holds the number of pulses

    //Continuously listen for packets from teensy
    while(!kill_all_threads){  
        if(ser_dev!=-1){  
            r_res = read(ser_dev,ob,1);
            if(r_res==0){
                usleep(BETWEEN_CHARACTERS_TIMEOUT_US);
            }
            else if(r_res<0){
                cerr<<"Read error:"<<(int)errno<<" ("<<strerror(errno)<<")"<<endl;
            }
            //this means we have received a byte, the byte is in ob[0]
            else{
                //handle the byte according to the current count
                if(count == 0 && ob[0] == PACKET_START_BYTE){
                    //this byte signals the beginning of a new packet
                    buffer[count] = ob[0];
                    count++;
  	            continue;
                }
       	        else if(count == 0){
                    //the first byte is not valid, ignore it and continue
       	            continue;
       	        }
       	        else if(count == 1){    
                    //this byte contains the overall packet length
       	            buffer[count] = ob[0];
                    //reset the count if the packet length is not in range
                    if(buffer[count] < PACKET_MIN_BYTES || buffer[count] > PACKET_MAX_BYTES){  
                        count = 0;
                    }
                    else{  //Otherwise store the packet size and increment count
                        packetSize = ob[0];
                        count++;
                    }
                    continue;
                }  
                else if(count < packetSize){ //If the byte is part of the payload
                    //store the byte
                    buffer[count] = ob[0];
                    count++;
                }
                //check to see if we have acquired enough bytes for a full packet
                if(count >= packetSize){
                    if(validatePacket(packetSize, buffer)){  //If the packet is valid,
                        //Use the data from the packet
                        g_mutex_lock(mutex_to_protect_voltage_display);
                        sprintf(label_recieved_value,"%s %d", "Value recieved!", (int)buffer[3]);
                        g_mutex_unlock(mutex_to_protect_voltage_display);
                    }
                    //reset the count
                    count = 0;
                }  
            }
        }
        else
            usleep(READ_THREAD_SLEEP_DURATION_US);
    }
    return NULL;
}
/*!
 * \brief Validates the packet recieved from the Teensy
 * \param packetSize is the size of the packet, packet is a pointer to the packet itself
 * \details Checks every byte of the packet and validates the checksum at the end
 */
bool validatePacket(unsigned int packetSize, unsigned char *packet)
{
    //check the packet size      
    if(packetSize < PACKET_MIN_BYTES || packetSize > PACKET_MAX_BYTES){
        return false;
    }
    //check the start byte
    if(packet[0] != PACKET_START_BYTE){
        return false;
    }
    //check the length byte
    if(packet[1] != packetSize){
        return false;
    }
    //compute the checksum
    char checksum = 0x00;		//The calculated checksum of the packet
    for(unsigned int i = 0; i < packetSize - 1; i++){
        checksum = checksum ^ packet[i];
    }
    // check to see if the computed checksum and packet checksum are equal
    if(packet[packetSize - 1] != checksum){
        return false;
    }
    // all validation checks passed, the packet is valid
    return true;
}
