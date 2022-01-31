j start 

; SYSE ($0)
exit:	li %0 $0 sys %0 jr %3
; SYSW ($2)
write:	li %0 $2 jr %3
