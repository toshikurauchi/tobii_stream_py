import time
import tobii_client as tobii

print(tobii.__version__)
with tobii.TobiiAPI() as api:
    print(api.latest_gaze_point)
    time.sleep(2)
    