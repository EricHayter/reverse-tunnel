#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>

template<typename T>
class Queue {
public:
   void push(T val);
   T pop();
private:
   std::mutex mut_m;
   std::condition_variable cond_m;
   std::queue<T> queue_m;
};

template<typename T>
void Queue<T>::push(T val)
{
    std::lock_guard<std::mutex> lg{ mut_m };
    queue_m.push(std::move(val));
    cond_m.notify_one();
}


template<typename T>
T Queue<T>::pop()
{
    std::unique_lock<std::mutex> lk{ mut_m };
    cond_m.wait(lk, [this](){
        return !queue_m.empty();
    });
    T ret = std::move(queue_m.front());
    queue_m.pop();
    return ret;
}
