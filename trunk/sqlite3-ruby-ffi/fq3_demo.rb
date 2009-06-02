#! /usr/bin/env ruby
# by pts@fazekas.hu at Thu May 28 17:28:03 CEST 2009
require 'rubygems'
require 'ffi'

module FQ3
  extend FFI::Library
  ffi_lib "libsqlite3.so"  # finds it in /usr/lib
  #int callback(void *dummy, int argc, char **data, char **colnames)
  callback :result, [:pointer, :int, :pointer, :pointer], :int
  attach_function :sqlite3_open, [:string, :pointer], :int
  attach_function :sqlite3_close, [:pointer], :void
  attach_function :sqlite3_errmsg, [:pointer], :string
  attach_function :sqlite3_exec, [
      :pointer, :string, :result, :int, :pointer], :int
end

SQLITE_OK = 0

Result = Proc.new do |dummy,argc,data,colnames|
  h = {}
  argc.times { |i|
    h[colnames.get_pointer(FFI.type_size(:pointer) * i).get_string(0)] = (
        data.get_pointer(FFI.type_size(:pointer) * i).get_string(0))
  }
  $stdout << "row #{h.inspect}\n"
  0
end

dbfn = 't.sqlite3'
query = 'SELECT * FROM foo'
dbptr = FFI::MemoryPointer.new(:pointer)
rc = FQ3.sqlite3_open(dbfn, dbptr)  # creates the db file if needed
fail "#{rc}" if rc != SQLITE_OK  # use sqlite3_errmsg
db = dbptr.get_pointer(0)
errmsgptr = FFI::MemoryPointer.new(:pointer)
rc = FQ3::sqlite3_exec(db, query, Result, 0, errmsgptr)
#: row {"id"=>"1", "s"=>"hello"}
#: row {"id"=>"2", "s"=>"world"}
#: row {"id"=>"42", "s"=>"answer"}
fail "#{rc}" if rc != SQLITE_OK
FQ3.sqlite3_close(db)

# official: http://kenai.com/projects/ruby-ffi/pages/Home
# http://blog.headius.com/2008/10/ffi-for-ruby-now-available.html
# http://lifegoo.pluskid.org/?p=370
