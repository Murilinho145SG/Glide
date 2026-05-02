#include <cstdio>

struct Node {
    int val;
    Node* next;
};

int main() {
    constexpr int N = 5000000;

    Node* head = nullptr;
    for (int i = 0; i < N; i++) {
        Node* n = new Node{i, head};
        head = n;
    }

    int total = 0;
    for (Node* c = head; c; c = c->next) {
        total += c->val;
    }

    Node* c = head;
    while (c) {
        Node* nx = c->next;
        delete c;
        c = nx;
    }

    std::printf("alloc total = %d\n", total);
    return 0;
}
