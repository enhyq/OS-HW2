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
// should I put static to the variables?
int error_pipe[2];
int main_pipe[2];
struct itimerval t;
int timeover = 0;
int child_running = 0;
int interrupt_exit = 0;
pid_t child_pid;

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
void parent_proc(arg_data*, char*, char*);


int main(int argc, char *argv[]) {

    arg_data data = parser(argc, argv);

    // printf("---------------\n");
    // print(&data);
    // printf("---------------\n");

    // this will modify the given input
    minimize(&data);

    // need to free data (of arg_data size)?

    // TODO: need to save output file before exiting
    FILE *f = fopen(data.output,"w");
    fprintf(f, "%s", data.crashing_input);
    fclose(f);

    printf("Program done\n");
    return 0;
}


void minimize(arg_data *data) {
    // set interrupt signal handler before running algorithm
    signal(SIGINT, handler);
    signal(SIGALRM, handler);
    signal(SIGCHLD, handler); // masaka...

    // set timer value
    t.it_value.tv_sec = 3;
    t.it_value.tv_usec = 100000; // micro second -> (10^-6) second
    t.it_interval = t.it_value; // 3.1 sec

    /* you could just use alarm() function */
    
    // recursive function that will perform minimization
    reduce(data);
}

void reduce(arg_data *data) {
    // reduce(char* crashing_input)
    // char* minimized = data->crashing_input;
    // int minimized_size = data->crashing_input_size;

    // This function will directly update the crashing_input and its size variable
    int tm = data->crashing_input_size;
    char* t = data->crashing_input;
    int s = tm - 1;

    // Implementation of the algorithm
    while(s > 0) {
        // head + tail
        for( int i=0 ; i<=tm - s ; i++ ) {
            char head_tail[tm]; // begins with 1 character + null at the end
            
            // strncpy(dst, src, cnt);
            strncpy(head_tail, t, i);
            strcpy(head_tail+i, t+s+i);
            head_tail[i+(tm-s-i)] = 0x0;

            // printf(" %s\n", head_tail);

            /* COMMON CODE FOR MID AND HEAD_TAIL */
            if (pipe(error_pipe) != 0 || pipe(main_pipe) != 0) {
                perror("Error creating pipe");
                exit(1); // 1 to indicate process failed
            }
            
            /* child_pid IS GLOBAL VARIABLE */
            child_pid = fork();
            child_running = 1;
            if(child_pid == 0 /* it's CHILD !!!*/) {
                child_proc(data);
            } else /* it's PARENT !!! */ {
                char buf[4097] = {'\0'};

                printf("head_tail is: %s\n", head_tail);

                /* parent_proc WILL UPDATE buf */
                parent_proc(data, head_tail, buf); // update buf

                close(main_pipe[1]); // close write end
                close(error_pipe[0]); // close read end

                // printf("Error Read:\n");
                // printf("%s\n", buf);
                // sleep(5);

                /* CHECK INTERRUPT EXIT */
                if(interrupt_exit) {
                    return; // return out immediately
                }

                /* CHECK CRASH BY TIME OVER */
                if(timeover) {
                    timeover = 0;
                    printf("Crash detected by time over!\n");
                    strcpy(data->crashing_input, head_tail);
                    reduce(data);
                    return;
                }

                /* CHECK IF MESSAGE IS IN ERROR STRING */
                else if(strstr(buf, data->message) != NULL) { // strstr returns pointer
                    printf("Crash detected!\n");
                    // update crashing input
                    strcpy(data->crashing_input, head_tail);
                    // update crashing input size
                    data->crashing_input_size = strlen(head_tail);
                    // recursively call reduce()
                    reduce(data);
                    return;
                }
            }
            /* END OF COMMON CODE */

        }
        // mid
        for( int i=0 ; i<=tm - s - 1 ; i++ ) {
            char mid[tm]; // doesn't really have to fit the size (okay to have more)
            strncpy(mid, t+i, s);
            mid[i+s] = 0x0;
            
            // printf(" %s\n", mid);

            /* COMMON CODE FOR MID AND HEAD_TAIL */
            if (pipe(error_pipe) != 0 || pipe(main_pipe) != 0) {
                perror("Error creating pipe");
                exit(1); // 1 to indicate process failed
            }
            
            int child_pid = fork();
            child_running = 1;
            if(child_pid == 0 /* it's CHILD !!!*/) {
                child_proc(data);
            } else /* it's PARENT !!! */ {
                char buf[4097] = {'\0'};

                printf("mid is: %s\n", mid);

                /* parent_proc WILL UPDATE buf */
                parent_proc(data, mid, buf); // update buf

                close(main_pipe[1]); // close write end
                close(error_pipe[0]); // close read end

                // printf("Error Read:\n");
                // printf("%s\n", buf);
                // sleep(5);

                /* CHECK INTERRUPT EXIT */
                if(interrupt_exit) {
                    return; // return out immediately
                }

                /* CHECK CRASH BY TIME OVER */
                if(timeover) {
                    timeover = 0;
                    printf("Crash detected by time over!\n");
                    strcpy(data->crashing_input, mid);
                    reduce(data);
                    return;
                }

                /* CHECK IF MESSAGE IS IN ERROR STRING */
                else if(strstr(buf, data->message) != NULL) { // strstr returns pointer
                    printf("Crash detected!\n");
                    // update crashing input
                    strcpy(data->crashing_input, mid);
                    // update crashing input size
                    data->crashing_input_size = strlen(mid);
                    // recursively call reduce()
                    reduce(data);
                    return;
                }
            }
            /* END OF COMMON CODE */
        }
        s--;
    }
    // end of while means that there was nothing to reduce
    printf("Reduce finished\n"); // testing
    return; // yes
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

    // read input file
    int input_fd = open(data.input, O_RDONLY);
    int c_i_arr_size = sizeof(data.crashing_input)/sizeof(data.crashing_input[0]);
    data.crashing_input_size = read(input_fd, data.crashing_input, c_i_arr_size);
    data.crashing_input_size--; // since EOF is also read, remove this count
    data.crashing_input[c_i_arr_size-1] = '\0'; // mark the last char as NULL just in case

    // testing
    // printf("crashing input size: %d\n", data.crashing_input_size);

    return data;
}

void child_proc(arg_data *data) {
    // pipes are global variable
    close(error_pipe[0]); // close read end of error_pipe
    close(main_pipe[1]); // close write end of main_pipe


    dup2(error_pipe[1], 2 /*standard error*/);
    dup2(main_pipe[0], 0 /*standard input*/);
    close(1 /*standard output*/); // needed?

    // testing
    // fprintf(stderr, "hello this is tesing!\n");
    // fprintf(stdout, "wow this is tesing!\n");

    // sleep(5);

    int test = execv(data->program, data->argv);
    printf("something wrong from exec in child process.\n return code: %d\n", test);
}

/* 
 * parent will update "buf" variable directly
 * buf contains the stderr output of the target program
 */
void parent_proc(arg_data *data, char* ci, char* buf) {
    // pipes are global vairable
    // close unused end points
    close(error_pipe[1]); // parent error write not needed
    close(main_pipe[0]);

    /* START TIMER */
    timeover = 0;
    setitimer(ITIMER_REAL, &t, 0x0);

    char *send_data = ci;
    ssize_t s = strlen(ci);
    ssize_t sent = 0;

    // printf("s: %d\n", s);
    // printf("sent: %d\n", sent);

    while(sent < s) {
        // printf("sending: %s\n", send_data+sent);
        sent += write(main_pipe[1], send_data + sent, s - sent);
        // printf("sent: %d", sent);
        // repeat until all data is sent
        // sleep(2);
    }
    close(main_pipe[1]); // need to close pipe for the child read to finish
    
    // printf("finished sending..\n");

    // read() will return how many bytes was taken from the kernel (read)
    // read() will by default block if there is nothing to read when requested
    int count = 0;
    while(s = read(error_pipe[0], buf+count, 100)) {
        count += s;
        // TODO: concatenate all of the read strings ???
        // printf("(read: %ld)\n", s);  // testing
        // buf[s] = 0x0;           // put last character as NULL so it can be printed out as string
        // printf(" > %s\n", buf);     // testing
    }
    // printf("read: %ld\n", s);
    buf[count] = 0x0;

    // printf("parent is finised reading\n"); // testing
    // int return_status = wait(0x0); // wait until child is finished (terminated?) ???
    // Even when signal occurs during wait(), the block state is not freed (not quite sure)
    // printf("child returned with: %d\n", return_status);
    // child_running = 0; // child is not running any more

    // This while loop will suspend the process until child is finished
    // It will also terminate the child proecss if time is up
    while(child_running){
        if(timeover || interrupt_exit) {
            /* child_pid IS GLOBAL */
            kill(child_pid, SIGKILL); // or use SIGSTOP?
            child_running = 0;
            // IF TIME IS OVER, REGARDLESS OF THE stderr OUTPUT, CONSIDER IT AS CRASH
            // or check buf size?
            break;
        }
    };
    
    /* STOP TIMER ! */
    setitimer(ITIMER_REAL, 0x0, 0x0); // stop because child has finished
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
        /* STOP TIMER ! */
        // setitimer(ITIMER_REAL, 0x0, 0x0);
        interrupt_exit = 1;
        // printf("Do you want to quit?");
        // if (getchar() == 'y') {
        //     exit(0);
        // }

        // need to save current minimized crash input

    } else if (sig == SIGALRM) {
        // printf("RING!\n"); // testing
        timeover = 1;

        // if(child_running) {
        //     /* STOP TIMER ! */
        //     setitimer(ITIMER_REAL, 0x0, 0x0);
        //     kill(child_pid, SIGKILL); // or use SIGSTOP?
        //     child_running = 0;
        // } else {
        //     printf("something is wrong with the timer\n");
        // }

        // signal handler에서 return 하면 어디로 가는 걸까?
        // return;
        // exit 한다는 것은 어떤 의미?
        // exit(0); // exit current process
        // exec 때문에 signal handler가 없어졌으니 이해하지 못한게 당연하다 

        // TODO: need to kill child process
    } else if (sig == SIGCHLD) {
        // this is not working
        // I think this signal is modified(?) by the server to clean zombie processes
        // race condition between the arrival of SIGCHLD and the return from wait() ???
        // printf("child was terminated!\n");
        child_running = 0;
    } else {
        printf("what SIG is this ???: %d\n", sig);
    }
}

