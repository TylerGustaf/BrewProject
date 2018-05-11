/*!
 * \author Tyler Gustaf
 * \version 1.0
 * \date 5-3-2018
 * \bug The application does not terminate properly by clicking the X on the main window
*  \bug The application does not automatically terminate when it does not sucessfully connect to the Teensy.
 *
 * \copyright GNU Public License
 * \mainpage The Pi-Flow Controller
 * \section intro_sec Introduction
 * This code is used along with the TeensyMotorController.ino code to control the Flow Valve connected to the Teensy.
 * \section compile_sec Compilation
 * The following will explain how to complie the code using CMake and Make
 * \subsection CMake
 * While in the build directory run the command "cmake .."
 * \subsection Make
 * After CMake has been executed run the "make" command while still in the build directory
 */
#include "global.h"
#include "string.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <glib.h>
#include <errno.h>
#include <ctime>

#define FLOW_LABEL_UPDATE_MS 100	//!< Time(in milliseconds) between calling the UpdateFlowLabel function
#define READ_THREAD_SLEEP_DURATION_US 100000	//!< Duration to sleep while waiting for input
#define BETWEEN_CHARACTERS_TIMEOUT_US 1000		//!< Timeout time between character input

 #define GuiappGET(xx) gui_app->xx=GTK_WIDGET(gtk_builder_get_object(p_builder,#xx)) //!< Defines an easier way to access a widget

/*!
 * \brief Connects the widget pointers to the glade widgets
 * \param p_builder is the pointer to the builder structure
 * \details  All glade widgets that are being connected must exist on the GUI for this function to work.
 */
void ObtainGuiWidgets(GtkBuilder *p_builder)
{
  GuiappGET(window1);
  GuiappGET(exitbutton);
  GuiappGET(StartButton);
  GuiappGET(PauseButton);
  GuiappGET(FOpenButton);
  GuiappGET(FCloseButton);
  GuiappGET(TargetFlowInput);
  GuiappGET(FlowLabel);
}
/*
**Constants and function prototypes
*/
const char PACKET_START_BYTE = 0xAA;
const char MOTOR_COMMAND = 'M';
const char FLOW_COMMAND = 'F';
const char TEST_COMMAND = 'T';
const unsigned int PACKET_OVERHEAD_BYTES = 3;
const unsigned int PACKET_MIN_BYTES = PACKET_OVERHEAD_BYTES + 1;
const unsigned int PACKET_MAX_BYTES = 255;
const int MAX_NUM_OF_STEPS = 2000;

bool TurnMotor(char, int, int);
double GetFlow(time_t);
int GetSerialPacket();

/*!
 * \brief Updates the current flow that is displayed to the user.
 */
gboolean  UpdateFlowLabel(gpointer p_gptr)
{
  // do not change this function
  g_mutex_lock(flow_label_mutex);
  gtk_label_set_text(GTK_LABEL(gui_app->FlowLabel),label_recieved_value);
  g_mutex_unlock(flow_label_mutex);
  return true;
}

/*!
 * \brief Fully opens the valve from wherever it is.
 */
bool FullyOpen()
{
    int steps = MAX_NUM_OF_STEPS - numOfSteps;
    int multi = 0;
    while(steps >= 200){
        steps -= 200;
        multi++;
    }
    TurnMotor('B', 200, multi);
    TurnMotor('B', steps, 1);
    numOfSteps = MAX_NUM_OF_STEPS;
    return true;
}
/*!
 * \brief Fully closes the valve from wherever it is.
 */
bool FullyClose()
{
    int steps = numOfSteps;
    int multi = 0;
    while(steps >= 200){
        steps -= 200;
        multi++;
    }
    TurnMotor('F', 200, multi);
    TurnMotor('F', steps, 1);
    numOfSteps = 0;
    return true;
}

/*!
 * \brief Controls the valve based on the current flow from the flow sensor.
* \details This function is created as a thread when the Start Button is pressed. Once created, this function will read the current flow from the flow sensor and then either open or close the valve to better achieve the target flow.
 */
gpointer MasterLogic()
{
    time_t startTime;
    int steps = 0, multi = 0;
    double smallError = targetFlow*0.25;
    double flowRate;
    startTime = time(0);
    flowRate = GetFlow(startTime);
    startTime = time(0);
    usleep(1000000); //sleep for 1 second
    while(!kill_all_threads){
        flowRate = GetFlow(startTime);
        startTime = time(0);

        g_mutex_lock(flow_label_mutex);
        sprintf(label_recieved_value,"%.2f", flowRate);
        g_mutex_unlock(flow_label_mutex);

        //Handle Small Error
        if(flowRate < (targetFlow - smallError)){
            steps = 200;
            multi = 1;
            if((numOfSteps+(steps*multi)) > MAX_NUM_OF_STEPS){
                steps -= (((numOfSteps+(steps*multi)) - MAX_NUM_OF_STEPS) / multi);
            }
            TurnMotor('B', steps, multi);
            startTime = time(0);
            numOfSteps += (steps*multi);
  printf("Opening Small Error\n"); //Removing these print statements caused the program to not behave properly
        }
        else if(flowRate > (targetFlow + smallError)){
            steps = 200;
            multi = 1;
            if((numOfSteps-(steps*multi)) < 0){
                steps += ((numOfSteps-(steps*multi)) / multi);
            }
            TurnMotor('F', steps, multi);
            startTime = time(0);
            numOfSteps -= (steps*multi);
  printf("Closing Small Error\n"); //Removing these print statements caused the program to not behave properly
        }
        usleep(1000000); //sleep for 1 second

    }//end of while loop

}//end of MasterLogic
/*!
 * \brief Sends a message to the Teensy to turn the motor.
 * \param motorDirection specifies if the motor should be opened or closed.
* \param numOfSteps must be between 0-255.
* \param stepMultiplier indicates how many times numOfSteps should be executed.
* \details Sends a message to the Teensy and then waits for a response. If the correct response is recieved this function returns true.
 */
bool TurnMotor(char motorDirection, int numOfSteps, int stepMultiplier)
{
    if(!(motorDirection == 'F' || motorDirection == 'B')){
        return false;
    }
    int length_send_buff = 7;				//Length of the package to be sent to the Teensy
    char send_buff[length_send_buff];		//Character array to hold the package that will be sent to the Teensy

    //Set up the buffer with the start byte, packet size, command byte, and checksum
    send_buff[0] = PACKET_START_BYTE;
    send_buff[1] = length_send_buff;
    send_buff[2] = MOTOR_COMMAND;
    send_buff[3] = motorDirection;
    send_buff[4] = numOfSteps;
    send_buff[5] = stepMultiplier;
    send_buff[6] = send_buff[0] ^ send_buff[1];
    send_buff[6] = send_buff[6] ^ send_buff[2];
    send_buff[6] = send_buff[6] ^ send_buff[3];
    send_buff[6] = send_buff[6] ^ send_buff[4];
    send_buff[6] = send_buff[6] ^ send_buff[5];    
    //this is how you send an array out on the serial port:
    write(ser_teensy1, send_buff, length_send_buff);

    if(GetSerialPacket() == 1){
        return true;
    }
    else{
        return false;
    }
}
/*!
 * \brief Calculates the current flow through the valve
 * \param System time
 * \details Using the system time parameter, this function will return the flow rate since that time.
 */
double GetFlow(time_t startTime)
{
    time_t endTime;
    double flowRate;
    int length_send_buff = 4;				//Length of the package to be sent to the Teensy
    char send_buff[length_send_buff];		//Character array to hold the package that will be sent to the Teensy
    //Set up the buffer with the start byte, packet size, command byte, and checksum
    send_buff[0] = PACKET_START_BYTE;
    send_buff[1] = length_send_buff;
    send_buff[2] = FLOW_COMMAND;
    send_buff[3] = send_buff[0] ^ send_buff[1];
    send_buff[3] = send_buff[3] ^ send_buff[2];
    //this is how you send an array out on the serial port:
    write(ser_teensy1, send_buff, length_send_buff);

    flowRate = GetSerialPacket();
    endTime = time(0);
    if((endTime - startTime) == 0){
        return 0.0;
    }
    flowRate = (flowRate * 6.50) / (endTime - startTime);
    return flowRate;
}
/*!
 * \brief Validates a packet
 * \param Size of the packet and a pointer to the packet
 * \details This function validates a packet recieved from the Teensy.
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
/*!
 * \brief Function that recieves a packet from the Teensy
 * \details This function collects a packet from the Teensy, validates it, and then returns the important information from it. 
 */
int GetSerialPacket()
{
    ssize_t r_res;
    char ob[50];					//Holds the bytes of the package sent by the Teensy
    unsigned int count=0;		// Counts how many bytes of the current package have been recieved
    static unsigned char buffer[PACKET_MAX_BYTES];	//Holds the full package recieved from the Teensy
    unsigned int packetSize = PACKET_MIN_BYTES;		//Holds the size of the packet recieved from the Teensy

    //Continuously listen for packets from teensy
    while(!kill_all_threads){  
        if(ser_teensy1!=-1){  
            r_res = read(ser_teensy1,ob,1);
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
                        if(buffer[2] == 'M'){
                            return 1;
                        }
                        else if(buffer[2] == 'F'){
                            return (buffer[3] + (buffer[4]*256));
                        }
                        else if(buffer[2] == 'T'){
                            return 1007;
                        }
                        else{
                            return 0;
                        }
                    }
                    //reset the count
                    count = 0;
                }  
            }
        }
        else
            usleep(READ_THREAD_SLEEP_DURATION_US);
    }
    return 0;
}
/*!
 * \brief Callback for when the FullyClose button is clicked
 * \param Standard parameters for callback function
 * \details Fullyopens the valve.
 */
extern "C" void FO_Button_Clicked(GtkWidget *p_wdgt, gpointer p_data )
{
    FullyOpen();
}
/*!
 * \brief Callback for when the FullyClose button is clicked
 * \param Standard parameters for callback function
 * \details Fully closes the valve.
 */
extern "C" void FC_Button_Clicked(GtkWidget *p_wdgt, gpointer p_data )
{
    FullyClose();
}
/*!
 * \brief Callback for when the Start button is clicked
 * \param Standard parameters for callback function
 * \details Creates a MasterLogic thread if there isn't one already and then disables all buttons except the exit button and enables the pause button.
 */
extern "C" void Start_Button_Clicked(GtkWidget *p_wdgt, gpointer p_data ) 
{
    g_mutex_lock(master_logic_mutex);
    targetFlow = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gui_app->TargetFlowInput));
    if(targetFlow > 0){
        if(makeMLThread){
            makeMLThread = false;
            //this is how you disable a button:
            gtk_widget_set_sensitive (gui_app->StartButton,FALSE);
            gtk_widget_set_sensitive (gui_app->FCloseButton,FALSE);
            gtk_widget_set_sensitive (gui_app->FOpenButton,FALSE);
            //this is how you enable a button:
            gtk_widget_set_sensitive (gui_app->PauseButton,TRUE);

            // this is used to signal all threads to exit
            kill_all_threads=false;
            GThread *master_thread;
            //spawn the master logic thread
            master_thread = g_thread_new(NULL,(GThreadFunc)MasterLogic,NULL);
        }
    }
    g_mutex_unlock(master_logic_mutex);

}
/*!
 * \brief Callback for when the Pause button is clicked
 * \param Standard parameters for callback function
 * \details Ends the MasterLogic thread and enables all other buttons.
 */
extern "C" void Pause_Button_Clicked(GtkWidget *p_wdgt, gpointer p_data ) 
{
    g_mutex_lock(master_logic_mutex);
    kill_all_threads=true;
    gtk_widget_set_sensitive (gui_app->StartButton,TRUE);
    gtk_widget_set_sensitive (gui_app->FCloseButton,TRUE);
    gtk_widget_set_sensitive (gui_app->FOpenButton,TRUE);
    gtk_widget_set_sensitive (gui_app->PauseButton,FALSE);
    makeMLThread = true;
    g_mutex_unlock(master_logic_mutex);
}

/*!
 * \brief Callback for when the exit button is clicked
 * \param Standard parameters for callback function
 * \details Properly terminates the GUI
 */
extern "C" void button_exit_clicked(GtkWidget *p_wdgt, gpointer p_data ) 
{
  kill_all_threads = true;
  //do not change the next two lines; they close the serial port
  close(ser_teensy1);
  ser_teensy1=-1;
  gtk_main_quit();
}

bool ConnectTeensy()
{
  //do not change  the next few lines
  //they contain the mambo-jumbo to open a serial port
  const char *teensy_serial_port;	//Holds the string entered in entry_sd
  struct termios my_serial;
  teensy_serial_port = "/dev/ttyACM0";
  //open serial port with read and write, no controling terminal (we don't
  //want to get killed if serial sends CTRL-C), non-blocking 
  ser_teensy1 = open(teensy_serial_port, O_RDWR | O_NOCTTY );
  bzero(&my_serial, sizeof(my_serial)); // clear struct for new port settings 
  //B9600: set baud rate to 9600
  //   CS8     : 8n1 (8bit,no parity,1 stopbit)
  //   CLOCAL  : local connection, no modem contol
  //   CREAD   : enable receiving characters  */
  my_serial.c_cflag = B9600 | CS8 | CLOCAL | CREAD; 
  tcflush(ser_teensy1, TCIFLUSH);
  tcsetattr(ser_teensy1,TCSANOW,&my_serial);
  //You can add code beyond this line but do not change anything above this line
  int length_send_buff = 4;				//Length of the package to be sent to the Teensy
  char send_buff[length_send_buff];		//Character array to hold the package that will be sent to the Teensy
  //Set up the buffer with the start byte, packet size, command byte, and checksum
  send_buff[0] = PACKET_START_BYTE;
  send_buff[1] = length_send_buff;
  send_buff[2] = TEST_COMMAND;
  send_buff[3] = send_buff[0] ^ send_buff[1];
  send_buff[3] = send_buff[3] ^ send_buff[2];
  //this is how you send an array out on the serial port:
  write(ser_teensy1, send_buff, length_send_buff);

  if(GetSerialPacket() == 1007){
    return true;
  }
  return false;
}



//********************************************************************
//********************************************************************
// 
//   Main loop
//
//********************************************************************
//********************************************************************
/*!
 * \brief Sets up the GUI pointers, the mutex, and everything else needed for the GUI to run properly
 */
int main(int argc, char **argv)
{
  GtkBuilder *builder;
  GError *err = NULL;
        sprintf(label_recieved_value,"%.2f", 0.00);
  //GThread *read_thread;

  //this is how you allocate a Glib mutex
  g_assert(master_logic_mutex == NULL);
  master_logic_mutex = new GMutex;
  g_mutex_init(master_logic_mutex);
  g_assert(flow_label_mutex == NULL);
  flow_label_mutex = new GMutex;
  g_mutex_init(flow_label_mutex);

  // this is used to signal all threads to exit
  kill_all_threads=false;
  
  //spawn the serial read thread
  //read_thread = g_thread_new(NULL,(GThreadFunc)Serial_Read_Thread,NULL);
  
  // Now we initialize GTK+ 
  gtk_init(&argc, &argv);
  
  //create gtk_instance for visualization
  gui_app = g_slice_new(Gui_Window_AppWidgets);

  //builder
  builder = gtk_builder_new();
  gtk_builder_add_from_file(builder, "teensy_control.glade", &err);

  //error handling
  if(err){
     g_error(err->message);
     g_error_free(err);
     g_slice_free(Gui_Window_AppWidgets, gui_app);
     exit(-1);
  }

  // Obtain widgets that we need
  ObtainGuiWidgets(builder);

  // Connect signals
  gtk_builder_connect_signals(builder, gui_app);

  // Destroy builder now that we created the infrastructure
  g_object_unref(G_OBJECT(builder));

  //display the gui
  gtk_widget_show(GTK_WIDGET(gui_app->window1));

  //this is going to call the UpdateFlowLabel function periodically
  gdk_threads_add_timeout(FLOW_LABEL_UPDATE_MS, UpdateFlowLabel, NULL);

  //TODO: If the Teensy does not connect, exit cleanly.

  //Try to connect to the Teensy
  if(ConnectTeensy()){
    //the main loop
    gtk_main();
  }
  else{
    close(ser_teensy1);
    ser_teensy1=-1;
    gtk_main_quit();
  }

  //signal all threads to die and wait for them (only one child thread)
  kill_all_threads=true;
  //g_thread_join(read_thread);
  
  //destroy gui if it still exists
  if(gui_app)
    g_slice_free(Gui_Window_AppWidgets, gui_app);

  return 0;
}
