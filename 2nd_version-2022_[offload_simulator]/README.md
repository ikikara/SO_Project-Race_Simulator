# SO_Project-Offload-Simulator

- [x] Finished

## Index
- [Description](#description)
- [To run this project](#to-run-this-project)

## Description
A program that simulates an offloading system.

## To run this project:
You have one way to run this project:
1. Using Terminal:
    * Download the folder "src"
    * Compile the program (using the makefile)
      ```shellscript
      [your-disk]:[name-path]> make
      ```
    * Finally just run it<br>
      + To run mobile node
        ```shellscript 
        [your-disk]:[name-path]> ./mobile_node [num-of-requests] [ms-each-request] [instructions*1000] [max-of-execution]
        ``` 
      + To run offload_simulator
        ```shellscript 
        [your-disk]:[name-path]> ./offload_simulator Config.txt
        ```
