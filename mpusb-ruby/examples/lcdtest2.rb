#!/usr/bin/env ruby

$:.unshift("../lib")
require "mpusb"

require "pp"

#MPUSB.debug = 1
MPUSB.set_i2c_range(8,16)

device = MPUSB.open().i2c_devices[0]

#MPUSB.debug=1
#puts "Brightness: #{device.brightness}"
#MPUSB.debug=0

device.clear
device.puts "Brightness: 0x0a"
device.brightness = 0x0a
sleep 2

device.clear
device.puts "Brightness: 0x80"
device.brightness = 0x80
sleep 2

device.clear
device.puts "Brightness: 0xff"
device.brightness = 0xff
sleep 2




