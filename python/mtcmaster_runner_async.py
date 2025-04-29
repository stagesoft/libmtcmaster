from mtcmaster import libmtcmaster
import asyncio
from cuemsutils.ComunicatorServices import Comunicator


class MtcmasterRunner():


    def __init__(self):
        self.mtcmaster = libmtcmaster.MTCSender_create()
        self.address = "ipc:///tmp/cuems/libmtcmaster.sock"
        self.comunicator = Comunicator(address = self.address)
        self.command = {'play': self.play, 'pause': self.pause,'stop': self.stop,'set_time': self.set_time}

    def process_request(self, request):
        try:
            if 'params' in request:  # Check if the request is valid
                self.command.get(request['cmd'])(request['params']['nanos']) 
            else:
                self.command.get(request['cmd'])()
            return {'resp': 'ok'}
        except Exception as e:
            print(f"Error while processing request: {e}")
            return {'resp': f"Error while processing request: {e}"}

    async def run(self):
        # The "server" thread has its own asyncio loop
        await self.comunicator.reply(self.process_request)

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




if __name__ == "__main__":
    asyncio.run(MtcmasterRunner().run())

