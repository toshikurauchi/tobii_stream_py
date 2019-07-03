import time
import tobii_client as tobii

print(tobii.__version__)
with tobii.TobiiAPI() as api:
    for i in range(10):
        time.sleep(0.2)
        