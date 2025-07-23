A basic memory scanner in C++ that can access and modify stored integer values. *Windows only*.  
>Based on a old video tutorial [C programming: Write a memory scanner](https://www.youtube.com/watch?v=YRPMdb1YMS8) 
(has total of 8 parts).

Two sources are included:
- **src/memscan** is the main tool. Written in C++ and has some minor improvements compared to original C code.
- **src/memscanC** is almost a 1-to-1 copy of tutorial C code.


No executable is provided so you need to compile the .ccp or .c files to create one: install gcc (C) or g++ (C++), then
run either ``g++ -o scanner *.cpp`` for memscan, or ``gcc -o memscanC memscanC.c`` for memscanC.

[**Note**] There is a ``s`` option in byte size selection for strings i.e. search string values from memory, but it does not 
work. No idea why, but I'm done trying fixing it. At this point the codebase is a messy mix of C and C++ code, and should be rewritten from scratch.

### How to use

- Run executable file which is created after compiling
- give 3 values to UI:
    - process id: open terminal and type *tasklist* to display all open processes and their ids
    - byte size: 1, 2, 4 or 8. Empty input means 8
    - value to search for. Leave empty to search for all possible registers
- after this, UI opens and explains rest of the commands