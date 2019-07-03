#include <tobii/tobii.h>
#include <tobii/tobii_streams.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#pragma warning( push )
#pragma warning( disable: 4255 4668 )
#include <windows.h>
#pragma warning( pop )


static void gaze_callback( tobii_gaze_point_t const* gaze_point, void* user_data );

struct c_api_data_t
{
    CRITICAL_SECTION log_mutex;
    tobii_custom_log_t custom_log;
    tobii_gaze_point_t latest_gaze_point;
};

struct url_receiver_context_t
{
    char** urls;
    int capacity;
    int count;
};

static void url_receiver( char const* url, void* user_data );

struct device_list_t
{
    char** urls;
    int count;
};

static struct device_list_t list_devices( tobii_api_t* api );

static void free_device_list( struct device_list_t* list );

static char const* select_device( struct device_list_t* devices );

struct thread_context_t
{
    tobii_device_t* device;
    HANDLE reconnect_event; // Used to signal that a reconnect is needed
    HANDLE timesync_event; // Timer event used to signal that time synchronization is needed
    HANDLE exit_event; // Used to signal that the background thead should exit
    volatile LONG is_reconnecting;
    int stop_requested;
    int stopped;
};

static DWORD WINAPI reconnect_and_timesync_thread( LPVOID param );

static void log_func( void* log_context, tobii_log_level_t level, char const* text );

tobii_api_t* c_init_api( struct c_api_data_t* api_data );

tobii_device_t* c_connect_device( tobii_api_t* api, struct c_api_data_t* api_data );

int collect_gaze( struct thread_context_t* context );