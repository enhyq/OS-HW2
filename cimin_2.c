#include <error.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#define DEBUG

#ifdef DEBUG
#define DPRINT printf
#else
#define DPRINT // macros
#endif

/* SIZE OF CRASHING INPUT (CI) AND BUF */
#define MAX_CI_SIZE 4097
#define MAX_BUF_SIZE 4097   // used to define size of array that stores stderr from target program

/* GLOBAL VARIABLES */
int error_pipe[2];          // should I make the variables static?
int main_pipe[2];
struct itimerval t;
pid_t child_pid;

/* flags to keep track of state */
int timeover = 0;
int child_running = 0;
int interrupt_exit = 0;

/* for counting total number of crashes */
int crash_count = 0;


/* THINGS TO DO */
// 1. return proper error message if given argument is invalid


/* SOME IDEAS TO IMPROVE THE ALGORITHM */
// use binary search?
// for program like libpng,
// crash happens for almost all input until it gets significantly small
// using binary search like step, the time can be reduced


/**
 * Initially creeated structure to store parsed argument data 
 * It now also have "test_inputs" to store temporary head+tail or mid string
 *  and also "buf", which is to store stderr output of the target program, if any
 */
typedef struct _arg_data
{
    char *input;   // file containing crashing input
    char *message; // message that is used to identify crash
    char *output;  // file to store the minimized crashing input
    char *program; // program to run

    char *program_name;
    char **argv;
    int argc;

    // maybe I could just take out below variables to global scope
    char crashing_input[MAX_CI_SIZE];
    int crashing_input_size;

    /* BELOW VARABLES ARE NOT INITIALIZED IN PARSER FUNCTION */

    // both head_tail and mid will be stored in this char array
    char test_input[MAX_CI_SIZE];
    int test_input_size;

    // target program stderr output
    char buf[MAX_BUF_SIZE];
} arg_data;


/* FUNCTION DECLARATIONS */
arg_data parser(int, char *[]);

void minimize(arg_data *);
void reduce(arg_data *);
int head_tail_mid_common(arg_data *);
void child_proc(arg_data *);
void parent_proc(arg_data *);

void print_data(arg_data *);
void strncpy_my(char *, char *, int);
void print_as_hex(char *, int);

void usage_error(char *);
void usage_error_with_message(char *, char *);
void handler(int);



int main(int argc, char *argv[])
{

    arg_data data = parser(argc, argv);

    printf("---------------\n");
    print_data(&data);
    printf("---------------\n");

    // data is modified!
    minimize(&data);

    /* need to fix this! */
    FILE *f = fopen(data.output, "w");
    data.crashing_input[data.crashing_input_size] = 0x0;
    fprintf(f, "%s", data.crashing_input);
    fclose(f);

    printf("Crash count: %d\n", crash_count); // testing
    printf("Program done\n");
    return 0;
}



/* FUNCTION DEFINITIONS */
arg_data parser(int argc, char *argv[])
{

    arg_data data;

    int opt_index = 1;
    int opt_requirement = 3;

    for (; opt_index < argc && argv[opt_index][0] == '-'; opt_index += 2)
    {
        switch (argv[opt_index][1])
        {
        case 'i':
            data.input = argv[opt_index + 1];
            // TODO: check if input is a valid file?
            break;
        case 'm':
            data.message = argv[opt_index + 1];
            break;
        case 'o':
            data.output = argv[opt_index + 1];
            break;
        default:
            usage_error(argv[0]);
            break;
        }
        // DPRINT("-%c: %s\n", argv[opt_index][1], argv[opt_index+1]); // testing
        opt_requirement--;
    }

    if (opt_requirement != 0)
        usage_error(argv[0]);

    if (argv[opt_index] == 0x0)
        usage_error(argv[0]);

    data.program = argv[opt_index++];
    // need program_name ??
    data.program_name = data.program; // TODO: get only the name of program

    data.argc = argc - opt_index;

    data.argv = (char **)malloc(sizeof(char *) * (2 + data.argc));
    data.argv[0] = data.program; // TODO: get only the name of program
    data.argv[data.argc + 1] = 0x0;

    for (int i = 0; i < data.argc; i++)
    {
        data.argv[i + 1] = argv[opt_index + i]; // TODO: need to check if this works
    }

    // testing if argv is good
    // for(int i=0; i<data.argc+1; i++) {
    //     DPRINT("argv[%d]: %s\n", i, data.argv[i]);
    // }

    // read input file
    int input_fd = open(data.input, O_RDONLY);
    if (input_fd < 0)
    {
        usage_error_with_message(data.program_name, "No input file found!");
    }
    int c_i_arr_size = sizeof(data.crashing_input) / sizeof(data.crashing_input[0]); // 4097
    // printf("c_i_arr_size: %d\n", c_i_arr_size);

    data.crashing_input_size = read(input_fd, data.crashing_input, c_i_arr_size - 1);

    /* TEST PRINT IN HEX DATA FORMAT */
    print_as_hex(data.crashing_input, data.crashing_input_size);

    // data.crashing_input_size--; // since EOF is also read, remove this count
    // data.crashing_input[c_i_arr_size-1] = '\0'; // mark the last char as NULL just in case

    // testing
    // printf("crashing input size: %d\n", data.crashing_input_size);

    return data;
}


void minimize(arg_data *data)
{
    // set interrupt signal handler before running algorithm
    signal(SIGINT, handler);
    signal(SIGALRM, handler);
    signal(SIGCHLD, handler); // masaka...

    // set timer value
    t.it_value.tv_sec = 3;
    // t.it_value.tv_usec = 100000; // micro second -> (10^-6) second
    t.it_interval = t.it_value; // 3.1 sec

    /* you could just use alarm() function */

    // recursive function that will perform minimization
    reduce(data);
}

void reduce(arg_data *data)
{
    // This function will directly update the crashing_input and its size variable
    int tm = data->crashing_input_size;
    // input size is size of the input
    char *t = data->crashing_input;
    int s = tm - 1;

    // Implementation of the algorithm
    while (s > 0)
    {
        /* HEAD TAIL */
        for (int i = 0; i <= tm - s; i++)
        {
            data->test_input_size = tm - s; // size of head_tail will be tm-s
            strncpy_my(data->test_input, t, i);
            strncpy_my(data->test_input + i, t + s + i, data->test_input_size - i); // -i? // seems like it doesn't matter

            printf("HEAD_TAIL(%d): ", data->test_input_size);

            int return_code = head_tail_mid_common(data);
            if (return_code == 1 /* CRASH */)
            {
                reduce(data);
                return;
            }
            else if (return_code == 2 /* INTERRUPT */)
            {
                return;
            } /* else continue */
        }

        /* MID */
        for (int i = 0; i <= tm - s - 1; i++)
        {
            data->test_input_size = s;
            strncpy_my(data->test_input, t + i, data->test_input_size);

            printf("MID(%d): ", data->test_input_size);

            int return_code = head_tail_mid_common(data);
            if (return_code == 1 /* CRASH */)
            {
                reduce(data);
                return;
            }
            else if (return_code == 2 /* INTERRUPT */)
            {
                return;
            } /* else continue */
        }
        s--;
    }
    // end of while means that there was nothing to reduce
    DPRINT("Reduce finished\n"); // testing
    return;                      // yes
}

/**
 * @brief creates child process and run target program
 * write input to child and read stderr from child
 * returns when child process is terminated
 * 
 * @param data arg_data type which holds all data necessary for running the function
 * @return int 0 = no crash, 1 = crash, 2 = interrupt
 */
int head_tail_mid_common(arg_data *data)
{
    /* COMMON CODE FOR MID AND HEAD_TAIL */
    if (pipe(error_pipe) != 0 || pipe(main_pipe) != 0)
    {
        perror("Error creating pipe");
        exit(1); // 1 to indicate process failed
    }

    /* child_pid IS GLOBAL VARIABLE */
    child_pid = fork();
    child_running = 1;
    if (child_pid == 0 /* it's CHILD !!!*/)
    {
        child_proc(data);
    }
    else /* it's PARENT !!! */
    {
        // char buf[MAX_CI_SIZE] = {'\0'};
        data->buf[0] = 0x0; // initialize the first bit of buf to null (to mark end of string)
        // because if the child process was terminated by timer,
        // then strstr would use the previously stored value

        // DPRINT("head_tail is: %s\n", head_tail);
        print_as_hex(data->test_input, data->test_input_size); // test

        /* parent_proc WILL UPDATE buf */
        parent_proc(data); // update buf

        /* CHECK INTERRUPT EXIT */
        if (interrupt_exit)
            return 2;

        /* CHECK CRASH BY TIME OVER */
        /* CHECK IF MESSAGE IS IN ERROR STRING */
        if (strstr(data->buf, data->message) != NULL || // strstr returns pointer
            timeover == 1)
        {
            crash_count++; // testing
            // printf(">> Increment crash_count: is now: %d\n", crash_count);
            if (timeover)
                DPRINT("Crash detected by time over!\n");
            else
                DPRINT("Crash detected!\n");
            // update crashing input
            strncpy_my(data->crashing_input, data->test_input, data->test_input_size);
            // update crashing input size
            data->crashing_input_size = data->test_input_size;
            // recursively call reduce()

            return 1;

            // calling recursive call inside another function make things complicated
            // reduce(data);
            // return;
        }
        return 0;
    }
    /* END OF COMMON CODE */
    return 0;
}

void parent_proc(arg_data *data)
{
    // pipes are global vairable
    // close unused end points
    close(error_pipe[1]);
    close(main_pipe[0]);

    /* START TIMER */
    timeover = 0;
    setitimer(ITIMER_REAL, &t, 0x0);

    ssize_t s = data->test_input_size; // same
    ssize_t count = 0;
    while (count < data->test_input_size)
    {
        count += write(main_pipe[1], data->test_input + count, s - count);
    }
    close(main_pipe[1]); // need to close pipe for the child read to finish

    s = 0;
    count = 0;
    while (s = read(error_pipe[0], data->buf + count, 100))
    {
        count += s;
        // printf(" > %s\n", buf);     // testing
    }
    data->buf[count] = 0x0; // needed
    close(error_pipe[0]);   // close read end

    int return_status = wait(0x0); // wait until child is finished
    child_running = 0;             // child is not running any more

    /* STOP TIMER ! */
    setitimer(ITIMER_REAL, 0x0, 0x0); // stop because child has finished
}

void child_proc(arg_data *data)
{
    // pipes are global variable
    close(error_pipe[0]); // close read end of error_pipe
    close(main_pipe[1]);  // close write end of main_pipe

    dup2(error_pipe[1], 2 /*standard error*/);
    dup2(main_pipe[0], 0 /*standard input*/);

    // dup2(error_pipe[1], 1 /*standard output*/); // testing
    close(1 /*standard output*/); // needed?

    int test = execv(data->program, data->argv);
    DPRINT("something wrong from exec in child process.\n return code: %d\n", test);
}


void print_data(arg_data *d)
{
    printf("input: %s\n", d->input);
    printf("message: %s\n", d->message);
    printf("output: %s\n", d->output);
    printf("program: %s\n", d->program);

    printf("program_name: %s\n", d->program_name);
    for (int i = 0; i < d->argc + 1; i++)
    {
        printf("argv[%d]: %s\n", i, d->argv[i]);
    }
    printf("argc: %d\n", d->argc);
    printf("crashing_input: %s\n", d->crashing_input);
    printf("crashing_input as hex:\n");
    print_as_hex(d->crashing_input, d->crashing_input_size);
    printf("crashing_input_size: %d\n", d->crashing_input_size);
}

void strncpy_my(char *dst, char *src, int size)
{
    // printf("strncpy_my will cpy size: %d\n", size);
    for (int i = 0; i < size; i++)
    {
        dst[i] = src[i]; // libpng gets segmentation fault at mid size = 1010
    }
    // printf("strncpy_my succesful! (size: %d)\n", size);
}

void print_as_hex(char *hex_input, int len)
{
    /* TEST PRINT IN HEX DATA FORMAT */
    for (int i = 0; i < len; i++)
    {
        printf("%x ", (unsigned char)hex_input[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n");
}


void usage_error(char *program_name)
{
    fprintf(stderr, "Usage: %s -i input -m message -o output program [args...]\n", program_name);
    exit(EXIT_FAILURE);
}

void usage_error_with_message(char *program_name, char *message)
{
    fprintf(stderr, "%s\n", message);
    usage_error(program_name);
}

void handler(int sig)
{
    if (sig == SIGINT)
    {
        printf(" !!! *** --- +++ ^^^ INTERRUPT ^^^ +++ --- *** !!! \n");
        interrupt_exit = 1;
        if (child_running)
            kill(child_pid, SIGKILL); // or use SIGSTOP?
    }
    else if (sig == SIGALRM)
    {
        timeover = 1;
        DPRINT("RING! kill child_pid: %d\n", child_pid);

        if (child_running)
            kill(child_pid, SIGKILL); // or use SIGSTOP?
        else
            DPRINT("timer should not be running at this moment!\n");
    }
    else if (sig == SIGCHLD)
    {
        // this is for signal
        child_running = 0;
    }
    else
    {
        DPRINT("what SIG is this ???: %d\n", sig);
    }
}