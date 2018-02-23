#include <gtk/gtk.h>
#include <stdlib.h>
#include <iostream>
#define __STDC_FORMAT_MACROS


#ifndef _MY__GLOBAL__H
#define _MY__GLOBAL__H	//!< Used to ensure the header is only included once during compilation

using namespace std;


/**************************************************************
 * GUI window stuff
 **************************************************************/
/*!
 *  Struct which holds pointers for all the GUI widgets
 */
typedef struct 
{
  GtkWidget *window1; 		//!< The main window for the GUI
  GtkWidget *entry_sd; 		//!< The entry field for the serial port
  GtkWidget *exitbutton;		//!< The exit button at the bottom of the GUI
  GtkWidget *opendevice;		//!< The open device button for the serial port
  GtkWidget *closedevice;		//!< The close device button for the serial port
  GtkWidget *sendbutton;		//!< The button to send a command to the Teensy
  GtkWidget *label_sent;			//!< The display field for the byte package being sent to the Teensy
  GtkWidget *label_recieved;		//!< The display field for the package recieved from the Teensy
  GtkWidget *teensycommand;			//!< The entry field for the red color value
  
} Gui_Window_AppWidgets; 

extern Gui_Window_AppWidgets *gui_app;	//!< Main pointer for all the GUI widgets

//this is the serial devices handle
extern int ser_dev;		//!< Serial devices handle

//this is to gracefully shut down threads
extern int kill_all_threads;	//!< Used to gracefully shut down threads

//this variable is for communicating the voltage value string
extern char label_recieved_value[40];			//!< Holds the calculated value of the voltage for displaying

//this is the mutex for the above variable
extern GMutex *mutex_to_protect_voltage_display;		//!< Mutex for protecting the voltage display

//prototype of function for serial read thread
gpointer Serial_Read_Thread();

#endif
