#include "global.h"

Gui_Window_AppWidgets *gui_app; //!< Structure to keep all interesting widgets


int ser_teensy1=-1;

char label_recieved_value[40];	//!< Holds the current flow value that will be shown to the user

int kill_all_threads;		//!< Used to gracefully shut down threads
int targetFlow;			//!< Stores the target flow specified by the user
bool makeMLThread = true;	//!< Used to specify if a MasterLogic thread can be made
int numOfSteps = 0;		//!< Stores the number of steps the motor has taken so far

GMutex *master_logic_mutex;	//!< Mutex for protecting the creation of a MasterLogic thread
GMutex *flow_label_mutex;	//!< Mutex for protecting the flow label
