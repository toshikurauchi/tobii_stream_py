# Python Tobii Stream API

Simple wrapper for Tobii Stream API (**Windows Only**). 

## Dependencies

1. Cython: `pip install cython`

## Installing

1. Clone this project
1. Install [Tobii Eye Tracking Core Software](https://gaming.tobii.com/getstarted/?bundle=tobii-core)
1. Download [Stream Engine 3.3.0 for Windows x86](https://developer.tobii.com/download-packages/stream-engine-3-3-0-for-windows-x86/?wpdmdl=11556&refresh=5d1e3887c2a1b1562261639) (it must be x86 even if you are using a 64-bit Windows because Python needs the 32-bit library)
1. Extract the files
1. Move `tobii_stream_engine.dll` and `tobii_stream_engine.lib` from `lib/tobii` to this project's root directory
1. Move `tobii` directory from `include` to this project's `include` directory
1. Run `python setup.py install`

## Usage

An example is provided in [tobii_client_test.py](tobii_client_test.py).

```python
api = tobii_client.TobiiAPI(gaze_callback)
api.start_stream()
time.sleep(1)
api.stop_stream()
```

You can alternatively use the `with` statement, which will call `start_stream()` and `stop_stream()`:

```python
with tobii_client.TobiiAPI(gaze_callback) as api:
    time.sleep(1)
```

The `TobiiAPI` object requires a callback function with a single argument. The callback function will be called for every new gaze sample. The gaze sample is represented by a `namedtuple` with the following fields:

- `timestamp`: integer timestamp value, as provided by Tobii Stream Engine
- `valid`: boolean indicating whether this sample is valid or not
- `x`: float in range [0, 1] (normalized screen size)
- `y`: float in range [0, 1] (normalized screen size)
