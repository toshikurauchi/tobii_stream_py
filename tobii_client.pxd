cdef extern from "stdint.h":
    ctypedef signed int int64_t

cdef extern from "tobii/tobii.h":
    enum tobii_error_t:
        TOBII_ERROR_NO_ERROR,
        TOBII_ERROR_INTERNAL,
        TOBII_ERROR_INSUFFICIENT_LICENSE,
        TOBII_ERROR_NOT_SUPPORTED,
        TOBII_ERROR_NOT_AVAILABLE,
        TOBII_ERROR_CONNECTION_FAILED,
        TOBII_ERROR_TIMED_OUT,
        TOBII_ERROR_ALLOCATION_FAILED,
        TOBII_ERROR_INVALID_PARAMETER,
        TOBII_ERROR_CALIBRATION_ALREADY_STARTED,
        TOBII_ERROR_CALIBRATION_NOT_STARTED,
        TOBII_ERROR_ALREADY_SUBSCRIBED,
        TOBII_ERROR_NOT_SUBSCRIBED,
        TOBII_ERROR_OPERATION_FAILED,
        TOBII_ERROR_CONFLICTING_API_INSTANCES,
        TOBII_ERROR_CALIBRATION_BUSY,
        TOBII_ERROR_CALLBACK_IN_PROGRESS,
        TOBII_ERROR_TOO_MANY_SUBSCRIBERS
    
    enum tobii_validity_t:
        TOBII_VALIDITY_INVALID,
        TOBII_VALIDITY_VALID

    struct tobii_version_t:
        int major
        int minor
        int revision
        int build
    
    struct tobii_api_t:
        pass
    
    struct tobii_custom_alloc_t:
        pass
    
    struct tobii_custom_log_t:
        pass
    
    struct tobii_device_t:
        pass
    
    tobii_error_t tobii_get_api_version(tobii_version_t* version);
    tobii_error_t tobii_api_create(tobii_api_t** api, tobii_custom_alloc_t* custom_alloc, tobii_custom_log_t* custom_log );

cdef extern from "tobii/tobii_streams.h":
    struct tobii_gaze_point_t:
        int64_t timestamp_us
        tobii_validity_t validity
        float position_xy[ 2 ]

cdef extern from "tobii_c_api.h":
    struct url_receiver_context_t:
        char** urls
        int capacity
        int count
    
    struct device_list_t:
        char** urls
        int count

    struct c_api_data_t:
        tobii_gaze_point_t latest_gaze_point

    struct thread_context_t:
        pass
    
    void url_receiver(char* url, void* user_data);
    device_list_t list_devices(tobii_api_t* api);
    void free_device_list(device_list_t* list);
    char* select_device(device_list_t* devices);
    tobii_api_t* c_init_api(c_api_data_t* api_data);
    tobii_device_t* c_connect_device(tobii_api_t* api, c_api_data_t* api_data);
    int subscribe(tobii_api_t* api, tobii_device_t* device, c_api_data_t* data);
    int setup_thread_context(tobii_api_t* api, tobii_device_t* device, c_api_data_t* data, thread_context_t* thread_context);
    int start_reconnect_and_timesync_thread(tobii_api_t* api, tobii_device_t* device, c_api_data_t* data, thread_context_t* thread_context);
    int schedule_timesync(tobii_api_t* api, tobii_device_t* device, c_api_data_t* data, thread_context_t* thread_context);
    int update_data( tobii_device_t* device, thread_context_t* thread_context );
    void cleanup(tobii_api_t* api, tobii_device_t* device, c_api_data_t* data, thread_context_t* thread_context);