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
    cdef bint stop_requested
    cdef object thread_handle
    cdef tobii_api_t* p_api
    cdef tobii_device_t* p_device
    cdef c_api_data_t api_data
    cdef thread_context_t gaze_thread_context

    def __init__(self):
        self.stop_requested = False
        self.thread_handle = None

        # Tobii API
        self.p_api = tobii_client.c_init_api(&self.api_data)
        if self.p_api:
            self.p_device = tobii_client.c_connect_device(self.p_api, &self.api_data)
        else:
            self.p_device = NULL
        self.gaze_thread_context.stop_requested = False
        self.gaze_thread_context.stopped = False
    
    def start_stream(self):
        self.thread_handle = threading.Thread(target=self.gaze_loop_thread)
        self.thread_handle.start()
    
    def stop_stream(self):
        self.stop_requested = True
        while not self.gaze_thread_context.stopped:
            time.sleep(0.1)
    
    def gaze_loop_thread(self):
        while not self.stop_requested:
            time.sleep(0.1)
            print('THREAD')
        self.gaze_thread_context.stop_requested = True
        self.gaze_thread_context.stopped = True # TODO REMOVE!!! TEST ONLY!!!
    
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