#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int val;
    struct Node* next;
} Node;

typedef struct {
    unsigned char* head;
    int cap;
    int used;
} Arena;

static Arena* arena_new(int cap) {
    Arena* a = (Arena*)malloc(sizeof(Arena));
    a->head = (unsigned char*)malloc((size_t)cap);
    a->cap = cap;
    a->used = 0;
    return a;
}

static void* arena_alloc(Arena* a, int size) {
    int aligned = (size + 7) & ~7;
    if (a->used + aligned > a->cap) {
        fprintf(stderr, "arena oom\n");
        exit(1);
    }
    void* p = a->head + a->used;
    a->used += aligned;
    return p;
}

static void arena_free(Arena* a) {
    free(a->head);
    free(a);
}

int main(void) {
    const int N = 5000000;
    Arena* a = arena_new(N * 16 + 64);

    Node* head = NULL;
    for (int i = 0; i < N; i++) {
        Node* n = (Node*)arena_alloc(a, sizeof(Node));
        n->val = i;
        n->next = head;
        head = n;
    }

    int total = 0;
    for (Node* c = head; c; c = c->next) {
        total += c->val;
    }

    arena_free(a);
    printf("arena total = %d\n", total);
    return 0;
}
