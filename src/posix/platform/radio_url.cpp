/*
 *  Copyright (c) 2020, The OpenThread Authors.
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

#include "posix/platform/radio_url.hpp"

#include <stdio.h>

#include <openthread/openthread-system.h>

#include "core/common/code_utils.hpp"
#include "posix/platform/platform-posix.h"

const char *otSysGetRadioUrlHelpString(void)
{
#if OPENTHREAD_POSIX_CONFIG_RCP_BUS == OT_POSIX_RCP_BUS_SPI
#define OT_RADIO_URL_HELP_BUS                                                                                  \
    "    spinel+spi://${PATH_TO_SPI_DEVICE}?${Parameters}\n"                                                   \
    "Parameters:\n"                                                                                            \
    "    gpio-int-device[=gpio-device-path]\n"                                                                 \
    "                                  Specify a path to the Linux sysfs-exported GPIO device for the\n"       \
    "                                  `I̅N̅T̅` pin. If not specified, `SPI` interface will fall back to\n" \
    "                                  polling, which is inefficient.\n"                                       \
    "    gpio-int-line[=line-offset]\n"                                                                        \
    "                                  The offset index of `I̅N̅T̅` pin for the associated GPIO device.\n"  \
    "                                  If not specified, `SPI` interface will fall back to polling,\n"         \
    "                                  which is inefficient.\n"                                                \
    "    gpio-reset-dev[=gpio-device-path]\n"                                                                  \
    "                                  Specify a path to the Linux sysfs-exported GPIO device for the\n"       \
    "                                  `R̅E̅S̅` pin.\n"                                                     \
    "    gpio-reset-line[=line-offset]"                                                                        \
    "                                  The offset index of `R̅E̅S̅` pin for the associated GPIO device.\n"  \
    "    spi-mode[=mode]               Specify the SPI mode to use (0-3).\n"                                   \
    "    spi-speed[=hertz]             Specify the SPI speed in hertz.\n"                                      \
    "    spi-cs-delay[=usec]           Specify the delay after C̅S̅ assertion, in µsec.\n"                  \
    "    spi-reset-delay[=ms]          Specify the delay after R̅E̅S̅E̅T̅ assertion, in milliseconds.\n"  \
    "    spi-align-allowance[=n]       Specify the maximum number of 0xFF bytes to clip from start of\n"       \
    "                                  MISO frame. Max value is 16.\n"                                         \
    "    spi-small-packet=[n]          Specify the smallest packet we can receive in a single transaction.\n"  \
    "                                  (larger packets will require two transactions). Default value is 32.\n"

#else

#define OT_RADIO_URL_HELP_BUS                                                                        \
    "    forkpty-arg[=argument string]  Command line arguments for subprocess, can be repeated.\n"   \
    "    spinel+hdlc+uart://${PATH_TO_UART_DEVICE}?${Parameters} for real uart device\n"             \
    "    spinel+hdlc+forkpty://${PATH_TO_UART_DEVICE}?${Parameters} for forking a pty subprocess.\n" \
    "Parameters:\n"                                                                                  \
    "    uart-parity[=even|odd]         Uart parity config, optional.\n"                             \
    "    uart-stop[=number-of-bits]     Uart stop bit, default is 1.\n"                              \
    "    uart-baudrate[=baudrate]       Uart baud rate, default is 115200.\n"                        \
    "    uart-flow-control              Enable flow control, disabled by default.\n"

#endif // OPENTHREAD_POSIX_CONFIG_RCP_BUS == OT_POSIX_RCP_BUS_SPI

#if OPENTHREAD_POSIX_CONFIG_MAX_POWER_TABLE_ENABLE
#define OT_RADIO_URL_HELP_MAX_POWER_TABLE                                                                  \
    "    max-power-table               Max power for channels in ascending order separated by commas,\n"   \
    "                                  If the number of values is less than that of supported channels,\n" \
    "                                  the last value will be applied to all remaining channels.\n"        \
    "                                  Special value 0x7f disables a channel.\n"
#else
#define OT_RADIO_URL_HELP_MAX_POWER_TABLE
#endif

    return "RadioURL:\n" OT_RADIO_URL_HELP_BUS OT_RADIO_URL_HELP_MAX_POWER_TABLE
           "    region[=region-code]          Set the radio's region code.\n"
           "    cca-threshold[=dbm]           Set the radio's CCA ED threshold in dBm measured at antenna connector.\n"
           "    fem-lnagain[=dbm]             Set the Rx LNA gain in dBm of the external FEM.\n"
           "    ncp-dataset                   Retrieve dataset from ncp.\n"
           "    no-reset                      Do not send Spinel reset command to RCP on initialization.\n"
           "    skip-rcp-compatibility-check  Skip checking RCP API version and capabilities during initialization.\n";
}

namespace ot {
namespace Posix {

RadioUrl::RadioUrl(const char *aUrl)
{
    VerifyOrDie(strnlen(aUrl, sizeof(mUrl)) < sizeof(mUrl), OT_EXIT_INVALID_ARGUMENTS);
    strncpy(mUrl, aUrl, sizeof(mUrl) - 1);
<<<<<<< HEAD
    SuccessOrDie(Url::Url::Init(mUrl));
=======
    url = strstr(url, "://");
    VerifyOrExit(url != NULL, OT_NOOP);
    url += sizeof("://") - 1;
    mPath = url;

    mStart = strstr(url, "?");
=======
#include "core/common/code_utils.hpp"
#include "posix/platform/platform-posix.h"

namespace ot {
namespace Posix {

Arguments::Arguments(char *aUrl)
{
    aUrl = strstr(aUrl, "://");
    VerifyOrDie(aUrl != NULL, OT_EXIT_INVALID_ARGUMENTS);
    aUrl[0] = '\0';
    aUrl += sizeof("://") - 1;
    mDevice = aUrl;

    mStart = strstr(aUrl, "?");
>>>>>>> e9260e7de (add radio.cpp)

    if (mStart != NULL)
    {
        mStart[0] = '\0';
<<<<<<< HEAD
        mStart++;

        mEnd = mStart + strlen(mStart);

        for (char *cur = strtok(mStart, "&"); cur != NULL; cur = strtok(NULL, "&"))
            ;
    }
    else
    {
        mEnd   = url + strlen(url);
        mStart = mEnd;
    }
exit:
    return;
=======

        mStart += sizeof("?") - 1;

        mEnd = mStart + strlen(mStart);
    }
    else
    {
        mEnd = aUrl + strlen(aUrl);
    }
>>>>>>> e9260e7de (add radio.cpp)
}

const char *Arguments::GetValue(const char *aName, const char *aLastValue)
{
    const char * rval  = NULL;
    const size_t len   = strlen(aName);
    char *       start = (aLastValue == NULL ? mStart : (const_cast<char *>(aLastValue) + strlen(aLastValue) + 1));

    while (start < mEnd)
    {
        char *last = NULL;

<<<<<<< HEAD
        if (!strncmp(aName, start, len))
        {
            if (start[len] == '=')
            {
                ExitNow(rval = &start[len + 1]);
            }
            else if (start[len] == '&' || start[len] == '\0')
            {
                ExitNow(rval = "");
            }
        }
        last  = start;
=======
        for (char *cur = strtok(start, "&"); cur != NULL; cur = strtok(NULL, "&"))
        {
            if (!strncmp(aName, cur, len))
            {
                if (cur[len] == '=')
                {
                    ExitNow(rval = &cur[len + 1]);
                }
                else if (cur[len] == '&' || cur[len] == '\0')
                {
                    ExitNow(rval = "");
                }
            }
            last = cur;
        }

>>>>>>> e9260e7de (add radio.cpp)
        start = last + strlen(last) + 1;
    }

exit:
    return rval;
>>>>>>> 4ec04bf9d (add radio.cpp)
}

} // namespace Posix
} // namespace ot
<<<<<<< HEAD
=======

<<<<<<< HEAD
#ifndef SELF_TEST
#define SELF_TEST 0
#endif

#if SELF_TEST
=======
const char *otPosixRadioArgumentsGetPath(otPosixRadioArguments *aArguments)
{
    return static_cast<ot::Posix::Arguments *>(aArguments)->GetPath();
}

const char *otPosixRadioArgumentsGetValue(otPosixRadioArguments *aArguments, const char *aName, const char *aLast)
{
    return static_cast<ot::Posix::Arguments *>(aArguments)->GetValue(aName, aLast);
}

#if OT_UNIT_TEST_RADIO_URL
>>>>>>> e9260e7de (add radio.cpp)
#include <assert.h>

void TestSimple()
{
    char                 url[] = "spinel:///dev/ttyUSB0?baudrate=115200";
    ot::Posix::Arguments args(url);

    assert(!strcmp(args.GetPath(), "/dev/ttyUSB0"));
    assert(!strcmp(args.GetValue("baudrate"), "115200"));

    printf("PASS %s\r\n", __func__);
}

void TestSimpleNoArguments()
{
    char                 url[] = "spinel:///dev/ttyUSB0";
    ot::Posix::Arguments args(url);

    assert(!strcmp(args.GetPath(), "/dev/ttyUSB0"));

    printf("PASS %s\r\n", __func__);
}

void TestMultipleProtocols()
{
    char                 url[] = "spinel+spi:///dev/ttyUSB0?baudrate=115200";
    ot::Posix::Arguments args(url);

    assert(!strcmp(args.GetPath(), "/dev/ttyUSB0"));
    assert(!strcmp(args.GetValue("baudrate"), "115200"));

    printf("PASS %s\r\n", __func__);
}

void TestMultipleProtocolsAndDuplicateParameters()
{
    char                 url[] = "spinel+exec:///path/to/ot-rcp?arg=1&arg=arg2&arg=3";
    ot::Posix::Arguments args(url);
    const char *         arg = NULL;

    assert(!strcmp(args.GetPath(), "/path/to/ot-rcp"));

    arg = args.GetValue("arg");
    assert(!strcmp(arg, "1"));

    arg = args.GetValue("arg", arg);
    assert(!strcmp(arg, "arg2"));

    arg = args.GetValue("arg", arg);
    assert(!strcmp(arg, "3"));

    printf("PASS %s\r\n", __func__);
}

void TestPublicAPI()
{
    char                 url[] = "spinel+exec:///path/to/ot-rcp?arg=1&arg=arg2&arg=3";
    ot::Posix::Arguments args(url);
    const char *         arg = NULL;

    assert(!strcmp(otPosixRadioArgumentsGetPath(&args), "/path/to/ot-rcp"));

    arg = otPosixRadioArgumentsGetValue(&args, "arg", NULL);
    assert(!strcmp(arg, "1"));

    arg = otPosixRadioArgumentsGetValue(&args, "arg", arg);
    assert(!strcmp(arg, "arg2"));

    arg = otPosixRadioArgumentsGetValue(&args, "arg", arg);
    assert(!strcmp(arg, "3"));

    printf("PASS %s\r\n", __func__);
}

int main()
{
    TestSimple();
    TestSimpleNoArguments();
    TestMultipleProtocols();
    TestMultipleProtocolsAndDuplicateParameters();
    return 0;
}

#endif // SELF_TEST
    TestPublicAPI();

    return 0;
}
#endif // OT_UNIT_TEST_RADIO_URL
