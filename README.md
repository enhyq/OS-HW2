# cimin

cimin is a program that minimizes crashing input of a program by detecting crashing message or detecting infinite loop using timer.
At its core, it uses delta debugging algorithm to reduce the crashing input. 

## How to build
Make file is provided for the ease of building the program. Open the directory containing this folder and just type in `make` to build the program.

```bash
$ make
```

## Usage
`cimin` receives command-line arguments as following example:
```bash
$ cimin -i input -m "crash messag" -o output target_program [argument for target program...]
```
- `-i` : a file path of the crashing input </br>
- `-m` : a string whose appearance in standard error </br>
- `-o` : a new file path to store the reduced crashing input </br>

## Testcases
### jsmn
```bash
$ ls
cimin    jsmn         
$ ./cimin -i jsmn/testcases/crash.json -m "AddressSanitizer: heap-buffer-overflow" -o reduced_jsmn jsmn/jsondump
…
$ ls
cimin    jsmn     reduced_jsmn
$ cat reduced_jsmn
{y}
$ jsmn/jsondump < reduced_jsmn
==7459==ERROR:AddressSanitizer:heap-buffer-overflow on …
...
```
### libxml2
```bash
$ ls
cimin     libxml2
$ ./cimin -i libxml2/testcases/crash.xml -m "SEGV on unknown address" -o reduced_libxml2 libxml2/xmllint --recover --postvalid -
...
$ ls
cimin     libxml2     reduced_libxml2
$ cat reduced_libxml2
<!DOCTYPE[<!ELEMENT
:(�,()><:
$ libxml2/xmllint --recover --postvalid - < reduced_libxml2
...
==37570==ERROR: AddressSanitizer: SEGV on unknown address ...
...
```
### balance
```bash
$ ls
cimin     balance         
$ ./cimin -i balance/testcases/fail -m "balance" -o reduced_balance balance/balance
...
$ ls
cimin     libxml2     reduced_balance
$ cat reduced_balance
][]*[][
$ balance/balance < reduced_balance
(infinite loop…)
>(ctrl+c)
```
### libpng
```bash
$ ls
cimin     libpng         
$ ./cimin -i libpng/crash.png -m "MemorySanitizer: use-of-uninitialized-value" -o reduced_libpng libpng/libpng/test_pngfix
… (very long long time spent…)
$ ls
cimin     libxml2     reduced_libpng
$ xxd reduced_libpng
00000000: 8950 4e47 0d0a 1a0a 0000 000d 4948 4452 .PNG........IHDR
...
000000c0: 5d00 0000 0970 4859 73 ]....pHYs
$ libpng/libpng/test_pngfix < reduced_libpng
...
==8557==WARNING: MemorySanitizer: use-of-uninitialized-value
...
```
## Contact
If you have any questions about `cimin` then contact us.<br>
21900050 Kwon Eunhyeok eunhyeoq@handong.ac.kr<br>
21900215 Kim Hyeongi 21900215@handong.ac.kr<br>
