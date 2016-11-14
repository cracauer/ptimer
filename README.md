# ptimer
A program you wrap about a command that reports specific exit status
in a grep/script friendly manner.

It reports everything you can find out from waitpid(2) and reports it
line by line.  The point of this is explained here:
https://www.cons.org/cracauer/sigint.html

Times are given as integers with labels, so that you can use pattern
matching and shell arithmetic.  I will probably report in at least two
different scales so that it is safe to do shell arithmetic on 32 bit
platforms [unimplemented as of 20161114].

Use it like time(1).  Commandline option syntax will be kept
compatible with time(1) within reason.  Right now the only option
supported and mandatory for now is "-o".  If you need to append you
can use /dev/stderr or /dev/fd/<n> and shell syntax like this:
    `ptimer -o /dev/fd/5 foo [...shellcode using fd1 and fd2] 5>&1 | bar`
    `ptimer -o /dev/fd/5 foo [...shellcode using fd1 and fd2] >> timefile 5>&1`
