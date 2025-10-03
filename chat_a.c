// chat_a.c
// Compile: gcc chat_a.c -o chat_a
// Run in terminal 1: ./chat_a

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
        // send message
        msg.msg_type = 1;
        strcpy(msg.sender, "Process-A");
        printf("A: ");
        fgets(msg.text, MAX, stdin);

        msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0);

        if (strncmp(msg.text, "exit", 4) == 0)
            break;

        // receive reply from B (type=2)
        msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), 2, 0);
        printf("%s: %s", msg.sender, msg.text);

        if (strncmp(msg.text, "exit", 4) == 0)
            break;
    }

    msgctl(msgid, IPC_RMID, NULL); // remove queue when A ends
    return 0;
}
