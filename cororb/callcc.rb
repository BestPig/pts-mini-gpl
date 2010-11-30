#! /usr/bin/ruby -w
#
# callcc.rb: a Fiber implementation for Ruby 1.8, based on callcc
# by pts@fazekas.hu at Tue Nov 30 21:36:00 CET 2010
#
# This script works in Ruby 1.9 (using the built-in Fiber class) and Ruby 1.8
# (using the emulation provided here).
#
# TODO(pts): Run the Ruby unit tests.
$-w = true

class FiberError < StandardError
end

# TODO(pts): Measure speed.
# TODO(pts): Evaluate memory use and GC memory leaks.
class Fiber;
  fail "Fiber replacement loaded multiple times" if defined?(@@current)
  fail "Fiber emulation loaded over another Fiber" if method_defined?(:alive?)
  @@current = @@root = nil
  def initialize(&block)
    if nil == block;
      fail "multiple root fibers" if nil != @@current
      @cc = nil
      @is_alive = true
      @resumed_by = nil
    else
      @is_alive = true  # Can be resumed/transferred later.
      @resumed_by = false
      x = callcc { |cc| @cc = cc; return }
      x = block.call(*x)
      @is_alive = false
      # TODO(pts): Discard the current stack frame to reclaim memory.
      # TODO(pts): Avoid creating local variables.
      # Ruby 1.8 doesn't have tail recursion optimization, and 1.9.2 doesn't
      # have it either, see also http://redmine.ruby-lang.org/issues/show/1256
      if @resumed_by;
        resumed_by = @resumed_by
        @resumed_by = nil  # Break the reference to save memory.
        resumed_by.transfer(x)
      else
        @@root.transfer(x)
      end
    end
  end
  def alive?()
    @is_alive
  end
  def transfer(*args)
    raise FiberError, "dead fiber called" if !@is_alive
    if self == @@current;
      fail if args.size > 1  # ruby 1.9.2 does this.
      return args[0]  # nil if args.empty?
    end
    fail if nil != @@current.cc
    fail if nil == @cc
    x = callcc { |cc|
      @@current.cc = cc
      cc = @cc
      @cc = nil
      @@current = self
      cc.call(args)  # TODO(pts): Forget about cc (for garbage collection).
    }
    x.size > 1 ? x : x[0]
  end
  def resume(*args)
    raise FiberError, "dead fiber called" if !@is_alive
    raise FiberError, "double resume" if @resumed_by
    @resumed_by = @@current
    transfer(*args)
  end
  def __yield(*args)  # TODO(pts): Don't expose this publicly.
    raise FiberError, "can't yield from root fiber" if
        nil == @resumed_by and self == @@root
    resumed_by = @resumed_by || @@root
    @resumed_by = nil
    # TODO(pts): Clean up our stack to save memory.
    resumed_by.transfer(*args)
  end
  class <<self;
    def current()
      @@current
    end
    def yield(*args)
      @@current.__yield(*args)
    end
  end
  @@root = @@current = self.new
  protected
  attr_accessor :cc
end if RUBY_VERSION < "1.9"

# ---

p RUBY_VERSION

if true;
  root = Fiber.current
  fail if "can't yield from root fiber" != begin
    Fiber.yield
  rescue FiberError
    $!.to_s
  end
  fail if true != root.alive?
  fail if nil != root.transfer()
  fail if 3 != root.transfer(3)
  fail if 4 != root.resume(4)
  fail if "double resume" != begin
    root.resume() && nil
  rescue FiberError
    $!.to_s
  end
  # Yes, we can yield from the root fiber, if we have previously resumed it.
  # This works in Ruby 1.9.2.
  fail if 5 != Fiber.yield(5)
  fail if "can't yield from root fiber" != begin
    Fiber.yield 6
  rescue FiberError
    $!.to_s
  end

  fi = Fiber.new { |*x| root.transfer({:x=>x}).inspect }
  fail if true != fi.alive?
  fail if {:x=>[]} != fi.transfer()
  fail if true != fi.alive?
  fail if 56.inspect != fi.transfer(56)
  fail if false != fi.alive?
  fail if "dead fiber called" != begin
    fi.transfer()
  rescue FiberError
    $!.to_s
  end
  fail if "dead fiber called" != begin
    fi.resume()
  rescue FiberError
    $!.to_s
  end
  fail if "dead fiber called" != begin  # Not a "double resume".
    fi.resume()
  rescue FiberError
    $!.to_s
  end
  fi = Fiber.new { |*x| root.transfer({:x=>x}).inspect }
  fail if {:x=>[nil]} != fi.transfer(nil)
  fail if [57, 58].inspect != fi.transfer(57, 58)
  fi = Fiber.new { |*x| root.transfer({:x=>x}) + [3, 4] }
  fail if {:x=>[[:a]]} != fi.transfer([:a])
  fail if [1, 2, 3, 4] != fi.transfer([1, 2])
  fi = Fiber.new { |*x| root.transfer({:x=>x}) }
  fail if {:x=>[:a]} != fi.transfer(:a)
  fi = Fiber.new { |*x| root.transfer({:x=>x}) }
  fail if {:x=>[:a, :b]} != fi.transfer(:a, :b)
  fi = Fiber.new { |*x| root.transfer({:x=>x}) }
  fail if {:x=>[[:a, :b]]} != fi.transfer([:a, :b])
  fi = Fiber.new { |*x| Fiber.yield({:x=>x}) }
  fail if {:x=>[[:a, :b, :c]]} != fi.resume([:a, :b, :c])
  fi = Fiber.new { |*x| Fiber.yield(x) }
  fail if [[:a, :b, :c]] != fi.resume([:a, :b, :c])
  fi = Fiber.new { |*x| Fiber.yield(x) }
  fail if [42] != fi.resume(42)
  fi = Fiber.new { |*x| Fiber.yield(x[0] * 2) }
  fail if 84 != fi.resume(42)
  fi = Fiber.new { |*x| Fiber.yield(x[0] + 1, x[0] + 2) }
  fail if [43, 44] != fi.resume(42)

  # This is to demonstrate that if a non-resume()d fiber yield()s, then it
  # will give control to the root fiber. If it gave control to any of its
  # caller fibers instead, the return value would be be multiplied by 10
  # and/or -1.
  fail if 42 != Fiber.new {
    Fiber.new { Fiber.new { Fiber.yield 42 }.transfer() * 10 }.resume() * -1
  }.transfer()

  # This is to demonstrate that if a resume()d fiber yields(), then it will
  # give control to the fiber which resumed() it.
  fail if -420 != Fiber.new {
    Fiber.new { Fiber.new { Fiber.yield 42 }.resume() * 10 }.resume() * -1
  }.transfer()

  # Combination of the rules above.
  fail if 420 != Fiber.new {
    Fiber.new { Fiber.new { Fiber.yield 42 }.resume() * 10 }.transfer() * -1
  }.resume()

  yield_for_me = lambda { |x| Fiber.yield x }
  squares = Fiber.new { |n|
    Fiber.yield :a
    i = 1
    while i * i <= n;
      Fiber.yield i * i
      i += 1
    end
  }
  double = Fiber.new { |fi1|
    Fiber.yield :b
    loop {
      x = fi1.resume()
      break if nil == x
      yield_for_me.call x * 2
    }
  }
  fail if :a != squares.resume(25)
  fail if :b != double.resume(squares)
  items = []
  loop {
    x = double.resume()
    break if nil == x
    items << x
  }
  fail if [2, 8, 18, 32, 50] != items

  # This is to demonstrate that if a transfer()-ed fiber returns, it will
  # return control to the root fiber. If control was returned to f2 and/or f3,
  # then the returned value would be multiplied by 10 and/or -1.
  f2 = Fiber.new { |fi1| fi1.transfer() * 10 }
  f3 = Fiber.new { f2.transfer(Fiber.new { 42 }) * -1 }
  fail if 42 != f3.transfer()

  # This is to demonstrate that when a resume()d fiber returns, it will
  # return control to the fiber which resumed it.
  f2 = Fiber.new { |fi1| fi1.resume() * 10 }
  f3 = Fiber.new { f2.resume(Fiber.new { 42 }) * -1 }
  fail if -420 != f3.resume()

  # Combination of the rules above.
  f2 = Fiber.new { |fi1| fi1.transfer() * 10 }
  f3 = Fiber.new { f2.resume(Fiber.new { 42 }) * -1 }
  fail if 42 != f3.resume()

  # Combination of the rules above.
  f2 = Fiber.new { |fi1| fi1.resume() * 10 }
  f3 = Fiber.new { f2.transfer(Fiber.new { 42 }) * -1 }
  fail if 420 != f3.resume()

  #p root.cc  # Can't call protected method.
  foo = Fiber.new { |*x|
    fail if x != [41, 410]
    fail if [26, 260] != root.transfer(15)
    fail if 27 != root.transfer(16)
    root.transfer(17)
    fail
  }
  fail if 15 != foo.transfer(41, 410)
  fail if 16 != foo.transfer(26, 260)
  fail if 17 != foo.transfer(27)
end

def f(n, root)
  if n > 0;
    return 1 + f(n - 1, root)
  end
  while true;
    root.transfer 5
  end
end

def g(n, fi)
  if n > 0;
    return 1 + g(n - 1, fi)
  end
  100000.times {
    fail if 5 != fi.transfer
  }
  0
end

p "All OK."

#depth = 230  # SystemStackError above that.
#root = Fiber.current
#g(depth, Fiber.new { f(depth, root) })

# depth=230 iter=100000 ./callcc.rb  1.80s user 0.06s system 100% cpu 1.825 total
# depth=2   iter=100000 ./callcc.rb  0.74s user 0.00s system 100% cpu 0.737 total
