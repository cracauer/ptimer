ptimer: ptimer.c
	gcc -Wall -Werror -o $@ $<

test: ptimer test-ptimer.sh
	./test-ptimer.sh

clean:
	@rm ptimer
	@rm test-ptimer.report
