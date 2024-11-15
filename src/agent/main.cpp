/*
 *    Copyright (c) 2017, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

#define OTBR_LOG_TAG "AGENT"

#include <openthread-br/config.h>

#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openthread/logging.h>
#include <openthread/platform/radio.h>

#if OTBR_ENABLE_PLATFORM_ANDROID
#include <cutils/properties.h>
#endif

#include "agent/application.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/mainloop.hpp"
#include "common/types.hpp"
#include "ncp/thread_host.hpp"

#ifdef OTBR_ENABLE_PLATFORM_ANDROID
#ifndef __ANDROID__
#error "OTBR_ENABLE_PLATFORM_ANDROID can be enabled for only Android devices"
#endif
#endif

static const char kDefaultInterfaceName[] = "wpan0";

// Port number used by Rest server.
static const uint32_t kPortNumber = 8081;

enum
{
    OTBR_OPT_BACKBONE_INTERFACE_NAME = 'B',
    OTBR_OPT_DEBUG_LEVEL             = 'd',
    OTBR_OPT_HELP                    = 'h',
    OTBR_OPT_INTERFACE_NAME          = 'I',
    OTBR_OPT_VERBOSE                 = 'v',
    OTBR_OPT_SYSLOG_DISABLE          = 's',
    OTBR_OPT_VERSION                 = 'V',
    OTBR_OPT_SHORTMAX                = 128,
    OTBR_OPT_RADIO_VERSION,
    OTBR_OPT_AUTO_ATTACH,
    OTBR_OPT_REST_LISTEN_ADDR,
    OTBR_OPT_REST_LISTEN_PORT,
};

#ifndef OTBR_ENABLE_PLATFORM_ANDROID
static jmp_buf sResetJump;
#endif
static otbr::Application *gApp = nullptr;

void                       __gcov_flush();
static const struct option kOptions[] = {
    {"backbone-ifname", required_argument, nullptr, OTBR_OPT_BACKBONE_INTERFACE_NAME},
    {"debug-level", required_argument, nullptr, OTBR_OPT_DEBUG_LEVEL},
    {"help", no_argument, nullptr, OTBR_OPT_HELP},
    {"thread-ifname", required_argument, nullptr, OTBR_OPT_INTERFACE_NAME},
    {"verbose", no_argument, nullptr, OTBR_OPT_VERBOSE},
    {"syslog-disable", no_argument, nullptr, OTBR_OPT_SYSLOG_DISABLE},
    {"version", no_argument, nullptr, OTBR_OPT_VERSION},
    {"radio-version", no_argument, nullptr, OTBR_OPT_RADIO_VERSION},
    {"auto-attach", optional_argument, nullptr, OTBR_OPT_AUTO_ATTACH},
    {"rest-listen-address", required_argument, nullptr, OTBR_OPT_REST_LISTEN_ADDR},
    {"rest-listen-port", required_argument, nullptr, OTBR_OPT_REST_LISTEN_PORT},
    {0, 0, 0, 0}};

static bool ParseInteger(const char *aStr, long &aOutResult)
{
    bool  successful = true;
    char *strEnd;
    long  result;

    VerifyOrExit(aStr != nullptr, successful = false);
    errno  = 0;
    result = strtol(aStr, &strEnd, 0);
    VerifyOrExit(errno != ERANGE, successful = false);
    VerifyOrExit(aStr != strEnd, successful = false);

    aOutResult = result;

exit:
    return successful;
}

#ifndef OTBR_ENABLE_PLATFORM_ANDROID
static constexpr char kAutoAttachDisableArg[] = "--auto-attach=0";
static char           sAutoAttachDisableArgStorage[sizeof(kAutoAttachDisableArg)];

static std::vector<char *> AppendAutoAttachDisableArg(int argc, char *argv[])
{
    std::vector<char *> args(argv, argv + argc);

    args.erase(std::remove_if(
                   args.begin(), args.end(),
                   [](const char *arg) { return arg != nullptr && std::string(arg).rfind("--auto-attach", 0) == 0; }),
               args.end());
    strcpy(sAutoAttachDisableArgStorage, kAutoAttachDisableArg);
    args.push_back(sAutoAttachDisableArgStorage);
    args.push_back(nullptr);

    return args;
}
#endif

static void PrintHelp(const char *aProgramName)
{
    fprintf(stderr,
            "Usage: %s [-I interfaceName] [-B backboneIfName] [-d DEBUG_LEVEL] [-v] [-s] [--auto-attach[=0/1]] "
            "RADIO_URL [RADIO_URL]\n"
            "    --auto-attach defaults to 1\n"
            "    -s disables syslog and prints to standard out\n",
            aProgramName);
    fprintf(stderr, "%s", otSysGetRadioUrlHelpString());
}

static void PrintVersion(void)
{
    printf("%s\n", OTBR_PACKAGE_VERSION);
}

static void OnAllocateFailed(void)
{
    otbrLogCrit("Allocate failure, exiting...");
    exit(1);
}

static otbrLogLevel GetDefaultLogLevel(void)
{
    otbrLogLevel level = OTBR_LOG_INFO;

#if OTBR_ENABLE_PLATFORM_ANDROID
    char value[PROPERTY_VALUE_MAX];

    property_get("ro.build.type", value, "user");
    if (!strcmp(value, "user"))
    {
        level = OTBR_LOG_WARNING;
    }
#endif

    return level;
}

static void PrintRadioVersionAndExit(const std::vector<const char *> &aRadioUrls)
{
    auto host = std::unique_ptr<otbr::Ncp::ThreadHost>(
        otbr::Ncp::ThreadHost::Create(/* aInterfaceName */ "", aRadioUrls,
                                      /* aBackboneInterfaceName */ "",
                                      /* aDryRun */ true, /* aEnableAutoAttach */ false));
    const char *coprocessorVersion;

    host->Init();

    coprocessorVersion = host->GetCoprocessorVersion();
    printf("%s\n", coprocessorVersion);

    host->Deinit();

    exit(EXIT_SUCCESS);
}

void setInstance(otInstance *aInstance)
{
    static char          aNetworkName[] = "OTprova";
    otOperationalDataset aDataset;

    memset(&aDataset, 0, sizeof(otOperationalDataset));

    /*
     * Fields that can be configured in otOperationDataset to override defaults:
     *     Network Name, Mesh Local Prefix, Extended PAN ID, PAN ID, Delay Timer,
     *     Channel, Channel Mask Page 0, Network Key, PSKc, Security Policy
     */
    aDataset.mActiveTimestamp.mSeconds             = 1;
    aDataset.mActiveTimestamp.mTicks               = 0;
    aDataset.mActiveTimestamp.mAuthoritative       = false;
    aDataset.mComponents.mIsActiveTimestampPresent = true;

    /* Set Channel to 15 */
    aDataset.mChannel                      = 15;
    aDataset.mComponents.mIsChannelPresent = true;

    /* Set Pan ID to 2222 */
    aDataset.mPanId                      = (otPanId)0x2222;
    aDataset.mComponents.mIsPanIdPresent = true;

    /* Set Extended Pan ID to C0DE1AB5C0DE1AB5 */
    uint8_t extPanId[OT_EXT_PAN_ID_SIZE] = {0xC0, 0xDE, 0x1A, 0xB5, 0xC0, 0xDE, 0x1A, 0xB5};
    memcpy(aDataset.mExtendedPanId.m8, extPanId, sizeof(aDataset.mExtendedPanId));
    aDataset.mComponents.mIsExtendedPanIdPresent = true;

    /* Set network key to 1234C0DE1AB51234C0DE1AB51234C0DE */
    uint8_t key[OT_NETWORK_KEY_SIZE] = {0x12, 0x34, 0xC0, 0xDE, 0x1A, 0xB5, 0x12, 0x34, 0xC0, 0xDE, 0x1A, 0xB5, 0x12, 0x34, 0xC0, 0xDE};
    memcpy(aDataset.mNetworkKey.m8, key, sizeof(aDataset.mNetworkKey));
    aDataset.mComponents.mIsNetworkKeyPresent = true;

    /* Set Network Name to OTCodelab */
    size_t length = strlen(aNetworkName);
    assert(length <= OT_NETWORK_NAME_MAX_SIZE);
    memcpy(aDataset.mNetworkName.m8, aNetworkName, length);
    aDataset.mComponents.mIsNetworkNamePresent = true;

    otDatasetSetActive(aInstance, &aDataset);
    /* Set the router selection jitter to override the 2 minute default.
       CLI cmd > routerselectionjitter 20
       Warning: For demo purposes only - not to be used in a real product */
    uint8_t jitterValue = 20;
    otThreadSetRouterSelectionJitter(aInstance, jitterValue);
}

static int realmain(int argc, char *argv[])
{
    otbrLogLevel              logLevel = GetDefaultLogLevel();
    int                       opt;
    int                       ret               = EXIT_SUCCESS;
    const char               *interfaceName     = kDefaultInterfaceName;
    bool                      verbose           = false;
    bool                      syslogDisable     = false;
    bool                      printRadioVersion = false;
    bool                      enableAutoAttach  = true;
    const char               *restListenAddress = "";
    int                       restListenPort    = kPortNumber;
    std::vector<const char *> radioUrls;
    std::vector<const char *> backboneInterfaceNames;
    long                      parseResult;
    std::ofstream file;
    std::set_new_handler(OnAllocateFailed);

    while ((opt = getopt_long(argc, argv, "B:d:hI:Vvs", kOptions, nullptr)) != -1)
    {
        switch (opt)
        {
        case OTBR_OPT_BACKBONE_INTERFACE_NAME:
            backboneInterfaceNames.push_back(optarg);
            otbrLogNotice("Backbone interface: %s", optarg);
            break;

        case OTBR_OPT_DEBUG_LEVEL:
            VerifyOrExit(ParseInteger(optarg, parseResult), ret = EXIT_FAILURE);
            VerifyOrExit(OTBR_LOG_EMERG <= parseResult && parseResult <= OTBR_LOG_DEBUG, ret = EXIT_FAILURE);
            logLevel = static_cast<otbrLogLevel>(parseResult);
            break;

        case OTBR_OPT_INTERFACE_NAME:
            interfaceName = optarg;
            break;

        case OTBR_OPT_VERBOSE:
            verbose = true;
            break;

        case OTBR_OPT_SYSLOG_DISABLE:
            syslogDisable = true;
            break;

        case OTBR_OPT_VERSION:
            PrintVersion();
            ExitNow();
            break;

        case OTBR_OPT_HELP:
            PrintHelp(argv[0]);
            ExitNow(ret = EXIT_SUCCESS);
            break;

        case OTBR_OPT_RADIO_VERSION:
            printRadioVersion = true;
            break;

        case OTBR_OPT_AUTO_ATTACH:
            if (optarg == nullptr)
            {
                enableAutoAttach = true;
            }
            else
            {
                VerifyOrExit(ParseInteger(optarg, parseResult), ret = EXIT_FAILURE);
                enableAutoAttach = parseResult;
            }
            break;
        case OTBR_OPT_REST_LISTEN_ADDR:
            restListenAddress = optarg;
            break;

        case OTBR_OPT_REST_LISTEN_PORT:
            VerifyOrExit(ParseInteger(optarg, parseResult), ret = EXIT_FAILURE);
            restListenPort = parseResult;
            break;

        default:
            PrintHelp(argv[0]);
            ExitNow(ret = EXIT_FAILURE);
            break;
        }
    }

    otbrLogInit(argv[0], logLevel, verbose, syslogDisable);
    otbrLogNotice("Running %s", OTBR_PACKAGE_VERSION);
    otbrLogNotice("Thread version: %s", otbr::Ncp::RcpHost::GetThreadVersion());
    otbrLogNotice("Thread interface: %s", interfaceName);

    if (backboneInterfaceNames.empty())
    {
        otbrLogNotice("Backbone interface is not specified");
    }

    for (int i = optind; i < argc; i++)
    {
        otbrLogNotice("Radio URL: %s", argv[i]);
        radioUrls.push_back(argv[i]);
    }

    if (printRadioVersion)
    {
        PrintRadioVersionAndExit(radioUrls);
        assert(false);
    }

    {
        otbr::Application app(interfaceName, backboneInterfaceNames, radioUrls, enableAutoAttach, restListenAddress,
                              restListenPort);
	    file.open("./coap.csv");
	    file << "aperto file\n";
	    file.close();
        gApp = &app;
        app.Init();
	    // trying to setup the app network instance
        app.StartCoapResources();

        ret = app.Run();

        app.Deinit();
    }

    otbrLogDeinit();

exit:
    return ret;
}

void otPlatReset(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    gPlatResetReason = OT_PLAT_RESET_REASON_SOFTWARE;

    VerifyOrDie(gApp != nullptr, "gApp is null");
    gApp->Deinit();
    gApp = nullptr;

#ifndef OTBR_ENABLE_PLATFORM_ANDROID
    longjmp(sResetJump, 1);
    assert(false);
#else
    // Exits immediately on Android. The Android system_server will receive the
    // signal and decide whether (and how) to restart the ot-daemon
    exit(0);
#endif
}

int main(int argc, char *argv[])
{
#ifndef OTBR_ENABLE_PLATFORM_ANDROID
    if (setjmp(sResetJump))
    {
        std::vector<char *> args = AppendAutoAttachDisableArg(argc, argv);

        alarm(0);
#if OPENTHREAD_ENABLE_COVERAGE
        __gcov_flush();
#endif

        execvp(args[0], args.data());
    }
#endif
    return realmain(argc, argv);
}
