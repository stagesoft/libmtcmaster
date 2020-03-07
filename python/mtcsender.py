import ctypes
import pathlib


###### Warp libmtcmaster library into python class

class MtcSender():

    def __init__(self, fps):
        # Load the shared library into ctypes
        libname = pathlib.Path.cwd().parent.parent / "libmtcmaster/libmtcmaster.so"
        self.mtc_lib = ctypes.CDLL(libname)
        self.mtc_lib.MTCSender_create.restype = ctypes.c_void_p
        
        self.mtcproc = self.mtc_lib.MTCSender_create()

        self.fps = 25 ### fps ##### TO-DO ####

    def __del__(self): 
        self.mtc_lib.MTCSender_release(ctypes.c_void_p(self.mtcproc))

    def play(self):
        self.mtc_lib.MTCSender_play(ctypes.c_void_p(self.mtcproc))
    
    def pause(self):
        self.mtc_lib.MTCSender_pause(ctypes.c_void_p(self.mtcproc))

    def stop(self):
        self.mtc_lib.MTCSender_stop(ctypes.c_void_p(self.mtcproc))

    def settime(self, seconds):
        self.nanos = seconds * 1000000000 
        self.mtc_lib.MTCSender_setTime(ctypes.c_void_p(self.mtcproc), ctypes.c_uint64(self.nanos))
    
    def settime_frames(self, frames):
        self.nanos = (frames / self.fps) * 1000000000
        self.mtc_lib.MTCSender_setTime(ctypes.c_void_p(self.mtcproc), ctypes.c_uint64(int(self.nanos)))
    
    def settime_nanos(self, nanos):
        self.nanos = nanos 
        self.mtc_lib.MTCSender_setTime(ctypes.c_void_p(self.mtcproc), ctypes.c_uint64(self.nanos))

##################################