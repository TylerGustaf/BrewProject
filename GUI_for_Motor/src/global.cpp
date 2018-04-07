#include "global.h"

Gui_Window_AppWidgets *gui_app; //!< Structure to keep all interesting widgets


int ser_teensy1=-1;

char label_recieved_value[40];	//!< Holds the calculated value of the voltage for displaying

int kill_all_threads;		//!< Used to gracefully shut down threads
int targetFlow;

GMutex *mutex_to_protect_voltage_display;	//!< Mutex for protecting the voltage display

