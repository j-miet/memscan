### Update 24.07.2025 
I have decided to abandon this project. Simply put, frustaration far exceeds the enjoyment when working on it.  

Specifically:
- Reading from and writing to memory is painfully inconsistent. 
It works on some values on some processes and on others it does not work at the slightest. Even administrator rights
couldn't solve the issue and instead limited these processes even more. And finally, I even tested a couple other open 
source projects and turns out they performed even worse...  Thus, I suppose the issues is on my end, which makes testing
 the code borderline impossible.
- mixing C++ and C. I should have just stuck with C and focused on the low-level stuff without messing with C++ standard
 library. Not only this, I lacked experience on both languages before this so a project with increasing learning curve 
 would have served me better.
- probably most important: this project felt like *I forced myself to do it* vs *I wanted to do it*. I  certainly wanted
 to do some lower-level programming, and topic of accessing computer memory sounded interesting. But this scanner just 
 wasn't it.
- relying too much on a tutorial. I did learn a lot by reading excessive amounts of C/C++ documentations, but the code
itself was mostly a bad C++ emulation from the original tutorial (here C++ means using various C++ tools randomly
without a real though behind it)

It was a good learning experience with 3 weeks spent on it, and now it shall serve as an example of a failed project.  

---

A basic memory scanner for *Windows OS* in C++ that can access and modify stored integer values. It can also read 
non-unicode strings, but cannot write new values.  
>Based on an older but informative video tutorial 
[C programming: Write a memory scanner](https://www.youtube.com/watch?v=YRPMdb1YMS8) 
(has total of 8 parts).

Two sources are included:
- **src/memscan** is the main tool. Written in C++ and has some minor improvements compared to original C code.
- **src/memscanC** is almost a 1-to-1 copy of tutorial C code.


No executable is provided so you need to compile the .ccp or .c files to create one: install gcc (C) or g++ (C++), then
run either ``g++ -o scanner *.cpp`` for memscan, or ``gcc -o memscanC memscanC.c`` for memscanC.

### How to use

- Run executable file which is created after compiling
- give 3 values to UI:
    - process id: open terminal and type *tasklist* to display all open processes and their ids
    - byte size: 1, 2, 4 8 or s for strings. Empty input means 8
    - value to search for. Leave empty to search for all possible registers
- after this, UI opens and explains rest of the commands