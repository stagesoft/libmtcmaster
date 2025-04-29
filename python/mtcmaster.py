from ctypes import *
#import .log

try:
    libmtcmaster = cdll.LoadLibrary('libmtcmaster.so.0')
except:
    libmtcmaster = None
    raise ImportError('libmtcmaster import error')

# void* MTCSender_create()
libmtcmaster.MTCSender_create.argtypes = None
libmtcmaster.MTCSender_create.restype = c_void_p

# void MTCSender_release(void* mtcsender);
libmtcmaster.MTCSender_release.argtypes = [c_void_p]
libmtcmaster.MTCSender_release.restype = None

# void MTCSender_openPort(void* mtcsender, unsigned int portnumber, const char* portname);
try:
    libmtcmaster.MTCSender_openPort.argtypes = [c_void_p, c_uint, c_char_p]
    libmtcmaster.MTCSender_openPort.restype = None
except:
    libmtcmaster.MTCSender_openPort = None

# void MTCSender_play(void* mtcsender);
libmtcmaster.MTCSender_play.argtypes = [c_void_p]
libmtcmaster.MTCSender_play.restype = None

# void MTCSender_stop(void* mtcsender);
libmtcmaster.MTCSender_stop.argtypes = [c_void_p]
libmtcmaster.MTCSender_stop.restype = None

# void MTCSender_pause(void* mtcsender);
libmtcmaster.MTCSender_pause.argtypes = [c_void_p]
libmtcmaster.MTCSender_pause.restype = None

# void MTCSender_setTime(void* mtcsender, uint64_t nanos);
libmtcmaster.MTCSender_setTime.argtypes = [c_void_p, c_uint64]
libmtcmaster.MTCSender_setTime.restype = None
