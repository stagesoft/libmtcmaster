require_relative 'ruby_ffi_module'

sender = MTCMaster.create
puts "create"
sleep(1)
MTCMaster.setTime sender, 50000000000
sleep(5)
MTCMaster.play sender
puts "play"
sleep(10)
MTCMaster.pause sender
puts "pause"
sleep(2)
MTCMaster.setTime sender, 80000000
sleep(2)
MTCMaster.play sender
puts "play"
sleep(10)
MTCMaster.stop sender
puts "stop"
sleep(2)
MTCMaster.play sender
puts "play"
sleep(2)
MTCMaster.stop sender
puts "stop"
sleep(1)
MTCMaster.release sender
puts "release"
