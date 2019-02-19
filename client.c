
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include "comm.h"

#include <signal.h>
#include <sys/types.h>
#include "util.h"

/* -------------------------Main function for the client ----------------------*/
void main(int argc, char * argv[]) {

	int pipe_user_reading_from_server[2], pipe_user_writing_to_server[2];

	// You will need to get user name as a parameter, argv[1].

	if(connect_to_server("SAM_CHAT", argv[1], pipe_user_reading_from_server, pipe_user_writing_to_server) == -1) {
		exit(-1);
	}

	/* -------------- YOUR CODE STARTS HERE -----------------------------------*/
    int nbytes;
    char buf[MAX_MSG];
    char buf2[MAX_USER_ID];
    char *user_id;

    user_id = argv[1];

    fcntl(pipe_user_reading_from_server[0], F_SETFL, fcntl(pipe_user_reading_from_server[0], F_GETFL) | O_NONBLOCK);  // make pipe_from_server read non-blocking I think
    fcntl(pipe_user_writing_to_server[1], F_SETFL, fcntl(pipe_user_writing_to_server[1], F_GETFL) | O_NONBLOCK);
    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);

    // closing these ends allows for detection of broken pipe
    if(close(pipe_user_reading_from_server[1]) == -1) {
			perror("Failed to close pipe_user_reading_from_server[1]\n");
			exit(1);
		}
    if(close(pipe_user_writing_to_server[0]) == -1) {
			perror("Failed to close pipe_user_writing_to_server[0]\n");
			exit(1);
		}

    print_prompt(user_id);
    while(1){
        //_____________poll stdin for forwarding USER commands____________
        memset(buf, '\0', sizeof(buf));  // clear the buffer
        nbytes = read(0, buf, MAX_MSG);
        if (nbytes == -1 && errno == EAGAIN){
            usleep(500);
        }
        else if (nbytes > 0){
            // send data from buf to child process using pipe_to_server
            print_prompt(user_id);
            if(write(pipe_user_writing_to_server[1], buf, nbytes) == -1) {
							perror("write failed to pipe_user_writing_to_server");
							exit(1);
						}
            // hoping to clear buffer and stop inf loop
            memset(buf, '\0', sizeof(buf));  // clear the buffer
        }

        //_______________poll pipe from SERVER to read from child_______
        memset(buf, '\0', sizeof(buf));  // clear the buffer
        nbytes = read(pipe_user_reading_from_server[0], buf, MAX_MSG);
        // broken pipe is not being detected!
        if (nbytes == -1 && errno == EAGAIN){ // set to nonblock
            usleep(500);
        }
        //else if (nbytes > 0)
        else if (nbytes > 0) {
            // if read returns with some data from the child process, it will be printed out
            printf("\n%s", buf);
						print_prompt(user_id);
            sprintf(buf2, "User id '%s' already taken\n", user_id);
            if (strcmp(buf2, buf) == 0 || strcmp(buf, "\nNo more room for a new user") == 0)
                exit(0);
            fflush(stdout);
            //print_prompt(user_id);
        }
        else if (nbytes == 0){
            //printf("broken pipe detected\n");
            exit(0);
        }
    }

	/* -------------- YOUR CODE ENDS HERE -----------------------------------*/
}

/*--------------------------End of main for the client --------------------------*/
