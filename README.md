# cimin

cimin is a program that minimizes crashing input of a program by detecting crashing message or detecting infinite loop using timer.
At its core, it uses delta debugging algorithm to reduce the crashing input. 

## How to build
Make file is provided for the ease of building the program. Open the directory containing this folder and just type in `make` to build the program.

```bash
$ make
```

## Usage
```bash
$ cimin -i input -m "crash messag" -o output target_program [argument for target program...]
```
- `-i` : a file path of the crashing input </br>
- `-m` : a string whose appearance in standard error </br>
- `-o` : a new file path to store the reduced crashing input </br>
