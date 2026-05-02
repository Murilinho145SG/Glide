#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int val;
    struct Node* next;
} Node;

int main(void) {
    const int N = 5000000;

    Node* head = NULL;
    for (int i = 0; i < N; i++) {
        Node* n = (Node*)malloc(sizeof(Node));
        n->val = i;
        n->next = head;
        head = n;
    }

    int total = 0;
    for (Node* c = head; c; c = c->next) {
        total += c->val;
    }

    Node* c = head;
    while (c) {
        Node* nx = c->next;
        free(c);
        c = nx;
    }

    printf("alloc total = %d\n", total);
    return 0;
}
