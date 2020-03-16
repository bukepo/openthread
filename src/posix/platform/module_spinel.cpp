
namespace ot {

namespace Module {

otError RadioSpinel::Init(const Url::Url &aUrl)
{
    otError error = OT_ERROR_NONE;

    mUnderlying.SetDataCallback(this, RadioSpinel::Input);

    if (aUrl.GetValue("no-reset") == nullptr)
    {
        SuccessOrExit(error = SendReset());
    }

    SuccessOrExit(error = WaitResponse());
    VerifyOrExit(mIsReady, error = OT_ERROR_FAILED);

    SuccessOrExit(error = CheckSpinelVersion());
    SuccessOrExit(error = Get(SPINEL_PROP_NCP_VERSION, SPINEL_DATATYPE_UTF8_S, mVersion, sizeof(mVersion)));
    SuccessOrExit(error = Get(SPINEL_PROP_HWADDR, SPINEL_DATATYPE_EUI64_S, mIeeeEui64.m8));

    if (!IsRcp(supportsRcpApiVersion))
    {
        uint8_t exitCode = OT_EXIT_RADIO_SPINEL_INCOMPATIBLE;

        if ((radioUrl.GetValue("ncp-dataset") != nullptr))
        {
#if !OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
            exitCode = (RestoreDatasetFromNcp() == OT_ERROR_NONE) ? OT_EXIT_SUCCESS : OT_EXIT_FAILURE;
#endif
        }

        DieNow(exitCode);
    }

    if (!radioUrl.GetValue("skip-rcp-compatibility-check") == nullptr)
    {
        SuccessOrDie(CheckRcpApiVersion(supportsRcpApiVersion));
        SuccessOrDie(CheckRadioCapabilities());
    }

    return OT_ERROR_NONE;
}

otError RadioSpinel::Input(const uint8_t *aBuffer, uint16_t aLength)
{
    otError        error = OT_ERROR_NONE;
    uint8_t        header;
    spinel_ssize_t unpacked;

    unpacked = spinel_datatype_unpack(aBuffer, aLength, "C", &header);

    VerifyOrExit(unpacked > 0 && (header & SPINEL_HEADER_FLAG) == SPINEL_HEADER_FLAG &&
                     SPINEL_HEADER_GET_IID(header) == 0,
                 error = OT_ERROR_PARSE);

    if (SPINEL_HEADER_GET_TID(header) == 0)
    {
        error = TryHandleNotification(aBuffer, aLength);
    }
    else
    {
        HandleResponse(aBuffer, aLength);
    }

exit:
    if (error != OT_ERROR_NONE)
    {
        otLogWarnPlat("Error handling hdlc frame: %s", otThreadErrorToString(error));
    }

    return error;
}

otError RadioSpinel::WaitResponse(void)
{
    uint64_t end = otPlatTimeGet() + kMaxWaitTime * US_PER_MS;

    otLogDebgPlat("Wait response: tid=%u key=%u", mWaitingTid, mWaitingKey);

    do
    {
        uint64_t now;
        uint64_t remain;

        now = otPlatTimeGet();
        if (end <= now)
        {
            HandleRcpTimeout();
            ExitNow(mError = OT_ERROR_NONE);
        }
        remain = end - now;

        if (mUnderlying.Wait(remain) != OT_ERROR_NONE)
        {
            HandleRcpTimeout();
            ExitNow(mError = OT_ERROR_NONE);
        }
    } while (mWaitingTid || !mIsReady);

    LogIfFail("Error waiting response", mError);
    // This indicates end of waiting response.
    mWaitingKey = SPINEL_PROP_LAST_STATUS;

exit:
    return mError;
}

static DriverInstance *RadioSpinel::CreateDriverInstance(const char *    aProtocol,
                                                         const Url::Url &aUrl,
                                                         StreamInstance *aUnderlying)
{
    if (strcmp(aProtocol, "spinel"))
    {
        return nullptr;
    }

    RadioSpinel *radioSpinel = new RadioSpinel(aUnderlying);
    if (radioSpinel->Init(aUrl) != OT_ERROR_NONE)
    {
        delete radioSpinel;
        radioSpinel = nullptr;
    }

    return radioSpinel;
}

} // namespace Module
} // namespace ot
