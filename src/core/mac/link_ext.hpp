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

// A frame with Header IEs cannot be parsed without Header IE support,
// and would be treated as malformed by the MAC frame filter.
#if !OPENTHREAD_CONFIG_MAC_HEADER_IE_SUPPORT
#error "OPENTHREAD_CONFIG_LINK_EXT_ENABLE requires OPENTHREAD_CONFIG_MAC_HEADER_IE_SUPPORT"
#endif

#if (OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES > 0) && \
    (OPENTHREAD_CONFIG_LINK_EXT_MAC_NEIGHBOR_TABLE_MAX_ENTRIES == 0)
#error "OPENTHREAD_CONFIG_LINK_EXT_MAC_NEIGHBOR_TABLE_MAX_ENTRIES must be non-zero when the key table is enabled"
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
        uint16_t mDurationUs; ///< Window duration (in usec), at least the SHR duration (160) and at most `mPeriodUs`.
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

    static constexpr uint8_t kMacFilterPolicyDropMalformed  = (1 << 0); ///< Drop frames which cannot be parsed.
    static constexpr uint8_t kMacFilterPolicyDropUnsecured  = (1 << 1); ///< Drop frames without security enabled.
    static constexpr uint8_t kMacFilterPolicyDropUnknownKey = (1 << 2); ///< Drop secured frames with an unknown key.
    static constexpr uint8_t kMacFilterPolicyDropUnknownNeighbor = (1 << 3); ///< Drop frames from unknown neighbors.
    static constexpr uint8_t kMacFilterPolicyDropReplayed        = (1 << 4); ///< Drop replayed secured frames.

    /**
     * Sets the MAC frame filter policy.
     *
     * Received frames are checked in the following order, and each check drops the frame only if its flag is set in
     * @p aPolicy. A frame passing all checks is delivered to the raw link-layer receive callback unmodified.
     *
     * 1. `kMacFilterPolicyDropMalformed`: The frame cannot be parsed. A malformed frame which is not dropped is
     *    delivered without further checks.
     * 2. `kMacFilterPolicyDropUnsecured`: The frame does not have security enabled. An unsecured frame which is not
     *    dropped is delivered without further checks.
     * 3. `kMacFilterPolicyDropUnknownKey`: The key of the secured frame is unknown. A key is known if it matches an
     *    entry of the MAC key table (see `SetMacKeyTable()`). If no entry has the Key ID of the frame, a Key ID Mode 1
     *    frame with Security Level 5 using the key index of the previous, current, or next MAC key (see
     *    `otLinkRawSetMacKey()`) is also known.
     * 4. A frame matching an entry without a key is delivered. For a matching entry with a key, the AES-CCM* MIC is
     *    verified using the extended address of each neighbor of that entry matching the frame (see
     *    `SetMacNeighborTable()`) for the nonce. When a neighbor verifies the MIC, the frame is checked for replay
     *    against, and then updates, the neighbor's next expected frame counter. If no neighbor matches, a frame with an
     *    extended source address is verified using that address. Entries may share a Key ID, so all matching entries
     *    are checked. A frame whose MIC verification fails is dropped.
     *    - `kMacFilterPolicyDropUnknownNeighbor`: No neighbor matches the frame.
     *    - `kMacFilterPolicyDropReplayed`: The frame counter is lower than the next expected frame counter of the
     *      neighbor which verified the MIC.
     *
     * The MIC is verified on a copy of the frame. Frames are not decrypted. A policy of zero disables the filter.
     *
     * @param[in] aPolicy  The policy bitmask (`kMacFilterPolicy*` flags).
     *
     * @retval kErrorNone            Successfully set the policy.
     * @retval kErrorInvalidArgs     @p aPolicy contains an unknown flag.
     * @retval kErrorNotImplemented  @p aPolicy requires the MAC key table, which is disabled
     *                               (`OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES` is zero).
     */
    Error SetMacFilterPolicy(uint8_t aPolicy);

    /**
     * Gets the MAC frame filter policy.
     *
     * @returns The policy bitmask (`kMacFilterPolicy*` flags).
     */
    uint8_t GetMacFilterPolicy(void) const { return mMacFilterPolicy; }

#if (OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES > 0)
    static constexpr uint8_t kMaxMacKeyTableEntries  = OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES;
    static constexpr uint8_t kMacKeyedSecurityLevels = (1 << Frame::kSecurityEncMic32) |
                                                       (1 << Frame::kSecurityEncMic64) |
                                                       (1 << Frame::kSecurityEncMic128); ///< Levels with a key.
    static constexpr uint8_t kMaxMacNeighborTableEntries = OPENTHREAD_CONFIG_LINK_EXT_MAC_NEIGHBOR_TABLE_MAX_ENTRIES;

    /**
     * Represents an entry of the MAC key table.
     *
     * The Key ID Mode of the entry is determined by `mKeyIdLength`:
     * - `kKeyIdLengthMode0`: Key ID Mode 0 (no Key ID).
     * - `kKeyIdLengthMode1`: Key ID Mode 1 (`mKeyId` is the Key Index).
     * - `kKeyIdLengthMode2`: Key ID Mode 2 (`mKeyId` is the Key Source followed by the Key Index).
     */
    class MacKeyTableEntry
    {
    public:
        // Key ID lengths: Mode 0 has no Key ID, Mode 1 has a Key Index, and
        // Mode 2 has a Key Source followed by a Key Index.
        static constexpr uint8_t kKeyIndexSize     = sizeof(uint8_t);
        static constexpr uint8_t kKeyIdLengthMode0 = Frame::kKeySourceSizeMode0;
        static constexpr uint8_t kKeyIdLengthMode1 = Frame::kKeySourceSizeMode1 + kKeyIndexSize;
        static constexpr uint8_t kKeyIdLengthMode2 = Frame::kKeySourceSizeMode2 + kKeyIndexSize;
        static constexpr uint8_t kMaxKeyIdLength   = kKeyIdLengthMode2;

        uint8_t mKeyId[kMaxKeyIdLength]; ///< The Key ID (first `mKeyIdLength` bytes are used).
        uint8_t mKeyIdLength;            ///< The Key ID length, which determines the Key ID Mode.
        uint8_t mAllowedSecurityLevels;  ///< Allowed security levels (bit `i` allows level `i`), non-zero, bit 0 clear.
        bool    mHasKey;                 ///< Whether `mKey` is set, in which case the MIC is verified.
        Key     mKey;                    ///< The key (applicable if `mHasKey`).

        /**
         * Indicates whether the entry is valid.
         *
         * @retval TRUE   The entry is valid.
         * @retval FALSE  The entry is invalid.
         */
        bool IsValid(void) const;

        /**
         * Indicates whether a secured frame matches the entry.
         *
         * @param[in] aParseInfo  The parse info of the frame.
         *
         * @retval TRUE   The frame matches the entry.
         * @retval FALSE  The frame does not match the entry.
         */
        bool Matches(const RxFrame::ParseInfo &aParseInfo) const;

        /**
         * Indicates whether a secured frame uses the Key ID Mode and Key ID of the entry.
         *
         * Unlike `Matches()`, the security level of the frame is not checked.
         *
         * @param[in] aParseInfo  The parse info of the frame.
         *
         * @retval TRUE   The frame uses the Key ID Mode and Key ID of the entry.
         * @retval FALSE  The frame does not use the Key ID Mode and Key ID of the entry.
         */
        bool MatchesKeyId(const RxFrame::ParseInfo &aParseInfo) const;

        /**
         * Verifies the MIC of a secured frame using the key of the entry.
         *
         * The MIC is verified on a copy of the frame, which is left unmodified.
         *
         * @param[in] aParseInfo     The parse info of the frame.
         * @param[in] aNonceExtAddr  The extended address used for the AES-CCM* nonce.
         *
         * @retval TRUE   The MIC is verified.
         * @retval FALSE  The MIC verification failed.
         */
        bool VerifyMic(const RxFrame::ParseInfo &aParseInfo, const ExtAddress &aNonceExtAddr) const;

        /**
         * Indicates whether the entry has the same Key ID and key as another entry.
         *
         * @param[in] aOther  The other entry.
         *
         * @retval TRUE   The Key ID and the key (or the absence of a key) are equal.
         * @retval FALSE  The Key ID or the key differs.
         */
        bool HasSameKey(const MacKeyTableEntry &aOther) const;

    private:
        Error GetKeyIdMode(Frame::KeyIdMode &aKeyIdMode) const;
    };

    /**
     * Represents an entry of the MAC neighbor table.
     *
     * A frame matches a neighbor when its source address is the neighbor's extended address, or the neighbor's short
     * address if set, or when it has no source address. For a frame with a short source address, if the neighbor's PAN
     * ID is set and the frame's source PAN ID is known, the source PAN ID must also be equal to it. The PAN ID scopes
     * the short address, which is only unique within a PAN. Extended addresses are assumed to be unique, so the PAN ID
     * is not a security check.
     */
    class MacNeighborTableEntry
    {
    public:
        ExtAddress   mExtAddress;    ///< The extended address, which is used for the AES-CCM* nonce.
        uint32_t     mFrameCounter;  ///< The next expected frame counter.
        PanId        mPanId;         ///< The PAN ID (`kPanIdBroadcast` if not set).
        ShortAddress mShortAddress;  ///< The short address (`kShortAddrBroadcast` if not set), not `kShortAddrInvalid`.
        uint8_t      mKeyTableIndex; ///< The index of the MAC key table entry, which MUST have a key.

        /**
         * Indicates whether the PAN ID is set.
         *
         * @retval TRUE   The PAN ID is set.
         * @retval FALSE  The PAN ID is not set.
         */
        bool HasPanId(void) const { return mPanId != kPanIdBroadcast; }

        /**
         * Indicates whether the short address is set.
         *
         * @retval TRUE   The short address is set.
         * @retval FALSE  The short address is not set.
         */
        bool HasShortAddress(void) const { return mShortAddress != kShortAddrBroadcast; }

        /**
         * Indicates whether the entry identifies the same neighbor as another entry.
         *
         * @param[in] aOther  The other entry.
         *
         * @retval TRUE   The extended address and key table index are equal.
         * @retval FALSE  The extended address or key table index differs.
         */
        bool IsSameNeighbor(const MacNeighborTableEntry &aOther) const
        {
            return (mKeyTableIndex == aOther.mKeyTableIndex) && (mExtAddress == aOther.mExtAddress);
        }

        /**
         * Indicates whether a secured frame matching a key table entry matches the neighbor.
         *
         * @param[in] aParseInfo      The parse info of the frame.
         * @param[in] aKeyTableIndex  The index of the key table entry which the frame matches.
         *
         * @retval TRUE   The frame matches the neighbor.
         * @retval FALSE  The frame does not match the neighbor.
         */
        bool Matches(const RxFrame::ParseInfo &aParseInfo, uint8_t aKeyTableIndex) const;
    };

    /**
     * Atomically replaces the MAC key table.
     *
     * A secured frame matches an entry if its Key ID Mode and Key ID equal those of the entry, and its security level
     * is allowed by the entry (see `SetMacFilterPolicy()`). An entry with a key MUST allow only Security Levels 5,
     * 6, and 7 (`kMacKeyedSecurityLevels`), which are the levels supported for MIC verification.
     *
     * A neighbor (see `SetMacNeighborTable()`) is kept, with its frame counter, only if the entry at its key table
     * index keeps the same Key ID and key. Other neighbors are removed.
     *
     * @param[in] aEntries     A pointer to an array of entries (can be `nullptr` if @p aNumEntries is zero).
     * @param[in] aNumEntries  The number of entries in @p aEntries.
     *
     * @retval kErrorNone         Successfully set the table.
     * @retval kErrorNoBufs       More than `kMaxMacKeyTableEntries` entries. The table is left unchanged.
     * @retval kErrorInvalidArgs  An entry is invalid. The table is left unchanged.
     * @retval kErrorNotCapable   An entry with a key allows a security level other than 5, 6, or 7. The table is left
     *                            unchanged.
     */
    Error SetMacKeyTable(const MacKeyTableEntry *aEntries, uint8_t aNumEntries);

    /**
     * Atomically replaces the MAC neighbor table.
     *
     * Each extended address and key table index pair MUST be unique. The next expected frame counter of a neighbor is
     * updated when the neighbor verifies a frame. For an entry with the same extended address and key table index as a
     * current entry, the frame counter is set to the larger of the two, so that it never moves backwards.
     *
     * @param[in] aEntries     A pointer to an array of entries (can be `nullptr` if @p aNumEntries is zero).
     * @param[in] aNumEntries  The number of entries in @p aEntries.
     *
     * @retval kErrorNone         Successfully set the table.
     * @retval kErrorNoBufs       More than `kMaxMacNeighborTableEntries` entries. The table is left unchanged.
     * @retval kErrorInvalidArgs  An entry is invalid (e.g. its short address is `kShortAddrInvalid`), or two entries
     *                            have the same extended address and key table index. The table is left unchanged.
     */
    Error SetMacNeighborTable(const MacNeighborTableEntry *aEntries, uint8_t aNumEntries);

    /**
     * Gets the MAC neighbor table.
     *
     * @param[out] aNumEntries  A reference to output the number of entries.
     *
     * @returns A pointer to the array of entries.
     */
    const MacNeighborTableEntry *GetMacNeighborTable(uint8_t &aNumEntries) const
    {
        aNumEntries = mNumMacNeighborTableEntries;
        return mMacNeighborTable;
    }

    /**
     * Gets the MAC key table.
     *
     * @param[out] aNumEntries  A reference to output the number of entries.
     *
     * @returns A pointer to the array of entries.
     */
    const MacKeyTableEntry *GetMacKeyTable(uint8_t &aNumEntries) const
    {
        aNumEntries = mNumMacKeyTableEntries;
        return mMacKeyTable;
    }
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
    bool FilterReceivedFrame(const RxFrame &aFrame);

    static constexpr uint8_t kMacFilterPolicyKeyTable =
        kMacFilterPolicyDropUnknownKey | kMacFilterPolicyDropUnknownNeighbor | kMacFilterPolicyDropReplayed;
    static constexpr uint8_t kMacFilterPolicyAll =
        kMacFilterPolicyDropMalformed | kMacFilterPolicyDropUnsecured | kMacFilterPolicyKeyTable;

#if (OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES > 0)
    enum MacKeyTableResult : uint8_t
    {
        kMacKeyTableNoMatch,         // The key is unknown.
        kMacKeyTableAccepted,        // The frame is accepted.
        kMacKeyTableUnknownNeighbor, // The sender is not in the neighbor table.
        kMacKeyTableMicFailed,       // The MIC verification failed.
        kMacKeyTableReplayed,        // The frame counter is lower than the neighbor's next expected one.
    };

    bool              IsMode1MacKeyIndex(uint8_t aKeyIndex) const;
    MacKeyTableResult CheckMacKeyTable(const RxFrame::ParseInfo &aParseInfo);
#endif

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
     * 4. Guard: `otPlatRadioReceiveAt()` enters Receive mode at the window start time, so the time the radio needs
     *    before it can receive (ramp-up, settling and channel switching) is accounted for here. A schedule also owns
     *    the `kPeriodicRxGuard` interval right before each of its window starts, so that the radio is listening on
     *    its channel no later than the window start. The guard is taken from idle time or from a lower-priority
     *    window (including a lower-priority window starting or ending within the guard), never from a higher-priority
     *    window: when the radio returns to a lower-priority window on a different channel
     *    after a higher-priority window ends, the lower-priority window starts when the higher-priority one ends, and
     *    loses the time the radio needs to switch. No guard is needed between windows on the same channel, since they
     *    are merged (rule 3).
     *
     * 5. Minimum fragment: A window shorter than `kPeriodicRxMinFragment` (guard time plus the SHR duration, i.e. the
     *    time needed to detect a frame) is dropped. Since a schedule's duration is at least the SHR duration, a full
     *    window (its duration plus the guard time) is never that short, so only a truncated window (e.g. preempted or
     *    following a higher-priority window) is dropped. A dropped fragment does not end the window around it: if the
     * channel after the fragment is the same as the channel before it, the window continues through it. A window
     * continuing a window on the same channel (e.g. the remainder of a merged window split by rule 3) is not a fragment
     * and is never dropped.
     *
     * Example: schedule A (higher priority, channel 11) window [5000, 6600), schedule B (lower priority, channel 12)
     * window [4000, 8000), guard `G`, no other window:
     *
     *       4000-G 4000      5000-G 5000          6600          8000
     *   A:                          |=============|
     *   B:          |=====================================================|
     *   Result:|=== ch 12 ======|== ch 11 ========|== ch 12 =====|
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
    static bool          IsDroppedPeriodicRxFragment(Radio::Time64 aStart, Radio::Time64 aEnd);

    uint8_t       GetPeriodicRxOwner(Radio::Time64 aTime) const;
    Radio::Time64 GetNextPeriodicRxEvent(Radio::Time64 aTime) const;
    bool          FindNextPeriodicRxWindow(Radio::Time64 aFrom, SubMac::TimedRx &aWindow) const;
    Radio::Time64 GetPeriodicRxChannelEnd(Radio::Time64 aTime, uint8_t aChannel, uint16_t &aNumEvents) const;
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

    uint8_t mMacFilterPolicy;
#if (OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES > 0)
    uint8_t               mNumMacKeyTableEntries;
    uint8_t               mNumMacNeighborTableEntries;
    MacKeyTableEntry      mMacKeyTable[kMaxMacKeyTableEntries];
    MacNeighborTableEntry mMacNeighborTable[kMaxMacNeighborTableEntries];
#endif
};

} // namespace Mac
} // namespace ot

#endif // OPENTHREAD_RADIO && OPENTHREAD_CONFIG_LINK_EXT_ENABLE

#endif // OT_CORE_MAC_LINK_EXT_HPP_
