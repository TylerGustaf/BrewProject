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

#define VOLTAGE_DISPLAY_UPDATE_MS 100	//!< Time(in milliseconds) between calls to voltage display function

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
    
}

const char PACKET_START_BYTE = 0xAA;
const char MOTOR_COMMAND = 'M';
const unsigned int PACKET_OVERHEAD_BYTES = 3;
const unsigned int PACKET_MIN_BYTES = PACKET_OVERHEAD_BYTES + 1;
const unsigned int PACKET_MAX_BYTES = 255;


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
  unsigned char commandNum;		//Holds the integer value of the command sent to the teensy
  char label_sent_value[40];			//Character array to hold the string that will be printed to the label_sent field
  char send_buff[5];					//Character array to hold the package that will be sent to the Teensy
  int length_send_buff = 5;				//Length of the package to be sent to the Teensy
  
  //getting text from widget:
  text_teensycommand = gtk_entry_get_text(GTK_ENTRY(gui_app->teensycommand));
  //Convert the text recieved into and integer and save it
  commandNum=atoi(text_teensycommand);

  //Set up the buffer with the start byte, packet size, command byte, and checksum
  send_buff[0] = PACKET_START_BYTE;
  send_buff[1] = length_send_buff;
  send_buff[2] = MOTOR_COMMAND;
  send_buff[3] = commandNum;
  send_buff[4] = send_buff[0] ^ send_buff[1];
  send_buff[4] = send_buff[4] ^ send_buff[2];
  send_buff[4] = send_buff[4] ^ send_buff[3];

  //Print the same thing as calculated above to the c_cc_value which will be used to send to the txtstring label
  sprintf(label_sent_value,"%x %x %c %x %x",PACKET_START_BYTE,length_send_buff, MOTOR_COMMAND, commandNum, send_buff[6]);
		      
  //setting text on label
  gtk_label_set_text(GTK_LABEL(gui_app->label_sent),label_sent_value);
  //this is how you send an array out on the serial port:
  write(ser_dev, send_buff, length_send_buff);
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