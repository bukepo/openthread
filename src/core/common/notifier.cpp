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
 *   This file implements the Notifier class.
 */

#include "notifier.hpp"

#include "instance/instance.hpp"

namespace ot {

RegisterLogModule("Notifier");

Events::Events(Flags aFlags)
{
    mEventFlags.Clear();
    for (uint8_t i = 0; i < 32; i++)
    {
        if (aFlags & (1U << i))
        {
            mEventFlags.Add(i);
        }
    }
}

bool Events::ContainsAny(Flags aFlags) const { return (GetAsFlags() & aFlags) != 0; }

bool Events::ContainsAll(Flags aFlags) const { return (GetAsFlags() & aFlags) == aFlags; }

Events::Flags Events::GetAsFlags(void) const
{
    Flags flags = 0;

    for (uint8_t i = 0; i < 32; i++)
    {
        if (mEventFlags.Has(i))
        {
            flags |= (1U << i);
        }
    }

    return flags;
}

Notifier::Notifier(Instance &aInstance)
    : InstanceLocator(aInstance)
    , mTask(aInstance)
{
}

Error Notifier::RegisterCallback(StateChangedCallback aCallback, void *aContext)
{
    Error            error = kErrorNone;
    ExternalCallback newCallback;

    newCallback.Set(aCallback, aContext);
    VerifyOrExit(!mExternalCallbacks.Contains(newCallback), error = kErrorAlready);
    error = mExternalCallbacks.PushBack(newCallback);

exit:
    return error;
}

void Notifier::RemoveCallback(StateChangedCallback aCallback, void *aContext)
{
    ExternalCallback callbackToRemove;

    callbackToRemove.Set(aCallback, aContext);
    mExternalCallbacks.Remove(callbackToRemove);
}

void Notifier::Signal(Event aEvent)
{
    mEventsToSignal.Add(aEvent);
    mSignaledEvents.Add(aEvent);
    mTask.Post();
}

void Notifier::SignalIfFirst(Event aEvent)
{
    if (!HasSignaled(aEvent))
    {
        Signal(aEvent);
    }
}

void Notifier::EmitEvents(void)
{
    Events events;

    VerifyOrExit(!mEventsToSignal.IsEmpty());

    // Note that the callbacks may signal new events, so we create a
    // copy of `mEventsToSignal` and then clear it.

    events = mEventsToSignal;
    mEventsToSignal.Clear();

    LogEvents(events);

    // Emit events to core internal modules

    Get<Mle::Mle>().HandleNotifierEvents(events);
#if OPENTHREAD_CONFIG_TMF_NETDATA_SERVICE_ENABLE
    Get<NetworkData::Service::Manager>().HandleNotifierEvents(events);
#endif
#if (OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2)
    Get<BackboneRouter::Leader>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_DHCP6_SERVER_ENABLE
    Get<Dhcp6::Server>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_NEIGHBOR_DISCOVERY_AGENT_ENABLE
    Get<NeighborDiscovery::Agent>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_DHCP6_CLIENT_ENABLE
    Get<Dhcp6::Client>().HandleNotifierEvents(events);
#endif
    Get<EnergyScanServer>().HandleNotifierEvents(events);
#if OPENTHREAD_FTD
    Get<MeshCoP::JoinerRouter>().HandleNotifierEvents(events);
#if OPENTHREAD_CONFIG_BACKBONE_ROUTER_ENABLE
    Get<BackboneRouter::Manager>().HandleNotifierEvents(events);
#endif
    Get<ChildSupervisor>().HandleNotifierEvents(events);
#if OPENTHREAD_CONFIG_DATASET_UPDATER_ENABLE || OPENTHREAD_CONFIG_CHANNEL_MANAGER_ENABLE
    Get<MeshCoP::DatasetUpdater>().HandleNotifierEvents(events);
#endif
#endif // OPENTHREAD_FTD
#if OPENTHREAD_FTD || OPENTHREAD_CONFIG_BORDER_ROUTER_ENABLE || OPENTHREAD_CONFIG_TMF_NETDATA_SERVICE_ENABLE
    Get<NetworkData::Notifier>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_ANNOUNCE_SENDER_ENABLE
    Get<AnnounceSender>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_BORDER_AGENT_ENABLE
    Get<MeshCoP::BorderAgent::Manager>().HandleNotifierEvents(events);
    Get<MeshCoP::BorderAgent::TxtData>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_BORDER_AGENT_ENABLE && OPENTHREAD_CONFIG_BORDER_AGENT_ADMITTER_ENABLE
    Get<MeshCoP::BorderAgent::Admitter>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_MLR_ENABLE || (OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_MLR_ENABLE)
    Get<MlrManager>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_DUA_ENABLE || (OPENTHREAD_FTD && OPENTHREAD_CONFIG_TMF_PROXY_DUA_ENABLE)
    Get<DuaManager>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_RADIO_LINK_TREL_ENABLE
    Get<Trel::Link>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_TIME_SYNC_ENABLE
    Get<TimeSync>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_IP6_SLAAC_ENABLE
    Get<Ip6::Slaac>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_JAM_DETECTION_ENABLE
    Get<Utils::JamDetector>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_OTNS_ENABLE
    Get<Utils::Otns>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_HISTORY_TRACKER_ENABLE
    Get<HistoryTracker::Local>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_ENABLE_VENDOR_EXTENSION
    Get<Extension::ExtensionBase>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_BORDER_ROUTING_ENABLE
    Get<BorderRouter::RxRaTracker>().HandleNotifierEvents(events);
    Get<BorderRouter::RoutingManager>().HandleNotifierEvents(events);
#if OPENTHREAD_CONFIG_BORDER_ROUTING_TRACK_PEER_BR_INFO_ENABLE
    Get<BorderRouter::NetDataBrTracker>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_BORDER_ROUTING_MULTI_AIL_DETECTION_ENABLE
    Get<BorderRouter::MultiAilDetector>().HandleNotifierEvents(events);
#endif
#endif
#if OPENTHREAD_CONFIG_SRP_CLIENT_ENABLE
    Get<Srp::Client>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_SRP_SERVER_ENABLE && OPENTHREAD_CONFIG_SRP_SERVER_FAST_START_MODE_ENABLE
    Get<Srp::Server>().HandleNotifierEvents(events);
#endif

#if OPENTHREAD_CONFIG_NETDATA_PUBLISHER_ENABLE
    // The `NetworkData::Publisher` is notified last (e.g., after SRP
    // client) to allow other modules to request changes to what is
    // being published (if needed).
    Get<NetworkData::Publisher>().HandleNotifierEvents(events);
#endif
#if OPENTHREAD_CONFIG_LINK_METRICS_MANAGER_ENABLE
    Get<Utils::LinkMetricsManager>().HandleNotifierEvents(events);
#endif

    for (ExternalCallback &callback : mExternalCallbacks)
    {
        callback.InvokeIfSet(events.GetAsFlags());
    }

exit:
    return;
}

// LCOV_EXCL_START

#if OT_SHOULD_LOG_AT(OT_LOG_LEVEL_INFO)

void Notifier::LogEvents(Events aEvents) const
{
    bool                           addSpace = false;
    bool                           didLog   = false;
    String<kFlagsStringBufferSize> string;

    for (uint16_t bit = 0; bit < Events::kMaxEvents; bit++)
    {
        Event event = static_cast<Event>(bit);

        if (aEvents.Contains(event))
        {
            if (string.GetLength() >= kFlagsStringLineLimit)
            {
                LogInfo("StateChanged (0x%08lx) %s%s ...", ToUlong(aEvents.GetAsFlags()), didLog ? "... " : "[",
                        string.AsCString());
                string.Clear();
                didLog   = true;
                addSpace = false;
            }

            string.Append("%s%s", addSpace ? " " : "", EventToString(event));
            addSpace = true;
        }
    }

    LogInfo("StateChanged (0x%08lx) %s%s]", ToUlong(aEvents.GetAsFlags()), didLog ? "... " : "[", string.AsCString());
}

const char *Notifier::EventToString(Event aEvent) const
{
    const char *retval = "(unknown)";

    // To ensure no clipping of flag names in the logs, the returned
    // strings from this method should have shorter length than
    // `kMaxFlagNameLength` value.
    static const char *const kEventStrings[] = {
        "Ip6+",              // kEventIp6AddressAdded                  (0)
        "Ip6-",              // kEventIp6AddressRemoved                (1)
        "Role",              // kEventThreadRoleChanged                (2)
        "LLAddr",            // kEventThreadLinkLocalAddrChanged       (3)
        "MLAddr",            // kEventThreadMeshLocalAddrChanged       (4)
        "Rloc+",             // kEventThreadRlocAdded                  (5)
        "Rloc-",             // kEventThreadRlocRemoved                (6)
        "PartitionId",       // kEventThreadPartitionIdChanged         (7)
        "KeySeqCntr",        // kEventThreadKeySeqCounterChanged       (8)
        "NetData",           // kEventThreadNetdataChanged             (9)
        "Child+",            // kEventThreadChildAdded                 (10)
        "Child-",            // kEventThreadChildRemoved               (11)
        "Ip6Mult+",          // kEventIp6MulticastSubscribed           (12)
        "Ip6Mult-",          // kEventIp6MulticastUnsubscribed         (13)
        "Channel",           // kEventThreadChannelChanged             (14)
        "PanId",             // kEventThreadPanIdChanged               (15)
        "NetName",           // kEventThreadNetworkNameChanged         (16)
        "ExtPanId",          // kEventThreadExtPanIdChanged            (17)
        "NetworkKey",        // kEventNetworkKeyChanged                (18)
        "PSKc",              // kEventPskcChanged                      (19)
        "SecPolicy",         // kEventSecurityPolicyChanged            (20)
        "CMNewChan",         // kEventChannelManagerNewChannelChanged  (21)
        "ChanMask",          // kEventSupportedChannelMaskChanged      (22)
        "CommissionerState", // kEventCommissionerStateChanged         (23)
        "NetifState",        // kEventThreadNetifStateChanged          (24)
        "BbrState",          // kEventThreadBackboneRouterStateChanged (25)
        "BbrLocal",          // kEventThreadBackboneRouterLocalChanged (26)
        "JoinerState",       // kEventJoinerStateChanged               (27)
        "ActDset",           // kEventActiveDatasetChanged             (28)
        "PndDset",           // kEventPendingDatasetChanged            (29)
        "Nat64",             // kEventNat64TranslatorStateChanged      (30)
        "ParentLq",          // kEventParentLinkQualityChanged         (31)
    };

    if (static_cast<uint16_t>(aEvent) < GetArrayLength(kEventStrings))
    {
        retval = kEventStrings[aEvent];
    }

    return retval;
}

#else // #if OT_SHOULD_LOG_AT( OT_LOG_LEVEL_INFO)

void Notifier::LogEvents(Events) const {}

const char *Notifier::EventToString(Event) const { return ""; }

#endif // #if OT_SHOULD_LOG_AT( OT_LOG_LEVEL_INFO)

// LCOV_EXCL_STOP

} // namespace ot
