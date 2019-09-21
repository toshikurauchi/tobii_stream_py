from ctypes import POINTER, WINFUNCTYPE, windll
from ctypes.wintypes import BOOL, HWND, RECT, INT, UINT, POINT


## Load functions
# GetActiveWindow
prototype = WINFUNCTYPE(HWND)
GetActiveWindow = prototype(("GetActiveWindow", windll.user32))

# MapWindowPoints
prototype = WINFUNCTYPE(INT, HWND, HWND, POINTER(POINT), UINT)
paramflags = (1, "hWndFrom"), (1, "hWndTo"), (1, "lpPoints"), (1, "cPoints")
MapWindowPoints = prototype(("MapWindowPoints", windll.user32), paramflags)

# GetSystemMetrics
prototype = WINFUNCTYPE(INT, INT)
paramflags = (1, "nIndex"),
GetSystemMetrics = prototype(("GetSystemMetrics", windll.user32), paramflags)

# Constants
SM_CXSCREEN = 0
SM_CYSCREEN = 1


def init_window():
    '''
    This function MUST be called from within the same thread that creates 
    the window AFTER the window has been created, otherwise it will not work.
    '''
    global hwnd
    hwnd = GetActiveWindow()


def map_to_window(x, y):
    '''x and y are normalized coordinates.'''
    width = GetSystemMetrics(SM_CXSCREEN)
    height = GetSystemMetrics(SM_CYSCREEN)
    point = POINT(2)
    point.x = int(x * width)
    point.y = int(y * height)
    MapWindowPoints(0, hwnd, point, 1)
    return point.x, point.y