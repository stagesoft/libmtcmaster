import time
from mtcsender import MtcSender


m = MtcSender(25)
m.play()

time.sleep(5)

m.pause()

time.sleep(5)

print("set time")
m.settime(60)

time.sleep(2)
m.settime_frames(5)

time.sleep(4)
m.settime_nanos(5000000000)

time.sleep(5)
print("going play")
m.play()

time.sleep(5)

m.stop()

time.sleep(5)

del m
print("freeing object (library)")
time.sleep(5)
