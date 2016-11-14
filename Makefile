ptimer: ptimer.c
	gcc -Wall -Werror -o $@ $<

test: ptimer test-ptimer
	./test-ptimer
