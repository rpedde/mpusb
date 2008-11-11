#!/usr/bin/env ruby

require 'mkmf'

find_header("mpusb/mpusb.h","../..")
if !have_library("mpusb","mp_init")
  find_library("mpusb", "mp_init", "../../mpusb/.libs")
end
have_library("c","va_end")
$LDFLAGS += ' -framework CoreFoundation -lc'
create_makefile "mpusb"
