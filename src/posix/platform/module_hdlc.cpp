#include "posix/platform/module_hdlc.hpp"

#include "common/code_utils.hpp"
#include "common/debug.hpp"
#include "common/logging.hpp"

namespace ot {
namespace Modules {

StreamHdlc::StreamHdlc(StreamDriver &aUnderlying)
    : mUnderlying(aUnderlying)
    , mHdlcDecoder(mRxFrameBuffer, HandleHdlcFrame, this)
{
}

StreamHdlc::~StreamHdlc(void)
{
    Deinit();
}

otError StreamHdlc::Init(const Url::Url &aUrl)
{
    OT_UNUSED_VARIABLE(aUrl);

    mUnderlying.SetStreamCallback(this, StreamHdlc::Input);

    return OT_ERROR_NONE;
}

otError StreamHdlc::Input(void *aInstance, const uint8_t *aBuffer, uint16_t aLength)
{
    static_cast<StreamHdlc *>(aInstance)->mHdlcDecoder.Decode(aBuffer, aLength);

    return OT_ERROR_NONE;
}

otError StreamHdlc::Write(const uint8_t *aFrame, uint16_t aLength)
{
    otError                              error = OT_ERROR_NONE;
    ot::Hdlc::FrameBuffer<kMaxFrameSize> encoderBuffer;
    ot::Hdlc::Encoder                    hdlcEncoder(encoderBuffer);

    SuccessOrExit(error = hdlcEncoder.BeginFrame());
    SuccessOrExit(error = hdlcEncoder.Encode(aFrame, aLength));
    SuccessOrExit(error = hdlcEncoder.EndFrame());

    error = mNext->mDriver->mData->Write(mNext, encoderBuffer.GetFrame(), encoderBuffer.GetLength());

exit:
    return error;
}

otError StreamHdlc::Wait(uint32_t aTimeout)
{
    return mUnderlying.Wait(aTimeout);
}

void StreamHdlc::HandleHdlcFrame(otError aError)
{
    if (aError == OT_ERROR_NONE)
    {
        otError error = mDataCallback(mDataContext, mRxFrameBuffer.GetFrame(), mRxFrameBuffer.GetLength());

        if (error == OT_ERROR_BUSY)
        {
            mRxFrameBuffer.SaveFrame();
        }
        else
        {
            mRxFrameBuffer.DiscardFrame();
        }
    }
    else
    {
        mRxFrameBuffer.DiscardFrame();
        otLogWarnPlat("Error decoding hdlc frame: %s", otThreadErrorToString(aError));
    }
}

} // namespace Modules
} // namespace ot

static otError Delete(otPosixRadioInstance *aInstance)
{
    delete static_cast<ot::Posix::Hdlc *>(aInstance);

    return OT_ERROR_NONE;
}

static otError Write(otPosixRadioInstance *aInstance, const uint8_t *aBuffer, uint16_t aLength)
{
    return static_cast<ot::Posix::Hdlc *>(aInstance)->Write(aBuffer, aLength);
}

static otError Wait(otPosixRadioInstance *aInstance, uint32_t aTimeout)
{
    return static_cast<ot::Posix::Hdlc *>(aInstance)->Wait(aTimeout);
}

static void SetCallback(otPosixRadioInstance *aInstance, otPosixDataCallback aCallback, void *aContext)
{
    static_cast<ot::Posix::Hdlc *>(aInstance)->SetDataCallback(aCallback, aContext);
}

otPosixRadioDataFuncs sDataFuncs = {
    .SetCallback = SetCallback,
    .Write       = Write,
    .Wait        = Wait,
};

static otPosixRadioDriver sHdlcDriver = {
    .mNext       = NULL,
    .mName       = "hdlc",
    .Create      = Create,
    .Delete      = Delete,
    .mOperations = NULL,
    .mData       = &sDataFuncs,
    .mPoll       = NULL,
};

#if OPENTHREAD_POSIX_MODULE_HDLC == OT_POSIX_MODULE_DYNAMIC
otDriver *otModuleCreateInstance(const char *aProtocol, const otUrl *aUrl, otDriver *aUnderlying)
{
    if (strcmp(aProtocol, "hdlc"))
    {
        return nullptr;
    }

    StreamHdlc *stream = new StreamHdlc;

    if (stream->Init(*static_cast<Url::Url *>(aUrl), *static_cast<StreamDriver *>(aUnderlying)) != OT_ERROR_NONE)
    {
        delete stream;
        stream = nullptr;
    }

    return stream;
}
#else
class HdlcModule : public Module
{
    constexpr HdlcModule(void)
        : Module{HdlcModule::Create, RadioMananger::mStaticModules}
    {
        RadioManager::mStaticModules = this;
    }

    static otDriver *Create(const char *aProtocol, const Url::Url &aUrl, StreamDriver *aUnderlying)
    {
        ot::Posix::Hdlc *driver = new ot::Posix::Hdlc();

        driver->Init(aArguments, aNext);

        return driver;
    }
};
constexpr HdlcModule gModule;
#endif
