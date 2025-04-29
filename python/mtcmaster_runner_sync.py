from mtcmaster import libmtcmaster
from pynng import Rep0
import json
import signal
import sys


class MtcmasterRunner():

    
    def __init__(self):
        self.mtcmaster = libmtcmaster.MTCSender_create()
        self.address = "ipc:///tmp/libmtcmaster.sock"

    def _listener(self):
        self.command = {'play': self.play, 'pause': self.pause,'stop': self.stop,'set_time': self.set_time}
        signal.signal(signal.SIGTERM, signal.SIG_DFL)
        signal.signal(signal.SIGINT, signal.SIG_DFL)
        while True:
        # while self.killer.kill_now is False:
            responder = Rep0(listen=self.address)
            
            request = responder.recv()
            print(f"Received: {request.decode()}")
            # Parse the request and call the appropriate method
            try:
                decoded_request = json.loads(request)  # Parse the JSON request
                if 'params' in decoded_request:  # Check if the request is valid
                    #print(decoded_request['cmd'])
                    self.command.get(decoded_request['cmd'])(decoded_request['params']['nanos']) 
                else:
                    self.command.get(decoded_request['cmd'])()
                responder.send(b"OK")
            except Exception as e:
                print(f"Error while processing request: {e}")
                responder.send(b"Error processing request")
            self.stop()
            self.release()

    

    def run(self) -> None:
        # The "server" thread has its own asyncio loop
        self._listener()
        print("Server stopped.")

    def stop_server(self):
        self.stop()  # Stop the MTC master playback
        self.release()


    def play(self):
        libmtcmaster.MTCSender_play(self.mtcmaster)
        print("MTC master started playing.")


    def stop(self):
        libmtcmaster.MTCSender_stop(self.mtcmaster)

    def pause(self):
        libmtcmaster.MTCSender_pause(self.mtcmaster)
    
    def set_time(self, nanos):
        libmtcmaster.MTCSender_setTime(self.mtcmaster, nanos)
        print(f"MTC master set time to {nanos} nanoseconds.")
    
    def release(self):
        libmtcmaster.MTCSender_release(self.mtcmaster)
    
    def __del__(self):
        self.release()


runner = MtcmasterRunner().run()
    