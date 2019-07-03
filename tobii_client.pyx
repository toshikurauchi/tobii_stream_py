# distutils: sources = tobii_c_api.c
# distutils: include_dirs = include

import sys
cimport tobii_client
from collections import namedtuple
import threading
import time


# Globals :(
cdef TobiiAPI __api_instance = None


# Load Tobii API version
cdef tobii_client.tobii_version_t tobii_version
tobii_client.tobii_get_api_version(&tobii_version)
__version__ = f'{tobii_version.major}.{tobii_version.minor}.{tobii_version.revision}.{tobii_version.build}'


GazePoint = namedtuple('GazePoint', 'timestamp, valid, x, y')


cdef void __api_gaze_callback(tobii_gaze_point_t* gaze_point, void* user_data):
    cdef tobii_gaze_point_t* latest
    # Get values
    timestamp = gaze_point.timestamp_us
    valid = gaze_point.validity == tobii_client.TOBII_VALIDITY_VALID
    x = gaze_point.position_xy[0]
    y = gaze_point.position_xy[1]
    
    # Store values
    latest = <tobii_gaze_point_t*>user_data
    latest.timestamp_us = timestamp
    latest.validity = gaze_point.validity
    latest.position_xy[0] = x
    latest.position_xy[1] = y

    if __api_instance and __api_instance.user_gaze_callback:
        __api_instance.user_gaze_callback(GazePoint(timestamp, valid, x, y))


cdef class TobiiAPI:
    cdef object thread_handle
    cdef object user_gaze_callback
    cdef bint stop_requested
    cdef bint stopped
    cdef tobii_api_t* p_api
    cdef tobii_device_t* p_device
    cdef c_api_data_t api_data
    cdef thread_context_t gaze_thread_context

    def __init__(self, gaze_callback=None):
        global __api_instance
        
        __api_instance = self
        self.thread_handle = None
        self.user_gaze_callback = gaze_callback

        # Tobii API
        self.p_api = tobii_client.c_init_api(&self.api_data)
        if self.p_api:
            self.p_device = tobii_client.c_connect_device(self.p_api, &self.api_data)
        else:
            self.p_device = NULL
        self.api_data.gaze_callback = &__api_gaze_callback
        self.stop_requested = False
        self.stopped = False
    
    def start_stream(self):
        retval = c_subscribe(self.p_api, self.p_device, &self.api_data)
        if retval != 0:
            raise RuntimeError('Could not subscribe to device')
        retval = c_setup_thread_context(self.p_api, self.p_device, &self.api_data, &self.gaze_thread_context)
        if retval != 0:
            raise RuntimeError('Could not setup thread context')
        retval = c_start_reconnect_and_timesync_thread(self.p_api, self.p_device, &self.api_data, &self.gaze_thread_context)
        if retval != 0:
            raise RuntimeError('Could not create reconnect and timesync thread')
        retval = c_schedule_timesync(self.p_api, self.p_device, &self.api_data, &self.gaze_thread_context)
        if retval != 0:
            raise RuntimeError('Could not schedule time synchronization event')
        self.thread_handle = threading.Thread(target=self.gaze_loop_thread)
        self.thread_handle.start()
     
    def stop_stream(self):
        self.stop_requested = True
        while not self.stopped:
            time.sleep(0.1)
        c_cleanup(self.p_api, self.p_device, &self.api_data, &self.gaze_thread_context)
    
    def gaze_loop_thread(self):
        while not self.stop_requested:
            c_update_data(self.p_device, &self.gaze_thread_context)
            time.sleep(0.1)
        self.stopped = True
        
    def __enter__(self):
        self.start_stream()
        return self
        
    def __exit__(self, type, value, traceback):
        self.stop_stream()

    @property
    def latest_gaze_point(self):
        timestamp = self.api_data.latest_gaze_point.timestamp_us
        valid = self.api_data.latest_gaze_point.validity == tobii_client.TOBII_VALIDITY_VALID
        x = self.api_data.latest_gaze_point.position_xy[0]
        y = self.api_data.latest_gaze_point.position_xy[1]
        return GazePoint(timestamp, valid, x, y)