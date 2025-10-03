// chat_b.c
// Compile: gcc chat_b.c -o chat_b
// Run in terminal 2: ./chat_b

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MAX 100

struct msg_buffer {
    long msg_type;
    char sender[20];
    char text[MAX];
};

int main() {
    key_t key = ftok("chatfile", 65);
    int msgid = msgget(key, 0666 | IPC_CREAT);

    struct msg_buffer msg;

    while (1) {
        // receive message from A (type=1)
        msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), 1, 0);
        printf("%s: %s", msg.sender, msg.text);

        if (strncmp(msg.text, "exit", 4) == 0)
            break;

        // reply back with type=2
        msg.msg_type = 2;
        strcpy(msg.sender, "Process-B");
        printf("B: ");
        fgets(msg.text, MAX, stdin);

        msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0);

        if (strncmp(msg.text, "exit", 4) == 0)
            break;
    }

    return 0;
}
