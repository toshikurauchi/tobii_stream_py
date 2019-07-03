# distutils: sources = tobii_c_api.c
# distutils: include_dirs = include

import sys
cimport tobii_client
from collections import namedtuple
import threading
import time


# Load Tobii API version
cdef tobii_client.tobii_version_t tobii_version
tobii_client.tobii_get_api_version(&tobii_version)
__version__ = f'{tobii_version.major}.{tobii_version.minor}.{tobii_version.revision}.{tobii_version.build}'


GazePoint = namedtuple('GazePoint', 'timestamp, valid, x, y')


cdef class TobiiAPI:
    cdef object thread_handle
    cdef bint stop_requested
    cdef bint stopped
    cdef tobii_api_t* p_api
    cdef tobii_device_t* p_device
    cdef c_api_data_t api_data
    cdef thread_context_t gaze_thread_context

    def __init__(self):
        self.thread_handle = None

        # Tobii API
        self.p_api = tobii_client.c_init_api(&self.api_data)
        if self.p_api:
            self.p_device = tobii_client.c_connect_device(self.p_api, &self.api_data)
        else:
            self.p_device = NULL
        self.api_data.gaze_callback = &self.gaze_callback
        self.stop_requested = False
        self.stopped = False
    
    cdef void gaze_callback(tobii_gaze_point_t* gaze_point, void* user_data):
        # Store the latest gaze point data in the supplied storage
        cdef tobii_gaze_point_t* ud
        ud = <tobii_gaze_point_t*>user_data
        ud.timestamp_us = gaze_point.timestamp_us
        ud.validity = gaze_point.validity
        ud.position_xy[0] = gaze_point.position_xy[0]
        ud.position_xy[1] = gaze_point.position_xy[1]
        print(gaze_point.position_xy[0], gaze_point.position_xy[1])
    
    def start_stream(self):
        retval = subscribe(self.p_api, self.p_device, &self.api_data)
        if retval != 0:
            raise RuntimeError('Could not subscribe to device')
        retval = setup_thread_context(self.p_api, self.p_device, &self.api_data, &self.gaze_thread_context)
        if retval != 0:
            raise RuntimeError('Could not setup thread context')
        retval = start_reconnect_and_timesync_thread(self.p_api, self.p_device, &self.api_data, &self.gaze_thread_context)
        if retval != 0:
            raise RuntimeError('Could not create reconnect and timesync thread')
        retval = schedule_timesync(self.p_api, self.p_device, &self.api_data, &self.gaze_thread_context)
        if retval != 0:
            raise RuntimeError('Could not schedule time synchronization event')
        self.thread_handle = threading.Thread(target=self.gaze_loop_thread)
        self.thread_handle.start()
     
    def stop_stream(self):
        self.stop_requested = True
        while not self.stopped:
            time.sleep(0.1)
        cleanup(self.p_api, self.p_device, &self.api_data, &self.gaze_thread_context)
    
    def gaze_loop_thread(self):
        while not self.stop_requested:
            update_data(self.p_device, &self.gaze_thread_context)
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