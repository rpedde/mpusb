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

    def debug
      MPUSBAPI.debug
    end

    def debug=(newvalue)
      MPUSBAPI.debug=newvalue
    end

    def set_i2c_range(max = MPUSBAPI::I2C_DEFAULT_MAX, min = MPUSBAPI::I2C_DEFAULT_MIN)
      MPUSBAPI.i2c_probe_max(max)
      MPUSBAPI.i2c_probe_min(min)
    end
  end

  def initialize
    MPUSBAPI.i2c_probe_max(20)
    MPUSBAPI.i2c_probe_min(8)
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

class MPUSBPowerDevice < MPUSBDevice
  #
  # device is the item id (for multi-device power devices)
  # state is true or false
  #
  def power_state(device, state) 
    @apidevice.power_set(device, state)
  end
end

class MPUSBI2CDevice
  def initialize(apidevice, device_address)
    @apidevice = apidevice
    @device_address = address
  end

  def write(address, value)
    @apidevice.write_i2c(@device_address, address, value)
  end

  # there are at least three standard EEPROM addresses
  # on all i2c devices.
  #
  # 0: boot mode (0: flash mode, 0xff: normal mode)
  # 1: device address (pre-shifted)
  # 2: 
  
end
