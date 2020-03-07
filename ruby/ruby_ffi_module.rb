require 'ffi'

module MTCMaster
  include FFI
  extend FFI::Library
  ffi_lib "../libmtcmaster.so.0.1" 
  attach_function "release", "MTCSender_release", [:pointer], :void
  attach_function "play", "MTCSender_play", [:pointer], :void
  attach_function "stop", "MTCSender_stop", [:pointer], :void
  attach_function "pause", "MTCSender_pause", [:pointer], :void
  attach_function "create", "MTCSender_create", [], :pointer
  attach_function "setTime", "MTCSender_setTime", [:pointer, :uint64], :bool

end