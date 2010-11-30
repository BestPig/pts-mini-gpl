#! ./ruby1.9

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
  10000000.times {
    fail if 5 != fi.transfer
  }
  0
end

GC.disable
depth = 230  # SystemStackError above that.
# Fiber is a ruby1.9 feature, missing from ruby-1.8.7
main = Fiber.current
g(depth, Fiber.new { f(depth, main) })

# iter=10e6 depth=230  # ./fibe.rb  11.12s user 0.02s system 100% cpu 11.124 total
# iter=10e6 depth=2    # ./fibe.rb  11.13s user 0.01s system 100% cpu 11.134 total
                       # ./fibe.rb  11.14s user 0.02s system 100% cpu 11.144 total
