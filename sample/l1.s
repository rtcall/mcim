.include 'sys.s'

start:
jal write
li %1 $41
li %2 $7e
li %3 $1
loop:
	sys %0
	add %1 %3 %1
	bne %1 %2 loop 
jal exit
