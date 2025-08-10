A basic memory scanner for Windows operating systems.  
Can access and modify stored integer values. It can also read strings, but writing string values doesn't really work. *I've tested a few other other open source string scanners, however they didn't work either so maybe the problem is on my end*.

Two sources are included:
- **src/memscanC** is almost a 1-to-1 copy of 
[this older youtube tutorial](https://www.youtube.com/watch?v=YRPMdb1YMS8) 
(has total of 8 parts)
- **src/memscan** is the main tool. Uses C version's concepts as baseline, but reworks the codebase by using C++ features: classes, improved data structures, reduced use of (raw) pointers etc.

No executable is provided, therefore requiring you to compile the .cpp/.hpp or .c files to create one: 
- for C++: install g++ (has to support C++17 so version 8 or newer), change directory to *memscan* and run command
``g++ -o memscan *.cpp``
- for C version, install gcc, change directory to *memscanC* then ``gcc -o memscanC memscanC.c``.

### How to use

1. After compiling, run executable file
2. give 3 values to UI:
    - process id: type command *tasklist* to display all open processes and their ids
    - byte size: *1, 2, 4 8* or *s* for strings. Empty input means 4.
    - value to search for: leave empty to search for all possible registers. For strings, empty input doesn't make sense so it searches empty string.
3. after this, UI opens and explains rest of the commands