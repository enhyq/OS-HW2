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

#define DEBUG
  
#ifdef DEBUG
#define DPRINT printf
#else
#define DPRINT // macros
#endif


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

    // data is modified!
    minimize(&data);

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

                DPRINT("head_tail is: %s\n", head_tail);

                /* parent_proc WILL UPDATE buf */
                parent_proc(data, head_tail, buf); // update buf

                close(main_pipe[1]); // close write end
                close(error_pipe[0]); // close read end

                /* CHECK INTERRUPT EXIT */
                if(interrupt_exit) return;

                /* CHECK CRASH BY TIME OVER */
                /* CHECK IF MESSAGE IS IN ERROR STRING */
                if(strstr(buf, data->message) != NULL || // strstr returns pointer
                    timeover == 1
                ) { 
                    if(timeover) DPRINT("Crash detected by time over!\n");
                    else DPRINT("Crash detected!\n");
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

                DPRINT("mid is: %s\n", mid);

                /* parent_proc WILL UPDATE buf */
                parent_proc(data, mid, buf); // update buf

                close(main_pipe[1]); // close write end
                close(error_pipe[0]); // close read end

                /* CHECK INTERRUPT EXIT */
                if(interrupt_exit) return;

                /* CHECK CRASH BY TIME OVER */
                /* CHECK IF MESSAGE IS IN ERROR STRING */
                if(strstr(buf, data->message) != NULL || // strstr returns pointer
                    timeover == 1
                ) { 
                    if(timeover) DPRINT("Crash detected by time over!\n");
                    else DPRINT("Crash detected!\n");
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
    DPRINT("Reduce finished\n"); // testing
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
            // TODO: check if input is a valid file?
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
        // DPRINT("-%c: %s\n", argv[opt_index][1], argv[opt_index+1]); // testing
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
    // for(int i=0; i<data.argc+1; i++) {
    //     DPRINT("argv[%d]: %s\n", i, data.argv[i]);
    // }

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
    
    // dup2(error_pipe[1], 1 /*standard output*/); // testing
    close(1 /*standard output*/); // needed?

    int test = execv(data->program, data->argv);
    DPRINT("something wrong from exec in child process.\n return code: %d\n", test);
}

/* 
 * parent will update "buf" variable directly
 * buf contains the stderr output of the target program
 */
void parent_proc(arg_data *data, char* ci, char* buf) {
    // pipes are global vairable
    // close unused end points
    close(error_pipe[1]);
    close(main_pipe[0]);

    /* START TIMER */
    timeover = 0;
    setitimer(ITIMER_REAL, &t, 0x0);

    char *send_data = ci;
    ssize_t s = strlen(ci);
    ssize_t sent = 0;


    while(sent < s) {
        sent += write(main_pipe[1], send_data + sent, s - sent);
    }
    close(main_pipe[1]); // need to close pipe for the child read to finish


    int count = 0;
    while(s = read(error_pipe[0], buf+count, 100)) {
        count += s;
        // printf(" > %s\n", buf);     // testing
    }
    buf[count] = 0x0;

    int return_status = wait(0x0); // wait until child is finished
    child_running = 0; // child is not running any more

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
    if (sig == SIGINT)
    {
        interrupt_exit = 1;
        if(child_running) kill(child_pid, SIGKILL); // or use SIGSTOP?
    } 
    else if (sig == SIGALRM)
    {
        timeover = 1;
        DPRINT("RING! kill child_pid: %d\n", child_pid);

        if(child_running) kill(child_pid, SIGKILL); // or use SIGSTOP?
        else DPRINT("timer should not be running at this moment!\n");
    } 
    else if (sig == SIGCHLD)
    {
        child_running = 0;
    } 
    else
    {
        DPRINT("what SIG is this ???: %d\n", sig);
    }
}

