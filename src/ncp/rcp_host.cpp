/*
 *    Copyright (c) 2019, The OpenThread Authors.
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

#define OTBR_LOG_TAG "RCP_HOST"
#define DATASET_CONFIG_FILE "./home/pi/dataset.yml"

#include "ncp/rcp_host.hpp"

#include <algorithm>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <vector>

#include <fstream>
#include <iostream>
#include <openthread/backbone_router_ftd.h>
#include <openthread/border_routing.h>
#include <openthread/coap.h>
#include <openthread/dataset.h>
#include <openthread/dataset_ftd.h>
#include <openthread/dnssd_server.h>
#include <openthread/link_metrics.h>
#include <openthread/logging.h>
#include <openthread/nat64.h>
#include <openthread/srp_server.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>
#include <openthread/thread_ftd.h>
#include <openthread/trel.h>
#include <openthread/platform/logging.h>
#include <openthread/platform/misc.h>
#include <openthread/platform/radio.h>
#include <openthread/platform/settings.h>

#include "common/code_utils.hpp"
#include "common/database.hpp"
#include "common/logging.hpp"
#include "common/types.hpp"

#if OTBR_ENABLE_FEATURE_FLAGS
#include "proto/feature_flag.pb.h"
#endif

namespace otbr {
namespace Ncp {

static const uint16_t kThreadVersion11 = 2; ///< Thread Version 1.1
static const uint16_t kThreadVersion12 = 3; ///< Thread Version 1.2
static const uint16_t kThreadVersion13 = 4; ///< Thread Version 1.3
static const uint16_t kThreadVersion14 = 5; ///< Thread Version 1.4

// =============================== OtNetworkProperties ===============================

OtNetworkProperties::OtNetworkProperties(void)
    : mInstance(nullptr)
{
}

otDeviceRole OtNetworkProperties::GetDeviceRole(void) const
{
    return otThreadGetDeviceRole(mInstance);
}

void OtNetworkProperties::SetInstance(otInstance *aInstance)
{
    mInstance = aInstance;
}

// =============================== RcpHost ===============================

RcpHost::RcpHost(const char                      *aInterfaceName,
                 const std::vector<const char *> &aRadioUrls,
                 const char                      *aBackboneInterfaceName,
                 bool                             aDryRun,
                 bool                             aEnableAutoAttach)
    : mInstance(nullptr)
    , mEnableAutoAttach(aEnableAutoAttach)
{
    VerifyOrDie(aRadioUrls.size() <= OT_PLATFORM_CONFIG_MAX_RADIO_URLS, "Too many Radio URLs!");

    memset(&mConfig, 0, sizeof(mConfig));

    mConfig.mInterfaceName         = aInterfaceName;
    mConfig.mBackboneInterfaceName = aBackboneInterfaceName;
    mConfig.mDryRun                = aDryRun;

    for (const char *url : aRadioUrls)
    {
        mConfig.mCoprocessorUrls.mUrls[mConfig.mCoprocessorUrls.mNum++] = url;
    }
    mConfig.mSpeedUpFactor = 1;
}

RcpHost::~RcpHost(void)
{
    // Make sure OpenThread Instance was gracefully de-initialized.
    assert(mInstance == nullptr);
}

otbrLogLevel RcpHost::ConvertToOtbrLogLevel(otLogLevel aLogLevel)
{
    otbrLogLevel otbrLogLevel;

    switch (aLogLevel)
    {
    case OT_LOG_LEVEL_NONE:
        otbrLogLevel = OTBR_LOG_EMERG;
        break;
    case OT_LOG_LEVEL_CRIT:
        otbrLogLevel = OTBR_LOG_CRIT;
        break;
    case OT_LOG_LEVEL_WARN:
        otbrLogLevel = OTBR_LOG_WARNING;
        break;
    case OT_LOG_LEVEL_NOTE:
        otbrLogLevel = OTBR_LOG_NOTICE;
        break;
    case OT_LOG_LEVEL_INFO:
        otbrLogLevel = OTBR_LOG_INFO;
        break;
    case OT_LOG_LEVEL_DEBG:
    default:
        otbrLogLevel = OTBR_LOG_DEBUG;
        break;
    }

    return otbrLogLevel;
}

#if OTBR_ENABLE_FEATURE_FLAGS
/* Converts ProtoLogLevel to otbrLogLevel */
otbrLogLevel ConvertProtoToOtbrLogLevel(ProtoLogLevel aProtoLogLevel)
{
    otbrLogLevel otbrLogLevel;

    switch (aProtoLogLevel)
    {
    case PROTO_LOG_EMERG:
        otbrLogLevel = OTBR_LOG_EMERG;
        break;
    case PROTO_LOG_ALERT:
        otbrLogLevel = OTBR_LOG_ALERT;
        break;
    case PROTO_LOG_CRIT:
        otbrLogLevel = OTBR_LOG_CRIT;
        break;
    case PROTO_LOG_ERR:
        otbrLogLevel = OTBR_LOG_ERR;
        break;
    case PROTO_LOG_WARNING:
        otbrLogLevel = OTBR_LOG_WARNING;
        break;
    case PROTO_LOG_NOTICE:
        otbrLogLevel = OTBR_LOG_NOTICE;
        break;
    case PROTO_LOG_INFO:
        otbrLogLevel = OTBR_LOG_INFO;
        break;
    case PROTO_LOG_DEBUG:
    default:
        otbrLogLevel = OTBR_LOG_DEBUG;
        break;
    }

    return otbrLogLevel;
}
#endif

otError RcpHost::SetOtbrAndOtLogLevel(otbrLogLevel aLevel)
{
    otError error = OT_ERROR_NONE;
    otbrLogSetLevel(aLevel);
    error = otLoggingSetLevel(ConvertToOtLogLevel(aLevel));
    return error;
}

void RcpHost::Init(void)
{
    otbrError  error = OTBR_ERROR_NONE;
    otLogLevel level = ConvertToOtLogLevel(otbrLogGetLevel());

#if OTBR_ENABLE_FEATURE_FLAGS && OTBR_ENABLE_TREL
    FeatureFlagList featureFlagList;
#endif

    VerifyOrExit(otLoggingSetLevel(level) == OT_ERROR_NONE, error = OTBR_ERROR_OPENTHREAD);

    mInstance = otSysInit(&mConfig);
    mResource = otCoapResource{};
    assert(mInstance != nullptr);

    {
        otError result = otSetStateChangedCallback(mInstance, &RcpHost::HandleStateChanged, this);

        agent::ThreadHelper::LogOpenThreadResult("Set state callback", result);
        VerifyOrExit(result == OT_ERROR_NONE, error = OTBR_ERROR_OPENTHREAD);
    }

#if OTBR_ENABLE_FEATURE_FLAGS && OTBR_ENABLE_TREL
    // Enable/Disable trel according to feature flag default value.
    otTrelSetEnabled(mInstance, featureFlagList.enable_trel());
#endif

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
#if OTBR_ENABLE_SRP_SERVER_AUTO_ENABLE_MODE
    // Let SRP server use auto-enable mode. The auto-enable mode delegates the control of SRP server to the Border
    // Routing Manager. SRP server automatically starts when bi-directional connectivity is ready.
    otSrpServerSetAutoEnableMode(mInstance, /* aEnabled */ true);
#else
    otSrpServerSetEnabled(mInstance, /* aEnabled */ true);
#endif
#endif

#if !OTBR_ENABLE_FEATURE_FLAGS
    // Bring up all features when feature flags is not supported.
#if OTBR_ENABLE_NAT64
    otNat64SetEnabled(mInstance, /* aEnabled */ true);
#endif
#if OTBR_ENABLE_DNS_UPSTREAM_QUERY
    otDnssdUpstreamQuerySetEnabled(mInstance, /* aEnabled */ true);
#endif
#if OTBR_ENABLE_DHCP6_PD && OTBR_ENABLE_BORDER_ROUTING
    otBorderRoutingDhcp6PdSetEnabled(mInstance, /* aEnabled */ true);
#endif
#endif // OTBR_ENABLE_FEATURE_FLAGS

    mThreadHelper = MakeUnique<otbr::agent::ThreadHelper>(mInstance, this);

    OtNetworkProperties::SetInstance(mInstance);

exit:
    SuccessOrDie(error, "Failed to initialize the RCP Host!");
}

#if OTBR_ENABLE_FEATURE_FLAGS
otError RcpHost::ApplyFeatureFlagList(const FeatureFlagList &aFeatureFlagList)
{
    otError error = OT_ERROR_NONE;
    // Save a cached copy of feature flags for debugging purpose.
    mAppliedFeatureFlagListBytes = aFeatureFlagList.SerializeAsString();

#if OTBR_ENABLE_NAT64
    otNat64SetEnabled(mInstance, aFeatureFlagList.enable_nat64());
#endif

    if (aFeatureFlagList.enable_detailed_logging())
    {
        error = SetOtbrAndOtLogLevel(ConvertProtoToOtbrLogLevel(aFeatureFlagList.detailed_logging_level()));
    }
    else
    {
        error = SetOtbrAndOtLogLevel(otbrLogGetDefaultLevel());
    }

#if OTBR_ENABLE_TREL
    otTrelSetEnabled(mInstance, aFeatureFlagList.enable_trel());
#endif
#if OTBR_ENABLE_DNS_UPSTREAM_QUERY
    otDnssdUpstreamQuerySetEnabled(mInstance, aFeatureFlagList.enable_dns_upstream_query());
#endif
#if OTBR_ENABLE_DHCP6_PD
    otBorderRoutingDhcp6PdSetEnabled(mInstance, aFeatureFlagList.enable_dhcp6_pd());
#endif
#if OTBR_ENABLE_LINK_METRICS_TELEMETRY
    otLinkMetricsManagerSetEnabled(mInstance, aFeatureFlagList.enable_link_metrics_manager());
#endif

    return error;
}
#endif

// static void PrintIntoFile()
// {
//     std::ofstream file;
//     file.open("home/pi/log.txt");
//     file << "setNetworkParameters\n";
//     file.close();
// }

// define enum types for yaml parsing
enum otDatasetParameter
{
    networkname,
    channel,
    panid,
    extpanid,
    networkkey,
    pskc,
    undefined
};

std::vector<std::string> split(const std::string &str, char delimiter)
{
    std::vector<std::string> tokens;
    std::string              token;
    std::istringstream       tokenStream(str); // Crea un flusso di input dalla stringa

    // Continua a leggere fino a che ci sono parti separate dal delimitatore
    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token); // Aggiungi il token alla lista
    }

    return tokens; // Restituisci il vettore di tokenj
}

// parse the string into enum type
otDatasetParameter hashit(std::string const &inString)
{
    if (inString == "networkname")
        return networkname;
    if (inString == "channel")
        return channel;
    if (inString == "panid")
        return panid;
    if (inString == "extpanid")
        return extpanid;
    if (inString == "networkkey")
        return networkkey;
    if (inString == "pskc")
        return pskc;
    else
        return undefined;
}

void RcpHost::HandleRequest(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
    static_cast<RcpHost *>(aContext)->HandleRequest(aMessage, aMessageInfo);
}

void RcpHost::HandleRequest(otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
    OT_UNUSED_VARIABLE(aMessageInfo);
    char     name[] = "/home/pi/coap.db";
    Database db(name);

    if (db.connect())
    {
        Payload payload{};

        // from message.cpp / message.hpp
        unsigned char aBuf[otMessageGetLength(aMessage)];

        uint16_t bytesread = otMessageRead(aMessage, 0, aBuf, otMessageGetLength(aMessage));

        uint16_t start_payload = otMessageGetOffset(aMessage);
        // allocating mem for eui

        if (bytesread != 0)
        {
            std::cerr << "parsando payload " << std::endl;
            payload.eui = extractEui64(aBuf, &start_payload, 8);
            otbrLogInfo("eui : %llx\n", payload.eui);
            std::cerr << "CONTROLLO NUOVO SENSORE " << std::endl;
            // check if the eui is a new sensor
            // bool existsSensor = db.CheckNewSensor(payload.eui);
            // if (existsSensor == false)
            //     db.InsertSensor(payload.eui);
            // }
            //
            payload.pktnum           = extractNumber(aBuf, &start_payload, 2);
            payload.timestamp        = extractNumber(aBuf, &start_payload, 4);
            payload.undef            = extractNumber(aBuf, &start_payload, 1);
            uint16_t temperature_raw = extractNumber(aBuf, &start_payload, 2);
            payload.temperature      = -45.0 + 175.0 * temperature_raw / (1 << 16);
            uint16_t humidity_raw    = extractNumber(aBuf, &start_payload, 2);
            payload.humidity         = humidity_raw * 100.0 / (1 << 16);
            uint16_t ir_raw          = extractNumber(aBuf, &start_payload, 2);
            payload.ir               = ir_raw / 0.5;
            uint16_t vis_raw         = extractNumber(aBuf, &start_payload, 2);
            payload.vis              = vis_raw / 0.5;
            uint16_t batt_raw        = extractNumber(aBuf, &start_payload, 2);
            payload.batt             = (((batt_raw >> 4) & 0xF)) + ((batt_raw & 0xF) / 1000);
            double temperatureSum;
            for (int i = 0; i < 10; i++)
            {
                uint16_t tmpraw = extractNumber(aBuf, &start_payload, 4);
                temperatureSum += tmpraw / 100.0;
            }
            payload.avg_temperature = temperatureSum / 10;
            double humiditySum;
            for (int i = 0; i < 10; i++)
            {
                uint16_t hum = extractNumber(aBuf, &start_payload, 8);
                humiditySum += hum / 1000.0;
            }
            payload.avg_humidity = humiditySum / 10;
            double pressureSum;
            for (int i = 0; i < 10; i++)
            {
                uint64_t pressure = extractNumber(aBuf, &start_payload, 8);
                pressureSum += pressure;
            }
            payload.avg_pressure = pressureSum / 10.0;
            double gasReSum;
            for (int i = 0; i < 10; i++)
            {
                uint64_t gas_res = extractNumber(aBuf, &start_payload, 8);
                gasReSum += gas_res;
            }
            payload.avg_gas_resistance = gasReSum / 10;

            // insert the payload into the table
            const char *erroredatabase = db.InsertData(payload);

            if (erroredatabase != nullptr)
            {
                otbrLogEmerg("impossibile inserire il payload \n");
            }
        }
    }
    else
    {
        otbrLogEmerg("impossibile connettersi al database\n");
    }
}

void RcpHost::SetNetworkParameters()
{
    std::ofstream fileoutput;
    std::ifstream file(DATASET_CONFIG_FILE);
    fileoutput.open("home/pi/log.txt");

    otError              error = OT_ERROR_NONE;
    otOperationalDataset aDataset;

    error = otDatasetCreateNewNetwork(mInstance, &aDataset);

    // if file can't be opened, log emergency and exit with errors
    if (!file)
    {
        otbrLogEmerg("impossibile aprire file di configurazione per la rete\n");
        exit(-1);
    }

    std::string line;

    while (std::getline(file, line))
    {
        // TODO: check for the correct format on input
        std::vector<std::string> result = split(line, ':');
        result[1].erase(remove(result[1].begin(), result[1].end(), ' '), result[1].end());
        fileoutput << result[1] << std::endl;

        if (result.size() >= 2)
        {
            switch (hashit(result[0]))
            {
            case networkname:
            {
                std::cout << result[1] << std::endl;
                // TODO: parse correctly the name from yaml to aNetworkName
                // static char aNetworkName[] = "OpenThreadDemo";
                // size_t length = strlen(aNetworkName);
                size_t length = result[1].length();
                assert(length <= OT_NETWORK_NAME_MAX_SIZE);
                // memcpy(aDataset.mNetworkName.m8, aNetworkName, length);

                memcpy(aDataset.mNetworkName.m8, result[1].c_str(), length);
                aDataset.mComponents.mIsNetworkNamePresent = true;
                break;
            }
            case channel:
            {
                aDataset.mChannel                      = std::stoi(result[1].c_str());
                aDataset.mComponents.mIsChannelPresent = true;
                break;
            }
            case panid:
            {
                uint16_t panId                       = static_cast<uint16_t>(std::stoul(result[1], nullptr, 16));
                aDataset.mPanId                      = (otPanId)panId;
                aDataset.mComponents.mIsPanIdPresent = true;
                break;
            }
            case extpanid:
            {
                uint8_t extPanId[OT_EXT_PAN_ID_SIZE];
                for (size_t i = 0; i < OT_EXT_PAN_ID_SIZE; ++i)
                {
                    std::string byteStr = result[1].substr(i * 2, 2);
                    extPanId[i]         = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
                }
                memcpy(aDataset.mExtendedPanId.m8, extPanId, sizeof(aDataset.mExtendedPanId));
                aDataset.mComponents.mIsExtendedPanIdPresent = true;
                break;
            }
            case networkkey:
            {
                uint8_t networkkey[OT_NETWORK_KEY_SIZE];
                for (size_t i = 0; i < OT_NETWORK_KEY_SIZE; ++i)
                {
                    std::string netkey = result[1].substr(i * 2, 2);
                    networkkey[i]      = static_cast<uint8_t>(std::stoul(netkey, nullptr, 16));
                }
                // std::string networkkeyStr = result[1];
                memcpy(aDataset.mNetworkKey.m8, networkkey, sizeof(aDataset.mNetworkKey));
                aDataset.mComponents.mIsNetworkKeyPresent = true;
                break;
            }
            case pskc:
            {
                error = otDatasetGeneratePskc(
                    result[1].c_str(), reinterpret_cast<const otNetworkName *>(otThreadGetNetworkName(mInstance)),
                    otThreadGetExtendedPanId(mInstance), &aDataset.mPskc);

                if (error != OT_ERROR_NONE)
                {
                    return;
                }
                aDataset.mComponents.mIsPskcPresent = true;
                break;
            }
            case undefined:
            {
                break;
            }
            }
        }
    }
    file.close();
    fileoutput.close();

    uint8_t mlp[OT_IP6_PREFIX_SIZE] = {
        0xfd, 0x30, 0x93, 0xd7, // fd30:93d7
        0x91, 0x54, 0xb7, 0xc2  // 9154:b7c2
    };

    memcpy(aDataset.mMeshLocalPrefix.m8, mlp, sizeof(aDataset.mMeshLocalPrefix.m8));

    aDataset.mActiveTimestamp.mSeconds             = 1;
    aDataset.mActiveTimestamp.mTicks               = 0;
    aDataset.mActiveTimestamp.mAuthoritative       = false;
    aDataset.mComponents.mIsActiveTimestampPresent = true;

    otDatasetSetActive(mInstance, &aDataset);
    otIp6SetEnabled(mInstance, true);
    otThreadSetEnabled(mInstance, true);

    error = otThreadBecomeLeader(mInstance);

    if (error)
    {
        printf("error:%s\n", otThreadErrorToString(error));
    }

    error = otCoapStart(mInstance, OT_DEFAULT_COAP_PORT);

    if (error)
    {
        fileoutput.open("home/pi/log.txt");
        fileoutput << "there was an error!\n";
        fileoutput.close();
    }
}

void RcpHost::StartCoapServer()
{
    mResource.mUriPath = "thermostat/temperature";
    mResource.mContext = this;
    mResource.mHandler = &RcpHost::HandleRequest;

    otCoapAddResource(mInstance, &mResource);
}

void RcpHost::Deinit(void)
{
    assert(mInstance != nullptr);

    otSysDeinit();
    mInstance = nullptr;

    OtNetworkProperties::SetInstance(nullptr);
    mThreadStateChangedCallbacks.clear();
    mResetHandlers.clear();
}

void RcpHost::HandleStateChanged(otChangedFlags aFlags)
{
    for (auto &stateCallback : mThreadStateChangedCallbacks)
    {
        stateCallback(aFlags);
    }

    mThreadHelper->StateChangedCallback(aFlags);
}

void RcpHost::Update(MainloopContext &aMainloop)
{
    if (otTaskletsArePending(mInstance))
    {
        aMainloop.mTimeout = ToTimeval(Microseconds::zero());
    }

    otSysMainloopUpdate(mInstance, &aMainloop);
}

void RcpHost::Process(const MainloopContext &aMainloop)
{
    otTaskletsProcess(mInstance);

    otSysMainloopProcess(mInstance, &aMainloop);

    if (IsAutoAttachEnabled() && mThreadHelper->TryResumeNetwork() == OT_ERROR_NONE)
    {
        DisableAutoAttach();
    }
}

bool RcpHost::IsAutoAttachEnabled(void)
{
    return mEnableAutoAttach;
}

void RcpHost::DisableAutoAttach(void)
{
    mEnableAutoAttach = false;
}

void RcpHost::PostTimerTask(Milliseconds aDelay, TaskRunner::Task<void> aTask)
{
    mTaskRunner.Post(std::move(aDelay), std::move(aTask));
}

void RcpHost::RegisterResetHandler(std::function<void(void)> aHandler)
{
    mResetHandlers.emplace_back(std::move(aHandler));
}

void RcpHost::AddThreadStateChangedCallback(ThreadStateChangedCallback aCallback)
{
    mThreadStateChangedCallbacks.emplace_back(std::move(aCallback));
}

void RcpHost::Reset(void)
{
    gPlatResetReason = OT_PLAT_RESET_REASON_SOFTWARE;

    otSysDeinit();
    mInstance = nullptr;

    Init();
    for (auto &handler : mResetHandlers)
    {
        handler();
    }
    mEnableAutoAttach = true;
}

const char *RcpHost::GetThreadVersion(void)
{
    const char *version;

    switch (otThreadGetVersion())
    {
    case kThreadVersion11:
        version = "1.1.1";
        break;
    case kThreadVersion12:
        version = "1.2.0";
        break;
    case kThreadVersion13:
        version = "1.3.0";
        break;
    case kThreadVersion14:
        version = "1.4.0";
        break;
    default:
        otbrLogEmerg("Unexpected thread version %hu", otThreadGetVersion());
        exit(-1);
    }
    return version;
}

void RcpHost::CheckSensorsState(std::vector<uint16_t> devicesMrloc16)
{
    otError      error = OT_ERROR_NONE;
    otChildInfo  childInfo;
    uint16_t     childId, maxChildren;
    otRouterInfo routerInfo;
    uint16_t     routerId;
    uint8_t      maxRouterId;

    maxChildren = otThreadGetMaxAllowedChildren(mInstance);

    for (uint16_t i = 0; i < maxChildren; i++)
    {
        error = otThreadGetChildInfoByIndex(mInstance, i, &childInfo);
        if (error == OT_ERROR_NONE)
        {
            childId = childInfo.mRloc16;
            std::cerr << "childId: " << childId << std::endl;
            devicesMrloc16.push_back(childId);
        }
    }

    maxRouterId = otThreadGetMaxRouterId(mInstance);
    for (uint8_t i = 0; i < maxRouterId; i++)
    {
        error = otThreadGetRouterInfo(mInstance, i, &routerInfo);
        if (error == OT_ERROR_NONE)
        {
            routerId = routerInfo.mRloc16;
            std::cerr << "routerId: " << routerId << std::endl;
            devicesMrloc16.push_back(routerId);
        }
    }

    Database db("home/pi/coap.db");
    if (db.connect())
    {
        db.SetSensorsState(devicesMrloc16);
    }
    else
    {
        otbrLogEmerg("impossibile connettersi al database\n");
    }
}

void RcpHost::Join(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs, const AsyncResultReceiver &aReceiver)
{
    OT_UNUSED_VARIABLE(aActiveOpDatasetTlvs);

    // TODO: Implement Join under RCP mode.
    mTaskRunner.Post([aReceiver](void) { aReceiver(OT_ERROR_NOT_IMPLEMENTED, "Not implemented!"); });
}

void RcpHost::Leave(const AsyncResultReceiver &aReceiver)
{
    // TODO: Implement Leave under RCP mode.
    mTaskRunner.Post([aReceiver](void) { aReceiver(OT_ERROR_NOT_IMPLEMENTED, "Not implemented!"); });
}

void RcpHost::ScheduleMigration(const otOperationalDatasetTlvs &aPendingOpDatasetTlvs,
                                const AsyncResultReceiver       aReceiver)
{
    OT_UNUSED_VARIABLE(aPendingOpDatasetTlvs);

    // TODO: Implement ScheduleMigration under RCP mode.
    mTaskRunner.Post([aReceiver](void) { aReceiver(OT_ERROR_NOT_IMPLEMENTED, "Not implemented!"); });
}

/*
 * Provide, if required an "otPlatLog()" function
 */
extern "C" void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    OT_UNUSED_VARIABLE(aLogRegion);

    otbrLogLevel otbrLogLevel = RcpHost::ConvertToOtbrLogLevel(aLogLevel);

    va_list ap;
    va_start(ap, aFormat);
    otbrLogvNoFilter(otbrLogLevel, aFormat, ap);
    va_end(ap);
}

extern "C" void otPlatLogHandleLevelChanged(otLogLevel aLogLevel)
{
    otbrLogSetLevel(RcpHost::ConvertToOtbrLogLevel(aLogLevel));
    otbrLogInfo("OpenThread log level changed to %d", aLogLevel);
}

} // namespace Ncp
} // namespace otbr
