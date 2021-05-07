//
// Created by daqige on 2021/1/5.
//
#include "locker.h"
#include <iostream>
#include <unistd.h>
#include <wait.h>

using namespace std;

Locker* locker = new Locker();

void* another(void* arg) {
    cout << "Join child thread, lock the mutex!\n";
    locker->lock();
    sleep(5);
    locker->unlock();
}

void prepare() {
    locker->lock();
}

void infork() {
    locker->unlock();
}

int main() {
    pthread_t id;
    pthread_create(&id, nullptr, another, nullptr);
    sleep(1);
    pthread_atfork(prepare, infork, infork);
    int pid = fork();
    if (pid < 0) {
        pthread_join(id, nullptr);
        delete locker;
        return 1;
    }
    else if (pid == 0) {
        cout << "This is a child progress, want to get the lock!\n";
        locker->lock();
        cout << "Can not run to here ....\n";
        locker->unlock();
    }
    else {
        wait(NULL);
    }
    pthread_join(id, nullptr);
    delete locker;
    return 0;
}
