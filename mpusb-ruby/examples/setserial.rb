#!/usr/bin/env ruby

$:.unshift("../lib")
require "mpusb"

if ARGV.length != 1
  puts "Usage: ./setserial.rb <serial>"
  exit(1)
end

new_serial = ARGV[0].to_i

device = MPUSB.open()
puts "Found a device of type: #{device.board_type}"
puts "Changing serial: #{device.serial} => #{new_serial}"
device.set_serial(new_serial)
puts "Done"





