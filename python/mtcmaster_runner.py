from mtcmaster import libmtcmaster
from pynng import Rep0
import asyncio


class MtcmasterRunner():


    def __init__(self):
        self.mtcmaster = libmtcmaster.MTCSender_create()
        self.address = "ipc:///tmp/libmtcmaster.sock"

    async def _listener(self):
        self.command = {'play': self.play, 'pause': self.pause,'stop': self.stop,'set_time': self.set_time}
        with Rep0(listen=self.address) as responder:
            while await asyncio.sleep(0, result=True):
                request = await responder.arecv()
                print(f"Received: {request}")
                # Parse the request and call the appropriate method
                try:
                    self.command.get(request.decode())()  # Call the appropriate method based on the request
                    await responder.asend(b"OK")
                except Exception as e:
                    print(f"Error while processing request: {e}")
                    await responder.asend(b"Error processing request")

    

    def run(self) -> None:
        # The "server" thread has its own asyncio loop
        asyncio.run(self._listener(), debug=False)
        print("Server stopped.")

    def stop_server(self):
        self.stop()  # Stop the MTC master playback
        self.release()
        asyncio.get_event_loop().stop()  # Stop the server's event loop


    def play(self):
        libmtcmaster.MTCSender_play(self.mtcmaster)
        print("MTC master started playing.")


    def stop(self):
        libmtcmaster.MTCSender_stop(self.mtcmaster)

    def pause(self):
        libmtcmaster.MTCSender_pause(self.mtcmaster)
    
    def set_time(self, nanos):
        libmtcmaster.MTCSender_setTime(self.mtcmaster, nanos)
    
    def release(self):
        libmtcmaster.MTCSender_release(self.mtcmaster)
    
    def __del__(self):
        self.release()


runner = MtcmasterRunner().run()
    