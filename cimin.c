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
int error_pipe[2];
int main_pipe[2];
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

    char crashing_input[4097];
    int crashing_input_size;
} arg_data;


void print(arg_data *d){
    printf("input: %s\n", d->input);
    printf("message: %s\n", d->message);
    printf("output: %s\n", d->output);
    printf("program: %s\n", d->program);

    printf("program_name: %s\n", d->program_name);
    for(int i=0; i<d->argc+1; i++) {
        printf("argv[%d]: %s\n", i, d->argv[i]);
    }
    printf("argc: %d\n", d->argc);
    printf("crashing_input: %s\n", d->crashing_input);
    printf("crashing_input_size: %d\n", d->crashing_input_size);
}



/* function declarations */
void usage_error(char*);
void usage_error_with_message(char*, char*);
void handler(int);
arg_data parser(int, char*[]);
void minimize(arg_data*);
void reduce(arg_data*);
void child_proc(arg_data*);
void parent_proc(arg_data*, char*);


int main(int argc, char *argv[]) {

    arg_data data = parser(argc, argv);

    int input_fd = open(data.input, O_RDONLY);
    int c_i_arr_size = sizeof(data.crashing_input)/sizeof(data.crashing_input[0]);
    data.crashing_input_size = read(input_fd, data.crashing_input, c_i_arr_size);
    data.crashing_input_size--; // since EOF is also read, remove this count
    data.crashing_input[c_i_arr_size-1] = '\0'; // mark the last char as NULL just in case

    // testing
    printf("crashing input size: %d\n", data.crashing_input_size);

    printf("---------------\n");
    print(&data);
    printf("---------------\n");

    // this will modify the given input
    minimize(&data);

    // need to free data (of arg_data size)?

    // TODO: need to save output file before exiting

    return 0;
}


void minimize(arg_data *data) {
    // set interrupt signal handler before running algorithm
    signal(SIGINT, handler);
    signal(SIGALRM, handler);

    // set timer value
    t.it_value.tv_sec = 3;
    t.it_value.tv_usec = 100000; // micro second -> (10^-6) second
    t.it_interval = t.it_value; // 3.1 sec

    /* you could just use alarm() function */
    // pipe variable is global
    if (pipe(error_pipe) != 0 || pipe(main_pipe) != 0) {
        perror("Error");
        exit(1); // 1 to indicate process failed
    }
    
    // recursive function that will perform minimization
    reduce(data);

    // is this needed?
    close(error_pipe[0]); // close read end
    close(main_pipe[1]); // close write end
}

void reduce(arg_data *data) {
    // reduce(char* crashing_input)
    // char* minimized = data->crashing_input;
    // int minimized_size = data->crashing_input_size;

    // This function will directly update the crashing_input and its size variable
    int tm = data->crashing_input_size;
    char* t = data->crashing_input;
    int s = tm - 1;

    // pipes are global vairable
    // close unused end points
    close(error_pipe[1]);
    close(main_pipe[0]);

    // Implementation of the algorithm
    while(s > 0) {
        printf("\n");
        // printf("s: %d\n", s);
        // head + tail
        printf("Testing head_tail: \n");
        for( int i=0 ; i<=tm - s ; i++ ) {
            char head_tail[tm]; // begins with 1 character + null at the end
            
            // strncpy(dst, src, cnt);
            strncpy(head_tail, t, i);
            strcpy(head_tail+i, t+s+i);
            head_tail[i+(tm-s-i)] = 0x0;

            printf(" %s\n", head_tail);



        }
        // mid
        printf("Testing mid: \n");
        for( int i=0 ; i<=tm - s - 1 ; i++ ) {
            char mid[tm]; // doesn't really have to fit the size (okay to have more)
            strncpy(mid, t+i, s);
            mid[i+s] = 0x0;
            
            printf(" %s\n", mid);
            
            int child_pid = fork();
            if(child_pid == 0) {
                child_proc(data);
            } else {
                char buf[4097];
                parent_proc(data, buf); // update buf
                // strstr returns pointer to of the index found
                if(strstr(buf, data->message) != NULL) {
                    printf("Crash detected!\n");
                    // update crashing input
                    // update crashing input size
                    // recursively call reduce()
                    
                }
            }

        }
        s--;
    }
    // end of while means that there was nothing to reduce
    printf("Nothing to reduce!\n"); // testing
    return;

    
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
        fprintf(stdout, "-%c: %s\n", argv[opt_index][1], argv[opt_index+1]); // testing
        opt_requirement--;
    }

    if(opt_requirement != 0) usage_error(argv[0]);

    if(argv[opt_index] == 0x0) usage_error(argv[0]);

    data.program = argv[opt_index++];
    // need program_name ??
    data.program_name = data.program; // TODO: get only the name of program

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

void child_proc(arg_data *data) {
    // pipes are global variable
        close(error_pipe[0]); // close read end of error_pipe
        close(main_pipe[1]); // close write end of main_pipe

        dup2(error_pipe[1], 2 /*standard error*/);
        dup2(main_pipe[0], 0 /*standard input*/);

        // testing
        // fprintf(stderr, "hello this is tesing!\n");
        // fprintf(stdout, "wow this is tesing!\n");

        int test = execv(data->program, data->argv);
        printf("something wrong from exec in child process.\n return code: %d\n", test);
}

/* 
 * parent will update "buf" variable directly
 * buf contains the stderr output of the target program
 */
void parent_proc(arg_data *data, char* buf) {

    // timer is stopped when interrupt signal is raised
    setitimer(ITIMER_REAL, &t, 0x0); // start timer

    ssize_t s;                      // write crashing input to pipe[1]
    s = data->crashing_input_size;

    ssize_t sent = 0;
    char *send_data = data->crashing_input;

    printf("send data: %s\n", send_data);
    printf("s: %d\n", s);

    char test_in[10] = "hello";
    send_data = test_in;

    while(sent < s) {
        printf("sending \n");
        sent += write(main_pipe[1], send_data + sent, s - sent);
        // repeat until all data is sent
    }
    // char nu = 0x0;
    // write(main_pipe[1], &nu, 1);
    // close(main_pipe[1]); // need to close pipe for the child read to finish

    // read() will return how many bytes was taken from the kernel (read)
    size_t buf_size = sizeof(buf)/sizeof(buf[0]);
    // read could be interrupted.. how to handle this kind of case?

    // TODO: should check if read can happen more than once
    // What if no output is given from child?
    while(s = read(error_pipe[0], buf, buf_size-1)) {
        // TODO: concatenate all of the read strings ???
        printf("(read: %ld)\n", s);  // testing
        buf[s] = 0x0;           // put last character as NULL so it can be printed out as string
        printf(" > %s\n", buf);     // testing
    }
    // buf[s] = 0x0;

    printf("parent is finised reading\n"); // testing
    wait(0x0); // wait until child is finished (terminated?) ???
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
    if (sig == SIGINT) {
        // timer is stopped when interrupt is raised
        setitimer(ITIMER_REAL, 0x0, 0x0); // STOP TIMER ! (this works!)

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

        // sleep(1);

        // signal handler에서 return 하면 어디로 가는 걸까?
        // return;
        // exit 한다는 것은 어떤 의미?
        // exit(0); // exit current process
        // exec 때문에 signal handler가 없어졌으니 이해하지 못한게 당연하다 

        // TODO: need to kill child process
    } else {
        printf("what SIG is this ???\n");
    }
}

