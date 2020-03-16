#ifndef OT_LIB_MODULES_RADIO_HPP_
#define OT_LIB_MODULES_RADIO_HPP_

#include <stdint.h>

#include <openthread/error.h>
#include <openthread/platform/radio.h>

#include "core/common/code_utils.hpp"
#include "lib/url/url.hpp"

struct otDriver
{
};

namespace ot {

namespace Modules {

class StreamDriver : public otDriver
{
    typedef otError (*DataCallback)(void *aContext, const uint8_t *aBuffer, uint16_t aLength);

public:
    void    SetDataCallback(void *aContext, DataCallback aDataCallback);
    otError Wait(uint32_t aTimeout);
};

class RadioDriver : public otDriver
{
public:
    void          Destroy() { mDestroy(this); }
    otRadioState  GetState() { return mGetState(this); }
    otError       Disable() { return mDisable(this); }
    otError       Enable() { return mEnable(this); }
    otError       Sleep() { return mSleep(this); }
    otError       Receive(uint8_t aChannel) { return mReceive(this, aChannel); }
    otError       Transmit(otRadioFrame *aFrame) { return mTransmit(this, aFrame); }
    otRadioCaps   GetRadioCaps(void) { return mGetRadioCaps(this); }
    const char *  GetVersion(void) { return mGetVersion(this); }
    bool          IsPromiscuous(void) { return mIsPromiscuous(this); }
    otError       SetPromiscuous(bool aEnable) { return mSetPromiscuous(this, aEnable); }
    otError       GetIeeeEui64(uint8_t *aIeeeEui64) { return mGetIeeeEui64(this, aIeeeEui64); }
    otRadioFrame *GetTransmitBuffer(void) { return mGetTransmitBuffer(this); }
    otError       GetTransmitPower(int8_t *aPower) { return mGetTransmitPower(this, aPower); }
    otError       SetTransmitPower(int8_t aPower) { return mSetTransmitPower(this, aPower); }
    int8_t        GetRssi(void) { return mGetRssi(this); }
    otError       SetPanId(uint16_t aPanId) { return mSetPanId(this, aPanId); }
    otError       SetExtendedAddress(const otExtAddress *aAddress) { return mSetExtendedAddress(this, aAddress); }
    otError       SetShortAddress(uint16_t aShortAddress) { return mSetShortAddress(this, aShortAddress); }
    otError       EnableSrcMatch(bool aEnable) { return mEnableSrcMatch(this, aEnable); }
    otError AddSrcMatchExtEntry(const otExtAddress *aExtAddress) { return mAddSrcMatchExtEntry(this, aExtAddress); }
    otError ClearSrcMatchExtEntry(const otExtAddress *aExtAddress) { return mClearSrcMatchExtEntry(this, aExtAddress); }
    otError ClearSrcMatchExtEntries(void) { return mClearSrcMatchExtEntries(this); }
    otError AddSrcMatchShortEntry(uint16_t aShortAddress) { return mAddSrcMatchShortEntry(this, aShortAddress); }
    otError ClearSrcMatchShortEntry(uint16_t aShortAddress) { return mClearSrcMatchShortEntry(this, aShortAddress); }
    otError ClearSrcMatchShortEntries(void) { return mClearSrcMatchShortEntries(this); }
    otError EnergyScan(uint8_t aScanChannel, uint16_t aScanDuration)
    {
        return mEnergyScan(this, aScanChannel, aScanDuration);
    }
    otError GetCcaEnergyDetectThreshold(int8_t *aThreshold) { return mGetCcaEnergyDetectThreshold(this, aThreshold); }
    otError SetCcaEnergyDetectThreshold(int8_t aThreshold) { return mSetCcaEnergyDetectThreshold(this, aThreshold); }
    int8_t  GetReceiveSensitivity(void) { return mGetReceiveSensitivity(this); }
    otError CoexEnable(bool aEnable) { return mCoexEnable(this, aEnable); }
    bool    CoexEnabled(void) { return mCoexEnabled(this); }
    otError GetCoexMetrics(otRadioCoexMetrics *aCoexMetrics) { return mGetCoexMetrics(this, aCoexMetrics); }
    otError DiagProcess(const char *aCommand, char *aOutput, size_t aOutputMaxLen)
    {
        return mDiagProcess(this, aCommand, aOutput, aOutputMaxLen);
    }
    uint32_t GetChannelMask(bool aPreferred) { return mGetChannelMask(this, aPreferred); }

protected:
    otError (*mAddSrcMatchExtEntry)(RadioDriver *aInstance, const otExtAddress *aExtAddress);
    otError (*mAddSrcMatchShortEntry)(RadioDriver *aInstance, uint16_t aShortAddress);
    otError (*mClearSrcMatchExtEntry)(RadioDriver *aInstance, const otExtAddress *aExtAddress);
    otError (*mClearSrcMatchExtEntries)(RadioDriver *aInstance);
    otError (*mClearSrcMatchShortEntry)(RadioDriver *aInstance, uint16_t aShortAddress);
    otError (*mClearSrcMatchShortEntries)(RadioDriver *aInstance);
    otError (*mCoexEnable)(RadioDriver *aInstance, bool aEnabled);
    bool (*mCoexEnabled)(RadioDriver *aInstance);
    void (*mDestroy)(RadioDriver *aInstance);
    otError (*mDiagProcess)(RadioDriver *aInstance, const char *aString, char *aOutput, size_t aOutputMaxLen);
    otError (*mDisable)(RadioDriver *aInstance);
    otError (*mEnable)(RadioDriver *aInstance);
    otError (*mEnableSrcMatch)(RadioDriver *aInstance, bool aEnable);
    otError (*mEnergyScan)(RadioDriver *aInstance, uint8_t aScanChannel, uint16_t aScanDuration);
    otError (*mGetCcaEnergyDetectThreshold)(RadioDriver *aInstance, int8_t *aThreshold);
    uint32_t (*mGetChannelMask)(RadioDriver *aInstance, bool aPreferred);
    otError (*mGetCoexMetrics)(RadioDriver *aInstance, otRadioCoexMetrics *aCoexMetrics);
    otError (*mGetIeeeEui64)(RadioDriver *aInstance, uint8_t *aIeeeEui64);
    otRadioCaps (*mGetRadioCaps)(RadioDriver *aInstance);
    int8_t (*mGetReceiveSensitivity)(RadioDriver *aInstance);
    int8_t (*mGetRssi)(RadioDriver *aInstance);
    otRadioState (*mGetState)(RadioDriver *aInstance);
    otRadioFrame *(*mGetTransmitBuffer)(RadioDriver *aInstance);
    otError (*mGetTransmitPower)(RadioDriver *aInstance, int8_t *aPower);
    const char *(*mGetVersion)(RadioDriver *aInstance);
    bool (*mIsPromiscuous)(RadioDriver *aInstance);
    otError (*mReceive)(RadioDriver *aInstance, uint8_t aChannel);
    otError (*mSetCcaEnergyDetectThreshold)(RadioDriver *aInstance, int8_t aThreshold);
    otError (*mSetExtendedAddress)(RadioDriver *aInstance, const otExtAddress *aAddress);
    otError (*mSetPanId)(RadioDriver *aInstance, uint16_t aPanId);
    otError (*mSetPromiscuous)(RadioDriver *aInstance, bool aEnable);
    otError (*mSetShortAddress)(RadioDriver *aInstance, uint16_t aAddress);
    otError (*mSetTransmitPower)(RadioDriver *aInstance, int8_t aPower);
    otError (*mSleep)(RadioDriver *aInstance);
    otError (*mTransmit)(RadioDriver *aInstance, otRadioFrame *aFrame);
    // bool (*mIsEnabled)(RadioDriver *aInstance);
};

template <typename T> class StaticRadioDriver : public RadioDriver
{
protected:
    static otRadioState GetState(RadioDriver *aInstance) { return static_cast<T *>(aInstance)->GetState(); }

    static void    Destroy(RadioDriver *aInstance) { static_cast<T>(aInstance)->Destroy(); }
    static otError Disable(RadioDriver *aInstance) { return static_cast<T *>(aInstance)->Disable(); }

    static otError Enable(RadioDriver *aInstance) { return static_cast<T *>(aInstance)->Enable(); }
    static otError Sleep(RadioDriver *aInstance) { return static_cast<T *>(aInstance)->Sleep(); }
    static otError Receive(RadioDriver *aInstance, uint8_t aChannel)
    {
        return static_cast<T *>(aInstance)->Receive(aChannel);
    }
    static otError Transmit(RadioDriver *aInstance, otRadioFrame *aFrame)
    {
        return static_cast<T *>(aInstance)->Transmit(*aFrame);
    }

    static otRadioCaps GetRadioCaps(RadioDriver *aInstance) { return static_cast<T *>(aInstance)->GetRadioCaps(); }

    static const char *GetVersion(RadioDriver *aInstance) { return static_cast<T *>(aInstance)->GetVersion(); }

    static bool IsPromiscuous(RadioDriver *aInstance) { return static_cast<T *>(aInstance)->IsPromiscuous(); }

    static void SetPromiscuous(RadioDriver *aInstance, bool aEnable)
    {
        static_cast<T *>(aInstance)->SetPromiscuous(aEnable);
    }

    static otError GetIeeeEui64(RadioDriver *aInstance, uint8_t *aIeeeEui64)
    {
        return static_cast<T *>(aInstance)->GetIeeeEui64(aIeeeEui64);
    }

    static otRadioFrame *GetTransmitBuffer(RadioDriver *aInstance)
    {
        return &static_cast<T *>(aInstance)->GetTransmitFrame();
    }

    static otError GetTransmitPower(RadioDriver *aInstance, int8_t *aPower)
    {
        return static_cast<T *>(aInstance)->GetTransmitPower(*aPower);
    }

    static otError SetTransmitPower(RadioDriver *aInstance, int8_t aPower)
    {
        return static_cast<T *>(aInstance)->SetTransmitPower(aPower);
    }

    static int8_t GetRssi(RadioDriver *aInstance) { return static_cast<T *>(aInstance)->GetRssi(); }

    static otError SetPanId(RadioDriver *aInstance, uint16_t aPanId)
    {
        return static_cast<T *>(aInstance)->SetPanId(aPanId);
    }

    static otError SetExtendedAddress(RadioDriver *aInstance, const otExtAddress *aAddress)
    {
        return static_cast<T *>(aInstance)->SetExtendedAddress(*aAddress);
    }

    static otError SetShortAddress(RadioDriver *aInstance, uint16_t aShortAddress)
    {
        return static_cast<T *>(aInstance)->SetShortAddress(aShortAddress);
    }

    static otError EnableSrcMatch(RadioDriver *aInstance, bool aEnable)
    {
        return static_cast<T *>(aInstance)->EnableSrcMatch(aEnable);
    }

    static otError AddSrcMatchExtEntry(RadioDriver *aInstance, const otExtAddress *aExtAddress)
    {
        return static_cast<T *>(aInstance)->AddSrcMatchExtEntry(*aExtAddress);
    }

    static otError ClearSrcMatchExtEntry(RadioDriver *aInstance, const otExtAddress *aExtAddress)
    {
        return static_cast<T *>(aInstance)->ClearSrcMatchExtEntry(*aExtAddress);
    }

    static otError ClearSrcMatchExtEntries(RadioDriver *aInstance)
    {
        return static_cast<T *>(aInstance)->ClearSrcMatchExtEntries();
    }

    static otError AddSrcMatchShortEntry(RadioDriver *aInstance, uint16_t aShortAddress)
    {
        return static_cast<T *>(aInstance)->AddSrcMatchShortEntry(aShortAddress);
    }

    static otError ClearSrcMatchShortEntry(RadioDriver *aInstance, uint16_t aShortAddress)
    {
        return static_cast<T *>(aInstance)->ClearSrcMatchShortEntry(aShortAddress);
    }

    static otError ClearSrcMatchShortEntries(RadioDriver *aInstance)
    {
        return static_cast<T *>(aInstance)->ClearSrcMatchShortEntries();
    }

    static otError EnergyScan(RadioDriver *aInstance, uint8_t aScanChannel, uint16_t aScanDuration)
    {
        return static_cast<T *>(aInstance)->EnergyScan(aScanChannel, aScanDuration);
    }

    static otError GetCcaEnergyDetectThreshold(RadioDriver *aInstance, int8_t *aThreshold)
    {
        return static_cast<T *>(aInstance)->GetCcaEnergyDetectThreshold(*aThreshold);
    }

    static otError SetCcaEnergyDetectThreshold(RadioDriver *aInstance, int8_t aThreshold)
    {
        return static_cast<T *>(aInstance)->SetCcaEnergyDetectThreshold(aThreshold);
    }

    static int8_t GetReceiveSensitivity(RadioDriver *aInstance)
    {
        return static_cast<T *>(aInstance)->GetReceiveSensitivity();
    }

    static otError CoexEnable(RadioDriver *aInstance, bool aEnable)
    {
        return static_cast<T *>(aInstance)->SetCoexEnabled(aEnable);
    }

    static bool CoexEnabled(RadioDriver *aInstance) { return static_cast<T *>(aInstance)->IsCoexEnabled(); }

    static otError GetCoexMetrics(RadioDriver *aInstance, otRadioCoexMetrics *aCoexMetrics)
    {
        return static_cast<T *>(aInstance)->GetCoexMetrics(*aCoexMetrics);
    }

    static otError DiagProcess(RadioDriver *aInstance, const char *aCommand, char *aOutput, size_t aOutputMaxLen)
    {
        return static_cast<T *>(aInstance)->PlatDiagProcess(aCommand, aOutput, aOutputMaxLen);
    }

    static uint32_t GetChannelMask(RadioDriver *aInstance, bool aPreferred)
    {
        return static_cast<T *>(aInstance)->GetRadioChannelMask(aPreferred);
    }
    StaticRadioDriver()
    {
        mAddSrcMatchExtEntry         = &StaticRadioDriver::AddSrcMatchExtEntry;
        mAddSrcMatchShortEntry       = &StaticRadioDriver::AddSrcMatchShortEntry;
        mClearSrcMatchExtEntries     = &StaticRadioDriver::ClearSrcMatchExtEntries;
        mClearSrcMatchExtEntry       = &StaticRadioDriver::ClearSrcMatchExtEntry;
        mClearSrcMatchShortEntries   = &StaticRadioDriver::ClearSrcMatchShortEntries;
        mClearSrcMatchShortEntry     = &StaticRadioDriver::ClearSrcMatchShortEntry;
        mCoexEnable                  = &StaticRadioDriver::CoexEnable;
        mCoexEnabled                 = &StaticRadioDriver::CoexEnabled;
        mDestroy                     = &StaticRadioDriver::Destroy;
        mDiagProcess                 = &StaticRadioDriver::DiagProcess;
        mDisable                     = &StaticRadioDriver::Disable;
        mEnable                      = &StaticRadioDriver::Enable;
        mEnableSrcMatch              = &StaticRadioDriver::EnableSrcMatch;
        mEnergyScan                  = &StaticRadioDriver::EnergyScan;
        mGetCcaEnergyDetectThreshold = &StaticRadioDriver::GetCcaEnergyDetectThreshold;
        mGetChannelMask              = &StaticRadioDriver::GetChannelMask;
        mGetCoexMetrics              = &StaticRadioDriver::GetCoexMetrics;
        mGetIeeeEui64                = &StaticRadioDriver::GetIeeeEui64;
        mGetRadioCaps                = &StaticRadioDriver::GetRadioCaps;
        mGetReceiveSensitivity       = &StaticRadioDriver::GetReceiveSensitivity;
        mGetRssi                     = &StaticRadioDriver::GetRssi;
        mGetState                    = &StaticRadioDriver::GetState;
        mGetTransmitBuffer           = &StaticRadioDriver::GetTransmitBuffer;
        mGetTransmitPower            = &StaticRadioDriver::GetTransmitPower;
        mGetVersion                  = &StaticRadioDriver::GetVersion;
        mIsPromiscuous               = &StaticRadioDriver::IsPromiscuous;
        mReceive                     = &StaticRadioDriver::Receive;
        mSetCcaEnergyDetectThreshold = &StaticRadioDriver::SetCcaEnergyDetectThreshold;
        mSetExtendedAddress          = &StaticRadioDriver::SetExtendedAddress;
        mSetPanId                    = &StaticRadioDriver::SetPanId;
        mSetPromiscuous              = &StaticRadioDriver::SetPromiscuous;
        mSetShortAddress             = &StaticRadioDriver::SetShortAddress;
        mSetTransmitPower            = &StaticRadioDriver::SetTransmitPower;
        mSleep                       = &StaticRadioDriver::Sleep;
        mTransmit                    = &StaticRadioDriver::Transmit;
    }
};

struct Module
{
    RadioDriver *CreateRadioDriver(const char *aProtocol, const Url::Url &aUrl, StreamDriver *aUnderlying)
    {
        return static_cast<RadioDriver *>(mCreateDriver(aProtocol, aUrl, aUnderlying));
    }

    StreamDriver *CreateStreamDriver(const char *aProtocol, const Url::Url &aUrl, StreamDriver *aUnderlying)
    {
        return static_cast<StreamDriver *>(mCreateDriver(aProtocol, aUrl, aUnderlying));
    }

    typedef otDriver *(*CreateDriverCallback)(const char *aProtocol, const Url::Url &aUrl, StreamDriver *aUnderlying);

    constexpr Module(CreateDriverCallback aCreateDriver)
        : mCreateDriver(aCreateDriver)
        , mNext(nullptr)
    {
    }

    CreateDriverCallback mCreateDriver;
    Module *             mNext;
};

class RadioManager
{
public:
    otError       Init(const Url::Url &aUrl);
    StreamDriver *Init(const char *aProtocol, const Url::Url &aUrl);
    RadioDriver & GetRadioDriver(void) { return *mRadioInstance; }

private:
    RadioDriver *mRadioInstance;

    Module *mStaticModules;
};

} // namespace Modules
} // namespace ot

#endif // POSIX_PLATFORM_RADIO_HPP_
