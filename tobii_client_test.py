import time
import tobii_client as tobii

print(f'Version: {tobii.__version__}')

def gaze_callback(gaze_point):
    print(f'Gaze Point:\n  Timestamp: {gaze_point.timestamp}\n  Valid: {gaze_point.valid}\n  Position: ({gaze_point.x}, {gaze_point.y})\n')

with tobii.TobiiAPI(gaze_callback) as api:
    time.sleep(1)

'''
# Same as
api = tobii.TobiiAPI(gaze_callback)
api.start_stream()
time.sleep(1)
api.stop_stream()
'''