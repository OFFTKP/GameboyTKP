CHECK_REG equs "rSTAT"
TEST_SIZE equ 64

INCLUDE "speed_switch_timing.inc"

label:
  db "STAT:", 0

preSwitch:
  ret

expect:
  db $80, $82, $82, $82, $82, $82, $82, $82
  db $82, $83, $83, $83, $83, $83, $83, $83
  db $83, $83, $83, $83, $83, $83, $83, $83
  db $83, $83, $80, $80, $80, $80, $80, $80
  db $80, $80, $80, $80, $80, $80, $80, $80
  db $80, $80, $80, $80, $80, $80, $80, $82
  db $82, $82, $82, $82, $82, $82, $82, $83
  db $83, $83, $83, $83, $83, $83, $83, $83
