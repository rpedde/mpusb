#!/usr/bin/env ruby

require "singleton"

class MPUSBAPI
  include Singleton
end

class MPUSB
  class << self
    def devicelist
      if !@apidevicelist
        @apidevicelist = MPUSBAPI.instance.devicelist
      end

      if !@devicelist
        @devicelist = []
        @apidevicelist.each do |apidevice|
          @devicelist << factory_create(apidevice)
        end
      end

      @devicelist
    end

    def open(serial = MPUSBAPI::BOARD_SERIAL_ANY, type = MPUSBAPI::BOARD_TYPE_ANY)
      begin
        device = MPUSBAPI.instance.open(type, serial)
        result = factory_create(device)
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

    def set_i2c_range(min = MPUSBAPI::I2C_DEFAULT_MIN, max = MPUSBAPI::I2C_DEFAULT_MAX)
      MPUSBAPI.i2c_probe_max(max)
      MPUSBAPI.i2c_probe_min(min)
    end

    # create an appropriate MPUSBDevice for the device
    def factory_create(device)
      if device.board_id == MPUSBAPI::BOARD_TYPE_POWER
        puts "Creating device of type Power"
        return MPUSBPowerDevice.new(device)
      elsif device.board_id == MPUSBAPI::BOARD_TYPE_I2C
        puts "Creating device of type I2C"
        return MPUSBI2CDevice.new(device)
      else
        puts "Creating generic device"
        return MPUSBDevice.new(device)
      end
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

class MPUSBI2CDevice < MPUSBDevice
  def initialize(apidevice)
    super(apidevice)
    @i2c_devices = nil
  end

  def i2c_device_by_serial(serial) 
    maybe_get_devices
    @i2c_devices.each do |device|
      if device.device_address == serial
        return device
      end
    end

    nil
  end

  def i2c_devices
    maybe_get_devices
    @i2c_devices
  end

  private
  def maybe_get_devices
    unless @i2c_devices
      @i2c_devices = []
      @apidevice.i2c_devices.each do |device|
        @i2c_devices << MPUSBI2CBoard.new(@apidevice,device,device.i2c_addr)
      end
    end
  end
end

class MPUSBI2CBoard
  def initialize(apidevice, i2c_device, device_address)
    @apidevice = apidevice
    @i2c_device = i2c_device
    @device_address = device_address
  end

  def write(address, value)
    @apidevice.i2c_write(@device_address, address, value)
  end

  def read(address, len)
    @apidevice.i2c_read(@device_address, address, len)
  end

  # there are at least three standard EEPROM addresses
  # on all i2c devices.
  #
  # READ
  # 0: mpusb magic 0xAE
  # 1: device type
  # 2: read EEPROM Index
  # 3: read EEPROM
  #
  # WRITE
  # 0: magic (non-writable)
  # 1: type (non-writable)
  # 2: set EEPROM index
  # 3: write EEPROM
  #
end
