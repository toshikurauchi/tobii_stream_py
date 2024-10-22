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

#define KILL_TIMEOUT 1000


struct c_api_data_t
{
    CRITICAL_SECTION log_mutex;
    tobii_custom_log_t custom_log;
    tobii_gaze_point_t latest_gaze_point;
    void (*gaze_callback)( tobii_gaze_point_t const* gaze_point, void* user_data );
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
    HANDLE reconnect_and_timesync_thread_handle;
    volatile LONG is_reconnecting;
};

static DWORD WINAPI reconnect_and_timesync_thread( LPVOID param );

static void log_func( void* log_context, tobii_log_level_t level, char const* text );

tobii_api_t* c_init_api( struct c_api_data_t* api_data );

tobii_device_t* c_connect_device( tobii_api_t* api, struct c_api_data_t* api_data );

int c_subscribe( tobii_api_t* api, tobii_device_t* device, struct c_api_data_t* data );

int c_setup_thread_context( tobii_api_t* api, tobii_device_t* device, struct c_api_data_t* data, struct thread_context_t* thread_context );

int c_start_reconnect_and_timesync_thread( tobii_api_t* api, tobii_device_t* device, struct c_api_data_t* data, struct thread_context_t* thread_context );

int c_schedule_timesync( tobii_api_t* api, tobii_device_t* device, struct c_api_data_t* data, struct thread_context_t* thread_context );

int c_update_data( tobii_device_t* device, struct thread_context_t* thread_context );

void c_cleanup( tobii_api_t* api, tobii_device_t* device, struct c_api_data_t* data, struct thread_context_t* thread_context );