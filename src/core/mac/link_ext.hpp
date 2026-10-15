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
 * Implements link-layer extensions built on top of the raw link-layer (`LinkRaw`), such as burst transmission and
 * periodic receive schedules.
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

#if (OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_MAX_SCHEDULES > 0)
    static constexpr uint8_t kMaxPeriodicRxSchedules =
        OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_MAX_SCHEDULES; ///< Max number of periodic RX schedules.

    /**
     * Represents a periodic receive schedule.
     *
     * A receive window starts at every radio time `T` (see `otPlatRadioGetNow()`) that satisfies
     * `T % mPeriodUs == mPhaseUs` and lasts `mDurationUs` microseconds.
     */
    struct PeriodicRxSchedule
    {
        uint32_t mPhaseUs;    ///< Phase of the window start within the period (in usec), less than `mPeriodUs`.
        uint32_t mPeriodUs;   ///< Period (in usec), non-zero and not more than `Timer::kMaxDelay` (2^31 - 1).
        uint16_t mDurationUs; ///< Window duration (in usec), non-zero and not more than `mPeriodUs`.
        uint8_t  mChannel;    ///< Channel to listen on, in the radio's supported channel mask.
    };

    /**
     * Atomically replaces the periodic RX schedules.
     *
     * While the radio is in the Sleep state, receive windows are opened according to the schedules using
     * `otPlatRadioReceiveAt()`. Receive, transmit and energy scan take precedence, and periodic RX resumes when the
     * radio returns to Sleep. Array order defines priority (index 0 has highest priority). Overlapping windows are
     * arbitrated as described in "Periodic RX arbitration" below. Passing zero schedules disables periodic RX. A
     * window already scheduled on the radio may still occur once after the schedules are changed.
     *
     * @param[in] aSchedules     A pointer to an array of schedules (can be `nullptr` if @p aNumSchedules is zero).
     * @param[in] aNumSchedules  The number of schedules in @p aSchedules.
     *
     * @retval kErrorNone            Successfully updated the schedules.
     * @retval kErrorNotImplemented  The radio doesn't support receive timing (`OT_RADIO_CAPS_RECEIVE_TIMING`), and
     *                               @p aNumSchedules is non-zero. The previous schedules are left unchanged.
     * @retval kErrorNoBufs          More than `kMaxPeriodicRxSchedules` schedules. The previous schedules are
     * unchanged.
     * @retval kErrorInvalidArgs     A schedule is invalid. The previous schedules are left unchanged.
     */
    Error SetPeriodicRxSchedules(const PeriodicRxSchedule *aSchedules, uint8_t aNumSchedules);

    /**
     * Gets the current periodic RX schedules.
     *
     * @param[out] aNumSchedules  A reference to output the number of schedules.
     *
     * @returns A pointer to the array of current schedules.
     */
    const PeriodicRxSchedule *GetPeriodicRxSchedules(uint8_t &aNumSchedules) const
    {
        aNumSchedules = mNumPeriodicRxSchedules;
        return mPeriodicRxSchedules;
    }

    /**
     * Determines the periodic RX window to schedule now.
     *
     * Called by `SubMac::ListenScheduler` whenever it evaluates its listen sources. The returned window is passed
     * to `SubMac::ReceiveAt()`, and the scheduler evaluates again at the window start time.
     *
     * @param[in]  aNow      The current radio time.
     * @param[out] aTimedRx  A reference to output the window.
     *
     * @retval TRUE   A window is provided in @p aTimedRx.
     * @retval FALSE  There is no window to schedule.
     */
    bool DeterminePeriodicRxWindow(const Radio::SyncedTime &aNow, SubMac::TimedRx &aTimedRx);
#endif

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

#if (OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_MAX_SCHEDULES > 0)
    /*
     * Periodic RX arbitration
     *
     * The periodic RX schedules are arbitrated here, and the resulting windows are passed one at a time to
     * `SubMac::ListenScheduler` as a listen source (see `DeterminePeriodicRxWindow()`). Against other listen sources
     * of `SubMac` (e.g. CSL receiver, which is not expected to be enabled on an RCP), the listen scheduler's own rule
     * applies (the earliest window that has not ended is scheduled).
     *
     * Rules among the periodic RX schedules (array order defines priority, index 0 has highest priority):
     *
     * 1. Owner: At any instant, the radio is owned by the highest-priority schedule whose window covers that instant.
     *    If no window covers the instant, there is no periodic RX window.
     *
     * 2. Truncation, not skipping: A lower-priority window overlapping a higher-priority window on a different
     *    channel is not dropped as a whole. It keeps the parts not covered by the higher-priority window, which can
     *    be a part before and/or a part after it.
     *
     * 3. Merging: Windows are formed from the owner channel, not the owner. A maximal interval during which the owner
     *    channel stays the same is one window, so overlapping or back-to-back windows on the same channel are merged
     *    regardless of their priorities. A merged window is capped at the largest schedule period. To bound the
     *    computation, a window is also ended after evaluating `kPeriodicRxMaxMergeEvents` events, so a longer
     *    continuous interval on the same channel is split into back-to-back windows.
     *
     * 4. Channel switch guard: When a higher-priority window preempts an ongoing lower-priority window on a different
     *    channel, the higher-priority schedule also owns the `kPeriodicRxGuard` interval right before its window
     *    start, so that the radio is listening on the new channel no later than the window start. The guard time is
     *    taken from the lower-priority window. Whether the guard is taken is determined at the guard start: if a
     *    window on a different channel is active then, the whole guard is owned, even if that window ends within the
     *    guard. No guard is added when the higher-priority window starts while no other window is active at the guard
     *    start (even if a lower-priority window starts within the guard), or when the radio returns to a
     *    lower-priority window after a higher-priority window ends.
     *
     * 5. Minimum fragment: A truncated window (shorter than the full window duration of its owner) which is shorter
     *    than `kPeriodicRxMinFragment` (guard time plus the SHR duration, i.e. the time needed to detect a frame) is
     *    dropped. A dropped fragment does not end the window around it: if the channel after the fragment is the same
     *    as the channel before it, the window continues through it. A window continuing a window on the same channel
     *    (e.g. the remainder of a merged window split by rule 3) is not a fragment and is never dropped.
     *
     * Example: schedule A (higher priority, channel 11) window [5000, 6600), schedule B (lower priority, channel 12)
     * window [4000, 8000), guard `G`:
     *
     *          4000          5000-G 5000          6600          8000
     *   A:                          |=============|
     *   B:       |========================================================|
     *   Result:  |== ch 12 ===|== ch 11 ==========|== ch 12 =====|
     *
     * Scheduling: since a subsequent `ReceiveAt()` replaces a window which has not started yet, the window following
     * the active one is provided only once the active one has started. If the active window changes (e.g. it is
     * truncated after the schedules are updated), it is provided again keeping its start time.
     *
     * 6. Frame in progress: The rules above define when windows start and end. If the radio has started receiving a
     *    frame (e.g. SHR detected) in a window, it completes the frame reception and any ACK transmission before
     *    switching to the channel of the next window or returning to Sleep, even if the frame extends into a
     *    higher-priority window. This is required of the radio platform by `otPlatRadioReceiveAt()`, so it needs no
     *    handling here. A frame failing the address filter is expected to be aborted as soon as the mismatch is
     *    determined, so that it does not delay the next window.
     *
     * Timed RX windows only take effect while the radio is in Sleep state. Other operations (receive, transmit,
     * energy scan) take precedence, see `otPlatRadioReceiveAt()`.
     *
     * Limitation: when the schedules are updated, a window already scheduled on the radio is not cancelled, since
     * `SubMac::ListenScheduler` does not cancel a window scheduled on the radio. Hence the previously scheduled
     * window may still occur once after the schedules are cleared or changed.
     */

    static constexpr uint32_t kPeriodicRxGuard       = OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_CHANNEL_SWITCH_GUARD_US;
    static constexpr uint32_t kPeriodicRxMinFragment = kPeriodicRxGuard + Radio::kHeaderShrDuration;
    static constexpr uint16_t kPeriodicRxMaxMergeEvents = 64;
    static constexpr uint8_t  kPeriodicRxNoOwner        = 0xff;

    bool                 IsValid(const PeriodicRxSchedule &aSchedule) const;
    static Radio::Time64 GetNextStart(const PeriodicRxSchedule &aSchedule, Radio::Time64 aTime);
    static bool          IsCovering(const PeriodicRxSchedule &aSchedule, Radio::Time64 aTime, Radio::Time64 &aEnd);
    static bool          IsInGuard(const PeriodicRxSchedule &aSchedule, Radio::Time64 aTime);
    static Radio::Time64 GetNextEvent(const PeriodicRxSchedule &aSchedule, Radio::Time64 aTime);

    uint8_t       GetPeriodicRxOwner(Radio::Time64 aTime) const;
    bool          IsGuardTaken(uint8_t aIndex, Radio::Time64 aTime) const;
    Radio::Time64 GetNextPeriodicRxEvent(Radio::Time64 aTime) const;
    bool          FindNextPeriodicRxWindow(Radio::Time64 aFrom, SubMac::TimedRx &aWindow) const;
    Radio::Time64 GetPeriodicRxChannelEnd(Radio::Time64 aTime, uint8_t aChannel, uint16_t &aNumEvents) const;
    bool          IsDroppedPeriodicRxFragment(Radio::Time64 aStart, Radio::Time64 aEnd, uint8_t aOwner) const;
    bool          IsPeriodicRxContinuation(Radio::Time64 aStart, uint8_t aChannel) const;
#endif

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

#if (OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_MAX_SCHEDULES > 0)
    uint8_t            mNumPeriodicRxSchedules;
    PeriodicRxSchedule mPeriodicRxSchedules[kMaxPeriodicRxSchedules];
    SubMac::TimedRx    mPeriodicRxActiveWindow; // The window on the radio which has started.
    SubMac::TimedRx    mPeriodicRxNextWindow;   // The window provided to `SubMac` which has not started yet.
#endif
};

} // namespace Mac
} // namespace ot

#endif // OPENTHREAD_RADIO && OPENTHREAD_CONFIG_LINK_EXT_ENABLE

#endif // OT_CORE_MAC_LINK_EXT_HPP_
