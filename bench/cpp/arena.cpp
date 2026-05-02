#include <cstdio>
#include <memory_resource>
#include <vector>

struct Node {
    int val;
    Node* next;
};

int main() {
    constexpr int N = 5000000;

    std::vector<std::byte> buf(static_cast<std::size_t>(N) * 16 + 64);
    std::pmr::monotonic_buffer_resource res(buf.data(), buf.size());

    Node* head = nullptr;
    for (int i = 0; i < N; i++) {
        void* mem = res.allocate(sizeof(Node), alignof(Node));
        Node* n = new (mem) Node{i, head};
        head = n;
    }

    int total = 0;
    for (Node* c = head; c; c = c->next) {
        total += c->val;
    }

    std::printf("arena total = %d\n", total);
    return 0;
}
