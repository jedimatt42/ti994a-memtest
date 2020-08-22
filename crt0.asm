 def _start
 def _start2
 def _start3

# burnin version
_start:
  limi 0       # Disable interrupts
  lwpi >8300   # Set initial workspace
# Create stack
  li sp, >8400
# Enter C environment
  li r1, >0100   # flag to indicate operating mode
  b @main

# quick version
_start2:
  limi 0
  lwpi >8300
# Create stack
  li sp, >8400
# Enter C environment
  li r1, >0001     # flag to indicate operating mode
  b @main

# quick version
_start3:
  limi 0
  lwpi >8300
# Create stack
  li sp, >8400
# Enter C environment
  li r1, >0000     # flag to indicate operating mode
  b @main