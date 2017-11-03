Instructions to compile the files

1)There are two important files namely main.c,clearout.c.

for main.c
-------------------------------
gcc -g -std=c99 main.c -o main -pthread -lrt

for clearout.c 
-------------------------------
gcc -g -std=c99 clearout.c -o clearout -pthread -lrt


2)Usage of these files

./main or ./main <filename>
starts the table reservation program


./clearout 
destroys all the named semaphore and shared memory.


These executables can be run in different terminals opened in the same directory.