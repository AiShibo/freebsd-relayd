#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/queue.h>
#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>

#define MSG_TYPE_TEST 1
#define PAYLOAD_SIZE 10240

struct test_payload {
    char data[PAYLOAD_SIZE];
};

void parent_process(int sockfd) {
    struct imsgbuf ibuf;
    struct test_payload payload;
    int fd;
    ssize_t bytes_read;
    
    printf("Parent: Starting parent process\n");
    imsg_init(&ibuf, sockfd);
    
    // Open /dev/random
    fd = open("/dev/random", O_RDONLY);
    if (fd == -1)
        err(1, "open /dev/random");
    
    // Read PAYLOAD_SIZE bytes from /dev/random
    bytes_read = read(fd, payload.data, PAYLOAD_SIZE);
    if (bytes_read == -1)
        err(1, "read from /dev/random");
    if (bytes_read != PAYLOAD_SIZE)
        errx(1, "read only %zd bytes from /dev/random, expected %d", 
             bytes_read, PAYLOAD_SIZE);
    
    close(fd);
    
    printf("Parent: Sending message with payload size %zu bytes\n", sizeof(payload));
    
    // Send message
    if (imsg_compose(&ibuf, MSG_TYPE_TEST, 0, 0, -1, &payload, sizeof(payload)) == -1)
        err(1, "imsg_compose");
    
    if (imsg_flush(&ibuf) == -1)
        err(1, "imsg_flush");
    
    printf("Parent: Message sent, waiting for child to terminate\n");
    
    // Wait for child
    wait(NULL);
    
    printf("Parent: Child has terminated\n");
    
    imsg_clear(&ibuf);
    close(sockfd);
}

void child_process(int sockfd) {
    struct imsgbuf ibuf;
    struct imsg imsg;
    struct test_payload *payload;
    int read_ret;
    int imsg_get_ret;

    imsg_init(&ibuf, sockfd);
    
    printf("Child: Waiting for messages...\n");
    
    // Read in a loop until input is depleted
    while ((read_ret = imsg_read(&ibuf)) > 0) {
        printf("read_ret is %d\n", read_ret);
        
        while ((imsg_get_ret = imsg_get(&ibuf, &imsg)) > 0) {
            printf("imsg.hdr.type is %d\n", imsg.hdr.type);
            printf("imsg.hdr.size is %d\n", imsg.hdr.len);
            if (imsg.hdr.type == MSG_TYPE_TEST) {
                printf("Child: Received message type %u with payload size %u bytes\n", 
                       imsg.hdr.type, (unsigned int)(imsg.hdr.len - IMSG_HEADER_SIZE));
                
                payload = (struct test_payload *)imsg.data;
		// claude: do not print the fized value, get it from hdr.len
                printf("Child: Received %u bytes of random data\n", (unsigned int)(imsg.hdr.len - IMSG_HEADER_SIZE));
		// claude: print the last char of the received data, by hdr.len
                if (imsg.hdr.len > IMSG_HEADER_SIZE) {
                    unsigned int data_len = imsg.hdr.len - IMSG_HEADER_SIZE;
                    printf("Child: Last char of received data: 0x%02x\n", 
                           (unsigned char)payload->data[data_len - 1]);
                }
            }
            imsg_free(&imsg);
        }
	printf("imsg_get_ret is %d\n", imsg_get_ret);
    }
    
    if (read_ret == -1)
        err(1, "imsg_read");
    
    printf("Child: Input depleted, exiting...\n");
    imsg_clear(&ibuf);
    close(sockfd);
}

int main(void) {
    int sockpair[2];
    pid_t pid;
    
    printf("Starting imsg test program...\n");
    
    // Create socket pair for communication
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) == -1)
        err(1, "socketpair");
    
    pid = fork();
    if (pid == -1)
        err(1, "fork");
    
    if (pid == 0) {
        // Child process
        close(sockpair[0]);
        child_process(sockpair[1]);
        exit(0);
    } else {
        // Parent process
        close(sockpair[1]);
        parent_process(sockpair[0]);
    }
    
    printf("Program completed successfully.\n");
    return 0;
}
