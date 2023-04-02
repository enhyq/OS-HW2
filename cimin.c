#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <error.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>

// declared as global variable
int pipes[2];
int new_pipes[2];

void usage_error(char* program_name) {
    fprintf(stderr, "Usage: %s -i [file path of crashing input] -m [crash message] -o [file path of output] [target program] [additional arguments]\n", program_name);
    exit(EXIT_FAILURE);
}
void usage_error_with_message(char* program_name, char* message) {
    fprintf(stderr, "%s\n", message);
    usage_error(program_name);
}

void handler(int sig) {
    // can register multiple signal code to handler
    printf("this is handler function!\n");
    
    // can redefine signal
    if (sig == SIGINT) {
        printf("Do you want to quit?");
        if (getchar() == 'y') {
            exit(0);
        }
    } else if (sig == SIGALRM) {
        // only child will have SIGALRM NO NO NO NO NO
        // when exec is called signal handler will not survive!!!

        // balance program..
        // print가 출력되지 않는다..?
        printf("RING!\n");
        perror("hey parent, child is taking too long!\n");
        fprintf(stderr, "taking too long...\n");
        fprintf(stdout, "w n w ?\n");

        // sleep(1);

        // signal handler에서 return 하면 어디로 가는 걸까?
        // return;
        // exit 한다는 것은 어떤 의미?
        // exit(0); // exit current process
        // exec 때문에 signal handler가 없어졌으니 이해하지 못한게 당연하다 

        // TODO: need to kill child process
    } else {
        printf("what is this SIG? \n");
    }
    printf("end of signal handler\n");
}


int main(int argc, char *argv[]) {
    // TODO: read arguments ( -i -m -o )
    char *input_file;
    char *crash_message;
    char *output_file;
    char *target_program_path;
    char **target_program_args;
    int tp_argc;

    // testing
    for(int i=0; i<argc; i++) {
        fprintf(stdout, "%d: %s\n", i, argv[i]);
    }

    // argument count must be at least 8
    if(argc < 8) usage_error(argv[0]);

    // TODO: need to provide proper error message if a given argument is invalid 
    // examples:
        // input file does not exist
        // no specific argument provided
        // has no access to certain file
    int opt_index = 1;
    for(; opt_index < argc && argv[opt_index][0] == '-'; opt_index++) {
        switch (argv[opt_index][1]) {
            case 'i':
                input_file = argv[++opt_index];
                fprintf(stdout, "-i: %s\n", argv[opt_index]); // testing 
                // check if input file exists
                break;
            case 'm':
                crash_message = argv[++opt_index];
                fprintf(stdout, "-m: %s\n", argv[opt_index]); // testing 
                break;
            case 'o':
                output_file = argv[++opt_index];
                fprintf(stdout, "-o: %s\n", argv[opt_index]); // testing 
                break;
            default:
                usage_error(argv[0]);
                break;
        }
        // argv is pointer to char pointer
        // argv += 2;
    }

    // store program file to run
    target_program_path = argv[opt_index++];
    
    // first argument should be executing file name
    argv += opt_index;
    printf("testing current argv: %s\n", argv[0]);

    tp_argc = argc - opt_index + 2;
    char* some_args[tp_argc];
    some_args[0] = strdup(target_program_path); // why need to duplicate?

    // put remaining argv into some_args
    for(int i=0; i<tp_argc-2; i++) {
        some_args[1+i] = argv[i];
    }

    some_args[tp_argc-1] = NULL; // why need to duplicate?
    
    target_program_args = some_args;


    // testing
    printf("input file: %s\n", input_file);
    printf("crash message: %s\n", crash_message);
    printf("output file: %s\n", output_file);
    printf("target program: %s\n", target_program_path);
    printf("tp_argc: %d\n", tp_argc);
    for(int i=0; i<tp_argc-2; i++) {
        printf("%d: %s\n", i, argv[i]);
    }

    // works nice
    // some bugs if 3 options are not input well

    // crashing input should be read and stored as character array
    char crashing_input[4097]; // +1 for EOF ???
    int input_fd = open(input_file, O_RDONLY);
    
    int crashing_input_size;
    crashing_input_size = read(input_fd, crashing_input, 4096);
    crashing_input[crashing_input_size] = '\0';
    printf("last char is: %c\n", crashing_input[crashing_input_size-1]);
    crashing_input[4098] = '\0';
    // this read function is weird 
    // when there is only 1 character, it reads 2
    // when there are 4096 characters, it reads 4096 
    printf("crashing input size: %d\n", crashing_input_size);
    
    // set interrupt signal handler before running algorithm
    signal(SIGINT, handler);
    // signal(SIGALRM, handler);
    // when interrup signal is raised, need to terminate all child processes and stop algorithm

    // TODO: implement delta debugging algorithm
    // TODO: need to set timer signal
    // TODO: need to setup standard input of executed program to receive crash input
    // TODO: need to open up pipe for communication


    if (pipe(pipes) != 0) {
        perror("Error");
        exit(1); // 1 to indicate process failed
    }
    if (pipe(new_pipes) != 0) {
        perror("Error");
        exit(1); // 1 to indicate process failed
    }
    // testing
    printf("%d %d\n", pipes[0], pipes[1]);

    int child_pid = fork();
    if(child_pid == 0) {
        // this is child process
        printf("This is child process!\n");

        close(pipes[0]); // close read end of pipes
        // set stderr(2) as pipe[1] which is write pipe
        dup2(pipes[1], 2 /*standard error*/);
        // so that error output can be passed on to the parent process
        // now anything that prints to stderr will go through pipes[1]


        close(new_pipes[1]); // close write end of pipes
        // connect new_pipe[0] to stdin?
        int read_dup_test = dup2(new_pipes[0], 0 /*standard input*/);
        printf("child test -> %d\n", read_dup_test);
        // read crashing input from pipe[0]
        // now anything that comes through pipe[0] will be input like stdin

        // In child process now stdin file is closed, so any interrupt signal
        /// sent from console using ctrl+C will be from parent process 
        /// ???????

    
        fprintf(stderr, "hello this is tesing!\n");
        fprintf(stdout, "wow this is tesing!\n");

        int test = execv(target_program_path, target_program_args);
        printf("something wrong from exec: %d\n", test);
    } else {
        // this is parent process
        printf("This is parent process!\n");

        struct itimerval t;
        signal(SIGALRM, handler);
        t.it_value.tv_sec = 3;
        t.it_value.tv_usec = 100000; // micro second -> (10^-6) second
        // combination make 1.1 second -> put into t.it_interval
        t.it_interval = t.it_value;

        // set interval timer
        // setitimer API
        setitimer(ITIMER_REAL, &t, 0x0); // start timer

        close(pipes[1]);
        close(new_pipes[0]);
        
        ssize_t s;
        // write crashing input to pipe[1]
        ssize_t sent = 0;
        char * data = crashing_input;
        

        s = crashing_input_size;
        while(sent < s) {
            // in a fortunate case, all values are sent successfully
            // however sometimes, only part of the data is sent successfully
            // sent variable is for this case

            // return value is accumulated in [sent]
            sent += write(new_pipes[1], data + sent, s - sent);

            // repeat until all data is sent
        }
        close(new_pipes[1]);

        
        

        // here need to wait until child process reads
        
        
        // probably should malloc or continuously search for keyword in every buffer
        // continuous searching should consider current and previous

        char buf[101];
        // continuously read the pipe
        // read only 100 bytes of data at a time

        // read() will return how many bytes was taken from the kernel (read)
        while(1) {
            s = read(pipes[0], buf, 100) ;
            buf[s + 1] = 0x0; // put last character as NULL so it can be printed out as string
            printf(" > %s\n", buf);
            sleep(1);
        }
        // if there is nothing to read, the above loop will break

        printf("parent is finised reading\n"); // testing
        wait(0x0); // wait until child is finished (terminated?)
    }

    // need to save output file before exiting

    close(pipes[0]);
    close(pipes[1]);

    return 0;
}



// libxml2
// autoconf -i
// ./configure
// make
// make install

// if 'autoconf'ed first
// then to fix -> autoreconf -fi