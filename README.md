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
```
    `ptimer -o /dev/fd/5 foo [...shellcode using fd1 and fd2] 5>&1 | bar`
    `ptimer -o /dev/fd/5 foo [...shellcode using fd1 and fd2] >> timefile 5>&1`
```

OUTPUT FILE FORMAT:

All lines in the output file should be formatted as follows:
number [kmg] unit name$

The idea is that you can always safely do `grep ' name$'` (matching
end of line word) and be sure you get only the correct line - except
that the same field can be given in different units or multipliers.
The multiple multiplier thing is done for the benefit of shellscripts
on 32 bit platforms, so that you can do shellarithmetic (no forking)
without fear of overflow.  The first line for any given will *always*
be a unit that is always given in all future versions of this
program.  So if it gives you raw nanoseconds as the first line for
"realtime" today then you can write a shellscript today that will
always find that name in that unit with that multiplier.
Consequently, multipliers (or multiple units given) will be kept
forever once introduced.  If you find a certain multiplier/unit/name
combination then you will find that everywhere where the version of
this program is equal or greater.  Some platforms might miss fields,
though.  I have not decided whether that will be printed with a "N/A"
(for "not available") or simply missing that line.  I tend to decide
on the latter.  In general, you will have to make your script safe
against the field you are looking for not being present.  The file
might have been truncasted when running this progam and getting hard
interrupted, or later due to corruption.

I have not decided on whether I want to guarantee that fields are
integers.  I will guarantee that the first version of a field will be
integer.

TODO: have not thought about how to integrate signal reporting into
the numeric structure.

Special cases:
- if there is no multiplier the string "1x" is used
- I have not decided on how I will deal with multiplier and the x^10
  versus x^2 problem.  Probably taking the high road and writing full
  length "kilo" and "kibi".
- bytes and bits are written as "bytes" and "bits"


Supported platforms that I generally test before committing:
- FreeBSD/amd64
- Linux/amd64
- OSX/amd64
