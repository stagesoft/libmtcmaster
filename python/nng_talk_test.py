import asyncio
from cuemsutils.ComunicatorServices import Comunicator

address = "ipc:///tmp/cuems/libmtcmaster.sock"
command = {'cmd': 'stop'}


async def main():
    print(await Comunicator(address).send_request(command))

if __name__ == "__main__":
    asyncio.run(main())

