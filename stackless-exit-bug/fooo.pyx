import stackless

def SoftMainLoop():
  stackless.schedule()
  stackless.schedule()
  raise AssertionError
