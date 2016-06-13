#define VELIB_EXPECTED_TRACKING_NR			11

// Single CAN bus
#define CFG_CANHW_MAX_BUSSES				1

// Mark if there is something to do, to only execute when needed. 
#define CFG_UTILS_TODO_ENABLED				1

// QT timer is used for ACL
#define CFG_J1939_VELIB_TIMER_DISABLED		1

// Enable assertion checks
#define CFG_ASSERT_ENABLED					1

// velib_inc_J1939_app.h exists
#define	CFG_J1939_INC_APP					1

// Just to make device a pointer
#define CFG_J1939_DEVICES					2

// This will cause the VeSerialPort to create a thread for reading data.
#define CFG_INIT_CTX                        1

// MSVC 2005 fails to link VLogger..
#if defined(_MSC_VER) && _MSC_VER==1400
#define	CFG_LOG_DISABLE						1
#endif
