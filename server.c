
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "comm.h"
#include "util.h"

#include <sys/types.h>
#include <signal.h>

/* -----------Functions that implement server functionality -------------------------*/

// Returns the empty slot on success, or -1 on failure
int find_empty_slot(USER * user_list) {
    int i = 0;
    for(i=0;i<MAX_USER;i++) {
        if(user_list[i].m_status == SLOT_EMPTY) {
            return i;
        }
    }
    return -1;
}

// list the existing users on the server shell
int list_users(int idx, USER * user_list)
{
    int i, flag = 0;
    char buf[MAX_MSG] = {}, *s = NULL;

    /* construct a list of user names */
    s = buf;
    strncpy(s, "\n---connected user list---\n", strlen("\n---connected user list---\n"));
    s += strlen("\n---connected user list---\n");
    for (i = 0; i < MAX_USER; i++) {
        if (user_list[i].m_status == SLOT_EMPTY)
            continue;
        flag = 1;
        strncpy(s, user_list[i].m_user_id, strlen(user_list[i].m_user_id));
        s = s + strlen(user_list[i].m_user_id);
        strncpy(s, "\n", 1);
        s++;
    }
    if (flag == 0) {
        strcpy(buf, "<no users>\n");
    } else {
        s--;
        strncpy(s, "\0", 1);
    }

    if(idx < 0) {
        printf("%s", buf);
        printf("\n");
    } else {
        /* write to the given pipe fd */
        //char lst[MAX_MSG];
        strcat(buf, "\n");
        if (write(user_list[idx].m_fd_to_user, buf, strlen(buf) + 1) < 0)
            perror("writing to server shell");
    }

    return 0;
}

// add a new user
int add_user(int idx, USER * user_list, pid_t pid, char * user_id, int pipe_to_child, int pipe_to_parent)
{
    user_list[idx].m_pid = pid;
    strcpy(user_list[idx].m_user_id, user_id);
    user_list[idx].m_fd_to_user = pipe_to_child;
    user_list[idx].m_fd_to_server = pipe_to_parent;
    user_list[idx].m_status = SLOT_FULL;

    return idx;
}

// Kill a user
void kill_user(int idx, USER * user_list) {
    // kill a user (specified by idx) by using the systemcall kill()
    // then call waitpid on the user
    // int status, ret;
    //int pid = user_list[idx].m_pid;
    kill(user_list[idx].m_pid, SIGKILL);

    int childpid, wstatus = 1;
    while(wstatus) {
      childpid = waitpid(user_list[idx].m_pid, &wstatus, WNOHANG);
    if (childpid > 0 && WIFSIGNALED(wstatus)) {
      break;
    } else if (childpid == -1) {
      if (errno == ECHILD) {
        fprintf(stderr, "kill_user: user process id %d does not exist\n", user_list[idx].m_pid);
        break;
      } else if (errno == EINTR) wstatus = 1; //go back and wait for user process to terminate
    } else if (childpid == 0) wstatus = 1;
  }
}

// Perform cleanup actions after the user has been killed
void cleanup_user(int idx, USER * user_list)
{
    user_list[idx].m_pid = -1;
    memset(user_list[idx].m_user_id, 0, sizeof(user_list[idx].m_user_id));
    if (close(user_list[idx].m_fd_to_user) == -1) {
     fprintf(stderr, "cleanup_user: failed to close pipe to child, fd = %d\n", user_list[idx].m_fd_to_user);
     exit(1);
    }
    if (close(user_list[idx].m_fd_to_server) == -1) {
      fprintf(stderr, "cleanup_user: failed to close pipe to server, fd = %d\n", user_list[idx].m_fd_to_server);
      exit(1);
    }
    user_list[idx].m_fd_to_user = -1;
    user_list[idx].m_fd_to_server = -1;
    user_list[idx].m_status = SLOT_EMPTY;
}

// Kills the user and performs cleanup
void kick_user(int idx, USER * user_list) {
    // should kill_user()
    // then perform cleanup_user()
    //write(user_list[idx].m_fd_to_server, 0, sizeof(0));
    kill_user(idx, user_list);
    cleanup_user(idx, user_list);
}

// broadcast message to all users
int broadcast_msg(USER * user_list, char *buf, char *sender)
{
    //iterate over the user_list and if a slot is full, and the user is not
    //the sender itself, then send the message to that user
    //return zero on success
    char buf2[MAX_MSG];
    sprintf(buf2, "%s: %s", sender, buf);
    for (int i = 0; i < MAX_USER; i++){
        if (user_list[i].m_status == SLOT_FULL && strcmp(user_list[i].m_user_id, sender) != 0)
        if( write(user_list[i].m_fd_to_user, buf2, sizeof(buf2)) == -1) {
          fprintf(stderr, "broadcast_msg: could not send message to %s from %s\n", user_list[i].m_user_id, sender);
          exit(1);
        }
    }
    return 0;
}

// Cleanup user chat boxes
void cleanup_users(USER * user_list)
{
    // go over the user list and check for any empty slots
    // call cleanup user for each of those users.
    for (int i = 0; i < MAX_USER; i++){
        if (user_list[i].m_status != SLOT_EMPTY)
            cleanup_user(i, user_list);
    }
}

// find user index for given user name
int find_user_index(USER * user_list, char * user_id)
{
    // go over the  user list to return the index of the user which matches
    // the argument user_id
    // return -1 if not found
    int i, user_idx = -1;
    if (user_id == NULL) {
        fprintf(stderr, "NULL name passed.\n");
        return user_idx;
    }
    for (i=0;i<MAX_USER;i++) {
        if (user_list[i].m_status == SLOT_EMPTY)
            continue;
        if (strcmp(user_list[i].m_user_id, user_id) == 0) {
            return i;
        }
    }
    return -1;
}

// given a command's input buffer, extract name of user, the second token
int extract_name(char * buf, char * user_name)
{
    char inbuf[MAX_MSG];
    char * tokens[16];
    strcpy(inbuf, buf);

    int token_cnt = parse_line(inbuf, tokens, " ");

    if(token_cnt >= 2) {
        strcpy(user_name, tokens[1]);
        return 0;
    }
    return -1;
}

int extract_text(char *buf, char * text)
{
    char inbuf[MAX_MSG];
    char * tokens[16];
    char * s = NULL;
    strcpy(inbuf, buf);

    int token_cnt = parse_line(inbuf, tokens, " ");

    if(token_cnt >= 3) {
        //Find " "
        s = strchr(buf, ' ');
        s = strchr(s+1, ' ');

        strcpy(text, s+1);
        return 0;
    }

    return -1;
}

/*
 * send personal message
 */
void send_p2p_msg(int idx, USER * user_list, char *buf)
{
    char text[MAX_MSG];
    char user_id[MAX_USER];
    char msg[MAX_MSG];

    extract_name(buf, user_id);
    extract_text(buf, text);

    int i = find_user_index(user_list, user_id);
    // if user doesn't exist
    if (i == -1) {
      if( write(user_list[idx].m_fd_to_user, "User not found", 15) == -1) {
        perror("send_p2p_msg: Failed to write\n");
        exit(1);
      }
    }
    else {
        sprintf(msg, "%s: %s", user_list[idx].m_user_id, text);
        printf("%s", msg);
        fflush(stdout);
        //write(user_list[i].m_fd_to_server, msg, sizeof(msg));
        if (write(user_list[i].m_fd_to_user, msg, sizeof(msg)) == -1) {
          printf("send_p2p_msg: Failed to write\n");
        }
    }
}


// Populates the user list initially
void init_user_list(USER * user_list) {

    //iterate over the MAX_USER
    //memset() all m_user_id to zero
    //set all fd to -1
    //set the status to be EMPTY
    int i=0;
    for(i=0;i<MAX_USER;i++) {
        user_list[i].m_pid = -1;
        memset(user_list[i].m_user_id, '\0', MAX_USER_ID);
        user_list[i].m_fd_to_user = -1;
        user_list[i].m_fd_to_server = -1;
        user_list[i].m_status = SLOT_EMPTY;
    }
}
/* ---------------------End of the functions that implementServer functionality -----------------*/


/* ---------------------Start of the Main function ----------------------------------------------*/
int main(int argc, char * argv[]) {

    int nbytes;
    int empty_slot;
    int user_idx;
    setup_connection("SAM_CHAT"); // Specifies the connection point as argument.

    USER user_list[MAX_USER];
    init_user_list(user_list);          // Initialize user list

    char buf[MAX_MSG];
    fcntl(0, F_SETFL, fcntl(0, F_GETFL)| O_NONBLOCK); // manipulates fd for stdin, nonblock

    //DELETE THIS MAYBE
    fcntl(1, F_SETFL, fcntl(0, F_GETFL)| O_NONBLOCK);

    print_prompt("admin");

    while (1) {
        char user_id[MAX_USER_ID];
        int status;

        int pipe_SERVER_reading_from_child[2];
        int pipe_SERVER_writing_to_child[2];

        int pipe_CHILD_reading_from_client[2];
        int pipe_CHILD_writing_to_client[2];

        memset(buf, '\0', sizeof(buf));
        if (get_connection(user_id, pipe_CHILD_writing_to_client, pipe_CHILD_reading_from_client) != -1) {
            empty_slot = find_empty_slot(user_list);
            user_idx = find_user_index(user_list, user_id);

            if (empty_slot == -1)
                printf("\nNo more room for a new user");
            else if (user_idx != -1) {
                memset(buf, '\0', sizeof(buf));
                sprintf(buf, "User id '%s' already taken\n", user_id);
                if( write(pipe_CHILD_writing_to_client[1], buf, sizeof(buf)) == -1) {
                  perror("Write failed from buf to pipe_CHILD_writing_to_client");
                  exit(1);
                }
                memset(buf, '\0', sizeof(buf));
            }
            else {    //User joins here, since the above cases were passed by
                user_idx = empty_slot;

                printf("\n--User '%s' joined in slot %d--\n", user_id, user_idx);
                print_prompt("admin");
                fflush(stdout);

                if (pipe(pipe_SERVER_reading_from_child) == -1) {
                  perror("pipe_SERVER_reading_from_child error");
                  exit(1);
                }
                if(pipe(pipe_SERVER_writing_to_child) == -1) {
                  perror("pipe_SERVER_writing_to_child error");
                  exit(1);
                }

                //Make read/write non-blocking for serverReadChild non-blocking
                fcntl(pipe_SERVER_reading_from_child[0], F_SETFL, fcntl(pipe_SERVER_reading_from_child[0], F_GETFL) | O_NONBLOCK);

                //Make read/write non-blocking for serverWriteChild non-blocking
                fcntl(pipe_SERVER_writing_to_child[0], F_SETFL, fcntl(pipe_SERVER_writing_to_child[0], F_GETFL) | O_NONBLOCK);
                fcntl(pipe_SERVER_writing_to_child[1], F_SETFL, fcntl(pipe_SERVER_writing_to_child[0], F_GETFL) | O_NONBLOCK);

                pid_t childpid;

                childpid = fork();

                /*________________________Parent process_______________________________*/
                if (childpid > 0) {
                    add_user(empty_slot, user_list, childpid, user_id, pipe_SERVER_writing_to_child[1], pipe_SERVER_reading_from_child[0]);
                    // close pipes not used by the server in order to detect broken pipe
                    // and terminate client
                    if(close(pipe_SERVER_writing_to_child[0]) == -1) {
                      perror("Failed to close the read end of pipe_SERVER_writing_to_child\n");
                      exit(1);
                    }
                    if (close(pipe_SERVER_reading_from_child[1]) == -1) {
                      perror("Failed to close the write end of pipe_SERVER_reading_from_child\n");
                      exit(1);
                    }

                    if (close(pipe_CHILD_writing_to_client[0]) == -1) {
                      perror("Failed to close the read end of pipe_CHILD_writing_to_client\n");
                      exit(1);
                    }
                    if(close(pipe_CHILD_writing_to_client[1]) == -1) {
                      perror("Failed to close the write end of pipe_CHILD_writing_to_client\n");
                      exit(1);
                    }
                    if(close(pipe_CHILD_reading_from_client[0]) == -1) {
                      perror("Failed to close the read end of pipe_CHILD_reading_from_client\n");
                      exit(1);
                    }
                    if (close(pipe_CHILD_reading_from_client[1]) == -1) {
                      perror("Failed to close the write end of pipe_CHILD_reading_from_client\n");
                      exit(1);
                    }
                }
                /*_________________________Child process_______________________________*/
                else if (childpid == 0) {

                    if (close(pipe_CHILD_writing_to_client[0]) == -1) {
                      perror("Failed to close the read end of pipe_CHILD_writing_to_client\n");
                      exit(1);
                    }
                    if (close(pipe_CHILD_reading_from_client[1])  == -1) {
                      perror("Failed to close the write end of pipe_CHILD_reading_from_client\n");
                      exit(1);
                    }
                    if (close(pipe_SERVER_writing_to_child[1]) == -1) {
                      perror("Failed to close the write end of pipe_SERVER_writing_to_child\n");
                      exit(1);
                    }
                    if (close(pipe_SERVER_reading_from_child[0]) ==-1) {
                      perror("Failed to close the read end of pipe_SERVER_writing_to_child\n");
                      exit(1);
                    }

                    fcntl(pipe_CHILD_reading_from_client[0], F_SETFL, fcntl(pipe_CHILD_reading_from_client[0], F_GETFL) | O_NONBLOCK);
                    fcntl(pipe_SERVER_writing_to_child[0], F_SETFL, fcntl(pipe_SERVER_writing_to_child[0], F_GETFL) | O_NONBLOCK);
                    while (1) {
                        //_______________________poll USERS___________________________
                        memset(buf, '\0', sizeof(buf));
                        fcntl(pipe_CHILD_reading_from_client[0], F_SETFL, fcntl(pipe_CHILD_reading_from_client[0], F_GETFL) | O_NONBLOCK);
                        nbytes = read(pipe_CHILD_reading_from_client[0], buf, MAX_MSG);
                        if (nbytes == -1 && errno == EAGAIN) {
                            usleep(500);
                        }
                        //Pass along to server
                        else if (nbytes > 0) {
                            if (write(pipe_SERVER_reading_from_child[1], buf, nbytes) == -1) {
                              perror("Failed to write to pipe_SERVER_reading_from_child\n");
                              exit(1);
                            }
                            memset(buf, '\0', sizeof(buf));
                        }
                        else if (nbytes == 0){
                            if (write(pipe_SERVER_reading_from_child[1], 0, sizeof(0)) == -1) {
                              perror("Failed to write to pipe_SERVER_reading_from_child \n");
                              exit(1);
                            }
                            exit(0);
                        }
                        //_____________________poll SERVER___________________________
                        memset(buf, '\0', sizeof(buf));
                        fcntl(pipe_SERVER_writing_to_child[0], F_SETFL, fcntl(pipe_SERVER_writing_to_child[0], F_GETFL) | O_NONBLOCK);
                        nbytes = read(pipe_SERVER_writing_to_child[0], buf, MAX_MSG);
                        if (nbytes == -1 && errno == EAGAIN) {
                            usleep(500);
                        }
                        //Pass along to user
                        else if (nbytes > 0) {
                            if(write(pipe_CHILD_writing_to_client[1], buf, nbytes) == -1) {
                              perror("Failed to write to pipe_CHILD_writing_to_client\n");
                              exit(1);
                            }
                            memset(buf, '\0', sizeof(buf));
                        }
                        else if (nbytes == 0){
                            if(write(pipe_CHILD_writing_to_client[1], 0, sizeof(0)) == -1) {
                              perror("Failed to write to pipe_CHILD_writing_to_client\n");
                              exit(1);
                            }
                            exit(0);
                        }
                        usleep(500);
                    } // End polling loop
                }
                /*-----------------------------End Child-----------------------------*/
                else {
                    perror("fork");
                    exit(0);
                }
            } // End good user connection
        } // End if get connection.

        /*__________________________SERVER/parent process_____________________________*/
        else { // there is no new connection...
            /*__________________poll & process ADMIN commands from stdin___________*/
            // close pipes not used...
            memset(buf,'\0',sizeof(buf));
            fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
            nbytes = read(0, buf, MAX_MSG);
            if (nbytes == -1 && errno == EAGAIN){
                usleep(500);
            }
            else if (nbytes > 0) {
                int cmdidx;
                print_prompt("admin");
                cmdidx = get_command_type(buf);
                switch (cmdidx) {
                    case 0 : // list
                        list_users(-1, user_list);
                        break;
                    case 1 : // kick
                        memset(user_id,'\0',sizeof(user_id));
                        extract_name(buf, user_id);
                        user_id[strlen(user_id)-1] = '\0';
                        user_idx = find_user_index(user_list, user_id);
                        if ( user_idx != -1) {
                          memset(user_id,'\0',sizeof(user_id));
                          kick_user(user_idx, user_list);
                        }
                        else
                          printf("No such user exists\n");
                        break;
                    case 4 : // exit
                        cleanup_users(user_list);
                        exit(0);
                    default : // broadcast
                        broadcast_msg(user_list, buf, "admin");
                } // End switch
            } // End nbytes > 0

            /*---------poll appropriate children for USER command processing---------------*/
            memset(buf, '\0', sizeof(buf));  // overwrite the buffer
            for (int i = 0; i < MAX_USER; i++) {
                memset(buf, '\0', sizeof(buf));  // overwrite the buffer
                fcntl(user_list[i].m_fd_to_server, F_SETFL, fcntl(user_list[i].m_fd_to_server, F_GETFL) | O_NONBLOCK);

                if (user_list[i].m_status == SLOT_FULL) {
                    memset(buf,'\0',sizeof(buf));
                    nbytes = read(user_list[i].m_fd_to_server, buf, MAX_MSG);
                    if (nbytes == -1 && errno == EAGAIN)
                        usleep(500);
                    else if (nbytes > 0){
                        int cmdidx;
                        cmdidx = get_command_type(buf);
                        switch (cmdidx) {
                            case 0 : // list
                                list_users(i, user_list);
                                break;
                            case 2 : // p2p
                                send_p2p_msg(i, user_list, buf);
                                break;
                            case 4 : // exit
                                printf("\nUser %s in slot %d exited.\n", user_list[i].m_user_id, i);
                                kick_user(i, user_list);
                                break;
                            default : // broadcast
                                broadcast_msg(user_list, buf, user_list[i].m_user_id);
                        }
                    }
                    else if (nbytes == 0){
                        printf("\nUser %s in slot %d exited.\n", user_list[i].m_user_id, i);
                        fflush(stdout);
                        kick_user(i, user_list);
                    }
                }
            }//Polling users
        } //End !getConnection
    } // End outer while
} // End main()
