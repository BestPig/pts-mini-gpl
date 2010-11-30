#! ./ruby1.9

class MyFiber;
  attr_accessor :cc
  def initialize(&block)
    if nil == block;
      self.cc = nil
    else
      callcc { |cc| self.cc = cc; return }
      block.call()
      fail "MyFiber ended"
    end
  end
  def transfer(*args)
    callcc { |cc|
      fail if nil != @@current.cc
      fail if nil == self.cc
      fail if self == @@current
      @@current.cc = cc
      cc = self.cc
      self.cc = nil
      @@current = self
      cc.call(*args)  # TODO(pts): Forget about cc (for garbage collection).
    }
  end
  class <<self;
    def current()
      return @@current
    end
  end
  @@current = self.new
end

# ---

def f(n, main)
  if n > 0;
    return 1 + f(n - 1, main)
  end
  while true;
    main.transfer 5
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


if true;
  main = MyFiber.current
  foo = MyFiber.new {
    p 42
    fail if 26 != main.transfer(15)
    fail if 27 != main.transfer(16)
    main.transfer(17)
    fail
  }
  p 41
  fail if 15 != foo.transfer()
  p 43
  fail if 16 != foo.transfer(26)
  p 44
  fail if 17 != foo.transfer(27)
  p 45
end

depth = 230  # SystemStackError above that.
main = MyFiber.current
g(depth, MyFiber.new { f(depth, main) })

# depth=230 iter=100000 ./callcc.rb  1.80s user 0.06s system 100% cpu 1.825 total
# depth=2   iter=100000 ./callcc.rb  0.74s user 0.00s system 100% cpu 0.737 total
