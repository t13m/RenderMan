/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2016 - ROLI Ltd.

   Permission is granted to use this software under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license/

   Permission to use, copy, modify, and/or distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH REGARD
   TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
   FITNESS. IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
   OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
   USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
   TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
   OF THIS SOFTWARE.

   -----------------------------------------------------------------------------

   To release a closed-source product which uses other parts of JUCE not
   licensed under the ISC terms, commercial licenses are available: visit
   www.juce.com for more information.

  ==============================================================================
*/

#include <poll.h>

enum FdType {
    INTERNAL_QUEUE_FD,
    WINDOW_SYSTEM_FD,
    FD_COUNT,
};

//==============================================================================
class InternalMessageQueue
{
public:
    InternalMessageQueue()
        : fdCount (1),
          loopCount (0),
          bytesInSocket (0)
    {
        int ret = ::socketpair (AF_LOCAL, SOCK_STREAM, 0, fd);
        ignoreUnused (ret); jassert (ret == 0);

        auto internalQueueCb = [this] (int _fd)
        {
            if (const MessageManager::MessageBase::Ptr msg = this->popNextMessage (_fd))
            {
                JUCE_TRY
                {
                    msg->messageCallback();
                    return true;
                }
                JUCE_CATCH_EXCEPTION
            }
            return false;
        };

        pfds[INTERNAL_QUEUE_FD].fd = getReadHandle();
        pfds[INTERNAL_QUEUE_FD].events = POLLIN;
        readCallback[INTERNAL_QUEUE_FD] = new LinuxEventLoop::CallbackFunction<decltype(internalQueueCb)> (internalQueueCb);
    }

    ~InternalMessageQueue()
    {
        close (getReadHandle());
        close (getWriteHandle());

        clearSingletonInstance();
    }

    //==============================================================================
    void postMessage (MessageManager::MessageBase* const msg) noexcept
    {
        ScopedLock sl (lock);
        queue.add (msg);

        const int maxBytesInSocketQueue = 128;

        if (bytesInSocket < maxBytesInSocketQueue)
        {
            bytesInSocket++;

            ScopedUnlock ul (lock);
            const unsigned char x = 0xff;
            ssize_t bytesWritten = write (getWriteHandle(), &x, 1);
            ignoreUnused (bytesWritten);
        }
    }

    void setWindowSystemFd (int _fd, LinuxEventLoop::CallbackFunctionBase* _readCallback)
    {
        jassert (fdCount == 1);

        ScopedLock sl (lock);

        fdCount = 2;
        pfds[WINDOW_SYSTEM_FD].fd = _fd;
        pfds[WINDOW_SYSTEM_FD].events = POLLIN;
        readCallback[WINDOW_SYSTEM_FD] = _readCallback;
        readCallback[WINDOW_SYSTEM_FD]->active = true;
    }

    void removeWindowSystemFd ()
    {
        jassert (fdCount == FD_COUNT);

        ScopedLock sl (lock);

        fdCount = 1;
        readCallback[WINDOW_SYSTEM_FD]->active = false;
    }

    bool dispatchNextEvent() noexcept
    {
        for (int counter = 0; counter < fdCount; counter++)
        {
            const int i = loopCount++;
            loopCount %= fdCount;
            if (readCallback[i] != nullptr && readCallback[i]->active)
            {
                if ((*readCallback[i]) (pfds[i].fd))
                    return true;
            }
        }

        return false;
    }

    bool sleepUntilEvent (const int timeoutMs)
    {
        const int pnum = poll (pfds, static_cast<nfds_t> (fdCount), timeoutMs);
        return (pnum > 0);
    }

    //==============================================================================
    juce_DeclareSingleton_SingleThreaded_Minimal (InternalMessageQueue)

private:
    CriticalSection lock;
    ReferenceCountedArray <MessageManager::MessageBase> queue;
    int fd[2];
    pollfd pfds[FD_COUNT];
    ScopedPointer<LinuxEventLoop::CallbackFunctionBase> readCallback[FD_COUNT];
    int fdCount;
    int loopCount;
    int bytesInSocket;

    int getWriteHandle() const noexcept     { return fd[0]; }
    int getReadHandle() const noexcept      { return fd[1]; }

    MessageManager::MessageBase::Ptr popNextMessage (int _fd) noexcept
    {
        const ScopedLock sl (lock);

        if (bytesInSocket > 0)
        {
            --bytesInSocket;

            const ScopedUnlock ul (lock);
            unsigned char x;
            ssize_t numBytes = read (_fd, &x, 1);
            ignoreUnused (numBytes);
        }

        return queue.removeAndReturn (0);
    }
};

juce_ImplementSingleton_SingleThreaded (InternalMessageQueue)


//==============================================================================
namespace LinuxErrorHandling
{
    static bool keyboardBreakOccurred = false;

    //==============================================================================
    void keyboardBreakSignalHandler (int sig)
    {
        if (sig == SIGINT)
            keyboardBreakOccurred = true;
    }

    void installKeyboardBreakHandler()
    {
        struct sigaction saction;
        sigset_t maskSet;
        sigemptyset (&maskSet);
        saction.sa_handler = keyboardBreakSignalHandler;
        saction.sa_mask = maskSet;
        saction.sa_flags = 0;
        sigaction (SIGINT, &saction, 0);
    }
}

//==============================================================================
void MessageManager::doPlatformSpecificInitialisation()
{
    if (JUCEApplicationBase::isStandaloneApp())
    {
        LinuxErrorHandling::installKeyboardBreakHandler();
    }

    // Create the internal message queue
    InternalMessageQueue* queue = InternalMessageQueue::getInstance();
    ignoreUnused (queue);
}

void MessageManager::doPlatformSpecificShutdown()
{
    InternalMessageQueue::deleteInstance();
}

bool MessageManager::postMessageToSystemQueue (MessageManager::MessageBase* const message)
{
    if (InternalMessageQueue* queue = InternalMessageQueue::getInstanceWithoutCreating())
    {
        queue->postMessage (message);
        return true;
    }

    return false;
}

void MessageManager::broadcastMessage (const String& /* value */)
{
    /* TODO */
}

// this function expects that it will NEVER be called simultaneously for two concurrent threads
bool MessageManager::dispatchNextMessageOnSystemQueue (bool returnIfNoPendingMessages)
{
    for (;;)
    {
        if (LinuxErrorHandling::keyboardBreakOccurred)
            JUCEApplicationBase::getInstance()->quit();

        if (InternalMessageQueue* queue = InternalMessageQueue::getInstanceWithoutCreating())
        {
            if (queue->dispatchNextEvent())
                break;

            else if (returnIfNoPendingMessages)
                return false;

            // wait for 2000ms for next events if necessary
            queue->sleepUntilEvent (2000);
        }
    }

    return true;
}

//==============================================================================


void LinuxEventLoop::setWindowSystemFdInternal (int fd, LinuxEventLoop::CallbackFunctionBase* readCallback) noexcept
{
    if (InternalMessageQueue* queue = InternalMessageQueue::getInstanceWithoutCreating())
        queue->setWindowSystemFd (fd, readCallback);
}

void LinuxEventLoop::removeWindowSystemFd() noexcept
{
    if (InternalMessageQueue* queue = InternalMessageQueue::getInstanceWithoutCreating())
        queue->removeWindowSystemFd();
}
