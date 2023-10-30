# OS Project1

## Project Structure

├── PrjREADME.md 					 This file :)
├── PrjReport.pdf						 The report of this project which talks about all the things interesting I found.
├── PrjExample.pdf 					 Display the example input and output of programs.
├── **pipe_lab** 								Lab1
│  ├── data_genarator.c 			 Helper program, it can generate src.txt randomly for testing.
│  ├── dest.txt 							 		
│  ├── Copy.c 								
│  ├── Makefile 							 
│  └── src.txt 
├── **shell_lab** 								Lab2
│  ├── Makefile 
│  └── shell.c
└── **matrix_lab** 							Lab3
   ├── data.in 
   ├── data.out 
   ├── random.in 
   ├── random.out 
   ├── Makefile 
   ├── multi.c 							    Multi-thread matrix multiplication program.
   ├── single.c 							   Single-thread matrix multiplication program.
   ├──data_genarate.h 		        Random matrix generator.
   └── thread_pool.h					Thread pool library.

## Introduction

### Lab 1

* **source file**: Copy.c

* **compile**:

  ```bash
  make
  ```

* **usage**:

  ```bash
  ./Copy <InputFile> <OutputFile> <BufferSize> 
  ```

* Data-generator will randomly generate a matrix for testing and run the main program with given buffer size.

  ```bash
  ./Gen <Size> #A 3*Size x Size matrix will be generated. 
  ```

### Lab 2

* **source file**: shell.c

* **compile**:

  ```bash
  make
  ```

* **usage**:

  Run command below to run the server.

  ```bash
  ./shell <Port>
  ```

  Run the command below  to connect the server.

  ```bash
  telnet <IPAddress> <Port>
  ```

### Lab 3

* **source file**: multi.c, single.c

* **compile**:

  ```bash
  make
  ```

* **usage**:

  ```bash
  ./single
  ./multi
  ```

  When no command line argument given, the program automatically read input from “data.in”, and output to “data.out”.

  ```bash
  ./single <Size>
  ./multi <Size>
  ```
  
  The program randomly generates two matrix and compute.