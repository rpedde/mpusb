#!/usr/bin/env ruby

require "singleton"

class MPUSBAPI
  include Singleton
end

class MPUSB
  class << self
    def devicelist
      MPUSBAPI.instance.devicelist
    end

    def open(serial = MPUSBAPI::BOARD_SERIAL_ANY, type = MPUSBAPI::BOARD_TYPE_ANY)
      begin
        device = MPUSBAPI.instance.open(type, serial)
        result = MPUSBDevice.new(device)
      rescue Exception
        result = nil
      end

      result
    end
  end

  def initialize
  end
end

class MPUSBDevice
  def initialize(apidevice)
    @apidevice = apidevice
  end

  def read_eeprom(address)
    result = @apidevice.read_eeprom(address)
  end

  def write_eeprom(address, value)
    result = @apidevice.write_eeprom(address, value)
  end

  def write_i2c(device, address, value)
    result = @apidevice.i2c_write(device,address,value,value.length)
  end

  def set_serial(serial)
    write_eeprom(1, serial)
    if(read_eeprom(1) != serial)
      raise RuntimeError, "Error setting new serial"
    end
  end

  def method_missing(id, *args)
    if @apidevice.instance_variables.include?("@#{id.to_s}")
      @apidevice.instance_variable_get("@#{id.to_s}")
    else
      raise NoMethodError, "undefined method `#{id}' for #{self.class}"
    end
  end
end

