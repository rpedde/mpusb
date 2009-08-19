#!/usr/bin/env ruby

$:.unshift("../lib")
require "mpusb"

if ARGV.length != 1
  puts "Usage: ./setserial.rb <serial>"
  exit(1)
end

new_serial = ARGV[0].to_i

MPUSB.debug
MPUSB.set_i2c_range()

device = MPUSB.open()
raise "Can't find mpusb device" unless device

puts "Found a device of type: #{device.board_type}"
puts "Changing serial: #{device.serial} => #{new_serial}"
device.set_serial(new_serial)
puts "Done"





