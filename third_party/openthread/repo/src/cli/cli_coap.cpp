/*
 *  Copyright (c) 2017, The OpenThread Authors.
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
 *   This file implements a simple CLI for the CoAP service.
 */

#define kLogModuleName "CliCoap"

#include "cli_coap.hpp"

#if OPENTHREAD_CONFIG_COAP_API_ENABLE

#include <openthread/random_noncrypto.h>

#include <ctype.h>

#include "cli/cli.hpp"

namespace ot {
namespace Cli {

Coap::Coap(otInstance *aInstance, OutputImplementer &aOutputImplementer)
    : Utils(aInstance, aOutputImplementer)
    , mUseDefaultRequestTxParameters(true)
    , mUseDefaultResponseTxParameters(true)
#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
    , mObserveSerial(0)
    , mRequestTokenLength(0)
    , mSubscriberTokenLength(0)
    , mSubscriberConfirmableNotifications(false)
#endif
#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
    , mBlockCount(1)
#endif
{
    ClearAllBytes(mResource);
#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
    ClearAllBytes(mRequestAddr);
    ClearAllBytes(mSubscriberSock);
    ClearAllBytes(mRequestToken);
    ClearAllBytes(mSubscriberToken);
    ClearAllBytes(mRequestUri);
#endif
    ClearAllBytes(mUriPath);
    strncpy(mResourceContent, "0", sizeof(mResourceContent));
    mResourceContent[sizeof(mResourceContent) - 1] = '\0';
}

#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
otError Coap::CancelResourceSubscription(void)
{
    otError       error   = OT_ERROR_NONE;
    otMessage    *message = nullptr;
    otMessageInfo messageInfo;

    ClearAllBytes(messageInfo);
    messageInfo.mPeerAddr = mRequestAddr;
    messageInfo.mPeerPort = OT_DEFAULT_COAP_PORT;

    VerifyOrExit(mRequestTokenLength != 0, error = OT_ERROR_INVALID_STATE);

    message = otCoapNewMessage(GetInstancePtr(), nullptr);
    VerifyOrExit(message != nullptr, error = OT_ERROR_NO_BUFS);

    otCoapMessageInit(message, OT_COAP_TYPE_CONFIRMABLE, OT_COAP_CODE_GET);

    SuccessOrExit(error = otCoapMessageSetToken(message, mRequestToken, mRequestTokenLength));
    SuccessOrExit(error = otCoapMessageAppendObserveOption(message, 1));
    SuccessOrExit(error = otCoapMessageAppendUriPathOptions(message, mRequestUri));
    SuccessOrExit(error = otCoapSendRequest(GetInstancePtr(), message, &messageInfo, &Coap::HandleResponse, this));

    ClearAllBytes(mRequestAddr);
    ClearAllBytes(mRequestUri);
    mRequestTokenLength = 0;

exit:

    if ((error != OT_ERROR_NONE) && (message != nullptr))
    {
        otMessageFree(message);
    }

    return error;
}

void Coap::CancelSubscriber(void)
{
    ClearAllBytes(mSubscriberSock);
    mSubscriberTokenLength = 0;
}
#endif // OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE

void Coap::PrintPayload(otMessage *aMessage)
{
    uint8_t  buf[kMaxBufferSize];
    uint16_t bytesToPrint;
    uint16_t bytesPrinted = 0;
    uint16_t length       = otMessageGetLength(aMessage) - otMessageGetOffset(aMessage);

    if (length > 0)
    {
        OutputFormat(" with payload: ");

        while (length > 0)
        {
            bytesToPrint = Min(length, static_cast<uint16_t>(sizeof(buf)));
            otMessageRead(aMessage, otMessageGetOffset(aMessage) + bytesPrinted, buf, bytesToPrint);

            OutputBytes(buf, static_cast<uint8_t>(bytesToPrint));

            length -= bytesToPrint;
            bytesPrinted += bytesToPrint;
        }
    }

    OutputNewLine();
}

#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
/**
 * @cli coap cancel
 * @code
 * coap cancel
 * Done
 * @endcode
 * @par
 * Cancels an existing observation subscription to a remote resource on the CoAP server.
 * @note This command is available only when `OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE` is set.
 * @csa{coap observe}
 */
template <> otError Coap::Process<Cmd("cancel")>(Arg aArgs[])
{
    OT_UNUSED_VARIABLE(aArgs);

    return CancelResourceSubscription();
}
#endif

/**
 * @cli coap resource (get,set)
 * @code
 * coap resource test-resource
 * Done
 * @endcode
 * @code
 * coap resource
 * test-resource
 * Done
 * @endcode
 * @cparam coap resource [@ca{uri-path}]
 * @par
 * Gets or sets the URI path of the CoAP server resource.
 * @sa otCoapAddResource
 * @sa otCoapAddBlockWiseResource
 */
template <> otError Coap::Process<Cmd("resource")>(Arg aArgs[])
{
    otError error = OT_ERROR_NONE;

    if (!aArgs[0].IsEmpty())
    {
        VerifyOrExit(aArgs[0].GetLength() < kMaxUriLength, error = OT_ERROR_INVALID_ARGS);

        OutputLine("triggerata resource\n");

        mResource.mUriPath = mUriPath;
        mResource.mContext = this;
        mResource.mHandler = &Coap::HandleRequest;

#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
        mResource.mReceiveHook  = &Coap::BlockwiseReceiveHook;
        mResource.mTransmitHook = &Coap::BlockwiseTransmitHook;

        if (!aArgs[1].IsEmpty())
        {
            SuccessOrExit(error = aArgs[1].ParseAsUint32(mBlockCount));
        }
#endif

        strncpy(mUriPath, aArgs[0].GetCString(), sizeof(mUriPath) - 1);

#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
        OutputLine("blockwise\n");
        otCoapAddBlockWiseResource(GetInstancePtr(), &mResource);
#else
        OutputLine("normal\n");
        otCoapAddResource(GetInstancePtr(), &mResource);
#endif
    }
    else
    {
        OutputLine("%s", mResource.mUriPath != nullptr ? mResource.mUriPath : "");
    }

exit:
    return error;
}

/**
 * @cli coap set
 * @code
 * coap set Testing123
 * Done
 * @endcode
 * @cparam coap set @ca{new-content}
 * @par
 * Sets the content sent by the resource on the CoAP server.
 * If a CoAP client is observing the resource, a notification is sent to that client.
 * @csa{coap observe}
 * @sa otCoapMessageInit
 * @sa otCoapNewMessage
 */
template <> otError Coap::Process<Cmd("set")>(Arg aArgs[])
{
#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
    otMessage    *notificationMessage = nullptr;
    otMessageInfo messageInfo;
#endif
    otError error = OT_ERROR_NONE;

    if (!aArgs[0].IsEmpty())
    {
        VerifyOrExit(aArgs[0].GetLength() < sizeof(mResourceContent), error = OT_ERROR_INVALID_ARGS);
        strncpy(mResourceContent, aArgs[0].GetCString(), sizeof(mResourceContent));
        mResourceContent[sizeof(mResourceContent) - 1] = '\0';

#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
        if (mSubscriberTokenLength > 0)
        {
            // Notify the subscriber
            ClearAllBytes(messageInfo);
            messageInfo.mPeerAddr = mSubscriberSock.mAddress;
            messageInfo.mPeerPort = mSubscriberSock.mPort;

            OutputFormat("sending coap notification to ");
            OutputIp6AddressLine(mSubscriberSock.mAddress);

            notificationMessage = otCoapNewMessage(GetInstancePtr(), nullptr);
            VerifyOrExit(notificationMessage != nullptr, error = OT_ERROR_NO_BUFS);

            otCoapMessageInit(
                notificationMessage,
                ((mSubscriberConfirmableNotifications) ? OT_COAP_TYPE_CONFIRMABLE : OT_COAP_TYPE_NON_CONFIRMABLE),
                OT_COAP_CODE_CONTENT);

            SuccessOrExit(error = otCoapMessageSetToken(notificationMessage, mSubscriberToken, mSubscriberTokenLength));
            SuccessOrExit(error = otCoapMessageAppendObserveOption(notificationMessage, mObserveSerial++));
            SuccessOrExit(error = otCoapMessageSetPayloadMarker(notificationMessage));
            SuccessOrExit(error = otMessageAppend(notificationMessage, mResourceContent,
                                                  static_cast<uint16_t>(strlen(mResourceContent))));

            SuccessOrExit(error = otCoapSendRequest(GetInstancePtr(), notificationMessage, &messageInfo,
                                                    &Coap::HandleNotificationResponse, this));
        }
#endif // OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
    }
    else
    {
        OutputLine("%s", mResourceContent);
    }

exit:

#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
    if ((error != OT_ERROR_NONE) && (notificationMessage != nullptr))
    {
        otMessageFree(notificationMessage);
    }
#endif

    return error;
}

/**
 * @cli coap start
 * @code
 * coap start
 * Done
 * @endcode
 * @par
 * Starts the CoAP server. @moreinfo{@coap}.
 * @sa otCoapStart
 */
template <> otError Coap::Process<Cmd("start")>(Arg aArgs[])
{
    OT_UNUSED_VARIABLE(aArgs);

    return otCoapStart(GetInstancePtr(), OT_DEFAULT_COAP_PORT);
}

/**
 * @cli coap stop
 * @code
 * coap stop
 * Done
 * @endcode
 * @par api_copy
 * #otCoapStop
 */
template <> otError Coap::Process<Cmd("stop")>(Arg aArgs[])
{
    OT_UNUSED_VARIABLE(aArgs);

#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
    otCoapRemoveBlockWiseResource(GetInstancePtr(), &mResource);
#else
    otCoapRemoveResource(GetInstancePtr(), &mResource);
#endif

    return otCoapStop(GetInstancePtr());
}

/**
 * @cli coap parameters(get,set)
 * @code
 * coap parameters request
 * Transmission parameters for request:
 * ACK_TIMEOUT=1000 ms, ACK_RANDOM_FACTOR=255/254, MAX_RETRANSMIT=2
 * Done
 * @endcode
 * @code
 * coap parameters request default
 * Transmission parameters for request:
 * default
 * Done
 * @endcode
 * @code
 * coap parameters request 1000 255 254 2
 * Transmission parameters for request:
 * ACK_TIMEOUT=1000 ms, ACK_RANDOM_FACTOR=255/254, MAX_RETRANSMIT=2
 * Done
 * @endcode
 * @cparam coap parameters @ca{type} [@ca{default} | <!--
 * -->@ca{ack_timeout ack_random_factor_numerator <!--
 * -->ack_random_factor_denominator max_retransmit}]
 *   * `type`: `request` for CoAP requests, or `response` for CoAP responses.
        If no more parameters are given, the command prints the current configuration.
 *   * `default`: Sets the transmission parameters to
        the following default values:
 *       * `ack_timeout`: 2000 milliseconds
 *       * `ack_random_factor_numerator`: 3
 *       * `ack_random_factor_denominator`: 2
 *       * `max_retransmit`: 4
 *   * `ack_timeout`: The `ACK_TIMEOUT` (0-UINT32_MAX) in milliseconds.
       Refer to RFC7252.
 *   * `ack_random_factor_numerator`:
       The `ACK_RANDOM_FACTOR` numerator, with possible values
       of 0-255. Refer to RFC7252.
 *   * `ack_random_factor_denominator`:
 *     The `ACK_RANDOM_FACTOR` denominator, with possible values
 *     of 0-255. Refer to RFC7252.
 *   * `max_retransmit`: The `MAX_RETRANSMIT` (0-255). Refer to RFC7252.
 * @par
 * Gets current CoAP parameter values if the command is run with no optional
 * parameters.
 * @par
 * Sets the CoAP parameters either to their default values or to the values
 * you specify, depending on the syntax chosen.
 */
template <> otError Coap::Process<Cmd("parameters")>(Arg aArgs[])
{
    otError             error = OT_ERROR_NONE;
    bool               *defaultTxParameters;
    otCoapTxParameters *txParameters;

    if (aArgs[0] == "request")
    {
        txParameters        = &mRequestTxParameters;
        defaultTxParameters = &mUseDefaultRequestTxParameters;
    }
    else if (aArgs[0] == "response")
    {
        txParameters        = &mResponseTxParameters;
        defaultTxParameters = &mUseDefaultResponseTxParameters;
    }
    else
    {
        ExitNow(error = OT_ERROR_INVALID_ARGS);
    }

    if (!aArgs[1].IsEmpty())
    {
        if (aArgs[1] == "default")
        {
            *defaultTxParameters = true;
        }
        else
        {
            SuccessOrExit(error = aArgs[1].ParseAsUint32(txParameters->mAckTimeout));
            SuccessOrExit(error = aArgs[2].ParseAsUint8(txParameters->mAckRandomFactorNumerator));
            SuccessOrExit(error = aArgs[3].ParseAsUint8(txParameters->mAckRandomFactorDenominator));
            SuccessOrExit(error = aArgs[4].ParseAsUint8(txParameters->mMaxRetransmit));

            VerifyOrExit(txParameters->mAckRandomFactorNumerator > txParameters->mAckRandomFactorDenominator,
                         error = OT_ERROR_INVALID_ARGS);

            *defaultTxParameters = false;
        }
    }

    OutputLine("Transmission parameters for %s:", aArgs[0].GetCString());

    if (*defaultTxParameters)
    {
        OutputLine("default");
    }
    else
    {
        OutputLine("ACK_TIMEOUT=%lu ms, ACK_RANDOM_FACTOR=%u/%u, MAX_RETRANSMIT=%u", ToUlong(txParameters->mAckTimeout),
                   txParameters->mAckRandomFactorNumerator, txParameters->mAckRandomFactorDenominator,
                   txParameters->mMaxRetransmit);
    }

exit:
    return error;
}

/**
 * @cli coap get
 * @code
 * coap get fdde:ad00:beef:0:2780:9423:166c:1aac test-resource
 * Done
 * @endcode
 * @code
 * coap get fdde:ad00:beef:0:2780:9423:166c:1aac test-resource block-1024
 * Done
 * @endcode
 * @cparam coap get @ca{address} @ca{uri-path} [@ca{type}]
 *   * `address`: IPv6 address of the CoAP server.
 *   * `uri-path`: URI path of the resource.
 *   * `type`:
 *       * `con`: Confirmable
 *       * `non-con`: Non-confirmable (default)
 *       * `block-`: Use this option, followed by the block-wise value,
 *          if the response should be transferred block-wise. Valid
 *          values are: `block-16`, `block-32`, `block-64`, `block-128`,
 *          `block-256`, `block-512`, or `block-1024`.
 * @par
 * Gets information about the specified CoAP resource on the CoAP server.
 */
template <> otError Coap::Process<Cmd("get")>(Arg aArgs[]) { return ProcessRequest(aArgs, OT_COAP_CODE_GET); }

/**
 * @cli coap post
 * @code
 * coap post fdde:ad00:beef:0:2780:9423:166c:1aac test-resource con hellothere
 * Done
 * @endcode
 * @code
 * coap post fdde:ad00:beef:0:2780:9423:166c:1aac test-resource block-1024 10
 * Done
 * @endcode
 * @cparam coap post @ca{address} @ca{uri-path} [@ca{type}] [@ca{payload}]
 *   * `address`: IPv6 address of the CoAP server.
 *   * `uri-path`: URI path of the resource.
 *   * `type`:
 *         * `con`: Confirmable
 *         * `non-con`: Non-confirmable (default)
 *         * `block-`: Use this option, followed by the block-wise value,
 *            to send blocks with a randomly generated number of bytes
 *            for the payload. Valid values are:
 *            `block-16`, `block-32`, `block-64`, `block-128`,
 *            `block-256`, `block-512`, or `block-1024`.
 *   * `payload`: CoAP payload request, which if used is either a string or an
 *     integer, depending on the `type`. If the `type` is `con` or `non-con`,
 *     the `payload` parameter is optional. If you leave out the
 *     `payload` parameter, an empty payload is sent. However, If you use the
 *     `payload` parameter, its value must be a string, such as
 *     `hellothere`.  If the `type` is `block-`,
 *     the value of the`payload` parameter must be an integer that specifies
 *     the number of blocks to send. The `block-` type requires
 *     `OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE` to be set.
 * @par
 * Creates the specified CoAP resource. @moreinfo{@coap}.
 */
template <> otError Coap::Process<Cmd("post")>(Arg aArgs[]) { return ProcessRequest(aArgs, OT_COAP_CODE_POST); }

/**
 * @cli coap put
 * @code
 * coap put fdde:ad00:beef:0:2780:9423:166c:1aac test-resource con hellothere
 * Done
 * @endcode
 * @code
 * coap put fdde:ad00:beef:0:2780:9423:166c:1aac test-resource block-1024 10
 * Done
 * @endcode
 * @cparam coap put @ca{address} @ca{uri-path} [@ca{type}] [@ca{payload}]
 *   * `address`: IPv6 address of the CoAP server.
 *   * `uri-path`: URI path of the resource.
 *   * `type`:
 *         * `con`: Confirmable
 *         * `non-con`: Non-confirmable (default)
 *         * `block-`: Use this option, followed by the block-wise value,
 *            to send blocks with a randomly generated number of bytes
 *            for the payload. Valid values are:
 *            `block-16`, `block-32`, `block-64`, `block-128`,
 *            `block-256`, `block-512`, or `block-1024`.
 *   * `payload`: CoAP payload request, which if used is either a string or an
 *     integer, depending on the `type`. If the `type` is `con` or `non-con`,
 *     the `payload` parameter is optional. If you leave out the
 *     `payload` parameter, an empty payload is sent. However, If you use the
 *     `payload` parameter, its value must be a string, such as
 *     `hellothere`. If the `type` is `block-`,
 *     the value of the`payload` parameter must be an integer that specifies
 *     the number of blocks to send. The `block-` type requires
 *     `OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE` to be set.
 * @par
 * Modifies the specified CoAP resource. @moreinfo{@coap}.
 */
template <> otError Coap::Process<Cmd("put")>(Arg aArgs[]) { return ProcessRequest(aArgs, OT_COAP_CODE_PUT); }

/**
 * @cli coap delete
 * @code
 * coap delete fdde:ad00:beef:0:2780:9423:166c:1aac test-resource con hellothere
 * Done
 * @endcode
 * @cparam coap delete @ca{address} @ca{uri-path} [@ca{type}] [@ca{payload}]
 *   * `address`: IPv6 address of the CoAP server.
 *   * `uri-path`: URI path of the resource.
 *   * `type`:
 *       * `con`: Confirmable
 *       * `non-con`: Non-confirmable (default)
 *   * `payload`: The CoAP payload string. For example, `hellothere`.
 *  @par
 *  Deletes the specified CoAP resource.
 */
template <> otError Coap::Process<Cmd("delete")>(Arg aArgs[]) { return ProcessRequest(aArgs, OT_COAP_CODE_DELETE); }

#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
/**
 * @cli coap observe
 * @code
 * coap observe fdde:ad00:beef:0:2780:9423:166c:1aac test-resource
 * Done
 * @endcode
 * @cparam coap observe @ca{address} @ca{uri-path} [@ca{type}]
 *   * `address`: IPv6 address of the CoAP server.
 *   * `uri-path`: URI path of the resource.
 *   * `type`:
 *       * `con`: Confirmable
 *       * `non-con`: Non-confirmable (default).
 * @par
 * Triggers a subscription request which allows the CoAP client to
 * observe the specified resource on the CoAP server for possible changes
 * in its state.
 * @note This command is available only when `OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE` is set.
 */
template <> otError Coap::Process<Cmd("observe")>(Arg aArgs[])
{
    return ProcessRequest(aArgs, OT_COAP_CODE_GET, /* aCoapObserve */ true);
}
#endif

#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
otError Coap::ProcessRequest(Arg aArgs[], otCoapCode aCoapCode, bool aCoapObserve)
#else
otError Coap::ProcessRequest(Arg aArgs[], otCoapCode aCoapCode)
#endif
{
    otError       error   = OT_ERROR_NONE;
    otMessage    *message = nullptr;
    otMessageInfo messageInfo;
    uint16_t      payloadLength    = 0;
    char         *uriQueryStartPtr = nullptr;

    // Default parameters
    char         coapUri[kMaxUriLength] = "test";
    otCoapType   coapType               = OT_COAP_TYPE_NON_CONFIRMABLE;
    otIp6Address coapDestinationIp;
#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
    bool           coapBlock     = false;
    otCoapBlockSzx coapBlockSize = OT_COAP_OPTION_BLOCK_SZX_16;
    BlockType      coapBlockType = (aCoapCode == OT_COAP_CODE_GET) ? kBlockType2 : kBlockType1;
#endif

#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE && OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
    if (aCoapObserve)
    {
        coapBlockType = kBlockType1;
    }
#endif

    SuccessOrExit(error = aArgs[0].ParseAsIp6Address(coapDestinationIp));

    VerifyOrExit(!aArgs[1].IsEmpty(), error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(aArgs[1].GetLength() < sizeof(coapUri), error = OT_ERROR_INVALID_ARGS);
    strcpy(coapUri, aArgs[1].GetCString());

    // CoAP-Type
    if (!aArgs[2].IsEmpty())
    {
        if (aArgs[2] == "con")
        {
            coapType = OT_COAP_TYPE_CONFIRMABLE;
        }
#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
        else if (aArgs[2] == "block-16")
        {
            coapType      = OT_COAP_TYPE_CONFIRMABLE;
            coapBlock     = true;
            coapBlockSize = OT_COAP_OPTION_BLOCK_SZX_16;
        }
        else if (aArgs[2] == "block-32")
        {
            coapType      = OT_COAP_TYPE_CONFIRMABLE;
            coapBlock     = true;
            coapBlockSize = OT_COAP_OPTION_BLOCK_SZX_32;
        }
        else if (aArgs[2] == "block-64")
        {
            coapType      = OT_COAP_TYPE_CONFIRMABLE;
            coapBlock     = true;
            coapBlockSize = OT_COAP_OPTION_BLOCK_SZX_64;
        }
        else if (aArgs[2] == "block-128")
        {
            coapType      = OT_COAP_TYPE_CONFIRMABLE;
            coapBlock     = true;
            coapBlockSize = OT_COAP_OPTION_BLOCK_SZX_128;
        }
        else if (aArgs[2] == "block-256")
        {
            coapType      = OT_COAP_TYPE_CONFIRMABLE;
            coapBlock     = true;
            coapBlockSize = OT_COAP_OPTION_BLOCK_SZX_256;
        }
        else if (aArgs[2] == "block-512")
        {
            coapType      = OT_COAP_TYPE_CONFIRMABLE;
            coapBlock     = true;
            coapBlockSize = OT_COAP_OPTION_BLOCK_SZX_512;
        }
        else if (aArgs[2] == "block-1024")
        {
            coapType      = OT_COAP_TYPE_CONFIRMABLE;
            coapBlock     = true;
            coapBlockSize = OT_COAP_OPTION_BLOCK_SZX_1024;
        }
#endif // OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
    }

#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
    if (aCoapObserve && mRequestTokenLength)
    {
        // New observe request, cancel any existing observation
        SuccessOrExit(error = CancelResourceSubscription());
    }
#endif

    message = otCoapNewMessage(GetInstancePtr(), nullptr);
    VerifyOrExit(message != nullptr, error = OT_ERROR_NO_BUFS);

    otCoapMessageInit(message, coapType, aCoapCode);
    otCoapMessageGenerateToken(message, OT_COAP_DEFAULT_TOKEN_LENGTH);

#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
    if (aCoapObserve)
    {
        SuccessOrExit(error = otCoapMessageAppendObserveOption(message, 0));
    }
#endif

    uriQueryStartPtr = const_cast<char *>(StringFind(coapUri, '?'));

    if (uriQueryStartPtr == nullptr)
    {
        // "?" doesn't present in URI --> contains only URI path parts
        SuccessOrExit(error = otCoapMessageAppendUriPathOptions(message, coapUri));
    }
    else
    {
        // "?" presents in URI --> contains URI path AND URI query parts
        *uriQueryStartPtr++ = '\0';
        SuccessOrExit(error = otCoapMessageAppendUriPathOptions(message, coapUri));
        SuccessOrExit(error = otCoapMessageAppendUriQueryOptions(message, uriQueryStartPtr));
    }

#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
    if (coapBlock)
    {
        if (coapBlockType == kBlockType1)
        {
            SuccessOrExit(error = otCoapMessageAppendBlock1Option(message, 0, true, coapBlockSize));
        }
        else
        {
            SuccessOrExit(error = otCoapMessageAppendBlock2Option(message, 0, false, coapBlockSize));
        }
    }
#endif

    if (!aArgs[3].IsEmpty())
    {
#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
        if (coapBlock)
        {
            SuccessOrExit(error = aArgs[3].ParseAsUint32(mBlockCount));
        }
        else
        {
#endif
            payloadLength = aArgs[3].GetLength();

            if (payloadLength > 0)
            {
                SuccessOrExit(error = otCoapMessageSetPayloadMarker(message));
            }
#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
        }
#endif
    }

    // Embed content into message if given
    if (payloadLength > 0)
    {
        SuccessOrExit(error = otMessageAppend(message, aArgs[3].GetCString(), payloadLength));
    }

    ClearAllBytes(messageInfo);
    messageInfo.mPeerAddr = coapDestinationIp;
    messageInfo.mPeerPort = OT_DEFAULT_COAP_PORT;

#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
    if (aCoapObserve)
    {
        // Make a note of the message details for later so we can cancel it later.
        memcpy(&mRequestAddr, &coapDestinationIp, sizeof(mRequestAddr));
        mRequestTokenLength = otCoapMessageGetTokenLength(message);
        memcpy(mRequestToken, otCoapMessageGetToken(message), mRequestTokenLength);
        // Use `memcpy` instead of `strncpy` here because GCC will give warnings for `strncpy` when the dest's length is
        // not bigger than the src's length.
        memcpy(mRequestUri, coapUri, sizeof(mRequestUri) - 1);
    }
#endif

    if ((coapType == OT_COAP_TYPE_CONFIRMABLE) || (aCoapCode == OT_COAP_CODE_GET))
    {
#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
        if (coapBlock)
        {
            if (aCoapCode == OT_COAP_CODE_PUT || aCoapCode == OT_COAP_CODE_POST)
            {
                SuccessOrExit(error = otCoapMessageSetPayloadMarker(message));
            }
            error = otCoapSendRequestBlockWiseWithParameters(GetInstancePtr(), message, &messageInfo,
                                                             &Coap::HandleResponse, this, GetRequestTxParameters(),
                                                             Coap::BlockwiseTransmitHook, Coap::BlockwiseReceiveHook);
        }
        else
        {
#endif
            error = otCoapSendRequestWithParameters(GetInstancePtr(), message, &messageInfo, &Coap::HandleResponse,
                                                    this, GetRequestTxParameters());
#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
        }
#endif
    }
    else
    {
        error = otCoapSendRequestWithParameters(GetInstancePtr(), message, &messageInfo, nullptr, nullptr,
                                                GetResponseTxParameters());
    }

exit:

    if ((error != OT_ERROR_NONE) && (message != nullptr))
    {
        otMessageFree(message);
    }

    return error;
}

otError Coap::Process(Arg aArgs[])
{
#define CmdEntry(aCommandString)                            \
    {                                                       \
        aCommandString, &Coap::Process<Cmd(aCommandString)> \
    }

    static constexpr Command kCommands[] = {
#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
        CmdEntry("cancel"),
#endif
        CmdEntry("delete"),
        CmdEntry("get"),
#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
        CmdEntry("observe"),
#endif
        CmdEntry("parameters"),
        CmdEntry("post"),
        CmdEntry("put"),
        CmdEntry("resource"),
        CmdEntry("set"),
        CmdEntry("start"),
        CmdEntry("stop"),
    };

#undef CmdEntry

    static_assert(BinarySearch::IsSorted(kCommands), "kCommands is not sorted");

    otError        error = OT_ERROR_INVALID_COMMAND;
    const Command *command;

    if (aArgs[0].IsEmpty() || (aArgs[0] == "help"))
    {
        OutputCommandTable(kCommands);
        ExitNow(error = aArgs[0].IsEmpty() ? OT_ERROR_INVALID_ARGS : OT_ERROR_NONE);
    }

    command = BinarySearch::Find(aArgs[0].GetCString(), kCommands);
    VerifyOrExit(command != nullptr);

    error = (this->*command->mHandler)(aArgs + 1);

exit:
    return error;
}

void Coap::HandleRequest(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
    

    static_cast<Coap *>(aContext)->HandleRequest(aMessage, aMessageInfo);
}

void Coap::HandleRequest(otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
    otError    error           = OT_ERROR_NONE;
    otMessage *responseMessage = nullptr;
    otCoapCode responseCode    = OT_COAP_CODE_EMPTY;
#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
    uint64_t observe        = 0;
    bool     observePresent = false;
#endif
#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
    uint64_t blockValue   = 0;
    bool     blockPresent = false;
#endif
#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE || OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
    otCoapOptionIterator iterator;
#endif


    OutputFormat("coap request from ");
    OutputIp6Address(aMessageInfo->mPeerAddr);
    OutputFormat(" ");

    switch (otCoapMessageGetCode(aMessage))
    {
    case OT_COAP_CODE_GET:
        OutputFormat("GET");
#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE || OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
        SuccessOrExit(error = otCoapOptionIteratorInit(&iterator, aMessage));
#endif
#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
        if (otCoapOptionIteratorGetFirstOptionMatching(&iterator, OT_COAP_OPTION_OBSERVE) != nullptr)
        {
            SuccessOrExit(error = otCoapOptionIteratorGetOptionUintValue(&iterator, &observe));
            observePresent = true;

            OutputFormat(" OBS=");
            OutputUint64(observe);
        }
#endif
#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
        if (otCoapOptionIteratorGetFirstOptionMatching(&iterator, OT_COAP_OPTION_BLOCK2) != nullptr)
        {
            SuccessOrExit(error = otCoapOptionIteratorGetOptionUintValue(&iterator, &blockValue));
            blockPresent = true;
        }
#endif
        break;

    case OT_COAP_CODE_DELETE:
        OutputFormat("DELETE");
        break;

    case OT_COAP_CODE_PUT:
        OutputFormat("PUT");
        break;

    case OT_COAP_CODE_POST:
        OutputFormat("POST");
        break;

    default:
        OutputLine("Undefined");
        ExitNow(error = OT_ERROR_PARSE);
    }

    PrintPayload(aMessage);

    if (otCoapMessageGetType(aMessage) == OT_COAP_TYPE_CONFIRMABLE ||
        otCoapMessageGetCode(aMessage) == OT_COAP_CODE_GET)
    {
#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
        if (observePresent && (mSubscriberTokenLength > 0) && (observe == 0))
        {
            // There is already a subscriber
            responseCode = OT_COAP_CODE_SERVICE_UNAVAILABLE;
        }
        else
#endif
            if (otCoapMessageGetCode(aMessage) == OT_COAP_CODE_GET)
        {
            responseCode = OT_COAP_CODE_CONTENT;
#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
            if (observePresent)
            {
                if (observe == 0)
                {
                    // New subscriber
                    OutputLine("Subscribing client");
                    mSubscriberSock.mAddress = aMessageInfo->mPeerAddr;
                    mSubscriberSock.mPort    = aMessageInfo->mPeerPort;
                    mSubscriberTokenLength   = otCoapMessageGetTokenLength(aMessage);
                    memcpy(mSubscriberToken, otCoapMessageGetToken(aMessage), mSubscriberTokenLength);

                    /*
                     * Implementer note.
                     *
                     * Here, we try to match a confirmable GET request with confirmable
                     * notifications, however this is not a requirement of RFC7641:
                     * the server can send notifications of either type regardless of
                     * what the client used to subscribe initially.
                     */
                    mSubscriberConfirmableNotifications = (otCoapMessageGetType(aMessage) == OT_COAP_TYPE_CONFIRMABLE);
                }
                else if (observe == 1)
                {
                    // See if it matches our subscriber token
                    if ((otCoapMessageGetTokenLength(aMessage) == mSubscriberTokenLength) &&
                        (memcmp(otCoapMessageGetToken(aMessage), mSubscriberToken, mSubscriberTokenLength) == 0))
                    {
                        // Unsubscribe request
                        CancelSubscriber();
                    }
                }
            }
#endif // OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
        }
        else
        {
            responseCode = OT_COAP_CODE_CHANGED;
        }

        responseMessage = otCoapNewMessage(GetInstancePtr(), nullptr);
        VerifyOrExit(responseMessage != nullptr, error = OT_ERROR_NO_BUFS);

        SuccessOrExit(
            error = otCoapMessageInitResponse(responseMessage, aMessage, OT_COAP_TYPE_ACKNOWLEDGMENT, responseCode));

        if (responseCode == OT_COAP_CODE_CONTENT)
        {
#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
            if (observePresent && (observe == 0))
            {
                SuccessOrExit(error = otCoapMessageAppendObserveOption(responseMessage, mObserveSerial++));
            }
#endif
#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
            if (blockPresent)
            {
                SuccessOrExit(error = otCoapMessageAppendBlock2Option(responseMessage,
                                                                      static_cast<uint32_t>(blockValue >> 4), true,
                                                                      static_cast<otCoapBlockSzx>(blockValue & 0x7)));
                SuccessOrExit(error = otCoapMessageSetPayloadMarker(responseMessage));
            }
            else
            {
#endif
                SuccessOrExit(error = otCoapMessageSetPayloadMarker(responseMessage));
                SuccessOrExit(error = otMessageAppend(responseMessage, mResourceContent,
                                                      static_cast<uint16_t>(strlen(mResourceContent))));
#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
            }
#endif
        }

#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
        if (blockPresent)
        {
            SuccessOrExit(error = otCoapSendResponseBlockWiseWithParameters(GetInstancePtr(), responseMessage,
                                                                            aMessageInfo, GetResponseTxParameters(),
                                                                            this, mResource.mTransmitHook));
        }
        else
        {
#endif
            SuccessOrExit(error = otCoapSendResponseWithParameters(GetInstancePtr(), responseMessage, aMessageInfo,
                                                                   GetResponseTxParameters()));
#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
        }
#endif
    }

exit:

    if (error != OT_ERROR_NONE)
    {
        if (responseMessage != nullptr)
        {
            OutputLine("coap send response error %d: %s", error, otThreadErrorToString(error));
            otMessageFree(responseMessage);
        }
    }
    else if (responseCode >= OT_COAP_CODE_RESPONSE_MIN)
    {
        OutputLine("coap response sent");
    }
}

#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
void Coap::HandleNotificationResponse(void                *aContext,
                                      otMessage           *aMessage,
                                      const otMessageInfo *aMessageInfo,
                                      otError              aError)
{
    static_cast<Coap *>(aContext)->HandleNotificationResponse(aMessage, aMessageInfo, aError);
}

void Coap::HandleNotificationResponse(otMessage *aMessage, const otMessageInfo *aMessageInfo, otError aError)
{
    OT_UNUSED_VARIABLE(aMessage);

    switch (aError)
    {
    case OT_ERROR_NONE:
        if (aMessageInfo != nullptr)
        {
            OutputFormat("Received ACK in reply to notification from ");
            OutputIp6AddressLine(aMessageInfo->mPeerAddr);
        }
        break;

    default:
        OutputLine("coap receive notification response error %d: %s", aError, otThreadErrorToString(aError));
        CancelSubscriber();
        break;
    }
}
#endif // OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE

void Coap::HandleResponse(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo, otError aError)
{
    static_cast<Coap *>(aContext)->HandleResponse(aMessage, aMessageInfo, aError);
}

void Coap::HandleResponse(otMessage *aMessage, const otMessageInfo *aMessageInfo, otError aError)
{
    if (aError != OT_ERROR_NONE)
    {
        OutputLine("coap receive response error %d: %s", aError, otThreadErrorToString(aError));
    }
    else if ((aMessageInfo != nullptr) && (aMessage != nullptr))
    {
#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
        otCoapOptionIterator iterator;
#endif

        OutputFormat("coap response from ");
        OutputIp6Address(aMessageInfo->mPeerAddr);

#if OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE
        if (otCoapOptionIteratorInit(&iterator, aMessage) == OT_ERROR_NONE)
        {
            const otCoapOption *observeOpt =
                otCoapOptionIteratorGetFirstOptionMatching(&iterator, OT_COAP_OPTION_OBSERVE);

            if (observeOpt != nullptr)
            {
                uint64_t observeVal = 0;
                otError  error      = otCoapOptionIteratorGetOptionUintValue(&iterator, &observeVal);

                if (error == OT_ERROR_NONE)
                {
                    OutputFormat(" OBS=");
                    OutputUint64(observeVal);
                }
            }
        }
#endif
        PrintPayload(aMessage);
    }
}

#if OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE
otError Coap::BlockwiseReceiveHook(void          *aContext,
                                   const uint8_t *aBlock,
                                   uint32_t       aPosition,
                                   uint16_t       aBlockLength,
                                   bool           aMore,
                                   uint32_t       aTotalLength)
{
    return static_cast<Coap *>(aContext)->BlockwiseReceiveHook(aBlock, aPosition, aBlockLength, aMore, aTotalLength);
}

otError Coap::BlockwiseReceiveHook(const uint8_t *aBlock,
                                   uint32_t       aPosition,
                                   uint16_t       aBlockLength,
                                   bool           aMore,
                                   uint32_t       aTotalLength)
{
    OT_UNUSED_VARIABLE(aMore);
    OT_UNUSED_VARIABLE(aTotalLength);

    OutputLine("received block: Num %i Len %i", aPosition / aBlockLength, aBlockLength);

    for (uint16_t i = 0; i < aBlockLength / 16; i++)
    {
        OutputBytesLine(&aBlock[i * 16], 16);
    }

    return OT_ERROR_NONE;
}

otError Coap::BlockwiseTransmitHook(void     *aContext,
                                    uint8_t  *aBlock,
                                    uint32_t  aPosition,
                                    uint16_t *aBlockLength,
                                    bool     *aMore)
{
    return static_cast<Coap *>(aContext)->BlockwiseTransmitHook(aBlock, aPosition, aBlockLength, aMore);
}

otError Coap::BlockwiseTransmitHook(uint8_t *aBlock, uint32_t aPosition, uint16_t *aBlockLength, bool *aMore)
{
    static uint32_t blockCount = 0;
    OT_UNUSED_VARIABLE(aPosition);

    // Send a random payload
    otRandomNonCryptoFillBuffer(aBlock, *aBlockLength);

    OutputLine("send block: Num %i Len %i", blockCount, *aBlockLength);

    for (uint16_t i = 0; i < *aBlockLength / 16; i++)
    {
        OutputBytesLine(&aBlock[i * 16], 16);
    }

    if (blockCount == mBlockCount - 1)
    {
        blockCount = 0;
        *aMore     = false;
    }
    else
    {
        *aMore = true;
        blockCount++;
    }

    return OT_ERROR_NONE;
}
#endif // OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE

} // namespace Cli
} // namespace ot

#endif // OPENTHREAD_CONFIG_COAP_API_ENABLE
