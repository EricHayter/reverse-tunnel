#pragma once

#include <atomic>
#include <optional>

/* ============================================================================
 *                            Lockless Queue
 * ============================================================================
 * For the design of the reverse tunnel, I plan on creating a worker pool to
 * handle writes with values being entered into the queue by a thread reading
 * from epoll. Wanted to see if I could implement something lockless for fun.
 *
 * Design:
 *
 * The original design was just a linked list with no dummie nodes. It appears
 * in general to be fine minus one specific case: popping from a queue size of
 * 1. In that case you would imagine I suppose loading the head of the list
 * and its next element (in this case nullptr). You would then do a CAS on
 * the head with the expected value of the old head and setting it to nullptr
 * since that's what the next pointer was at the time of loading.
 *
 * THIS IS A BUG!
 *
 * in specific you could imagine another thread performing a push to the queue
 * after the load of the next variable. Despite the next variable changing
 * the CAS would still succeed since head itself didn't change. This
 *
 * effectively corrupts your queue and drops and element. Therefore, you need
 * something slightly better.
 *
 * The solution?
 *
 * A dumbie node. In this case when we create the queue we allocate one dumbie
 * node to the queue and set both the tail and head to point to this value.
 * How does this help? Well let's go back to our original problem we had
 * before: a pop operation on a queue of length 1. In that case we would
 * load the dumbie node, load its next pointer, and also the tail. Checking
 * if the queue is empty is trivial: compare head and tail (and that head->next
 * is pointing to nullptr since tail could potentially lag behind). Then we do
 * a CAS on the dumbie node with the dumbie node's next value.
 *
 * But how does this change anything?
 *
 * If you pay attention you'll notice that in the first non-dumbie node
 * implementation, we doing the pop we are using the workign with the SAME
 * pointer (i.e. the tail's next pointer) for the popping. This was a problem
 * since a push operation would change the tail value's next pointer from under
 * us. However in the dumby node version, we are just interacting with the tail
 * node directly (not using its next pointer). Next pointers are never changed
 * on internal nodes, only the tail's next pointer is ever changed in the case
 * of pushing (i.e. the tail's pointer goes from nullptr to a new node).
 */

namespace experimental {

template <typename T> class Queue {
  public:
    Queue() : head_m{new Node{}}, tail_m{head_m.load()} {}

    void push(T &&val);
    std::optional<T> pop();

  private:
    struct Node {
        T value{};
        std::atomic<Node *> next{};
    };

    std::atomic<Node *> head_m{};
    std::atomic<Node *> tail_m{};
};

template <typename T> std::optional<T> Queue<T>::pop() {
    while (true) {
        Node *old_head = head_m.load();
        Node *old_tail = tail_m.load();
        Node *old_next = old_head->next.load();

        if (old_head == old_tail) {
            // empty queue
            if (old_next == nullptr)
                return std::nullopt;

            // tail pointer is lagging
            tail_m.compare_exchange_weak(old_tail, old_next);
            continue;
        }

        T res = old_next->value;
        if (head_m.compare_exchange_weak(old_head, old_next)) {
            return res;
        }
    }
}

template <typename T> void Queue<T>::push(T &&val) {
    Node *new_node = new Node{.value = std::forward<T>(val), .next = nullptr};
    Node *old_tail = tail_m.load();
    while (true) {
        // We expect tail to be the last element
        Node *old_tail_next = nullptr;
        if (old_tail->next.compare_exchange_weak(old_tail_next, new_node)) {
            // Attempt to set the new tail. We can't keep busy retrying this
            // since our node may not have been the last insert in this case.
            // In which case we just attempt to set the tail but it's fine if
            // it doesn't work (i.e. it lags forwards a bit).
            tail_m.compare_exchange_weak(old_tail, new_node);
            return;
        }

        // There's two cases where the above compare exchange could fail:
        // A) the value we expected isn't correct (i.e. this isn't the true
        //    tail and it has a non-null next element. In this
        //    case old_tail would contain the next element in the queue.
        // B) the sneaky case where the expected value IS valid, but the
        //    set operation fails. In this case our expectations were valid
        //    so we should just retry inserting on the same node again.
        if (old_tail_next != nullptr) {
            old_tail = old_tail_next;
        }
    }
}

}; // namespace experimental
