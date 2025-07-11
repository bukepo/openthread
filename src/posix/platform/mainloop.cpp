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

#include "posix/platform/mainloop.hpp"

#include <assert.h>

#include <openthread/platform/time.h>

#include "common/code_utils.hpp"

namespace ot {
namespace Posix {
namespace Mainloop {

//---------------------------------------------------------------------------------------------------------------------

static void AddFd(int aFd, Context &aContext, fd_set &aFdSet)
{
    VerifyOrExit(aFd >= 0);

    FD_SET(aFd, &aFdSet);
    aContext.mMaxFd = OT_MAX(aContext.mMaxFd, aFd);

exit:
    return;
}

void AddToReadFdSet(int aFd, Context &aContext) { AddFd(aFd, aContext, aContext.mReadFdSet); }
void AddToWriteFdSet(int aFd, Context &aContext) { AddFd(aFd, aContext, aContext.mWriteFdSet); }
void AddToErrorFdSet(int aFd, Context &aContext) { AddFd(aFd, aContext, aContext.mErrorFdSet); }

uint64_t GetTimeout(const Context &aContext)
{
    return static_cast<uint64_t>(aContext.mTimeout.tv_sec) * OT_US_PER_S +
           static_cast<uint64_t>(aContext.mTimeout.tv_usec);
}

void SetTimeoutIfEarlier(uint64_t aTimeout, Context &aContext)
{
    VerifyOrExit(aTimeout < GetTimeout(aContext));

    aContext.mTimeout.tv_sec  = static_cast<time_t>(aTimeout / OT_US_PER_S);
    aContext.mTimeout.tv_usec = static_cast<suseconds_t>(aTimeout % OT_US_PER_S);

exit:
    return;
}

//---------------------------------------------------------------------------------------------------------------------
// Manager

void Manager::Add(Source &aSource)
{
    assert(aSource.mNext == nullptr);

    aSource.mNext = mSources;
    mSources      = &aSource;
}

void Manager::Remove(Source &aSource)
{
    for (Source **pnext = &mSources; *pnext != nullptr; pnext = &(*pnext)->mNext)
    {
        if (*pnext == &aSource)
        {
            *pnext = aSource.mNext;
            break;
        }
    }

    aSource.mNext = nullptr;
}

void Manager::Update(Context &aContext)
{
    for (Source *source = mSources; source != nullptr; source = source->mNext)
    {
        source->Update(aContext);
    }
}

void Manager::Process(const Context &aContext)
{
    for (Source *source = mSources; source != nullptr; source = source->mNext)
    {
        source->Process(aContext);
    }
}

Manager &Manager::Get(void)
{
    static Manager sInstance;

    return sInstance;
}

} // namespace Mainloop
} // namespace Posix
} // namespace ot
