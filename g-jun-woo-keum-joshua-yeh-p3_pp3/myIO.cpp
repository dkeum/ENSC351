//============================================================================
//
//% Student Name 1: Jun Woo Keum
//% Student 1 #: 301335490
//% Student 1 userid (email): dkeum (dkeum@sfu.ca)
//
//% Student Name 2: Joshua Yeh
//% Student 2 #: 301381664
//% Student 2 userid (email): jya163 (jy163@sfu.ca)
//
//% Below, edit to list any people who helped you with the code in this file,
//%      or put 'None' if nobody helped (the two of) you.
//
// Helpers: _everybody helped us/me with the assignment (list names or put 'None')__
//
// Also, list any resources beyond the course textbooks and the course pages on Piazza
// that you used in making your submission.
//
// Resources:  ___________
//
//%% Instructions:
//% * Put your name(s), student number(s), userid(s) in the above section.
//% * Also enter the above information in other files to submit.
//% * Edit the "Helpers" line and, if necessary, the "Resources" line.
//% * Your group name should be "P3_<userid1>_<userid2>" (eg. P3_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : myIO.cpp
// Version     : September 28, 2021
// Description : Wrapper I/O functions for ENSC-351
// Copyright (c) 2021 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include <unistd.h>         // for read/write/close
#include <fcntl.h>          // for open/creat
#include <sys/socket.h>     // for socketpair
#include <stdarg.h>         // for va stuff
#include "SocketReadcond.h"
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cerrno>

using namespace std;
class condVar_Mutex {
public:
    std::mutex mux;
    std::condition_variable data_cond_drain; // for draining
    std::condition_variable data_cond_read; // for reading
    int mybuffer;
    int descriptor;
    //constructor
    condVar_Mutex() {
        mybuffer = 0;
        descriptor = 0;
    }
    //2nd constructor
    condVar_Mutex(int socketnum) {
        mybuffer = 0;
        descriptor = socketnum;
    }
    void drain() {
        std::unique_lock<std::mutex> lock(mux);
        data_cond_drain.wait(lock, [this] {return mybuffer <= 0; });
    }
    void drain(unique_lock<mutex>& lock_drain) {
        unique_lock<mutex> lock(mux);
        lock_drain.unlock();
        data_cond_drain.wait(lock, [this] {return mybuffer <= 0; });
        sleep(0.01);
    }
    int write_withClassMux(int des, const void* buf, size_t nbyte) {
        lock_guard<mutex> lock(mux);
        int data_written = write(des, buf, nbyte);
        mybuffer += data_written;
        if (mybuffer >= 0) {
            data_cond_read.notify_one();
        }
        return data_written;
    }
    int readCond_withClassMux(int des, void* buf, int n, int min, int time, int timeout)
    {
        unique_lock<mutex> lock(mux);
        int read_data;
        if (mybuffer >= min) {
            read_data = wcsReadcond(des, buf, n, min, time, timeout);
            mybuffer -= read_data;
            if (mybuffer > 0) {
                data_cond_drain.notify_all();
            }
        }
        else {
            mybuffer -= min;
            if (mybuffer <= 0) {
                data_cond_drain.notify_all();
            }
            data_cond_read.wait(lock, [this] {return mybuffer >= 0; });
            read_data = wcsReadcond(des, buf, n, min, time, timeout);
            //printf("read_data is: %d and min is: %d \n",read_data, min);
            mybuffer -= read_data - min;
        }
        return read_data;
    }
    bool checkClose()
    {
        //lock_guard<mutex> lock(mux);
        unique_lock<mutex> lock(mux);
        if (mybuffer > 0)
        {
            mybuffer = 0;
            data_cond_drain.notify_all();
        }
        else if (mybuffer < 0)
        {
            mybuffer = 0;
            data_cond_read.notify_all();
        }
        return true;    //Returns true to show that it is done checking
    }
};

std::vector <condVar_Mutex*> mtx; //for storing file  descriptors and socket pairs
mutex globalMux; // for locking each function not inside the class

int myOpen(const char* pathname, int flags, ...) //, mode_t mode)
{
    std::lock_guard<mutex> lock(globalMux);
    mode_t mode = 0;
    // in theory we should check here whether mode is needed.
    va_list arg;
    va_start(arg, flags);
    mode = va_arg(arg, mode_t);
    va_end(arg);
    unsigned long int file_descriptor = open(pathname, flags, mode);
    //put file descriptors into vector
    if (mtx.size() <= file_descriptor) {
        mtx.resize(file_descriptor + 1);
    }
    mtx[file_descriptor] = new condVar_Mutex();
    return file_descriptor;
}
int myCreat(const char* pathname, mode_t mode)
{
    std::lock_guard<mutex> lock(globalMux);
    unsigned long int file_descriptor = creat(pathname, mode);

    //put file descriptors in vector
    if (mtx.size() <= file_descriptor) {
        mtx.resize(file_descriptor + 1);
    }
    mtx[file_descriptor] = new condVar_Mutex();
    return file_descriptor;
}
int mySocketpair(int domain, int type, int protocol, int des_array[2])
{
    std::lock_guard<mutex> lock(globalMux);
    int returnVal = socketpair(domain, type, protocol, des_array);

    long unsigned int socket_descriptors = max(des_array[0], des_array[1]);

    //put the socket in a vector
    if (mtx.size() < socket_descriptors) {
        mtx.resize(socket_descriptors + 1);
    }
    mtx[des_array[0]] = new condVar_Mutex(des_array[1]); //match index and descriptors
    mtx[des_array[1]] = new condVar_Mutex(des_array[0]); //match index and descriptors

    return returnVal;
}
ssize_t myRead(int des, void* buf, size_t nbyte)
{
    //std::lock_guard<std::mutex> lock_read(mtx.mux);
    int data_read;
    if (mtx[des] == nullptr) {
        errno = 9;
        data_read = read(des, buf, nbyte);
        return -1;
    }
    else {
        int isFile_descriptor = mtx[des]->descriptor;
        if (isFile_descriptor != 0) {
            data_read = mtx[des]->readCond_withClassMux(des, buf, nbyte, 1, 0, 0);
        }
        else {
            data_read = read(des, buf, nbyte);
        }
        return data_read;
    }
}
ssize_t myWrite(int des, const void* buf, size_t nbyte)
{
    //use global mux here because it is not in a class
    std::lock_guard<mutex> lock(globalMux);
    long unsigned int data_written;
    if (mtx[des] == nullptr) {
        errno = 9;
        return -1;
    }
    else {
        int isFile_descriptor = mtx[des]->descriptor;
        if (isFile_descriptor != 0) {
            data_written = mtx[isFile_descriptor]->write_withClassMux(des, buf, nbyte);
        }
        else {
            data_written = write(des, buf, nbyte);
        }
        //lock_write.unlock();
        return data_written;
    }
}
int myClose(int des)
{
    std::lock_guard<mutex> lock(globalMux);
    //  unique_lock<mutex> close_lock(globalMux);
    //printf("des value: %d and mybuffer %d\n",des , mtx[des]->mybuffer);
    if (mtx[des] != nullptr) {
        if (mtx[des]->descriptor != 0) {  //Checks if socket or file
            mtx[des]->checkClose();
            if (mtx[mtx[des]->descriptor] != nullptr) {
                mtx[mtx[des]->descriptor]->checkClose();    //Returns true when it is done checking for the pair
                mtx[mtx[des]->descriptor]->descriptor = 0;
            }
            mtx[des]->data_cond_read.notify_all();
            sleep(0.1);
            delete mtx[des];
            mtx[des] = nullptr;
            return close(des);
        }
        else {
            delete mtx[des];
            mtx[des] = nullptr;
            return close(des);
        }
    }
    else {
        errno = 9;
        return -1;
    }
    return close(des);
}
int myTcdrain(int des)
{ //is also included for purposes of the course.
    //std::lock_guard<mutex> lock(globalMux);//does not work with guard by itself
    unique_lock<mutex> drain_lock(globalMux);
    if (mtx[des] == nullptr) {
        errno = 9;
        return -1;
    }
    else {
        int socket_descriptor = mtx[des]->descriptor;
        if (socket_descriptor != 0) {
            mtx[mtx[des]->descriptor]->drain(drain_lock);//works with lock
            //mtx[mtx[des]->descriptor]->drain(); //does not work without lock
        }
        return 0;
    }
    return 0;
}
/* Arguments:
des
    The file descriptor associated with the terminal device that you want to read from.
buf
    A pointer to a buffer into which readcond() can put the data.
n
    The maximum number of bytes to read.
min, time, timeout
    When used in RAW mode, these arguments override the behavior of the MIN and TIME members of the terminal's termios structure. For more information, see...
 *
 *  https://developer.blackberry.com/native/reference/core/com.qnx.doc.neutrino.lib_ref/topic/r/readcond.html
 *
 *  */
int myReadcond(int des, void* buf, int n, int min, int time, int timeout)
{
    if (mtx[des] == nullptr) {
        errno = 9;
        return -1;
    }
    else {
        int isFileDescriptor = mtx[des]->descriptor;
        int data_read;
        if (isFileDescriptor != 0) {
            data_read = mtx[des]->readCond_withClassMux(des, buf, n, min, time, timeout);
        }
        else {
            data_read = wcsReadcond(des, buf, n, min, time, timeout);
        }
        return data_read;
    }
}
