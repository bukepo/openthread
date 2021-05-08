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

/**
 * @file
 *   This file includes definitions for the SPI interface to radio (RCP).
 */

#ifndef OT_POSIX_PLATFORM_MAINLOOP_HPP_
#define OT_POSIX_PLATFORM_MAINLOOP_HPP_

#include <openthread/openthread-system.h>

namespace ot {
namespace Posix {
namespace Mainloop {

class Source
{
    friend class Manager;

    Source *mNext;

public:
    /**
     * This function updates the file descriptor sets with file descriptors.
     *
     * @param[inout]  aReadFdSet    A pointer to the read file descriptors.
     * @param[inout]  aWriteFdSet   A pointer to the write file descriptors.
     * @param[inout]  aErrorFdSet   A pointer to the error file descriptors.
     * @param[inout]  aMaxFd        A pointer to the max file descriptor.
     *
     */
    virtual void Update(otSysMainloopContext *aContext) = 0;

    /**
     * This function performs platform netif processing.
     *
     * @param[in]   aReadFdSet      A pointer to the read file descriptors.
     * @param[in]   aWriteFdSet     A pointer to the write file descriptors.
     * @param[in]   aErrorFdSet     A pointer to the error file descriptors.
     *
     */
    virtual void Process(const otSysMainloopContext *aContext) = 0;
};

class Manager
{
    Source *mSources;

public:
    /**
     * This method updates event polls in the mainloop context.
     *
     * @param[inout]    aMainloop   A reference to the mainloop context.
     *
     */
    void Update(otSysMainloopContext &aContext);

    /**
     * This method processes events in the mainloop context.
     *
     * @param[in]   aMainloop   A reference to the mainloop context.
     *
     */
    void Process(const otSysMainloopContext &aContext);

    void Add(Source &aSource);
    void Remove(Source &aSource);

    static Manager &Get(void);
};

} // namespace Mainloop
} // namespace Posix
} // namespace ot
#endif // OT_POSIX_PLATFORM_MAINLOOP_HPP_
