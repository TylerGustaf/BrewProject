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
  GtkWidget *window1; 
  GtkWidget *exitbutton;
  GtkWidget *StartButton;
  GtkWidget *PauseButton;
  GtkWidget *FOpenButton;
  GtkWidget *FCloseButton;
  GtkWidget *TargetFlowInput;
  GtkWidget *FlowLabel;
} Gui_Window_AppWidgets; 

extern Gui_Window_AppWidgets *gui_app;	//!< Main pointer for all the GUI widgets

//this is the serial devices handle
extern int ser_teensy1;		//!< Serial devices handle

//this is to gracefully shut down threads
extern int kill_all_threads;	//!< Used to gracefully shut down threads
extern int targetFlow;
extern bool makeMLThread;
extern int numOfSteps;

//this variable is for communicating the voltage value string
extern char label_recieved_value[40];			//!< Holds the calculated value of the voltage for displaying

//this is the mutex for the above variable
extern GMutex *master_logic_mutex;		//!< Mutex for protecting the voltage display

//prototype of function for serial read thread
gpointer Serial_Read_Thread();
gpointer MasterLogic();

#endif
