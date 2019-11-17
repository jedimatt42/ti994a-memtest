 def _start
 def _start2

# burnin version
_start:
  limi 0       # Disable interrupts
  lwpi >8300   # Set initial workspace
# Create stack
  li sp, >8400
# Enter C environment
  li r12, >0100
  b @main

# quick version
_start2:
  limi 0
  lwpi >8300
# Create stack
  li sp, >8400
# Enter C environment
  li r12, >01
  b @main

