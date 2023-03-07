# Shell
A program made in C that supports the following shell features:
• Starting a program that can be found anywhere in the user’s search path ($PATH).
1
• String parsing, for example ./a.out "some string <> with 'special' characters".
• Command composition, for example: ./a.out && ./b.out || ./c.out; ./d.out.
• I/O redirection, for example: ./a.out < in > out.
• Pipes, for example: ./a.out | ./b.out | ./c.out.
• Background processes, for example: ./a.out &.
• Signals, so that you can send Ctrl + C to terminate the child application instead of the shell
itself.
• A kill command that will terminate a specific (or all) child process(es)
