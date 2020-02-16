require 'ffi'

module MTCMaster
  include FFI
  extend FFI::Library
  ffi_lib "./libmtcmaster.so.0.1" 
  attach_function "release", "MTCSender_release", [:pointer], :void
  attach_function "play", "MTCSender_play", [:pointer], :void
  attach_function "stop", "MTCSender_stop", [:pointer], :void
  attach_function "pause", "MTCSender_pause", [:pointer], :void
  attach_function "create", "MTCSender_create", [], :pointer
  attach_function "setTime", "MTCSender_setTime", [:pointer, :int], :bool

end

sender = MTCMaster.create
puts "create"
sleep(15)
MTCMaster.play sender
puts "play"
sleep(30)
MTCMaster.pause sender
puts "pause"
sleep(2)
MTCMaster.play sender
puts "play"
sleep(10)
MTCMaster.stop sender
puts "stop"
sleep(2)
MTCMaster.play sender
puts "play"
sleep(800)
MTCMaster.stop sender
puts "stop"
sleep(1)
MTCMaster.release sender
puts "release"
