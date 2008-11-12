#!/usr/bin/env ruby

require 'rubygems'
require 'pp'

spec = Gem::Specification.new do |s|
  s.name = "MPUSB"
  s.version = "0.0.1"
  s.author = "Ron Pedde"
  s.email = "ron@pedde.com"
  s.homepage = "http://www.monkeypuppetlabs.com"
  s.platform = Gem::Platform::RUBY
  s.summary = "A Ruby wrapper for the Monkey Puppet USB controller"
  candidates = Dir.glob("{bin,docs,lib,test,examples,ext}/**/*")
  s.files = candidates.delete_if do | item |
    item.include?("CVS") || item.include?("rdoc") || item.include?("svn") ||
      /~$/.match(item)
  end

  puts "Files\n-----\n"
  pp(s.files)

  s.require_path = "lib"
  s.autorequire = "mpusb"

  #s.has_rdoc = true
  #s.extra_rdoc_files = ["README"]

  #s.add_dependancy("Foo", ">= 0.0.4")

  s.extensions = ["ext/extconf.rb"]
end

if $0 == __FILE__
  Gem::manage_gems
  Gem::Builder.new(spec).build
end


