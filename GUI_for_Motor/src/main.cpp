/*!
 * \author Tyler Gustaf
 * \version 1.0
 * \date 11-16-2016
 * \bug The application does not terminate properly by clicking the X on the main window
 *
 * \copyright GNU Public License
 * \mainpage The Teensy LED Controller
 * \section intro_sec Introduction
 * This code is used along with the Teensy  Led_and_Pot_Serial code to control the LED connected to the Teensy as well as print the potentiometer voltage which is also connected to the Teensy.
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

#define VOLTAGE_DISPLAY_UPDATE_MS 100	//!< Time(in milliseconds) between calls to voltage display function
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

  GuiappGET(window1);			//The main window for the GUI
  GuiappGET(entry_sd);			//The entry field for the serial port
  GuiappGET(exitbutton);			//The exit button at the bottom of the GUI
  GuiappGET(opendevice);			//The open device button for the serial port
  GuiappGET(closedevice);			//The close device button for the serial port
  GuiappGET(sendbutton);			//The button to send a command to the Teensy
  GuiappGET(label_recieved);		//The display field for the recieved package
  GuiappGET(label_sent);			//The display field for the package being sent to the Teensy
  GuiappGET(teensycommand);		//The entry field for the command to be sent to the Teensy
  GuiappGET(stepnum);			//The entry field for the number of steps the motor should turn    
  GuiappGET(stepmulti);			//The entry field for the multiplier for the number of steps
  GuiappGET(forwardbutton);
  GuiappGET(backbutton);
  GuiappGET(motordir);
}

const char PACKET_START_BYTE = 0xAA;
const char MOTOR_COMMAND = 'M';
const char FLOW_COMMAND = 'F';
const unsigned int PACKET_OVERHEAD_BYTES = 3;
const unsigned int PACKET_MIN_BYTES = PACKET_OVERHEAD_BYTES + 1;
const unsigned int PACKET_MAX_BYTES = 255;

void CommandMotor();
void RequestFlow();

//********************************************************************
// GUI handlers
//********************************************************************
/*!
 * \brief Prints the Voltage value to the display field for the voltage
 * \param gpointer is one of the standard callback function parameters allowing it to change the voltage display value
 * \details Locks the mutex protecting the voltage display and sets the voltage label to the calculated voltage value
 * it then unlocks the mutex and returns true.
*/
gboolean  Voltage_Display_Displayer(gpointer p_gptr)
{
  //do not change this function
  g_mutex_lock(mutex_to_protect_voltage_display);
  gtk_label_set_text(GTK_LABEL(gui_app->label_recieved), label_recieved_value);
  g_mutex_unlock(mutex_to_protect_voltage_display);
  return true;
}
/*!
 * \brief Callback for when the open device button is clicked
 * \param Standard parameters for callback function
 * \details Disables the open device button and enables the close device button. Opens the device from the serial port
 * which has been entered in the entry_sd widget
 */
extern "C" void button_opendevice_clicked(GtkWidget *p_wdgt, gpointer p_data ) 
{
  //do not change  the next few lines
  //they contain the mambo-jumbo to open a serial port
  const char *t_device_value;	//Holds the string entered in entry_sd
  struct termios my_serial;
  t_device_value = gtk_entry_get_text(GTK_ENTRY(gui_app->entry_sd));
  //open serial port with read and write, no controling terminal (we don't
  //want to get killed if serial sends CTRL-C), non-blocking 
  ser_dev = open(t_device_value, O_RDWR | O_NOCTTY );
  bzero(&my_serial, sizeof(my_serial)); // clear struct for new port settings 
  //B9600: set baud rate to 9600
  //   CS8     : 8n1 (8bit,no parity,1 stopbit)
  //   CLOCAL  : local connection, no modem contol
  //   CREAD   : enable receiving characters  */
  my_serial.c_cflag = B9600 | CS8 | CLOCAL | CREAD; 
  tcflush(ser_dev, TCIFLUSH);
  tcsetattr(ser_dev,TCSANOW,&my_serial);
  //You can add code beyond this line but do not change anything above this line

  //this is how you disable a button:
  gtk_widget_set_sensitive (gui_app->opendevice,FALSE);
  //this is how you enable a button:
  gtk_widget_set_sensitive (gui_app->closedevice,TRUE);  
}

/*!
 * \brief Callback for when the close device button is clicked
 * \param Standard parameters for callback function
 * \details Disables the close device button and enables the open device button.Closes the serial port
 * which has been opened by the open device callback
 */
extern "C" void button_closedevice_clicked(GtkWidget *p_wdgt, gpointer p_data ) 
{
  //this is how you disable a button:
  gtk_widget_set_sensitive (gui_app->closedevice,FALSE);
  //this is how you enable a button:
  gtk_widget_set_sensitive (gui_app->opendevice,TRUE);
  //do not change the next two lines; they close the serial port
  close(ser_dev);
  ser_dev=-1;
}

/*!
 * \brief Callback for when the send button is clicked
 * \param Standard parameters for callback function
 * \details Gets the values from red/blue/green value, sets the red/blue/green sliders to the recieved values,
 * prepairs an appropriate package with recieved values, sends package to the Teensy, and sets the txtstring
 * to the sent package to display it to the user.
 */
extern "C" void button_send_clicked(GtkWidget *p_wdgt, gpointer p_data ) 
{
  const char *text_teensycommand;		//Holds the string entered in the teensycommand text field
		
  //getting text from widget:
  text_teensycommand = gtk_entry_get_text(GTK_ENTRY(gui_app->teensycommand));

  if(text_teensycommand[0] == 'M'){
      CommandMotor();
  }
  else if(text_teensycommand[0] == 'F'){
      RequestFlow();
  }
  else{
      //setting text on label
      gtk_label_set_text(GTK_LABEL(gui_app->label_sent),"Unrecognized Command");
  }      
}

void CommandMotor()
{
    char label_sent_value[40];			//Character array to hold the string that will be printed to the label_sent field
    int length_send_buff = 7;				//Length of the package to be sent to the Teensy
    char send_buff[length_send_buff];		//Character array to hold the package that will be sent to the Teensy

    const char *stepnum_string = gtk_entry_get_text(GTK_ENTRY(gui_app->stepnum));
    unsigned char numOfSteps = atoi(stepnum_string);
    const char *stepmulti_string = gtk_entry_get_text(GTK_ENTRY(gui_app->stepmulti));
    unsigned char stepMultiplier = atoi(stepmulti_string);
    const char *motordir_string = gtk_entry_get_text(GTK_ENTRY(gui_app->motordir));
    unsigned char motorDirection = motordir_string[0];

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
    write(ser_dev, send_buff, length_send_buff);

    //Print the same thing as calculated above to the c_cc_value which will be used to send to the txtstring label
    sprintf(label_sent_value,"%x %x %c %c %d %d %x",PACKET_START_BYTE,length_send_buff, MOTOR_COMMAND,motorDirection, numOfSteps, stepMultiplier, send_buff[6]);
    //setting text on label
    gtk_label_set_text(GTK_LABEL(gui_app->label_sent),label_sent_value);
}

void RequestFlow()
{
    char label_sent_value[40];			//Character array to hold the string that will be printed to the label_sent field
    int length_send_buff = 4;				//Length of the package to be sent to the Teensy
    char send_buff[length_send_buff];		//Character array to hold the package that will be sent to the Teensy
    //Set up the buffer with the start byte, packet size, command byte, and checksum
    send_buff[0] = PACKET_START_BYTE;
    send_buff[1] = length_send_buff;
    send_buff[2] = FLOW_COMMAND;
    send_buff[3] = send_buff[0] ^ send_buff[1];
    send_buff[3] = send_buff[3] ^ send_buff[2];
    //this is how you send an array out on the serial port:
    write(ser_dev, send_buff, length_send_buff);

    //Print the same thing as calculated above to the c_cc_value which will be used to send to the txtstring label
    sprintf(label_sent_value,"%x %x %c %x",PACKET_START_BYTE,length_send_buff, FLOW_COMMAND, send_buff[3]);
    //setting text on label
    gtk_label_set_text(GTK_LABEL(gui_app->label_sent),label_sent_value);

}

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

void GetSerialPacket()
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
                        if(buffer[2] == 'M'){
                            sprintf(label_recieved_value,"%s %d %s %d %s", "Motor Turned", (int)buffer[4],"Steps",(int)buffer[5],"Times");
                        }
                        else if(buffer[2] == 'F'){
                            sprintf(label_recieved_value,"%s %d", "Flow is:", (int)buffer[3]);
                        }
                        else if(buffer[2] == 'F'){
                            sprintf(label_recieved_value,"%s", "Unrecognized Packet");
                        }
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
}

/*!
 * \brief Callback for when the exit button is clicked
 * \param Standard parameters for callback function
 * \details Properly terminates the GUI
 */
extern "C" void button_exit_clicked(GtkWidget *p_wdgt, gpointer p_data ) 
{
  gtk_main_quit();
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

  GThread *read_thread;

  //this is how you allocate a Glib mutex
  g_assert(mutex_to_protect_voltage_display == NULL);
  mutex_to_protect_voltage_display = new GMutex;
  g_mutex_init(mutex_to_protect_voltage_display);

  // this is used to signal all threads to exit
  kill_all_threads=false;
  
  //spawn the serial read thread
  read_thread = g_thread_new(NULL,(GThreadFunc)Serial_Read_Thread,NULL);
  
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

  //this is going to call the Voltage_Display_Displayer function periodically
  gdk_threads_add_timeout(VOLTAGE_DISPLAY_UPDATE_MS, Voltage_Display_Displayer, NULL);

  //the main loop
  gtk_main();

  //signal all threads to die and wait for them (only one child thread)
  kill_all_threads=true;
  g_thread_join(read_thread);
  
  //destroy gui if it still exists
  if(gui_app)
    g_slice_free(Gui_Window_AppWidgets, gui_app);

  return 0;
}
