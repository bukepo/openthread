/*
 *  Copyright (c) 2021, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include "openthread-posix-config.h"
#include "platform-posix.h"

#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <openthread/cli.h>

#include "cli/cli_config.h"
#include "common/code_utils.hpp"

#if OPENTHREAD_POSIX_CONFIG_DAEMON_ENABLE

#define OPENTHREAD_POSIX_DAEMON_SOCKET_LOCK OPENTHREAD_POSIX_CONFIG_DAEMON_SOCKET_BASENAME ".lock"

#include "posix/platform/mainloop.hpp"

namespace ot {
namespace Posix {

namespace {
class Daemon : public Mainloop::Source
{
    static_assert(sizeof(OPENTHREAD_POSIX_DAEMON_SOCKET_NAME) < sizeof(sockaddr_un::sun_path),
                  "OpenThread daemon socket name too long!");

    int mListenSocket  = -1;
    int mDaemonLock    = -1;
    int mSessionSocket = -1;

    int        OutputFormatV(const char *aFormat, va_list aArguments);
    static int OutputFormatV(void *aContext, const char *aFormat, va_list aArguments)
    {
        return static_cast<Daemon *>(aContext)->OutputFormatV(aFormat, aArguments);
    }
    void InitializeSessionSocket(void);
    void Process(const otSysMainloopContext *aContext) override;
    void Update(otSysMainloopContext *aContext) override;

public:
    static Daemon &Get(void)
    {
        static Daemon sInstance;

        return sInstance;
    }

    void Enable(otInstance *aInstance);
    void Disable(void);
};

int Daemon::OutputFormatV(const char *aFormat, va_list aArguments)
{
    char buf[OPENTHREAD_CONFIG_CLI_MAX_LINE_LENGTH + 1];
    int  rval;

    buf[OPENTHREAD_CONFIG_CLI_MAX_LINE_LENGTH] = '\0';

    rval = vsnprintf(buf, sizeof(buf) - 1, aFormat, aArguments);

    VerifyOrExit(rval >= 0, otLogWarnPlat("Failed to format CLI output: %s", strerror(errno)));

    VerifyOrExit(mSessionSocket != -1, otLogDebgPlat("%s", buf));

#if defined(__linux__)
    // Don't die on SIGPIPE
    rval = send(mSessionSocket, buf, static_cast<size_t>(rval), MSG_NOSIGNAL);
#else
    rval = static_cast<int>(write(mSessionSocket, buf, static_cast<size_t>(rval)));
#endif

    if (rval < 0)
    {
        otLogWarnPlat("Failed to write CLI output: %s", strerror(errno));
        close(mSessionSocket);
        mSessionSocket = -1;
    }

exit:
    return rval;
}

void Daemon::InitializeSessionSocket(void)
{
    int newSessionSocket;
    int rval;

    VerifyOrExit((newSessionSocket = accept(mListenSocket, nullptr, nullptr)) != -1, rval = -1);

    VerifyOrExit((rval = fcntl(newSessionSocket, F_GETFD, 0)) != -1);

    rval |= FD_CLOEXEC;

    VerifyOrExit((rval = fcntl(newSessionSocket, F_SETFD, rval)) != -1);

#ifndef __linux__
    // some platforms (macOS, Solaris) don't have MSG_NOSIGNAL
    // SOME of those (macOS, but NOT Solaris) support SO_NOSIGPIPE
    // if we have SO_NOSIGPIPE, then set it. Otherwise, we're going
    // to simply ignore it.
#if defined(SO_NOSIGPIPE)
    rval = setsockopt(newSessionSocket, SOL_SOCKET, SO_NOSIGPIPE, &rval, sizeof(rval));
    VerifyOrExit(rval != -1);
#else
#warning "no support for MSG_NOSIGNAL or SO_NOSIGPIPE"
#endif
#endif // __linux__

    if (mSessionSocket != -1)
    {
        close(mSessionSocket);
    }
    mSessionSocket = newSessionSocket;

exit:
    if (rval == -1)
    {
        otLogWarnPlat("Failed to initialize session socket: %s", strerror(errno));
        if (newSessionSocket != -1)
        {
            close(newSessionSocket);
        }
    }
    else
    {
        otLogInfoPlat("Session socket is ready", strerror(errno));
    }
}

void Daemon::Enable(otInstance *aInstance)
{
    struct sockaddr_un sockname;
    int                ret;

    // This allows implementing pseudo reset.
    VerifyOrExit(mListenSocket == -1);

    mListenSocket = SocketWithCloseExec(AF_UNIX, SOCK_STREAM, 0, kSocketNonBlock);

    if (mListenSocket == -1)
    {
        DieNow(OT_EXIT_FAILURE);
    }

    mDaemonLock = open(OPENTHREAD_POSIX_DAEMON_SOCKET_LOCK, O_CREAT | O_RDONLY | O_CLOEXEC, 0600);

    if (mDaemonLock == -1)
    {
        DieNowWithMessage("open", OT_EXIT_ERROR_ERRNO);
    }

    if (flock(mDaemonLock, LOCK_EX | LOCK_NB) == -1)
    {
        DieNowWithMessage("flock", OT_EXIT_ERROR_ERRNO);
    }

    memset(&sockname, 0, sizeof(struct sockaddr_un));

    (void)unlink(OPENTHREAD_POSIX_DAEMON_SOCKET_NAME);

    sockname.sun_family = AF_UNIX;
    strncpy(sockname.sun_path, OPENTHREAD_POSIX_DAEMON_SOCKET_NAME, sizeof(sockname.sun_path) - 1);

    ret = bind(mListenSocket, (const struct sockaddr *)&sockname, sizeof(struct sockaddr_un));

    if (ret == -1)
    {
        DieNowWithMessage("bind", OT_EXIT_ERROR_ERRNO);
    }

    //
    // only accept 1 connection.
    //
    ret = listen(mListenSocket, 1);
    if (ret == -1)
    {
        DieNowWithMessage("listen", OT_EXIT_ERROR_ERRNO);
    }

    otCliInit(aInstance, OutputFormatV, this);

    Mainloop::Manager::Get().Add(*this);

exit:
    return;
}

void Daemon::Disable(void)
{
    Mainloop::Manager::Get().Remove(*this);

    if (mSessionSocket != -1)
    {
        close(mSessionSocket);
        mSessionSocket = -1;
    }

    if (mListenSocket != -1)
    {
        close(mListenSocket);
        mListenSocket = -1;
    }

    if (gPlatResetReason != OT_PLAT_RESET_REASON_SOFTWARE)
    {
        otLogDebgPlat("Removing daemon socket: %s", OPENTHREAD_POSIX_DAEMON_SOCKET_NAME);
        (void)unlink(OPENTHREAD_POSIX_DAEMON_SOCKET_NAME);
    }

    if (mDaemonLock != -1)
    {
        (void)flock(mDaemonLock, LOCK_UN);
        close(mDaemonLock);
        mDaemonLock = -1;
    }
}

void Daemon::Update(otSysMainloopContext *aContext)
{
    if (mListenSocket != -1)
    {
        FD_SET(mListenSocket, &aContext->mReadFdSet);
        FD_SET(mListenSocket, &aContext->mErrorFdSet);

        if (aContext->mMaxFd < mListenSocket)
        {
            aContext->mMaxFd = mListenSocket;
        }
    }

    if (mSessionSocket != -1)
    {
        FD_SET(mSessionSocket, &aContext->mReadFdSet);
        FD_SET(mSessionSocket, &aContext->mErrorFdSet);

        if (aContext->mMaxFd < mSessionSocket)
        {
            aContext->mMaxFd = mSessionSocket;
        }
    }

    return;
}

void Daemon::Process(const otSysMainloopContext *aContext)
{
    ssize_t rval;

    VerifyOrExit(mListenSocket != -1);

    if (FD_ISSET(mListenSocket, &aContext->mErrorFdSet))
    {
        DieNowWithMessage("daemon socket error", OT_EXIT_FAILURE);
    }
    else if (FD_ISSET(mListenSocket, &aContext->mReadFdSet))
    {
        InitializeSessionSocket();
    }

    VerifyOrExit(mSessionSocket != -1);

    if (FD_ISSET(mSessionSocket, &aContext->mErrorFdSet))
    {
        close(mSessionSocket);
        mSessionSocket = -1;
    }
    else if (FD_ISSET(mSessionSocket, &aContext->mReadFdSet))
    {
        uint8_t buffer[OPENTHREAD_CONFIG_CLI_MAX_LINE_LENGTH];

        rval = read(mSessionSocket, buffer, sizeof(buffer));

        if (rval > 0)
        {
            buffer[rval] = '\0';
            otLogInfoPlat("> %s", reinterpret_cast<const char *>(buffer));
            otCliInputLine(reinterpret_cast<char *>(buffer));
            otCliOutputFormat("> ");
        }
        else
        {
            if (rval < 0)
            {
                otLogWarnPlat("Daemon read: %s", strerror(errno));
            }
            close(mSessionSocket);
            mSessionSocket = -1;
        }
    }

exit:
    return;
}
} // namespace

extern "C" void platformDaemonEnable(otInstance *aInstance)
{
    Daemon::Get().Enable(aInstance);
}

extern "C" void platformDaemonDisable(void)
{
    Daemon::Get().Disable();
}

} // namespace Posix
} // namespace ot
#endif // OPENTHREAD_POSIX_CONFIG_DAEMON_ENABLE

