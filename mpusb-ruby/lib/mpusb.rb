#!/usr/bin/env ruby

$:.unshift(File.dirname(__FILE__)) unless
  $:.include?(File.dirname(__FILE__)) || $:.include?(File.expand_path(File.dirname(__FILE__)))

if(File.exists?("#{File.dirname(__FILE__)}/../mpusb.gemspec"))
  puts "WARNING: using dev files, not system gem"
  require "../ext/mpusbapi"
else
  require 'mpusbapi'
end

require "mpusb/mpusb"
