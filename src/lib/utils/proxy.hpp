/*
 *  Copyright (c) 2025, The OpenThread Authors.
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

#ifndef OT_LIB_UTILS_PROXY_HPP_
#define OT_LIB_UTILS_PROXY_HPP_

#include <stdint.h>

#include <utility>

namespace ot {

struct ProxyBase
{
    template <typename Target>
    explicit ProxyBase(Target &aTarget)
        : mContext(&aTarget)
    {
    }
    ProxyBase() = default;
    bool                            IsProxied() const { return mContext != nullptr; }
    template <typename Target> bool IsProxiedTo(const Target &aTarget) const { return mContext == &aTarget; }
    void                           *mContext;
};

template <int N> struct ProxyLocator : public ProxyLocator<N - 1>
{
};

template <> struct ProxyLocator<0>
{
};

} // namespace ot

#define CurrentProxyType decltype(CurrentProxy(ot::ProxyLocator<__LINE__>{}))

#define OT_PROXY_CLASS_BEGIN(ProxyName) \
    struct ProxyName##Wrapper           \
    {                                   \
        static constexpr ot::ProxyBase CurrentProxy(ot::ProxyLocator<__LINE__>)

#define OT_PROXY_CLASS_METHOD(ReturnType, MethodName, ...)                                                             \
    template <typename... Args> struct MethodName##Wrapper : public CurrentProxyType                                   \
    {                                                                                                                  \
        template <typename Target>                                                                                     \
        explicit MethodName##Wrapper(Target &aTarget)                                                                  \
            : CurrentProxyType(aTarget)                                                                                \
            , m##MethodName(&MethodName##Wrapper::Static##MethodName<Target>)                                          \
        {                                                                                                              \
        }                                                                                                              \
        MethodName##Wrapper() = default;                                                                               \
                                                                                                                       \
        auto MethodName(Args &&...args) -> ReturnType { return m##MethodName(mContext, std::forward<Args>(args)...); } \
                                                                                                                       \
        template <typename Target> static auto Static##MethodName(void *aContext, Args &&...args) -> ReturnType        \
        {                                                                                                              \
            return static_cast<Target *>(aContext)->MethodName(std::forward<Args>(args)...);                           \
        }                                                                                                              \
        ReturnType (*m##MethodName)(void *aContext, Args &&...);                                                       \
    };                                                                                                                 \
    static constexpr MethodName##Wrapper<__VA_ARGS__> CurrentProxy(ot::ProxyLocator<__LINE__ + 1>)

#define OT_PROXY_CLASS_END(ProxyName)        \
    using FinalProxyType = CurrentProxyType; \
    }                                        \
    ;                                        \
    using ProxyName = ProxyName##Wrapper::FinalProxyType

#endif // OT_LIB_UTILS_PROXY_HPP_
