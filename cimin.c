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


/* global variables */
int child_to_parent_pipe[2];
int parent_to_child_pipe[2];
struct itimerval t;

/* structure to store argument data */
typedef struct _arg_data {
    char *input;            // file containing crashing input
    char *message;          // message that is used to identify crash
    char *output;           // file to store the minimized crashing input
    char *program;          // program to run

    char *program_name;
    char **argv;
    int argc;

    // is this needed?
    // ~_arg_data() {
    //     free(argv);
    // }

} arg_data;


/* function declarations */
void usage_error(char*);
void usage_error_with_message(char*, char*);
void handler(int);
arg_data parser(int, char*[]);
char* minimize(char*);
char* reduce(char*);
void child_proc(arg_data*);
void parent_proc(char*, int);


int main(int argc, char *argv[]) {

    arg_data data = parser(argc, argv);

    char crashing_input[4097]; // +1 for EOF
    int input_fd = open(data.input, O_RDONLY);
    int crashing_input_size = read(input_fd, crashing_input, 4096);
    crashing_input[crashing_input_size] = '\0'; // need this?

    // this read function is weird 
    // when there is only 1 character, it reads 2
    // when there are 4096 characters, it reads 4096 ??
    printf("crashing input size: %d\n", crashing_input_size);
    
    // set interrupt signal handler before running algorithm
    signal(SIGINT, handler);
    signal(SIGALRM, handler);
    // when interrup signal is raised, need to terminate all child processes and stop algorithm
    
    // set timer value
    t.it_value.tv_sec = 3;
    t.it_value.tv_usec = 100000; // micro second -> (10^-6) second
    t.it_interval = t.it_value; // 3.1 sec

    if (pipe(child_to_parent_pipe) != 0 || pipe(parent_to_child_pipe) != 0) {
        perror("Error");
        exit(1); // 1 to indicate process failed
    }

    int child_pid = fork();
    if(child_pid == 0) {
        child_proc(&data);
    } else {
        parent_proc(crashing_input, crashing_input_size);
    }

    // is this needed?
    close(child_to_parent_pipe[0]);
    close(child_to_parent_pipe[1]);
    close(parent_to_child_pipe[0]);
    close(parent_to_child_pipe[1]);

    // need to free data?

    // TODO: need to save output file before exiting

    return 0;
}


/* function definitions */
arg_data parser(int argc, char* argv[]) {

    arg_data data;

    int opt_index = 1;
    int opt_requirement = 3;

    for(; opt_index < argc && argv[opt_index][0] == '-'; opt_index+=2) {
        switch (argv[opt_index][1]) {
        case 'i':
            data.input = argv[opt_index+1];
            // TODO: check if input is a valid file
            break;
        case 'm':
            data.message = argv[opt_index+1];
            break;
        case 'o':
            data.output = argv[opt_index+1];
            break;
        default:
            usage_error(argv[0]);
            break;
        }
        fprintf(stdout, "-%c: %s\n", argv[opt_index][1], argv[opt_index]); // testing
        opt_requirement--;
    }

    if(opt_requirement != 0) usage_error(argv[0]);

    if(argv[opt_index] == 0x0) usage_error(argv[0]);

    data.program = argv[opt_index++];

    data.argc = argc - opt_index;

    data.argv = (char**) malloc(sizeof(char*) * (2+data.argc));
    data.argv[0] = data.program; // TODO: get only the name of program
    data.argv[data.argc+1] = 0x0;
    
    for(int i=0; i<data.argc; i++) {
        data.argv[i+1] = argv[opt_index+i]; // TODO: need to check if this works
    }

    // testing if argv is good
    for(int i=0; i<data.argc+1; i++) {
        printf("argv[%d]: %s\n", i, data.argv[i]);
    }

    return data;
}

void child_proc(arg_data* data) {
    // pipes are global variable
        close(child_to_parent_pipe[0]); // close read end of child_to_parent_pipe
        close(parent_to_child_pipe[1]); // close write end of parent_to_child_pipe

        dup2(child_to_parent_pipe[1], 2 /*standard error*/);
        dup2(parent_to_child_pipe[0], 0 /*standard input*/);

        // testing
        fprintf(stderr, "hello this is tesing!\n");
        fprintf(stdout, "wow this is tesing!\n");

        int test = execv(data->program, data->argv);
        printf("something wrong from exec: %d\n", test);
}

void parent_proc(char* crashing_input, int crashing_input_size) {
    // pipes are global vairable
    close(child_to_parent_pipe[1]);
    close(parent_to_child_pipe[0]);

    // timer is stopped when interrupt signal is raised
    setitimer(ITIMER_REAL, &t, 0x0); // start timer

    ssize_t s;                      // write crashing input to pipe[1]
    s = crashing_input_size;

    ssize_t sent = 0;
    char * data = crashing_input;

    while(sent < s) {
        // in a fortunate case, all values are sent successfully
        // however sometimes, only part of the data is sent successfully
        // sent variable is for this case

        // return value is accumulated in [sent]
        sent += write(parent_to_child_pipe[1], data + sent, s - sent);

        // repeat until all data is sent
    }
    close(parent_to_child_pipe[1]); // is this needed for the child to finish reading?

    char buf[3001]; // will it be too slow?
    // continuously read the pipe
    // read only 100 bytes of data at a time
    // read() will return how many bytes was taken from the kernel (read)
    while(s = read(child_to_parent_pipe[0], buf, 3000)) {
        // TODO: concatenate all of the read strings
        printf("(read: %ld)\n", s);  // testing
        buf[s + 1] = 0x0;           // put last character as NULL so it can be printed out as string
        printf(" > %s\n", buf);     // testing
    }
    // if there is nothing to read, the above loop will break

    printf("parent is finised reading\n"); // testing
    wait(0x0); // wait until child is finished (terminated?)
}

void usage_error(char* program_name) {
    fprintf(stderr, "Usage: %s -i input -m message -o output program [args...]\n", program_name);
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
        // timer is stopped when interrupt is raised
        setitimer(ITIMER_REAL, 0x0, 0x0); // stop timer

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

