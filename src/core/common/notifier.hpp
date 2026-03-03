/*
 *  Copyright (c) 2016-2017, The OpenThread Authors.
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
 *  This file defines OpenThread Notifier class.
 */

#ifndef OT_CORE_COMMON_NOTIFIER_HPP_
#define OT_CORE_COMMON_NOTIFIER_HPP_

#include "openthread-core-config.h"

#include <stdbool.h>
#include <stdint.h>

#include <openthread/instance.h>
#include <openthread/platform/toolchain.h>

#include "common/array.hpp"
#include "common/bit_set.hpp"
#include "common/callback.hpp"
#include "common/error.hpp"
#include "common/locator.hpp"
#include "common/non_copyable.hpp"
#include "common/tasklet.hpp"

namespace ot {

/**
 * @addtogroup core-notifier
 *
 * @brief
 *   This module includes definitions for OpenThread Notifier class.
 *
 * @{
 */

/**
 * Type represents events emitted from OpenThread Notifier.
 */
enum Event : uint16_t
{
    kEventIp6AddressAdded                  = 0,  ///< IPv6 address was added
    kEventIp6AddressRemoved                = 1,  ///< IPv6 address was removed
    kEventThreadRoleChanged                = 2,  ///< Role changed
    kEventThreadLinkLocalAddrChanged       = 3,  ///< Link-local address changed
    kEventThreadMeshLocalAddrChanged       = 4,  ///< Mesh-local address changed
    kEventThreadRlocAdded                  = 5,  ///< RLOC was added
    kEventThreadRlocRemoved                = 6,  ///< RLOC was removed
    kEventThreadPartitionIdChanged         = 7,  ///< Partition ID changed
    kEventThreadKeySeqCounterChanged       = 8,  ///< Key Sequence changed
    kEventThreadNetdataChanged             = 9,  ///< Network Data changed
    kEventThreadChildAdded                 = 10, ///< Child was added
    kEventThreadChildRemoved               = 11, ///< Child was removed
    kEventIp6MulticastSubscribed           = 12, ///< Multicast address added
    kEventIp6MulticastUnsubscribed         = 13, ///< Multicast address removed
    kEventThreadChannelChanged             = 14, ///< Network channel changed
    kEventThreadPanIdChanged               = 15, ///< Network PAN ID changed
    kEventThreadNetworkNameChanged         = 16, ///< Network name changed
    kEventThreadExtPanIdChanged            = 17, ///< Extended PAN ID changed
    kEventNetworkKeyChanged                = 18, ///< Network Key changed
    kEventPskcChanged                      = 19, ///< PSKc changed
    kEventSecurityPolicyChanged            = 20, ///< Security Policy changed
    kEventChannelManagerNewChannelChanged  = 21, ///< New Channel (channel-manager)
    kEventSupportedChannelMaskChanged      = 22, ///< Channel mask changed
    kEventCommissionerStateChanged         = 23, ///< Commissioner state changed
    kEventThreadNetifStateChanged          = 24, ///< Netif state changed
    kEventThreadBackboneRouterStateChanged = 25, ///< Backbone Router state changed
    kEventThreadBackboneRouterLocalChanged = 26, ///< Local Backbone Router changed
    kEventJoinerStateChanged               = 27, ///< Joiner state changed
    kEventActiveDatasetChanged             = 28, ///< Active Dataset changed
    kEventPendingDatasetChanged            = 29, ///< Pending Dataset changed
    kEventNat64TranslatorStateChanged      = 30, ///< Nat64Translator state changed
    kEventParentLinkQualityChanged         = 31, ///< Parent link quality changed
};

/**
 * Represents a list of events.
 */
class Events : public Equatable<Events>
{
public:
    /**
     * Maximum number of events.
     */
    static constexpr uint16_t kMaxEvents = 64;

    /**
     * Represents a bit-field indicating a list of events (with values from `Event`)
     */
    typedef otChangedFlags Flags;

    /**
     * Initializes the `Events` list (as empty).
     */
    Events(void) { mEventFlags.Clear(); }

    /**
     * Initializes the `Events` list with a given event.
     *
     * @param[in] aEvent  The event to initialize the list with.
     */
    explicit Events(Event aEvent)
    {
        mEventFlags.Clear();
        mEventFlags.Add(aEvent);
    }

    /**
     * Initializes the `Events` list from a bit-field `Flags` value.
     *
     * @param[in] aFlags  The bit-field `Flags` value.
     */
    explicit Events(Flags aFlags);

    /**
     * Clears the `Events` list.
     */
    void Clear(void) { mEventFlags.Clear(); }

    /**
     * Indicates whether the `Events` list contains a given event.
     *
     * @param[in] aEvent  The event to check.
     *
     * @returns TRUE if the list contains the @p aEvent, FALSE otherwise.
     */
    bool Contains(Event aEvent) const { return mEventFlags.Has(aEvent); }

    /**
     * Indicates whether the `Events` list contains any of a given set of events.
     *
     * @param[in] aOther  The events set to check.
     *
     * @returns TRUE if the list contains any of the @p aOther set, FALSE otherwise.
     */
    bool ContainsAny(const Events &aOther) const { return mEventFlags.Intersects(aOther.mEventFlags); }

    /**
     * Indicates whether the `Events` list contains any of a given set of events.
     *
     * @param[in] aFlags  The events set to check (as bit-field `Flags` value).
     *
     * @returns TRUE if the list contains any of the @p aFlags set, FALSE otherwise.
     */
    bool ContainsAny(Flags aFlags) const;

    /**
     * Indicates whether the `Events` list contains all of a given set of events.
     *
     * @param[in] aFlags  The events set to check (as bit-field `Flags` value).
     *
     * @returns TRUE if the list contains all of the @p aFlags set, FALSE otherwise.
     */
    bool ContainsAll(Flags aFlags) const;

    /**
     * Adds a given event to the `Events` list.
     *
     * @param[in] aEvent  The event to add.
     */
    void Add(Event aEvent) { mEventFlags.Add(aEvent); }

    /**
     * Adds a given set of events to the `Events` list.
     *
     * @param[in] aOther  The events set to add.
     */
    void Add(const Events &aOther) { mEventFlags.Add(aOther.mEventFlags); }

    /**
     * Indicates whether the `Events` list is empty.
     *
     * @returns TRUE if the list is empty, FALSE otherwise.
     */
    bool IsEmpty(void) const { return mEventFlags.IsEmpty(); }

    /**
     * Gets the `Events` list as bit-field `Flags` value.
     *
     * @returns The list as bit-field `Flags` value.
     */
    Flags GetAsFlags(void) const;

    /**
     * Overloads bitwise OR operator to combine an `Events` list with an `Event`.
     *
     * @param[in] aEvent  The event to combine with.
     *
     * @returns A new `Events` list containing the combined events.
     */
    Events operator|(Event aEvent) const
    {
        Events events(*this);
        events.Add(aEvent);
        return events;
    }

    /**
     * Overloads bitwise OR operator to combine two `Events` lists.
     *
     * @param[in] aOther  The other `Events` list to combine with.
     *
     * @returns A new `Events` list containing the combined events.
     */
    Events operator|(const Events &aOther) const
    {
        Events events(*this);
        events.Add(aOther);
        return events;
    }

private:
    BitSet<kMaxEvents> mEventFlags;
};

/**
 * Overloads bitwise OR operator to combine two `Event` values into an `Events` list.
 *
 * @param[in] aLhs  The first event.
 * @param[in] aRhs  The second event.
 *
 * @returns A new `Events` list containing both events.
 */
inline Events operator|(Event aLhs, Event aRhs)
{
    Events events(aLhs);
    events.Add(aRhs);
    return events;
}

/**
 * Implements the OpenThread Notifier.
 *
 * For core internal modules, `Notifier` class emits events directly to them by invoking method `HandleNotifierEvents()`
 * on the module instance.
 */
class Notifier : public InstanceLocator, private NonCopyable
{
public:
    /**
     * Maximum number of external callback handlers that can be registered.
     */
    static constexpr uint16_t kMaxExternalHandlers = OPENTHREAD_CONFIG_MAX_STATECHANGE_HANDLERS;

    typedef otStateChangedCallback StateChangedCallback; ///< State changed callback

    /**
     * Initializes a `Notifier` instance.
     *
     *  @param[in] aInstance     A reference to OpenThread instance.
     */
    explicit Notifier(Instance &aInstance);

    /**
     * Registers an external `StateChangedCallback`.
     *
     * This is intended for use by external users (i.e., provided as an OpenThread public API). `kMaxExternalHandlers`
     * specifies the maximum number of callbacks.
     *
     * @param[in]  aCallback     A pointer to the handler function that is called to notify of the changes.
     * @param[in]  aContext      A pointer to arbitrary context information.
     *
     * @retval kErrorNone     Successfully registered the callback.
     * @retval kErrorAlready  The callback was already registered.
     * @retval kErrorNoBufs   Could not add the callback due to resource constraints.
     */
    Error RegisterCallback(StateChangedCallback aCallback, void *aContext);

    /**
     * Removes/unregisters a previously registered `StateChangedCallback` handler.
     *
     * @param[in]  aCallback     A pointer to the callback function pointer.
     * @param[in]  aContext      A pointer to arbitrary context information.
     */
    void RemoveCallback(StateChangedCallback aCallback, void *aContext);

    /**
     * Schedules signaling of an event.
     *
     * @param[in]  aEvent     The event to signal.
     */
    void Signal(Event aEvent);

    /**
     * Schedules signaling of am event only if the event has not been signaled before (first time signal).
     *
     * @param[in]  aEvent     The event to signal.
     */
    void SignalIfFirst(Event aEvent);

    /**
     * Indicates whether or not an event signal callback is pending/scheduled.
     *
     * @returns TRUE if a callback is pending, FALSE otherwise.
     */
    bool IsPending(void) const { return !mEventsToSignal.IsEmpty(); }

    /**
     * Indicates whether or not an event has been signaled before.
     *
     * @param[in]  aEvent    The event to check.
     *
     * @retval TRUE    The event @p aEvent have been signaled before.
     * @retval FALSE   The event @p aEvent has not been signaled before.
     */
    bool HasSignaled(Event aEvent) const { return mSignaledEvents.Contains(aEvent); }

    /**
     * Updates a variable of a type `Type` with a new value and signals the given event.
     *
     * If the variable is already set to the same value, this method returns `kErrorAlready` and the event is
     * signaled using `SignalIfFirst()` (i.e., signal is scheduled only if event has not been signaled before).
     *
     * The template `Type` should support comparison operator `==` and assignment operator `=`.
     *
     * @param[in,out] aVariable    A reference to the variable to update.
     * @param[in]     aNewValue    The new value.
     * @param[in]     aEvent       The event to signal.
     *
     * @retval kErrorNone      The variable was update successfully and @p aEvent was signaled.
     * @retval kErrorAlready   The variable was already set to the same value.
     */
    template <typename Type> Error Update(Type &aVariable, const Type &aNewValue, Event aEvent)
    {
        Error error = kErrorNone;

        if (aVariable == aNewValue)
        {
            SignalIfFirst(aEvent);
            error = kErrorAlready;
        }
        else
        {
            aVariable = aNewValue;
            Signal(aEvent);
        }

        return error;
    }

private:
    // Character limit to divide the log into multiple lines in `LogChangedFlags()`.
    static constexpr uint16_t kFlagsStringLineLimit = 70;

    // Max length for string representation of a flag by `FlagToString()`.
    static constexpr uint8_t kMaxFlagNameLength = 25;

    static constexpr uint16_t kFlagsStringBufferSize = kFlagsStringLineLimit + kMaxFlagNameLength;

    typedef Callback<StateChangedCallback> ExternalCallback;

    void EmitEvents(void);

    void        LogEvents(Events aEvents) const;
    const char *EventToString(Event aEvent) const;

    using EmitEventsTask        = TaskletIn<Notifier, &Notifier::EmitEvents>;
    using ExternalCallbackArray = Array<ExternalCallback, kMaxExternalHandlers>;

    Events                mEventsToSignal;
    Events                mSignaledEvents;
    EmitEventsTask        mTask;
    ExternalCallbackArray mExternalCallbacks;
};

/**
 * @}
 */

} // namespace ot

#endif // OT_CORE_COMMON_NOTIFIER_HPP_
