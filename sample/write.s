; memory test
li %0 $41
sb %0 $100
li %0 $42
sb %0 $101
li %0 $43
sb %0 $102
li %0 $0
sb %0 $103
li %2 $0
li %3 $2
li %8 $100
print:
	lb %1 $0
	beq %1 %2 end
	sys %3
	addi %8 $1 %8
	j print
end:
	li %0 $0
	sys %0
