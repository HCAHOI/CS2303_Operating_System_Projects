# OS Project3

## Project Structure

├── PrjExample.pdf		Display the example input and output of the programs.
├── PrjREADME.md		This file :)
├── PrjReport.pdf		   The report of this project.
├── step1
│   ├── disk.c
│   └── Makefile
├── step2
│   ├── fs.c
│   ├── Makefile
│   └── test.cpp
└── step3
    ├── client.c
    ├── disk.c
    ├── fs.c
    └── Makefile

## Introduction

### Step 1: Design a basic disk-storage system

* **source file**: disk.c

* **compile**:

  ```bash
  make
  ```

* **usage**:

  ```bash
  ./disk <cylinders> <sector per cylinder> <track-to-track delay> <disk-storage-filename>
  ```

### Step 2: Design a basic file system

* **source file**: fs.c

* **compile**:

  ```bash
  make
  ```

* **usage**:

  ```bash
  ./fs
  ```

### Step 3: Work together

* **source file**: disk.c, fs.c, client.c

* **compile**:

  ```bash
  make
  ```

* **usage**:

  ```bash
  ./disk <cylinders> <sector per cylinder> <track-to-track delay> <disk-storage-filename> <DiskPort>
  ./fs <DiskPort> <FSPort>
  ./client <FSPort>
  ```

