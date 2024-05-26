# mtws
2024-1 Operating Systems (ITP30002) - HW #3

A homework assignment to implement word search program using multi-threading and synchronization.

### Author
Hyunseo Lee (22100600) <hslee@handong.ac.kr>

## 1. How to build
This program is written in C language on Ubuntu 22.04.1 LTS. To build this program, you need to use Ubuntu 22.04.1 LTS with GCC and Make installed.

After unarchiving the folder, you should see the following files:
```bash
$ ls
Makefile  README.md  mtws.c
```

To build the program, run the following command in the terminal:
```bash
$ make
```

To build the program with debug mode, run the following command in the terminal:
```bash
$ make debug
```

To cleanup the build files and test files, run the following command in the terminal:
```bash
$ make clean
```

You can also generate the test files by running the following command in the terminal:
```bash
$ make testenv
```

## 2. Usage
To run the program, run the following command in the terminal:
```bash
$ ./mtws -b <bounded buffer size> -t <number of threads> -d <search directory> -w <search word>
```
