#include "tobii_c_api.h"


static void url_receiver( char const* url, void* user_data )
{
    // The memory context is passed through the user_data void pointer
    struct url_receiver_context_t* context = (struct url_receiver_context_t*) user_data;
    // Allocate more memory if maximum capacity has been reached
    if( context->count >= context->capacity )
    {
        context->capacity *= 2;
        char** urls = (char**) realloc( context->urls, sizeof( char* ) * context->capacity );
        if( !urls )
        {
            fprintf( stderr, "Allocation failed\n" );
            return;
        }
        context->urls = urls;
    }
    // Copy the url string input parameter to allocated memory
    size_t url_length = strlen( url ) + 1;
    context->urls[ context->count ] = malloc( url_length );
    if( !context->urls[ context->count ] )
    {
        fprintf( stderr, "Allocation failed\n" );
        return;
    }
    memcpy( context->urls[ context->count++ ], url, url_length );
}


static struct device_list_t list_devices( tobii_api_t* api )
{
    struct device_list_t list = { NULL, 0 };
    // Create a memory context that can be used by the url receiver callback
    struct url_receiver_context_t url_receiver_context;
    url_receiver_context.count = 0;
    url_receiver_context.capacity = 16;
    url_receiver_context.urls = (char**) malloc( sizeof( char* ) * url_receiver_context.capacity );
    if( !url_receiver_context.urls )
    {
        fprintf( stderr, "Allocation failed\n" );
        return list;
    }

    // Enumerate the connected devices, connected devices will be stored in the supplied memory context
    tobii_error_t error = tobii_enumerate_local_device_urls( api, url_receiver, &url_receiver_context );
    if( error != TOBII_ERROR_NO_ERROR ) fprintf( stderr, "Failed to enumerate devices.\n" );

    list.urls = url_receiver_context.urls;
    list.count = url_receiver_context.count;
    return list;
}


static void free_device_list( struct device_list_t* list )
{
    for( int i = 0; i < list->count; ++i )
        free( list->urls[ i ] );

    free( list->urls );
}


static char const* select_device( struct device_list_t* devices )
{
    int tmp = 0, selection = 0;

    // Present the available devices and loop until user has selected a valid device
    while( selection < 1 || selection > devices->count)
    {
        printf( "\nSelect a device\n\n" );
        for( int i = 0; i < devices->count; ++i ) printf( "%d. %s\n",  i + 1, devices->urls[ i ] ) ;
        if( scanf( "%d", &selection ) <= 0 ) while( ( tmp = getchar() ) != '\n' && tmp != EOF );
    }

    return devices->urls[ selection - 1 ];
}


static DWORD WINAPI reconnect_and_timesync_thread( LPVOID param )
{
    struct thread_context_t* context = (struct thread_context_t*) param;

    HANDLE objects[ 3 ];
    objects[ 0 ] = context->reconnect_event;
    objects[ 1 ] = context->timesync_event;
    objects[ 2 ] = context->exit_event;

    for( ; ; )
    {
        // Block here, waiting for one of the three events.
        DWORD result = WaitForMultipleObjects( 3, objects, FALSE, INFINITE );
        if( result == WAIT_OBJECT_0 ) // Handle reconnect event
        {
            tobii_error_t error;
            // Run reconnect loop until connection succeeds or the exit event occurs
            while( ( error = tobii_device_reconnect( context->device ) ) != TOBII_ERROR_NO_ERROR )
            {
                if( error != TOBII_ERROR_CONNECTION_FAILED )
                    fprintf( stderr, "Reconnection failed: %s.\n", tobii_error_message( error ) );
                // Blocking waiting for exit event, timeout after 250 ms and retry reconnect
                result = WaitForSingleObject( context->exit_event, 250 ); // Retry every quarter of a second
                if( result == WAIT_OBJECT_0 )
                    return 0; // exit thread
            }
            InterlockedExchange( &context->is_reconnecting, 0L );
        }
        else if( result == WAIT_OBJECT_0 + 1 ) // Handle timesync event
        {
            tobii_error_t error = tobii_update_timesync( context->device );
            LARGE_INTEGER due_time;
            LONGLONG const timesync_update_interval = 300000000LL; // Time sync every 30 s
            LONGLONG const timesync_retry_interval = 1000000LL; // Retry time sync every 100 ms
            due_time.QuadPart = ( error == TOBII_ERROR_NO_ERROR ) ? -timesync_update_interval : -timesync_retry_interval;
            // Re-schedule timesync event
            SetWaitableTimer( context->timesync_event, &due_time, 0, NULL, NULL, FALSE );
        }
        else if( result == WAIT_OBJECT_0 + 2 ) // Handle exit event
        {
            // Exit requested, exiting the thread
            return 0;
        }
    }
}


static void log_func( void* log_context, tobii_log_level_t level, char const* text )
{
    CRITICAL_SECTION* log_mutex = (CRITICAL_SECTION*)log_context;

    EnterCriticalSection( log_mutex );
    if( level == TOBII_LOG_LEVEL_ERROR )
        fprintf( stderr, "Logged error: %s\n", text );
    LeaveCriticalSection( log_mutex );
}


tobii_api_t* c_init_api( struct c_api_data_t* api_data )
{
    // Initialize critical section, used for thread synchronization in the log function
    InitializeCriticalSection( &api_data->log_mutex );
    api_data->custom_log.log_context = &api_data->log_mutex;
    api_data->custom_log.log_func = log_func;
    api_data->latest_gaze_point.timestamp_us = 0LL;
    api_data->latest_gaze_point.validity = TOBII_VALIDITY_INVALID;

    tobii_api_t* api;
    tobii_error_t error = tobii_api_create( &api, NULL, &api_data->custom_log );
    if( error != TOBII_ERROR_NO_ERROR )
    {
        fprintf( stderr, "Failed to initialize the Tobii Stream Engine API.\n" );
        DeleteCriticalSection( &api_data->log_mutex );
        return NULL;
    }
    return api;
} 

tobii_device_t* c_connect_device(tobii_api_t* api, struct c_api_data_t* api_data)
{
    struct device_list_t devices = list_devices( api );
    if( devices.count == 0 )
    {
        fprintf( stderr, "No stream engine compatible device(s) found.\n" );
        free_device_list( &devices );
        tobii_api_destroy( api );
        DeleteCriticalSection( &api_data->log_mutex );
        return NULL;
    }
    char const* selected_device = devices.count == 1 ? devices.urls[ 0 ] : select_device( &devices );
    printf( "Connecting to %s.\n", selected_device );

    tobii_device_t* device;
    tobii_error_t error = tobii_device_create( api, selected_device, &device );
    free_device_list( &devices );
    if( error != TOBII_ERROR_NO_ERROR )
    {
        fprintf( stderr, "Failed to initialize the device with url %s.\n", selected_device );
        tobii_api_destroy( api );
        DeleteCriticalSection( &api_data->log_mutex );
        return NULL;
    }
    return device;
}

int c_subscribe( tobii_api_t* api, tobii_device_t* device, struct c_api_data_t* data )
{
    // Start subscribing to gaze point data, in this sample we supply a tobii_gaze_point_t variable to store latest value.
    tobii_error_t error = tobii_gaze_point_subscribe( device, data->gaze_callback, &data->latest_gaze_point );
    if( error != TOBII_ERROR_NO_ERROR )
    {
        fprintf( stderr, "Failed to subscribe to gaze stream.\n" );

        tobii_device_destroy( device );
        tobii_api_destroy( api );
        DeleteCriticalSection( &data->log_mutex );
        return 1;
    }
    return 0;
}

int c_setup_thread_context( tobii_api_t* api, tobii_device_t* device, struct c_api_data_t* data, struct thread_context_t* thread_context )
{
    // Create event objects used for inter thread signaling
    HANDLE reconnect_event = CreateEvent( NULL, FALSE, FALSE, NULL );
    HANDLE timesync_event = CreateWaitableTimer( NULL, TRUE, NULL );
    HANDLE exit_event = CreateEvent( NULL, FALSE, FALSE, NULL );
    if( reconnect_event == NULL || timesync_event == NULL || exit_event == NULL )
    {
        fprintf( stderr, "Failed to create event objects.\n" );

        tobii_device_destroy( device );
        tobii_api_destroy( api );
        DeleteCriticalSection( &data->log_mutex );
        return 1;
    }

    thread_context->device = device;
    thread_context->reconnect_event = reconnect_event;
    thread_context->timesync_event = timesync_event;
    thread_context->exit_event = exit_event;
    thread_context->is_reconnecting = 0;
    return 0;
}

int c_start_reconnect_and_timesync_thread( tobii_api_t* api, tobii_device_t* device, struct c_api_data_t* data, struct thread_context_t* thread_context )
{
    // Create and run the reconnect and timesync thread
    thread_context->reconnect_and_timesync_thread_handle = CreateThread( NULL, 0U, reconnect_and_timesync_thread, &thread_context, 0U, NULL );
    if( thread_context->reconnect_and_timesync_thread_handle == NULL )
    {
        fprintf( stderr, "Failed to create reconnect_and_timesync thread.\n" );

        tobii_device_destroy( device );
        tobii_api_destroy( api );
        DeleteCriticalSection( &data->log_mutex );
        return 1;
    }
    return 0;
}

int c_schedule_timesync( tobii_api_t* api, tobii_device_t* device, struct c_api_data_t* data, struct thread_context_t* thread_context )
{
    LARGE_INTEGER due_time;
    LONGLONG const timesync_update_interval = 300000000LL; // time sync every 30 s
    due_time.QuadPart = -timesync_update_interval;
    // Schedule the time synchronization event
    BOOL timer_error = SetWaitableTimer( thread_context->timesync_event, &due_time, 0, NULL, NULL, FALSE );
    if( timer_error == FALSE )
    {
        fprintf( stderr, "Failed to schedule timer event.\n" );

        tobii_device_destroy( device );
        tobii_api_destroy( api );
        DeleteCriticalSection( &data->log_mutex );
        return 1;
    }
    return 0;
}

int c_update_data( tobii_device_t* device, struct thread_context_t* thread_context )
{
    if( InterlockedCompareExchange( &thread_context->is_reconnecting, 0L, 0L ) )
    {
        return 1;
    }
    tobii_error_t error = tobii_device_process_callbacks( device );

    if( error == TOBII_ERROR_CONNECTION_FAILED )
    {
        // Change state and signal that reconnect is needed.
        InterlockedExchange( &thread_context->is_reconnecting, 1L );
        SetEvent( thread_context->reconnect_event );
    }
    else if( error != TOBII_ERROR_NO_ERROR )
    {
        fprintf( stderr, "tobii_device_process_callbacks failed: %s.\n", tobii_error_message( error ) );
        return 1;
    }

    return 0;
}

void c_cleanup( tobii_api_t* api, tobii_device_t* device, struct c_api_data_t* data, struct thread_context_t* thread_context )
{
    // Signal reconnect and timesync thread to exit and clean up event objects.
    SetEvent( thread_context->exit_event );
    WaitForSingleObject( thread_context->reconnect_and_timesync_thread_handle, KILL_TIMEOUT );
    CloseHandle( thread_context->reconnect_and_timesync_thread_handle );
    CloseHandle( thread_context->timesync_event );
    CloseHandle( thread_context->exit_event );
    CloseHandle( thread_context->reconnect_event );

    tobii_error_t error = tobii_gaze_point_unsubscribe( device );
    if( error != TOBII_ERROR_NO_ERROR )
        fprintf( stderr, "Failed to unsubscribe from gaze stream.\n" );

    error = tobii_device_destroy( device );
    if( error != TOBII_ERROR_NO_ERROR )
        fprintf( stderr, "Failed to destroy device.\n" );

    error = tobii_api_destroy( api );
    if( error != TOBII_ERROR_NO_ERROR )
        fprintf( stderr, "Failed to destroy API.\n" );

    DeleteCriticalSection( &data->log_mutex );
}
