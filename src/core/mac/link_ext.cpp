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
 *   This file implements the Link Extension class.
 */

#include "link_ext.hpp"

#if OPENTHREAD_RADIO && OPENTHREAD_CONFIG_LINK_EXT_ENABLE

#include "instance/instance.hpp"

namespace ot {
namespace Mac {

RegisterLogModule("LinkExt");

LinkExt::LinkExt(Instance &aInstance)
    : InstanceLocator(aInstance)
    , mBurstTimer(aInstance)
    , mBurstTxTime(0)
    , mBurstPeriodUs(0)
    , mBurstCount(0)
    , mIsTimedBurst(false)
    , mIsBurstTxPending(false)
    , mIsBurstStopRequested(false)
    , mIsReceiving(false)
    , mIsSendingBurstFrame(false)
    , mIsBurstFrameToSend(false)
#if (OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_MAX_SCHEDULES > 0)
    , mNumPeriodicRxSchedules(0)
#endif
    , mMacFilterPolicy(0)
#if (OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES > 0)
    , mNumMacKeyTableEntries(0)
#endif
{
}

void LinkExt::Reset(void)
{
    CancelBurst();
    mBurstDoneCallback.Clear();
    mIsReceiving = false;

#if (OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_MAX_SCHEDULES > 0)
    mNumPeriodicRxSchedules = 0;
    mPeriodicRxActiveWindow.Clear();
    mPeriodicRxNextWindow.Clear();
#endif

    mMacFilterPolicy = 0;
#if (OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES > 0)
    mNumMacKeyTableEntries = 0;
    ClearAllBytes(mMacKeyTable);
#endif
}

void LinkExt::HandleLinkRawEnableChange(void)
{
    if (IsBurstActive())
    {
        FinishBurst();
    }

    mIsReceiving = false;

#if (OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_MAX_SCHEDULES > 0)
    // Disabling the radio leaves no timed RX window scheduled on it. When
    // the radio is enabled, `SubMac` has already scheduled the first
    // window, which must be kept.
    if (!Get<LinkRaw>().IsEnabled())
    {
        mPeriodicRxActiveWindow.Clear();
        mPeriodicRxNextWindow.Clear();
    }
#endif
}

bool LinkExt::ShouldDeferIdleState(bool aIsReceiving)
{
    if (mIsBurstTxPending)
    {
        mIsReceiving = aIsReceiving;
    }

    return mIsBurstTxPending;
}

void LinkExt::CancelBurst(void)
{
    mBurstTimer.Stop();
    mBurstChannelMask.Clear();
    mBurstTxTime          = 0;
    mBurstPeriodUs        = 0;
    mBurstCount           = 0;
    mIsTimedBurst         = false;
    mIsBurstTxPending     = false;
    mIsBurstStopRequested = false;
    mIsBurstFrameToSend   = false;
}

void LinkExt::FinishBurst(void)
{
    CancelBurst();
    mBurstDoneCallback.InvokeAndClearIfSet();
}

void LinkExt::ApplyIdleState(void)
{
    // Puts the radio in the state last requested by the raw link-layer
    // (see `ShouldDeferIdleState()`). Also drops a burst frame waiting in `SubMac`
    // (e.g., for its target TX time).

    SubMac &subMac = Get<SubMac>();

    mIsBurstTxPending = false;

    if (!mIsReceiving || (subMac.Receive(Get<LinkRaw>().GetChannel()) != kErrorNone))
    {
        // Unlike `SubMac::Receive()`, `SubMac::Sleep()` leaves the transmit
        // states even if the radio fails to sleep, so that a dropped burst
        // frame is not sent later.
        IgnoreError(subMac.Sleep());
    }
}

bool LinkExt::IsSlotMissed(Radio::Time32 aNow) const
{
    // A slot is missed if it starts at or before `aNow`. A timed slot is
    // also missed if `SubMac` would start its transmission
    // (`kBurstTxLeadUs` before the slot) at or before `aNow`, which would
    // send it late. `aNow` is compared first, so that adding the lead time
    // cannot wrap around the radio clock comparison window.

    return !Radio::IsTimeStrictlyBefore(aNow, mBurstTxTime) ||
           (mIsTimedBurst && (mBurstTxTime - aNow <= kBurstTxLeadUs));
}

void LinkExt::SkipMissedBurstSlots(Radio::Time32 aNow)
{
    // Skips all periodic slots that are missed (see `IsSlotMissed()`).
    // `mBurstCount` is decremented for every skipped slot. MUST be called
    // only when `mBurstPeriodUs` is non-zero.

    uint32_t missed;

    OT_ASSERT(mBurstPeriodUs != 0);

    VerifyOrExit(IsSlotMissed(aNow));

    missed = (aNow + (mIsTimedBurst ? kBurstTxLeadUs : 0) - mBurstTxTime) / mBurstPeriodUs + 1;
    missed = Min<uint32_t>(missed, mBurstCount);

    mBurstCount -= static_cast<uint16_t>(missed);
    mBurstTxTime += missed * mBurstPeriodUs;

exit:
    return;
}

void LinkExt::StartBurstTimer(Radio::Time32 aNow)
{
    // Arms `mBurstTimer` to start the next tick. A timed tick is handed
    // to `SubMac` `kBurstTxAheadUs` before its target TX time, so that
    // the radio is not held for the tick earlier than needed.

    uint32_t delay = 0;

    if (mIsTimedBurst)
    {
        if (Radio::IsTimeStrictlyBefore(aNow, mBurstTxTime) && (mBurstTxTime - aNow > kBurstTxAheadUs))
        {
            delay = mBurstTxTime - kBurstTxAheadUs - aNow;
        }
    }
    else if (mBurstPeriodUs != 0)
    {
        delay = mBurstTxTime - aNow;
    }

    mBurstTimer.Start(delay);
}

void LinkExt::ScheduleNextBurstTick(void)
{
    // Called when the current tick is done (all channels sent or the
    // tick is skipped). Determines the next tick on the radio clock and
    // arms `mBurstTimer` to start it.

    Radio::Time32 now = Get<Radio::Radio>().GetNowAsTime32();

    mBurstCount--;

    if (mBurstPeriodUs == 0)
    {
        // Back-to-back ticks: only the first tick may be timed.
        mIsTimedBurst = false;
    }
    else
    {
        mBurstTxTime += mBurstPeriodUs;
        SkipMissedBurstSlots(now);
    }

    VerifyOrExit(mBurstCount > 0, FinishBurst());
    StartBurstTimer(now);

exit:
    return;
}

void LinkExt::HandleBurstTimer(void)
{
    TxFrame &frame   = Get<SubMac>().GetTransmitFrame();
    uint8_t  channel = ChannelMask::kChannelIteratorFirst;

    VerifyOrExit(IsBurstActive());
    VerifyOrExit(Get<LinkRaw>().IsEnabled(), FinishBurst());

    if (mIsTimedBurst)
    {
        Radio::Time32 now = Get<Radio::Radio>().GetNowAsTime32();

        if (IsSlotMissed(now))
        {
            // The slot was missed, skip this tick.
            ScheduleNextBurstTick();
            ExitNow();
        }

        frame.SetTargetTxTime(mBurstTxTime, now);
    }
    else
    {
        frame.ClearTargetTxTime();
    }

    if (mBurstChannelMask.GetNextChannel(channel) == kErrorNone)
    {
        frame.mChannel = channel;
    }

    SendBurstFrame();

exit:
    return;
}

void LinkExt::SendBurstFrame(void)
{
    // Every burst transmission is requested through this method. A
    // request rejected by `SubMac` is handled as a completed
    // transmission, so that only this transmission is affected and the
    // burst continues.
    //
    // A transmission may complete within this method (rejected by
    // `SubMac`, or done within `Send()`), and `HandleTransmitDone()` then
    // requests the next channel. Such a nested request is sent by the
    // loop below rather than by recursion, so that the stack depth does
    // not grow with the number of channels.

    TxFrame &frame = Get<SubMac>().GetTransmitFrame();
    Error    error;

    VerifyOrExit(!mIsSendingBurstFrame, mIsBurstFrameToSend = true);

    mIsSendingBurstFrame = true;

    do
    {
        mIsBurstFrameToSend = false;

        // `ApplyIdleState()` puts the radio in its idle state after each
        // tick, so `SubMac` is kept on the TX channel rather than switching
        // the radio to the receive channel in between (e.g., within a
        // sweep).
        frame.SetRxChannelAfterTxDone(frame.GetChannel());

        // Set before `Send()`, since the transmission may complete within it.
        mIsBurstTxPending = true;
        error             = Get<SubMac>().Send();

        // `HandleTransmitDone()` or `CancelBurst()` may have already run
        // within `Send()`.
        if ((error != kErrorNone) && mIsBurstTxPending)
        {
            LogWarnOnError(error, "send burst frame");
            HandleTransmitDone(frame);
        }
    } while (mIsBurstFrameToSend);

    mIsSendingBurstFrame = false;

exit:
    return;
}

void LinkExt::HandleTransmitDone(TxFrame &aFrame)
{
    uint8_t nextChannel = aFrame.GetChannel();

    mIsBurstTxPending = false;

    if (!mIsBurstStopRequested && (mBurstChannelMask.GetNextChannel(nextChannel) == kErrorNone))
    {
        aFrame.ClearTargetTxTime();
        aFrame.mChannel = nextChannel;
        SendBurstFrame();
        ExitNow();
    }

    // The tick is done.
    ApplyIdleState();

    if (mIsBurstStopRequested)
    {
        FinishBurst();
    }
    else
    {
        ScheduleNextBurstTick();
    }

exit:
    return;
}

Error LinkExt::StartBurst(const BurstConfig &aConfig)
{
    // Validates the burst request and starts the burst. The first tick
    // is sent from `HandleBurstTimer()`, like all subsequent ticks.

    Error              error = kErrorNone;
    TxFrame           &frame = Get<SubMac>().GetTransmitFrame();
    TxFrame::ParseInfo frameInfo;
    Radio::Time32      now;

    VerifyOrExit(aConfig.mCount > 0, error = kErrorInvalidArgs);

    // Radio clock times compare modulo 2^32, and the burst timer accepts
    // delays up to `Timer::kMaxDelay`. A timed tick may end up to
    // `kBurstTxAheadUs` before its slot, so the next slot is at most
    // `mPeriodUs + kBurstTxAheadUs` ahead.
    VerifyOrExit(aConfig.mPeriodUs <= Timer::kMaxDelay - kBurstTxAheadUs, error = kErrorInvalidArgs);

    // Burst frames are sent repeatedly as-is, so they must not request
    // an ACK and must not require any further processing by the RCP.
    VerifyOrExit(frameInfo.ParseFrom(frame, Frame::kParseAddrFields) == kErrorNone, error = kErrorInvalidArgs);
    VerifyOrExit(!frameInfo.mIsAckRequest, error = kErrorInvalidArgs);
    VerifyOrExit(!frameInfo.mIsSecurityEnabled || frame.IsSecurityProcessed(), error = kErrorInvalidArgs);

    if (aConfig.mChannelMask != 0)
    {
        VerifyOrExit((aConfig.mChannelMask & ~Radio::kSupportedChannels) == 0, error = kErrorInvalidArgs);
        mBurstChannelMask.SetMask(aConfig.mChannelMask);
    }
    else
    {
        VerifyOrExit(ChannelMask(Radio::kSupportedChannels).ContainsChannel(frame.GetChannel()),
                     error = kErrorInvalidArgs);
    }

    now            = Get<Radio::Radio>().GetNowAsTime32();
    mBurstCount    = aConfig.mCount;
    mBurstPeriodUs = aConfig.mPeriodUs;
    mIsTimedBurst  = frame.IsTargetTxTimeSpecified();
    mBurstTxTime   = mIsTimedBurst ? frame.GetTargetTxTime() : now;

    if (mIsTimedBurst && IsSlotMissed(now))
    {
        // The first slot was missed.
        if (mBurstPeriodUs == 0)
        {
            mBurstCount--;
            mIsTimedBurst = false;
        }
        else
        {
            SkipMissedBurstSlots(now);
        }

        VerifyOrExit(mBurstCount > 0, error = kErrorInvalidArgs);
    }

    StartBurstTimer(now);

exit:
    return error;
}

Error LinkExt::TransmitBurst(const BurstConfig &aConfig, BurstDoneHandler aHandler, void *aContext)
{
    Error error = kErrorNone;

    VerifyOrExit(Get<LinkRaw>().IsEnabled(), error = kErrorInvalidState);
    VerifyOrExit(!IsBurstActive() && !Get<LinkRaw>().IsTransmitPending() && !Get<SubMac>().IsTransmittingOrScanning(),
                 error = kErrorBusy);

    SuccessOrExit(error = StartBurst(aConfig));
    mBurstDoneCallback.Set(aHandler, aContext);

exit:
    if (error == kErrorInvalidArgs)
    {
        CancelBurst();
    }

    return error;
}

Error LinkExt::StopBurst(void)
{
    Error error = kErrorNone;

    VerifyOrExit(Get<LinkRaw>().IsEnabled(), error = kErrorInvalidState);
    VerifyOrExit(IsBurstActive());

    if (mIsBurstTxPending && Get<SubMac>().IsTransmittingOrScanning())
    {
        // The frame is with the radio and cannot be taken back, so the
        // burst ends when the radio is done with it. Only the tick in
        // progress remains.
        mIsBurstStopRequested = true;
        mBurstCount           = 1;
        mBurstTimer.Stop();
        ExitNow();
    }

    if (mIsBurstTxPending)
    {
        ApplyIdleState();
    }

    // The burst ends right away, which the caller learns from the
    // configuration, so the callback is not called.
    CancelBurst();
    mBurstDoneCallback.Clear();

exit:
    return error;
}

void LinkExt::GetBurstConfig(BurstConfig &aConfig) const
{
    aConfig.mCount       = mBurstCount;
    aConfig.mPeriodUs    = mBurstPeriodUs;
    aConfig.mChannelMask = mBurstChannelMask.GetMask();
}

#if (OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_MAX_SCHEDULES > 0)

bool LinkExt::IsValid(const PeriodicRxSchedule &aSchedule) const
{
    // The next window starts at most one period ahead. The period is
    // bounded by `Timer::kMaxDelay`, so that a window start can be used
    // as a timer fire time and as a 32-bit radio time (in the future) for
    // `otPlatRadioReceiveAt()`. A window must be long enough to detect a
    // frame (the SHR duration).

    return ChannelMask(Get<Radio::Radio>().GetSupportedChannelMask()).ContainsChannel(aSchedule.mChannel) &&
           (aSchedule.mPeriodUs != 0) && (aSchedule.mPeriodUs <= Timer::kMaxDelay) &&
           (aSchedule.mDurationUs >= Radio::kHeaderShrDuration) && (aSchedule.mPhaseUs < aSchedule.mPeriodUs) &&
           (aSchedule.mDurationUs <= aSchedule.mPeriodUs);
}

Error LinkExt::SetPeriodicRxSchedules(const PeriodicRxSchedule *aSchedules, uint8_t aNumSchedules)
{
    Error error = kErrorNone;

    VerifyOrExit(aSchedules != nullptr || aNumSchedules == 0, error = kErrorInvalidArgs);

    // Windows are scheduled with `otPlatRadioReceiveAt()`, which an RCP
    // always delegates to the radio (`SubMac` has no software fallback).
    VerifyOrExit((aNumSchedules == 0) || (Get<Radio::Radio>().GetCaps() & Radio::kCapReceiveTiming),
                 error = kErrorNotImplemented);
    VerifyOrExit(aNumSchedules <= kMaxPeriodicRxSchedules, error = kErrorNoBufs);

    for (uint8_t i = 0; i < aNumSchedules; i++)
    {
        VerifyOrExit(IsValid(aSchedules[i]), error = kErrorInvalidArgs);
    }

    for (uint8_t i = 0; i < aNumSchedules; i++)
    {
        mPeriodicRxSchedules[i] = aSchedules[i];
    }

    mNumPeriodicRxSchedules = aNumSchedules;

    LogInfo("Periodic RX schedules updated, count:%u", mNumPeriodicRxSchedules);

    if (mNumPeriodicRxSchedules == 0)
    {
        // A window already scheduled on the radio is not cancelled, since
        // `SubMac::ListenScheduler` does not cancel a window scheduled on
        // the radio when no listen source provides a window.
        mPeriodicRxActiveWindow.Clear();
        mPeriodicRxNextWindow.Clear();
    }

    Get<SubMac>().UpdateOnPeriodicRxChange();

exit:
    return error;
}

bool LinkExt::DeterminePeriodicRxWindow(const Radio::SyncedTime &aNow, SubMac::TimedRx &aTimedRx)
{
    // Determines the window to provide to `SubMac::ListenScheduler`
    // (see "Periodic RX arbitration" in the header).
    //
    // Design: The radio holds a single timed RX window. A subsequent
    // `otPlatRadioReceiveAt()` replaces a window which has not started
    // yet, while for an active window it only updates the end time
    // (without interrupting reception). Hence windows are provided one at
    // a time, and the window following the active one must not be
    // provided before the active one has started. The listen scheduler
    // evaluates its sources again at the start time of the provided
    // window, which is exactly when the following window can be provided.
    //
    // The window on the radio which has started (`active`) is tracked
    // separately from the window provided after it (`next`), which has
    // not started yet. When `next` starts, it becomes `active`.
    //
    // On each evaluation, if `active` has not ended, the window starting at
    // `active`'s start time is determined from the current schedules. It is
    // determined from `active`'s start time rather than from `now`, since
    // a window's end depends on where the search starts (a merged window
    // is capped relative to its start, and merge events are counted from
    // it). Searching from `now` would yield a different end on every
    // evaluation (e.g., during continuous coverage), so `active` would
    // never be seen as unchanged.
    //
    // - `active` is unchanged (same start, channel and end time): it is
    //   already on the radio, so the window following it is provided
    //   instead. Providing `active` again would make the scheduler evaluate
    //   again immediately (at its start time) without making progress.
    //   This is also the case when the schedules are updated while
    //   `active` is ongoing and its window does not change, so the radio
    //   is not asked to receive the rest of it again.
    //
    // - `active` changed on the same channel without having ended (e.g.,
    //   its end moved after the schedules were updated): it is provided
    //   again keeping its start time, so the radio keeps receiving and
    //   only applies the new end time. The scheduler then evaluates again
    //   immediately and finds it unchanged.
    //
    // - Otherwise (no active window, or `active` no longer applies at
    //   `now`): the window covering (or following) `now` is provided as
    //   is. It either starts now (e.g., taking over from `active` on
    //   another channel) or replaces a window which has not started yet.
    //   `active` is cleared, since the radio no longer receives in it,
    //   unless it ends exactly at `now`: a window starting at `now` on the
    //   same channel then still continues it.

    SubMac::TimedRx &active = mPeriodicRxActiveWindow;
    SubMac::TimedRx &next   = mPeriodicRxNextWindow;
    bool             found  = false;
    SubMac::TimedRx  window;

    VerifyOrExit(mNumPeriodicRxSchedules > 0);

    if (next.IsSpecified() && next.HasStarted(aNow))
    {
        active = next;
        next.Clear();
    }

    if (active.IsSpecified() && !active.HasEnded(aNow) && FindNextPeriodicRxWindow(active.GetStartTime(), window) &&
        (window.GetStartTime() == active.GetStartTime()) && (window.GetChannel() == active.GetChannel()) &&
        !window.HasEnded(aNow))
    {
        if (window.GetEndTime() == active.GetEndTime())
        {
            VerifyOrExit(FindNextPeriodicRxWindow(active.GetEndTime(), window));
        }
    }
    else
    {
        if (active.GetEndTime() != aNow.GetAsTime64())
        {
            active.Clear();
        }

        VerifyOrExit(FindNextPeriodicRxWindow(aNow.GetAsTime64(), window));
    }

    if (window.HasStarted(aNow))
    {
        active = window;
        next.Clear();
    }
    else
    {
        next = window;
    }

    aTimedRx = window;
    found    = true;

exit:
    return found;
}

uint8_t LinkExt::GetPeriodicRxOwner(Radio::Time64 aTime) const
{
    // Determines which schedule owns the radio at `aTime` (rules 1 and
    // 4). Returns `kPeriodicRxNoOwner` if no window covers `aTime`.

    uint8_t       owner = kPeriodicRxNoOwner;
    Radio::Time64 end;

    for (uint8_t i = 0; i < mNumPeriodicRxSchedules; i++)
    {
        if (IsCovering(mPeriodicRxSchedules[i], aTime, end))
        {
            owner = i;
            break;
        }
    }

    // The highest-priority schedule within its guard owns `aTime` if it
    // has a higher priority than the owner (or if no window covers
    // `aTime`) (rule 4). The guard is thus taken from idle time or from a
    // lower-priority window, never from a higher-priority window. A
    // schedule on the owner's channel also takes its guard, so that a
    // lower-priority schedule on another channel cannot take it.

    for (uint8_t i = 0; i < Min(owner, mNumPeriodicRxSchedules); i++)
    {
        if (IsInGuard(mPeriodicRxSchedules[i], aTime))
        {
            owner = i;
            break;
        }
    }

    return owner;
}

Radio::Time64 LinkExt::GetPeriodicRxChannelEnd(Radio::Time64 aTime, uint8_t aChannel, uint16_t &aNumEvents) const
{
    // Returns the first event time after `aTime` at which the owner
    // channel is no longer `aChannel`, counting the evaluated events.

    Radio::Time64 time = aTime;

    while (aNumEvents < kPeriodicRxMaxMergeEvents)
    {
        uint8_t owner;

        time = GetNextPeriodicRxEvent(time);
        aNumEvents++;

        if (time == Radio::kMaxTime64)
        {
            break;
        }

        owner = GetPeriodicRxOwner(time);

        if ((owner == kPeriodicRxNoOwner) || (mPeriodicRxSchedules[owner].mChannel != aChannel))
        {
            break;
        }
    }

    return time;
}

bool LinkExt::IsDroppedPeriodicRxFragment(Radio::Time64 aStart, Radio::Time64 aEnd)
{
    // Indicates whether `[aStart, aEnd)` is shorter than the minimum
    // fragment (rule 5). A full window (its duration of at least the SHR
    // duration, plus the guard time) is never that short, so only a
    // truncated window can be dropped.

    return (aEnd - aStart < kPeriodicRxMinFragment);
}

bool LinkExt::IsPeriodicRxContinuation(Radio::Time64 aStart, uint8_t aChannel) const
{
    // Indicates whether a window starting at `aStart` on `aChannel`
    // continues the active window on the same channel (e.g., the
    // remainder of a merged window split by rule 3), rather than being
    // truncated. The radio must be receiving on `aChannel` right before
    // `aStart`: either `aStart` is the start of the active window (it is
    // determined again) or its end (it is followed).

    const SubMac::TimedRx &active = mPeriodicRxActiveWindow;
    uint8_t                owner  = kPeriodicRxNoOwner;

    VerifyOrExit(aStart > 0);
    VerifyOrExit(active.IsSpecified() && (active.GetChannel() == aChannel) &&
                 ((aStart == active.GetStartTime()) || (aStart == active.GetEndTime())));
    owner = GetPeriodicRxOwner(aStart - 1);

exit:
    return (owner != kPeriodicRxNoOwner) && (mPeriodicRxSchedules[owner].mChannel == aChannel);
}

Radio::Time64 LinkExt::GetNextPeriodicRxEvent(Radio::Time64 aTime) const
{
    // Returns the earliest time after `aTime` at which the owner may
    // change.

    Radio::Time64 event = Radio::kMaxTime64;

    for (uint8_t i = 0; i < mNumPeriodicRxSchedules; i++)
    {
        event = Min(event, GetNextEvent(mPeriodicRxSchedules[i], aTime));
    }

    return event;
}

bool LinkExt::FindNextPeriodicRxWindow(Radio::Time64 aFrom, SubMac::TimedRx &aWindow) const
{
    // Finds the next window starting at or after `aFrom`: a maximal
    // interval during which the owner channel stays the same (rules 2
    // and 3), dropping short fragments (rule 5).

    bool          found     = false;
    Radio::Time64 time      = aFrom;
    uint32_t      maxPeriod = 0;

    for (uint8_t i = 0; i < mNumPeriodicRxSchedules; i++)
    {
        maxPeriod = Max(maxPeriod, mPeriodicRxSchedules[i].mPeriodUs);
    }

    while (true)
    {
        uint8_t       owner;
        uint8_t       nextOwner;
        uint8_t       channel;
        Radio::Time64 start;
        Radio::Time64 limit;
        uint64_t      length;

        while ((owner = GetPeriodicRxOwner(time)) == kPeriodicRxNoOwner)
        {
            time = GetNextPeriodicRxEvent(time);
            VerifyOrExit(time != Radio::kMaxTime64);
        }

        start   = time;
        channel = mPeriodicRxSchedules[owner].mChannel;
        limit   = start + maxPeriod;

        for (uint16_t numEvents = 0; numEvents < kPeriodicRxMaxMergeEvents; numEvents++)
        {
            Radio::Time64 fragmentEnd;

            time = GetNextPeriodicRxEvent(time);

            if (time >= limit)
            {
                break;
            }

            nextOwner = GetPeriodicRxOwner(time);

            if (nextOwner == kPeriodicRxNoOwner)
            {
                break;
            }

            if (mPeriodicRxSchedules[nextOwner].mChannel == channel)
            {
                continue;
            }

            // A fragment on another channel which is dropped (rule 5) does
            // not end the window if the window's channel follows it.

            fragmentEnd = GetPeriodicRxChannelEnd(time, mPeriodicRxSchedules[nextOwner].mChannel, numEvents);

            if ((fragmentEnd >= limit) || !IsDroppedPeriodicRxFragment(time, fragmentEnd) ||
                (GetPeriodicRxOwner(fragmentEnd) == kPeriodicRxNoOwner) ||
                (mPeriodicRxSchedules[GetPeriodicRxOwner(fragmentEnd)].mChannel != channel))
            {
                break;
            }

            time = fragmentEnd;
        }

        time   = Min(time, limit);
        length = time - start;

        if (!IsDroppedPeriodicRxFragment(start, time) || IsPeriodicRxContinuation(start, channel))
        {
            aWindow.Init(start, ClampToUint32(length), channel);
            found = true;
            break;
        }
    }

exit:
    return found;
}

Radio::Time64 LinkExt::GetNextStart(const PeriodicRxSchedule &aSchedule, Radio::Time64 aTime)
{
    // Returns the earliest window start time strictly after `aTime`. The
    // modulo is evaluated on the full 64-bit radio time.

    Radio::Time64 next = aTime - (aTime % aSchedule.mPeriodUs) + aSchedule.mPhaseUs;

    if (next <= aTime)
    {
        next += aSchedule.mPeriodUs;
    }

    return next;
}

bool LinkExt::IsCovering(const PeriodicRxSchedule &aSchedule, Radio::Time64 aTime, Radio::Time64 &aEnd)
{
    // Indicates whether `aTime` is within a window, and if so, outputs
    // the end time of the window.

    bool          covering = false;
    Radio::Time64 start;

    VerifyOrExit(aTime >= aSchedule.mPhaseUs);

    start = aTime - ((aTime - aSchedule.mPhaseUs) % aSchedule.mPeriodUs);
    aEnd  = start + aSchedule.mDurationUs;

    covering = (aTime < aEnd);

exit:
    return covering;
}

bool LinkExt::IsInGuard(const PeriodicRxSchedule &aSchedule, Radio::Time64 aTime)
{
    // Indicates whether `aTime` is within the guard time right before
    // the start of a window.

    return (GetNextStart(aSchedule, aTime) - aTime <= kPeriodicRxGuard);
}

Radio::Time64 LinkExt::GetNextEvent(const PeriodicRxSchedule &aSchedule, Radio::Time64 aTime)
{
    // Returns the earliest time after `aTime` which is a window start,
    // a window end, or a guard start.

    Radio::Time64 next  = GetNextStart(aSchedule, aTime);
    Radio::Time64 event = next;
    Radio::Time64 end;

    if (next > aTime + kPeriodicRxGuard)
    {
        // The guard start of the next window is still ahead. Otherwise,
        // `aTime` is already within the guard of the next window. The
        // guard start of the following window is then not earlier than
        // `next` (if the period is not shorter than the guard time), or
        // is no transition (if it is, since every time is then within a
        // guard).
        event = next - kPeriodicRxGuard;
    }

    if (IsCovering(aSchedule, aTime, end))
    {
        event = Min(event, end);
    }

    return event;
}

#endif // (OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_MAX_SCHEDULES > 0)

//---------------------------------------------------------------------------------------------------------------------
// MAC frame filter

Error LinkExt::SetMacFilterPolicy(uint8_t aPolicy)
{
    Error error = kErrorNone;

    VerifyOrExit((aPolicy & ~kMacFilterPolicyAll) == 0, error = kErrorInvalidArgs);

#if (OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES == 0)
    VerifyOrExit((aPolicy & kMacFilterPolicyDropUnknownKey) == 0, error = kErrorNotImplemented);
#endif

    mMacFilterPolicy = aPolicy;

exit:
    return error;
}

bool LinkExt::FilterReceivedFrame(const RxFrame &aFrame)
{
    bool               accept = true;
    RxFrame::ParseInfo parseInfo;

    VerifyOrExit(mMacFilterPolicy != 0);

    // 1. Check if frame can be parsed
    if (parseInfo.ParseFrom(aFrame, Frame::kParseFully) != kErrorNone)
    {
        ExitNow(accept = !(mMacFilterPolicy & kMacFilterPolicyDropMalformed));
    }

    // 2. Check if frame is unsecured
    if (!parseInfo.mIsSecurityEnabled)
    {
        ExitNow(accept = !(mMacFilterPolicy & kMacFilterPolicyDropUnsecured));
    }

#if (OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES > 0)
    // 3. Check if the key is known
    if ((mMacFilterPolicy & kMacFilterPolicyDropUnknownKey) && !IsMacKeyKnown(parseInfo))
    {
        ExitNow(accept = false);
    }
#endif

exit:
    return accept;
}

#if (OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES > 0)

Error LinkExt::SetMacKeyTable(const MacKeyTableEntry *aEntries, uint8_t aNumEntries)
{
    Error error = kErrorNone;

    VerifyOrExit(aEntries != nullptr || aNumEntries == 0, error = kErrorInvalidArgs);
    VerifyOrExit(aNumEntries <= kMaxMacKeyTableEntries, error = kErrorNoBufs);

    for (uint8_t i = 0; i < aNumEntries; i++)
    {
        VerifyOrExit(aEntries[i].IsValid(), error = kErrorInvalidArgs);
    }

    for (uint8_t i = 0; i < aNumEntries; i++)
    {
        mMacKeyTable[i] = aEntries[i];
    }

    // Do not keep removed entries (e.g. keys) in memory.
    for (uint8_t i = aNumEntries; i < kMaxMacKeyTableEntries; i++)
    {
        ClearAllBytes(mMacKeyTable[i]);
    }

    mNumMacKeyTableEntries = aNumEntries;

exit:
    return error;
}

bool LinkExt::IsMacKeyKnown(const RxFrame::ParseInfo &aParseInfo) const
{
    bool    known    = false;
    bool    hasKeyId = false;
    uint8_t keyIndex;

    for (uint8_t i = 0; i < mNumMacKeyTableEntries; i++)
    {
        if (mMacKeyTable[i].Matches(aParseInfo))
        {
            ExitNow(known = true);
        }

        hasKeyId |= mMacKeyTable[i].MatchesKeyId(aParseInfo);
    }

    // If no entry has the Key ID of the frame, a Key ID Mode 1 frame
    // using Security Level 5 (as Thread does) and the key index of the
    // previous, current, or next MAC key (`SetMode1MacKeys()`) is known.
    // An entry with the Key ID takes precedence, e.g. to restrict the
    // allowed security levels.

    VerifyOrExit(!hasKeyId);
    VerifyOrExit(aParseInfo.mKeyIdMode == Frame::kKeyIdMode1);
    VerifyOrExit(aParseInfo.mSecurityLevel == Frame::kSecurityEncMic32);

    keyIndex = Get<SubMac>().GetMode1KeyIndex();

    known = IsValueInRange(keyIndex, kMinKeyIndex, kMaxKeyIndex) &&
            ((aParseInfo.mKeyIndex == keyIndex) || (aParseInfo.mKeyIndex == DeterminePrevKeyIndex(keyIndex)) ||
             (aParseInfo.mKeyIndex == DetermineNextKeyIndex(keyIndex)));

exit:
    return known;
}

Error LinkExt::MacKeyTableEntry::GetKeyIdMode(Frame::KeyIdMode &aKeyIdMode) const
{
    Error error = kErrorNone;

    switch (mKeyIdLength)
    {
    case kKeyIdLengthMode0:
        aKeyIdMode = Frame::kKeyIdMode0;
        break;
    case kKeyIdLengthMode1:
        aKeyIdMode = Frame::kKeyIdMode1;
        break;
    case kKeyIdLengthMode2:
        aKeyIdMode = Frame::kKeyIdMode2;
        break;
    default:
        error = kErrorInvalidArgs;
        break;
    }

    return error;
}

bool LinkExt::MacKeyTableEntry::IsValid(void) const
{
    bool             isValid = false;
    Frame::KeyIdMode keyIdMode;

    SuccessOrExit(GetKeyIdMode(keyIdMode));
    VerifyOrExit(mAllowedSecurityLevels != 0);
    VerifyOrExit((mAllowedSecurityLevels & (1 << Frame::kSecurityNone)) == 0);
    isValid = true;

exit:
    return isValid;
}

bool LinkExt::MacKeyTableEntry::Matches(const RxFrame::ParseInfo &aParseInfo) const
{
    return MatchesKeyId(aParseInfo) && ((mAllowedSecurityLevels & (1 << aParseInfo.mSecurityLevel)) != 0);
}

bool LinkExt::MacKeyTableEntry::MatchesKeyId(const RxFrame::ParseInfo &aParseInfo) const
{
    bool             matches = false;
    Frame::KeyIdMode keyIdMode;

    SuccessOrExit(GetKeyIdMode(keyIdMode));
    VerifyOrExit(aParseInfo.mKeyIdMode == keyIdMode);

    switch (keyIdMode)
    {
    case Frame::kKeyIdMode2:
        VerifyOrExit(memcmp(mKeyId, aParseInfo.mKeySource.GetBytes(), Frame::kKeySourceSizeMode2) == 0);
        VerifyOrExit(mKeyId[Frame::kKeySourceSizeMode2] == aParseInfo.mKeyIndex);
        break;

    case Frame::kKeyIdMode1:
        VerifyOrExit(mKeyId[0] == aParseInfo.mKeyIndex);
        break;

    default:
        break;
    }

    matches = true;

exit:
    return matches;
}

#endif // (OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES > 0)

} // namespace Mac
} // namespace ot

#endif // OPENTHREAD_RADIO && OPENTHREAD_CONFIG_LINK_EXT_ENABLE
