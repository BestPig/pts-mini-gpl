#! /usr/bin/ruby1.8
require 'test/unit'
require './callcc.rb'  # !!
#require 'fiber'
#require 'continuation'
#require_relative './envutil'

class SubProcessError <StandardError
end

def make_srcfile(src)
  filename = 'bootstraptest.tmp.rb'
  File.open(filename, 'w') {|f|
    f.puts "print(begin; #{src}; end))))"
  }
  filename
end


def assert_normal_exit(testsrc, *rest)
  opt = {}
  opt = rest.pop if Hash === rest.last
  message, ignore_signals = rest
  message ||= ''
  timeout = opt[:timeout]
  #$stderr.puts "\##{@count} #{@location}" if @verbose
  faildesc = nil
  filename = make_srcfile(testsrc)
  old_stderr = $stderr.dup
  timeout_signaled = false
  begin
    $stderr.reopen("assert_normal_exit.log", "w")
    # SUXX: doesn't work, $0 is not the ruby interpreter name
    io = IO.popen("#{$0} -W0 #{filename}")
    pid = io.pid
    th = Thread.new {
      io.read
      io.close
      $?
    }
    if !th.join(timeout)
      Process.kill :KILL, pid
      timeout_signaled = true
    end
    status = th.value
  ensure
    $stderr.reopen(old_stderr)
    old_stderr.close
  end
  if status.signaled?
    signo = status.termsig
    signame = Signal.list.invert[signo]
    unless ignore_signals and ignore_signals.include?(signame)
      sigdesc = "signal #{signo}"
      if signame
        sigdesc = "SIG#{signame} (#{sigdesc})"
      end
      if timeout_signaled
        sigdesc << " (timeout)"
      end
      faildesc = pretty(testsrc, "killed by #{sigdesc}", nil)
      stderr_log = File.read("assert_normal_exit.log")
      if !stderr_log.empty?
        faildesc << "\n" if /\n\z/ !~ faildesc
        stderr_log << "\n" if /\n\z/ !~ stderr_log
        stderr_log.gsub!(/^.*\n/) { '| ' + $& }
        faildesc << stderr_log
      end
    end
  end
  if !faildesc
    $stderr.print '.'
    true
  else
    $stderr.print 'F'
    raise SubProcessError, "#{faildesc}  #{message}"
    false
  end
rescue SubProcessError
  raise
rescue Exception => err
  $stderr.print 'E'
  raise SubProcessError, "#{err.message}  #{message}"
  false
end


class TestFiber < Test::Unit::TestCase
  def test_normal
    f = Fiber.current
    assert_equal(:ok2,
      Fiber.new{|e|
        assert_equal(:ok1, e)
        Fiber.yield :ok2
      }.resume(:ok1)
    )
    assert_equal([:a, :b], Fiber.new{|a, b| [a, b]}.resume(:a, :b))
  end

  def test_argument
    assert_equal(4, Fiber.new {|*i| i[0] || 4}.resume)
  end

  def test_term
    assert_equal(:ok, Fiber.new{:ok}.resume)
    assert_equal([:a, :b, :c, :d, :e],
      Fiber.new{
        Fiber.new{
          Fiber.new{
            Fiber.new{
              [:a]
            }.resume + [:b]
          }.resume + [:c]
        }.resume + [:d]
      }.resume + [:e])
  end

  # !! works but slow, and makes other tests slow as well
  def zzz_test_massign
    a,s=[],"aaa"
    300.times { a<<s; s=s.succ }
    eval <<-END__
    GC.stress=true
    Fiber.new do
      #{ a.join(",") },*zzz=1
    end.resume
    END__
    :ok
  end

  def zzz_test_many_fibers  # !!
    max = 10000
    assert_equal(max, max.times{
      Fiber.new{}
    })
    assert_equal(max,
      max.times{|i|
        Fiber.new{
        }.resume
      }
    )
  end

  # !!
  def zzz_test_many_fibers_with_threads
    max = 1000
    @cnt = 0
    (1..100).map{|ti|
      Thread.new{
        max.times{|i|
          Fiber.new{
            @cnt += 1
          }.resume
        }
      }
    }.each{|t|
      t.join
    }
    assert_equal(:ok, :ok)
  end

  def test_error
    assert_raise(ArgumentError){
      Fiber.new # Fiber without block
    }
    assert_raise(FiberError){
      f = Fiber.new{}
      Thread.new{f.resume}.join # Fiber yielding across thread
    }
    assert_raise(FiberError){
      f = Fiber.new{}
      f.resume
      f.resume
    }
    # !! Known limitation of the emulation (RuntimeError is not called)
    #assert_raise(RuntimeError){
    #  f = Fiber.new{
    #    @c = callcc{|c| @c = c}
    #  }.resume
    #  @c.call # cross fiber callcc
    #}

    assert_raise(RuntimeError){
      Fiber.new{
        raise
      }.resume
    }
    assert_raise(FiberError){
      Fiber.yield
    }
    assert_raise(FiberError){
      fib = Fiber.new{
        fib.resume
      }
      fib.resume
    }
    assert_raise(FiberError){
      fib = Fiber.new{
        Fiber.new{
          fib.resume
        }.resume
      }
      fib.resume
    }
  end

  # !! can LocalJumpError be implemented?
  #def test_return
  #  assert_raise(LocalJumpError){
  #    Fiber.new do
  #      return
  #    end.resume
  #  }
  #end

  # !! why ArgumentError -- nevertheless, emulated fibers would raise a
  # NameError instead of an ArgumentError.
  #def test_throw
  #  assert_raise(ArgumentError){
  #    Fiber.new do
  #      throw :a
  #    end.resume
  #  }
  #end

  def test_transfer
    ary = []
    f2 = nil
    f1 = Fiber.new{
      ary << f2.transfer(:foo)
      :ok
    }
    f2 = Fiber.new{
      ary << f1.transfer(:baz)
      :ng
    }
    assert_equal(:ok, f1.transfer)
    assert_equal([:baz], ary)
  end

  # !! Important incompatibility: Thread.current[var] is shared between fibers
  # in the emulation, but they are different in Ruby 1.9.
  # Solution idea: don't use callcc, use ruby Threads
  # (but don't schedule them).
  def zzz_test_tls
    #
    def tvar(var, val)
      old = Thread.current[var]
      begin
        Thread.current[var] = val
        yield
      ensure
        Thread.current[var] = old
      end
    end

    fb = Fiber.new {
      assert_equal(nil, Thread.current[:v]); tvar(:v, :x) {
      assert_equal(:x,  Thread.current[:v]);   Fiber.yield
      assert_equal(:x,  Thread.current[:v]); }
      assert_equal(nil, Thread.current[:v]); Fiber.yield
      raise # unreachable
    }

    assert_equal(nil, Thread.current[:v]); tvar(:v,1) {
    assert_equal(1,   Thread.current[:v]);   tvar(:v,3) {
    assert_equal(3,   Thread.current[:v]);     fb.resume
    assert_equal(3,   Thread.current[:v]);   }
    assert_equal(1,   Thread.current[:v]); }
    assert_equal(nil, Thread.current[:v]); fb.resume
    assert_equal(nil, Thread.current[:v]);
  end

  def test_alive
    fib = Fiber.new{Fiber.yield}
    assert_equal(true, fib.alive?)
    fib.resume
    assert_equal(true, fib.alive?)
    fib.resume
    assert_equal(false, fib.alive?)
  end

  def test_resume_self
    f = Fiber.new {f.resume}
    assert_raise(FiberError, '[ruby-core:23651]') {f.transfer}
  end

  def test_fiber_transfer_segv
    # Problem: there is no test assert_norm
    # !!assert_normal_exit %q{
    # Dead fiber called.
    #  require 'fiber'
    #  f2 = nil
    #  f1 = Fiber.new{ f2.resume }
    #  f2 = Fiber.new{ f1.resume }
    #  f1.transfer
    #}, '[ruby-dev:40833]'
  end
end
