#!/usr/bin/env ruby

$:.unshift("../lib")
require "mpusb"

require "pp"

#MPUSB.debug = 1
MPUSB.set_i2c_range(16,8)

device = MPUSB.open()
device.write_i2c(8,65,"\x01")
device.write_i2c(8,64,"Ron Pedde")




