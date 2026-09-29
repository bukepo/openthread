/*
 *  Copyright (c) 2026, The OpenThread Authors.
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
 *   This file includes definitions for the Link Extension class.
 */

#ifndef OT_CORE_MAC_LINK_EXT_HPP_
#define OT_CORE_MAC_LINK_EXT_HPP_

#include "openthread-core-config.h"

#if OPENTHREAD_RADIO && OPENTHREAD_CONFIG_LINK_EXT_ENABLE

#include "common/callback.hpp"
#include "common/locator.hpp"
#include "common/non_copyable.hpp"
#include "common/timer.hpp"
#include "mac/channel_mask.hpp"
#include "mac/mac_frame.hpp"
#include "mac/sub_mac.hpp"
#include "radio/radio.hpp"

#if !OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE
#error "OPENTHREAD_CONFIG_LINK_EXT_ENABLE requires OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE"
#endif

#if !OT_CONFIG_MAC_TARGET_TIME_TX_ENABLE
#error "OPENTHREAD_CONFIG_LINK_EXT_ENABLE requires target time TX support (e.g. CSL transmitter or software TX timing)"
#endif

namespace ot {
namespace Mac {

class LinkRaw;

/**
 * Implements link-layer extensions built on top of the raw link-layer (`LinkRaw`), such as burst transmission.
 */
class LinkExt : public InstanceLocator, private NonCopyable
{
    friend class LinkRaw;

public:
    /**
     * Initializes the object.
     *
     * @param[in]  aInstance  A reference to the OpenThread instance.
     */
    explicit LinkExt(Instance &aInstance);

    /**
     * Represents the configuration of a burst transmission.
     */
    struct BurstConfig
    {
        uint16_t mCount;       ///< Number of ticks (remaining ticks, including the one in progress, when read).
        uint32_t mPeriodUs;    ///< Tick period in microseconds (0 for back-to-back ticks).
        uint32_t mChannelMask; ///< Channels (bit N for channel N) to transmit on in each tick (0 for frame channel).
    };

    /**
     * Represents the handler called when a burst transmission ends.
     *
     * @param[in]  aContext  The context passed to `TransmitBurst()`.
     */
    typedef void (*BurstDoneHandler)(void *aContext);

    /**
     * Starts a burst transmission of the frame in the transmit buffer of the raw link-layer.
     *
     * The frame is transmitted unchanged in each of `mCount` ticks. In each tick, the frame is transmitted on every
     * channel in `mChannelMask` in ascending order (or on the frame channel if `mChannelMask` is 0), back-to-back.
     * The frame MUST NOT request an ACK and MUST NOT require security processing.
     *
     * When `mPeriodUs` is non-zero, ticks start every `mPeriodUs` on the radio clock, from the frame's target TX time
     * if specified, or from now. A timed tick is handed to `SubMac` `OPENTHREAD_CONFIG_LINK_EXT_BURST_TX_AHEAD_US`
     * before its target TX time. Only the first channel of a tick uses the target TX time; the other channels are
     * sent right after it. A tick that cannot be sent on time is skipped: its start time has passed (e.g. overrun by
     * the previous tick), or, for a timed tick, it is closer than the `SubMac` timed TX lead time. The frame's maximum
     * CSMA backoffs and maximum frame retries SHOULD be 0 for a timed burst, since any CSMA-CA backoff or
     * retransmission would be late.
     *
     * Between ticks, the radio is in the state last requested by a raw link-layer receive or sleep request (see
     * `ShouldDeferIdleState()`), which does not end the burst. The frame's RX channel after TX done is ignored.
     *
     * The transmit buffer MUST NOT be modified, and raw link-layer transmit and energy scan requests fail with
     * `kErrorInvalidState`, until the burst ends. The burst ends after its last tick, when stopped by `StopBurst()`,
     * or when the raw link-layer is disabled. @p aHandler is then called, except when `StopBurst()` ends the burst
     * right away.
     *
     * @param[in]  aConfig    The burst configuration.
     * @param[in]  aHandler   The handler called when the burst ends.
     * @param[in]  aContext   An arbitrary context passed to @p aHandler.
     *
     * @retval kErrorNone          Successfully started the burst.
     * @retval kErrorInvalidArgs   The configuration or the transmit frame is invalid for a burst, or all its ticks
     *                             are already in the past.
     * @retval kErrorInvalidState  The raw link-layer isn't enabled.
     * @retval kErrorBusy          A burst transmission, a transmission requested by `LinkRaw::Transmit()`, or an
     *                             energy scan is ongoing.
     */
    Error TransmitBurst(const BurstConfig &aConfig, BurstDoneHandler aHandler, void *aContext);

    /**
     * Stops the ongoing burst transmission.
     *
     * If no burst frame is with the radio, the burst ends right away, and the handler of `TransmitBurst()` is not
     * called (`GetBurstConfig()` reads `mCount` as 0). Otherwise, the frame being transmitted cannot be taken back,
     * and the burst ends when the radio is done with it, which is reported by the handler (until then,
     * `GetBurstConfig()` reads `mCount` as 1).
     *
     * @retval kErrorNone          The burst is stopped or will stop when the frame being transmitted is done, or no
     *                             burst is ongoing.
     * @retval kErrorInvalidState  The raw link-layer isn't enabled.
     */
    Error StopBurst(void);

    /**
     * Gets the configuration of the ongoing burst transmission.
     *
     * `mCount` is the number of remaining ticks, including the one in progress. All fields are zero if no burst is
     * ongoing.
     *
     * @param[out] aConfig  A reference to return the configuration.
     */
    void GetBurstConfig(BurstConfig &aConfig) const;

    /**
     * Indicates whether a burst transmission is ongoing.
     *
     * @retval TRUE   A burst transmission is ongoing.
     * @retval FALSE  No burst transmission is ongoing.
     */
    bool IsBurstActive(void) const { return mBurstCount > 0; }

    /**
     * Indicates whether a raw link-layer receive or sleep request should be deferred until the burst tick is done.
     *
     * Called on a raw link-layer receive or sleep request. If the request is deferred, it is recorded as the radio
     * state for the time between burst ticks.
     *
     * @param[in]  aIsReceiving  TRUE for a receive request, FALSE for a sleep request.
     *
     * @retval TRUE   A burst frame is with `SubMac`, which would drop it, so the request is applied once the tick is
     *                done instead.
     * @retval FALSE  The request can be applied right away. `HandleIdleStateApplied()` MUST be called if it succeeds.
     */
    bool ShouldDeferIdleState(bool aIsReceiving);

    /**
     * Records the radio state for the time between burst ticks after a raw link-layer receive or sleep request is
     * successfully applied.
     *
     * @param[in]  aIsReceiving  TRUE for a receive request, FALSE for a sleep request.
     */
    void HandleIdleStateApplied(bool aIsReceiving) { mIsReceiving = aIsReceiving; }

private:
    static constexpr uint32_t kBurstTxAheadUs = OPENTHREAD_CONFIG_LINK_EXT_BURST_TX_AHEAD_US;

#if OT_CONFIG_MAC_TARGET_TIME_TX_ENABLE
    // A timed tick handed to `SubMac` later than its TX start time would be sent late.
    static_assert(kBurstTxAheadUs >= SubMac::kTimedTxLeadTime,
                  "OPENTHREAD_CONFIG_LINK_EXT_BURST_TX_AHEAD_US is less than the SubMac timed TX lead time");

    // `SubMac` starts a timed transmission this long before its target TX time.
    static constexpr uint32_t kBurstTxLeadUs = SubMac::kTimedTxLeadTime;
#else
    static constexpr uint32_t kBurstTxLeadUs = 0;
#endif

    // Called by `LinkRaw`.
    void Reset(void);
    void HandleLinkRawEnableChange(void);
    void HandleTransmitDone(TxFrame &aFrame);

    void  CancelBurst(void);
    void  FinishBurst(void);
    void  ApplyIdleState(void);
    Error StartBurst(const BurstConfig &aConfig);
    bool  IsSlotMissed(Radio::Time32 aNow) const;
    void  SkipMissedBurstSlots(Radio::Time32 aNow);
    void  StartBurstTimer(Radio::Time32 aNow);
    void  ScheduleNextBurstTick(void);
    void  HandleBurstTimer(void);
    void  SendBurstFrame(void);

    using BurstTimer = TimerMicroIn<LinkExt, &LinkExt::HandleBurstTimer>;

    Callback<BurstDoneHandler> mBurstDoneCallback;
    BurstTimer                 mBurstTimer;
    ChannelMask                mBurstChannelMask;
    Radio::Time32              mBurstTxTime; // Slot start time (radio clock) of the current tick.
    uint32_t                   mBurstPeriodUs;
    uint16_t                   mBurstCount; // Remaining ticks, including the current one.
    bool                       mIsTimedBurst;
    bool                       mIsBurstTxPending;     // A burst frame is with `SubMac`.
    bool                       mIsBurstStopRequested; // Stop is requested while a burst frame is with the radio.
    bool                       mIsReceiving;         // Idle state between burst ticks: receive (true) or sleep (false).
    bool                       mIsSendingBurstFrame; // `SendBurstFrame()` is running.
    bool                       mIsBurstFrameToSend;  // `SendBurstFrame()` was called while running.
};

} // namespace Mac
} // namespace ot

#endif // OPENTHREAD_RADIO && OPENTHREAD_CONFIG_LINK_EXT_ENABLE

#endif // OT_CORE_MAC_LINK_EXT_HPP_
