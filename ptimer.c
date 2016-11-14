// Copyright Martin Cracauer 1997-2016
// Open Sorce under MIT license.
// Google IARC approved September 20 2016.

// A C program that you wrap around a binary.
//
// Pretty much like time(1), however:
// - reports msec without shell floating point arithmetic
// - correctly captures, returns and documents the exit
//   status of the program to be called including signal
//   exit status
// - writes all of that into an easily grep'able ASCII
//   file, also more suitable for awk (argument 1)
// - giving it a file to write the stats to is a mandatory
//   first argument
//
// Usage:
// ./thisprogram -o statfile_to_write wc /etc/hosts
// [normal output, full transparency like time(1)]
// # statfile_to_write has:
// 0.00155711 full seconds real time
// 1557 microseconds real time
// 0 command terminated on its own (exitcode)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

void usage(FILE *const f, int exitcode) {
    fprintf(f, "Usage: ");
    fprintf(f, "exiter -o reportfile unixcommand [args_for_unixcommand]\n");
    fprintf(f, "(mostly like GNU time, but report file is mandatory for now)\n");
    exit(exitcode);
}

int childpid;

#define UNSET -1

// We need to take care of exiting this surrounding program with the correct
// exit status.
int exit_myself_with_signal = UNSET;
void onsig(int signum) {
    // Send on to child.  This is necessary e.g. in shellscripts that get
    // a pid by `foobr & pidtosignal=$!
    kill(childpid, signum);
    exit_myself_with_signal = signum;
}

int main(int argc, char *argv[]) {
    const char *output_filename;
    const char *executable;

    if (argc == 1) {
        usage(stdout, 0);
    }
    if (argc < 4) {
        usage(stderr, 1);
    }
    if (strcmp(argv[1], "-o")) {
        usage(stderr, 1);
    }
    argv++; argc--;

    // Signals that might be sent to the progress
    // group, e.g. by an interactive shell.
    int signals_to_catch[] = {
        SIGINT,
        SIGTERM,
        SIGQUIT,
        SIGHUP,
        /* SIGINFO, Not on Linux */
        -1
    };
    int *sig;
    for (sig = signals_to_catch; *sig != -1; sig++)
        signal(*sig, onsig);

    output_filename = argv[1]; argv++; argc--;
    executable = argv[1]; argv++; argc--;

    FILE *out = fopen(output_filename, "w");
    if (out == NULL) {
        perror("fopen w");
        exit(2);
    }

    struct rusage before;
    if (getrusage(RUSAGE_CHILDREN, &before) == -1) {
        perror("getrusage1");
        exit(2);
    }
    struct timeval time1;
    if (gettimeofday(&time1, NULL) == -1) {
        perror("gettimeofday");
        exit(2);
    }
    double time_before = time1.tv_sec +
        (double)(time1.tv_usec) / 1000.0 / 1000.0;
    childpid = fork();
    switch (childpid) {
      case -1: perror("fork"); exit(2); break;
      case 0:
          if (execvp(executable, argv) < 0) {
              fprintf(stderr, "execvw '%s': ", executable);
              perror(NULL);
              exit(1);
          }
          fprintf(stderr, "not reachable 2322\n");
          break;
      default:
          break;
    }
    int status;
    if (wait(&status) == -1) {
        fprintf(stderr, "pids %d %d\n", getpid(), childpid);
        perror("wait");
        exit(2);
    }
    if (gettimeofday(&time1, NULL) == -1) {
        perror("gettimeofday");
        exit(2);
    }
    struct rusage after;
    if (getrusage(RUSAGE_CHILDREN, &after) == -1) {
        perror("getrusage2");
        exit(2);
    }
    double time_after = time1.tv_sec +
        (double)(time1.tv_usec) / 1000.0 / 1000.0;
    fprintf(out, "%g full seconds real time\n", time_after - time_before);
    fprintf(out, "%ld microseconds real time\n", (int64_t)
            ((time_after - time_before) * 1000 * 1000));

    int use_exit_status = 0;
    int use_this_exit_status = -1;
    int exit_signal = UNSET;

    /*
     * Proper exit for everybody.  For clarity, let's collect all info first
     * before we conditionalize on all of it below.
     */

    if (WIFSIGNALED(status)) {
        /*
         * The child exited on a signal.
         * Please note the following complications:
         * - you cannot fake this kind of status with a numeric exit
         *   code.  (in shellscripts it looks like you can but that
         *           is the shell translating it for you)
         * - the child might have received signals that normally
         *   exit the program and can be caught, such as SIGTERM and
         *   SIGHUP, but the child can decide to not exit on them,
         *   and subsequently on a later normal exit not carry this
         *   signal information.  In that case it is important that
         *   our wrapper behaves like the child did to programs
         *   calling our wrapper.  Emacs is an example of using
         *   SIGTERM for Control-G, processing the signal but not
         *   using it for any exiting.
         *   https://www.cons.org/cracauer/sigint.html
         *
         *   Regardless of whether our wrapper
         *   received the same signal (e.g. from the user issuing
         *   Control-C in a shell, in which case the whole stack of
         *   children of the shell forground process all get the same
         *   signal.
         *   For example, if we wrap around Emacs and the user sends
         *   control-G, then both emacs and our wrapper get SIGINT.
         *   Since emacs doesn't use it for exit we must not exit
         *   on it either.
         *
         *   For now, store whether there was a signal and which one
         *   it was.
         */
        fprintf(out, "%d command terminated by signal (signum)\n",
                WTERMSIG(status));
        exit_signal = WTERMSIG(status);
    }

    if (WIFEXITED(status)) {
        // Normal exit on behalf on child
        fprintf(out, "%d command terminated on its own (exitcode)\n",
                WEXITSTATUS(status));
        use_exit_status = 1;
        use_this_exit_status = WEXITSTATUS(status);
    }
    /*
     * This is complicated, attention please.  You can end up with a normal
     * exit status from the program and a signal that had been delivered to
     * us, both, here.  That happens when e.g. the user pressed control-c,
     * the interactive shell delivered SIGINT to every process in the forground
     * group but the program choice to catch SIGINT and not exit on SIGINT.
     * That can be a bug or it can be deliberate (e.g. emacs) but either way
     * our wrapper around it must follow and exit like the wrapper wasn't there.
     *
     * The program also might have exited on a different signal than what we
     * received, e.g. it blocked SIGINT and the shell delivered both SIGQUIT
     * and SIGINT during runtime, then we would have SIGINT and the program
     * SIGQUIT.  Again, we have to emulate what the program was doing.
     */
    if (use_exit_status) {
        if (exit_signal != UNSET) {
            // A signal shouldn't be signaled when the program terminated
            // normally.
            fprintf(stderr, "internal error 1 in wrapper\n");
        }
        exit(use_this_exit_status);
    }

    /*
     * Let's spell out the possible combinations one by one, commenting on
     * why we do what we are doing.
     *
     * exit_signal is the signal that the child exited on.  It should be set
     * at this point in the code.
     *
     * exit_myself_with_signal is a signal that we received while the child
     * was executing.
     */

    if (exit_signal == UNSET && exit_myself_with_signal == UNSET) {
        // This is an error.  If the child did not exit cleanly
        // there must have been a signal.
        fprintf(stderr, "internal error 2a in wrapper\n");
        exit(2);
    }
    if (exit_signal != UNSET && exit_myself_with_signal == UNSET) {
        // The child exited on a signal.  Although we did not get the
        // same (or any other) signal, this is perfectly valid.
        // Somebody sent kill(2) to the actual program.  Exit like
        // the program to keep the wrapper transparent.
        exit_myself_with_signal = exit_signal;
        // continue after if statements
    }
    if (exit_signal == UNSET && exit_myself_with_signal != UNSET) {
        // We have received a signal, but whether the child received
        // it, too, or not, the child did not exit on it.
        // This should not happen here as an absense of exit_signal
        // should have a normal program exit and exited above.
        fprintf(stderr, "internal error 2b in wrapper\n");
        exit(2);
    }
    if (exit_signal != UNSET && exit_myself_with_signal != UNSET) {
        // Both the child and the wrapper got a signal
        if (exit_signal == exit_myself_with_signal) {
            // Both the same signal.  Typical of e.g. Control-C
            // to a foreground terminal process.
            // No action required.
        } else {
            // Signals were different.  Add a warning to output
            // file.  We still exit like the child did since transparency
            // of the wrapper matters.
            fprintf(out, "%d wrapper received different signal (signum)\n",
                    exit_myself_with_signal);
            exit_myself_with_signal = exit_signal;
        }
    }
    // Flush, close, sync in case we get signal 9
    fclose(out);
    fflush(stderr);

    if (exit_myself_with_signal == UNSET) {
        fprintf(stderr, "wat\n");
    }
    // Everything sorted, we know how to exit.  Do it.
    // Do not check error status here.  Some signals you cannot
    // set here (e.g. 9) and if it fails there isn't anything to do.
    // The fallthrough would print the signal number so that you
    // can diagnose.
    signal(exit_myself_with_signal, SIG_DFL);
    if (kill(getpid(), exit_myself_with_signal) == -1) {
        perror("kill myself");
    }
    fprintf(stderr, "not reachable2, sigcodes %d %d\n",
            exit_signal, exit_myself_with_signal);
    exit(2);
}
