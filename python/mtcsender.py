import ctypes
import pathlib


###### Warp libmtcmaster library into python class

class MtcSender():

    def __init__(self, fps=25, port=0, portname="SLMTCPort"):
        # Load the shared library into ctypes
        libname = pathlib.Path.cwd().parent.parent / "libmtcmaster/libmtcmaster.so"
        self.mtc_lib = ctypes.CDLL(libname)
        self.mtc_lib.MTCSender_create.restype = ctypes.c_void_p
        
        self.mtcproc = self.mtc_lib.MTCSender_create()
        self.port=port
        self.char_portname=portname.encode('utf-8')
        self.mtc_lib.MTCSender_openPort.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p]
        self.mtc_lib.MTCSender_openPort(self.mtcproc, self.port, self.char_portname)

        self.fps = fps #### TO DO; Pass fps to the library #########

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