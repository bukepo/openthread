#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#if defined(__APPLE__) || defined(__NetBSD__)
#include <util.h>
#else
#include <pty.h>
#endif

#include "core/common/code_utils.hpp"
#include "posix/platform/module_fd.hpp"

namespace ot {
namespace Modules {

static otDriver *Create(const Url::Url &aUrl)
{
    int fd   = -1;
    int pid  = -1;
    int rval = -1;

    {
        struct termios tios;

        memset(&tios, 0, sizeof(tios));
        cfmakeraw(&tios);
        tios.c_cflag = CS8 | HUPCL | CREAD | CLOCAL;

        VerifyOrExit((pid = forkpty(&fd, nullptr, &tios, nullptr)) != -1, rval = -1);
    }

    if (0 == pid)
    {
        constexpr int kMaxArguments = 32;
        char *        argv[kMaxArguments + 1];
        size_t        index = 0;

        argv[index++] = const_cast<char *>(aUrl.GetPath());

        for (const char *arg = nullptr;
             index < OT_ARRAY_LENGTH(argv) && (arg = aUrl.GetValue("forkpty-arg", arg)) != nullptr;
             argv[index++] = const_cast<char *>(arg))
        {
        }

        VerifyOrExit(index < OT_ARRAY_LENGTH(argv), {
            errno = EINVAL;
            rval  = -1;
        });

        VerifyOrExit((rval = execvp(argv[0], argv)) != -1);
    }
    else
    {
        VerifyOrExit((rval = fcntl(fd, F_GETFL)) != -1 &&
                     (rval = fcntl(fd, F_SETFL, rval | O_NONBLOCK | O_CLOEXEC)) != -1);
    }

exit:
    if (rval == -1)
    {
        perror("forkpty");
        return nullptr;
    }
    else
    {
        return new FileDescriptor(fd);
    }
}

#if OPENTHREAD_POSIX_MODULE_HDLC == OT_POSIX_MODULE_DYNAMIC
extern "C" otDriver *otModuleCreateInstance(const char *aProtocol, const otUrl *aUrl, otDriver *aUnderlying)
{
    if (strcmp(aProtocol, "forkpty"))
    {
        return nullptr;
    }

    if (aUnderlying != nullptr)
    {
        return nullptr;
    }

    return Create(*static_cast<const Url::Url *>(aUrl));
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
} // namespace Modules
} // namespace ot
