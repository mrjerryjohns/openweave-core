/*
 *
 *    Copyright (c) 2013-2017 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *      This file defines types and objects for managing Weave session
 *      security state.
 *
 */

// Include WeaveCore.h OUTSIDE of the include guard for WeaveSecurityMgr.h.
// This allows WeaveCore.h to enforce a canonical include order for core
// header files, making it easier to manage dependencies between these files.
#include <Weave/Core/WeaveCore.h>

#ifndef WEAVESECURITYMANAGER_H_
#define WEAVESECURITYMANAGER_H_

#include <Weave/Support/NLDLLUtil.h>
#include <Weave/Profiles/security/WeaveSecurity.h>
#include <Weave/Profiles/security/WeavePASE.h>
#include <Weave/Profiles/security/WeaveCASE.h>
#include <Weave/Profiles/security/WeaveTAKE.h>
#include <Weave/Profiles/security/WeaveKeyExport.h>
#include <Weave/Profiles/common/WeaveMessage.h>
#include <Weave/Profiles/status-report/StatusReportProfile.h>

/**
 *   @namespace nl::Weave::Platform::Security
 *
 *   @brief
 *     This namespace includes all interfaces within Weave for the
 *     Weave Security Monitor memory manager. Functions in this
 *     namespace are to be implemented by platforms that use Weave,
 *     according to the needs/constraints of the particular environment.
 *
 */

namespace nl {
namespace Weave {

namespace Platform {
namespace Security {

/**
 * This function is called by the Weave layer to initialize memory and resources
 * required for proper functionality of the Weave Security Manager memory allocator.
 * This function is platform specific and might be empty in certain cases.
 * For example, this function is doing nothing when the C Standard Library malloc()
 * and free() functions are used for memory allocation.
 *
 * @param[in]  buf      A pointer to a dedicated memory buffer, which should be used as
 *                      a memory pool for Weave Security Manager memory allocation.
 *                      This input is optional (defaults to NULL) and shouldn't be used
 *                      if a dedicated memory buffer is not used.
 *
 * @param[in]  bufSize  Size of a dedicated memory buffer. This input is optional (defaults to 0)
 *                      and shouldn't be used if dedicated memory buffer is not used.
 *                      When a dedicated memory buffer is used the function checks and
 *                      generates an error if buffer size is not big enough to support
 *                      Weave Security Manager use cases.
 *
 * @retval  #WEAVE_ERROR_BUFFER_TOO_SMALL  If dedicated input buffer size is not sufficient
 *                                         to support Weave Security Manager use cases.
 * @retval  #WEAVE_NO_ERROR                On success.
 * @retval  other                          An error generated by platform-specific memory
 *                                         initialization function.
 *
 */
extern WEAVE_ERROR MemoryInit(void *buf = NULL, size_t bufSize = 0);

/**
 * This function is called by the Weave layer to releases all resources that were allocated
 * by MemoryInit() function.
 * This function can be an empty call if there is no need to release resources. For example,
 * this is the case when the C Standard Library malloc() and free() functions are used
 * for memory allocation.
 *
 */
extern void MemoryShutdown(void);

/**
 * This function is called by the Weave layer to allocate a block of memory of "size" bytes.
 *
 * @param[in]  size             Specifies requested memory size in bytes.
 *
 * @param[in]  isLongTermAlloc  A Boolean indicating whether (true) or not (false) the
 *                              requested memory block is for long term use. A long term
 *                              allocation is memory that should stay allocated until secure
 *                              session/handshake is complete. Examples of a long term
 *                              allocation include blocks allocated for CASE/PASE objects
 *                              and their context data. A short term allocation is a memory
 *                              needed to perform specific operation and can be released
 *                              immediately after that. This input helps to optimize memory
 *                              utilization in a memory constrained system. Use of this parameter
 *                              is arbitrary and depends on function implementer. For example,
 *                              this parameter is ignored when the C Standard Library malloc()
 *                              is used.
 *
 * @retval  Pointer to a memory block in case of success.
 * @retval  NULL-pointer if memory allocation fails.
 *
 */
extern void *MemoryAlloc(size_t size, bool isLongTermAlloc);

/**
 * This function is called by the Weave layer to allocate a block of memory of "size" bytes.
 * This function is equivalent to MemoryAlloc(size, false).
 *
 * @param[in]  size             Specifies requested memory size in bytes.
 *
 * @retval  Pointer to a memory block in case of success.
 * @retval  NULL-pointer if memory allocation fails.
 *
 */
extern void *MemoryAlloc(size_t size);

/**
 * This function is called by the Weave layer to release a memory block allocated by
 * the MemeoryAlloc() function.
 *
 * @param[in]  p                Pointer to a memory block that should be released.
 *
 */
extern void MemoryFree(void *p);

#if WEAVE_CONFIG_SECURITY_MGR_TIME_ALERTS_PLATFORM

/**
 * This function is called to notify the application when a time-consuming
 * cryptographic operation is about to start.
 *
 * @note If application wants to receive these alerts and adjust platform settings
 *       accordingly then it should provide its own implementation of these functions
 *       and enable (1) #WEAVE_CONFIG_SECURITY_MGR_TIME_ALERTS_PLATFORM option.
 *
 */
extern void OnTimeConsumingCryptoStart(void);

/**
 * This function is called to notify the application when a time-consuming
 * cryptographic operation has just finished.
 *
 * @note If application wants to receive these alerts and adjust platform settings
 *       accordingly then it should provide its own implementation of these functions
 *       and enable (1) #WEAVE_CONFIG_SECURITY_MGR_TIME_ALERTS_PLATFORM option.
 *
 */
extern void OnTimeConsumingCryptoDone(void);

#endif // WEAVE_CONFIG_SECURITY_MGR_TIME_ALERTS_PLATFORM

} // namespace Platform
} // namespace Security


using nl::Weave::Profiles::StatusReporting::StatusReport;
using nl::Weave::Profiles::Security::PASE::WeavePASEEngine;
using nl::Weave::Profiles::Security::CASE::WeaveCASEEngine;
using nl::Weave::Profiles::Security::CASE::WeaveCASEAuthDelegate;
using nl::Weave::Profiles::Security::TAKE::WeaveTAKEEngine;
using nl::Weave::Profiles::Security::TAKE::WeaveTAKEChallengerAuthDelegate;
using nl::Weave::Profiles::Security::TAKE::WeaveTAKETokenAuthDelegate;
using nl::Weave::Profiles::Security::KeyExport::WeaveKeyExport;
using nl::Weave::Profiles::Security::KeyExport::WeaveKeyExportDelegate;

class NL_DLL_EXPORT WeaveSecurityManager
{
public:
    enum State
    {
        kState_NotInitialized = 0,
        kState_Idle,
        kState_CASEInProgress,
        kState_PASEInProgress,
        kState_TAKEInProgress,
        kState_KeyExportInProgress
    };

    WeaveFabricState *FabricState;                      // [READ ONLY] Associated Fabric State object.
    WeaveExchangeManager *ExchangeManager;              // [READ ONLY] Associated Exchange Manager object.
    uint8_t State;                                      // [READ ONLY] State of the Weave Message Layer object
#if WEAVE_CONFIG_ENABLE_CASE_INITIATOR
    uint32_t InitiatorCASEConfig;                       // CASE configuration proposed when initiating a CASE session
    uint32_t InitiatorCASECurveId;                      // ECDH curve proposed when initiating a CASE session
    uint8_t InitiatorAllowedCASEConfigs;                // Set of allowed CASE configurations when initiating a CASE session
    uint8_t InitiatorAllowedCASECurves;                 // Set of allowed ECDH curves when initiating a CASE session
#endif
#if WEAVE_CONFIG_ENABLE_CASE_RESPONDER
    uint8_t ResponderAllowedCASEConfigs;                // Set of allowed CASE configurations when responding to CASE session
    uint8_t ResponderAllowedCASECurves;                 // Set of allowed ECDH curves when responding to CASE session
#endif
#if WEAVE_CONFIG_ENABLE_KEY_EXPORT_INITIATOR
    uint8_t InitiatorKeyExportConfig;                   // Key export configuration proposed when initiating key export request
    uint8_t InitiatorAllowedKeyExportConfigs;           // Set of allowed configurations when initiating key export request
#endif
#if WEAVE_CONFIG_ENABLE_KEY_EXPORT_RESPONDER
    uint8_t ResponderAllowedKeyExportConfigs;           // Set of allowed configurations when responding to key export request
#endif
#if WEAVE_CONFIG_SECURITY_TEST_MODE
    bool CASEUseKnownECDHKey;                           // Enable the use of a known ECDH key pair in CASE to allow man-in-the-middle
                                                        // key recovery for testing purposes.
#endif
    uint32_t SessionEstablishTimeout;                   // The amount of time after which an in-progress session establishment will timeout.
    uint32_t IdleSessionTimeout;                        // The amount of time after which an idle session will be removed.

    WeaveSecurityManager(void);

    WEAVE_ERROR Init(WeaveExchangeManager& aExchangeMgr, System::Layer& aSystemLayer);
    WEAVE_ERROR Shutdown(void);

#if WEAVE_CONFIG_PROVIDE_OBSOLESCENT_INTERFACES
    WEAVE_ERROR Init(WeaveExchangeManager* aExchangeMgr, InetLayer* aInetLayer);
#endif // WEAVE_CONFIG_PROVIDE_OBSOLESCENT_INTERFACES

    typedef void (*SessionEstablishedFunct)(WeaveSecurityManager *sm, WeaveConnection *con, void *reqState, uint16_t sessionKeyId, uint64_t peerNodeId, uint8_t encType);
    typedef void (*SessionErrorFunct)(WeaveSecurityManager *sm, WeaveConnection *con, void *reqState, WEAVE_ERROR localErr, uint64_t peerNodeId, StatusReport *statusReport);

    /**
     * Type of key error message handling function.
     *
     * @param[in] keyId         Encryption key caused the key error message response from the peer.
     * @param[in] encType       Encryption type associated with @a keyId.
     * @param[in] messageId     The identifier of the Weave message resulted in the key error response from the peer.
     * @param[in] peerNodeId    The identifier of the Weave node that sent key error message.
     * @param[in] keyErr        The error code received from the peer.
     *
     */
    typedef void (*KeyErrorMsgRcvdFunct)(uint16_t keyId, uint8_t encType, uint32_t messageId, uint64_t peerNodeId, WEAVE_ERROR keyErr);

    /**
     * Type of key export protocol complete handling function.
     *
     * @param[in] sm             A pointer to WeaveSecurityManager object.
     * @param[in] con            A pointer to WeaveConnection object.
     * @param[in] reqState       A pointer to the key export requester state.
     * @param[in] keyId          Exported key ID.
     * @param[in] exportedKey    A pointer to the exported secret key.
     * @param[in] exportedKeyLen A reference to the exported secret key length.
     *
     */
    typedef void (*KeyExportCompleteFunct)(WeaveSecurityManager *sm, WeaveConnection *con, void *reqState, uint32_t exportedKeyId, const uint8_t *exportedKey, uint16_t exportedKeyLen);

    /**
     * Type of key export protocol error handling function.
     *
     * @param[in] sm             A pointer to WeaveSecurityManager object.
     * @param[in] con            A pointer to WeaveConnection object.
     * @param[in] reqState       A pointer to the key export requester state.
     * @param[in] localErr       The WEAVE_ERROR encountered during key export protocol.
     * @param[in] statusReport   A pointer to StatusReport object if error status received from peer.
     *
     */
    typedef void (*KeyExportErrorFunct)(WeaveSecurityManager *sm, WeaveConnection *con, void *reqState, WEAVE_ERROR localErr, StatusReport *statusReport);

    // Initiate a secure PASE session, optionally providing a password.
    // Session establishment is done over connection that was specified.
    WEAVE_ERROR StartPASESession(WeaveConnection *con, WeaveAuthMode requestedAuthMode, void *reqState,
                                 SessionEstablishedFunct onComplete, SessionErrorFunct onError,
                                 const uint8_t *pw = NULL, uint16_t pwLen = 0);

    // Initiate a secure CASE session, optionally providing a CASE auth delegate.
    // Session establishment is done over specified connection or over UDP using WRM Protocol.
    WEAVE_ERROR StartCASESession(WeaveConnection *con, uint64_t peerNodeId, const IPAddress &peerAddr,
                                 uint16_t peerPort, WeaveAuthMode requestedAuthMode, void *reqState,
                                 SessionEstablishedFunct onComplete, SessionErrorFunct onError,
                                 WeaveCASEAuthDelegate *authDelegate = NULL, uint64_t terminatingNodeId = kNodeIdNotSpecified);

    // Initiate a secure TAKE session, optionally providing a TAKE auth delegate.
    // Session establishment is done over connection that was specified.
    WEAVE_ERROR StartTAKESession(WeaveConnection *con, WeaveAuthMode requestedAuthMode, void *reqState,
                                 SessionEstablishedFunct onComplete, SessionErrorFunct onError,
                                 bool encryptAuthPhase, bool encryptCommPhase,
                                 bool timeLimitedIK, bool sendChallengerId,
                                 WeaveTAKEChallengerAuthDelegate *authDelegate = NULL);

    // Initiate key export protocol.
    WEAVE_ERROR StartKeyExport(WeaveConnection *con, uint64_t peerNodeId, const IPAddress &peerAddr,
                               uint16_t peerPort, uint32_t keyId, bool signMessage, void *reqState,
                               KeyExportCompleteFunct onComplete, KeyExportErrorFunct onError,
                               WeaveKeyExportDelegate *keyExportDelegate = NULL);

    // General callback functions. These will be called when a secure session is established or fails.
    SessionEstablishedFunct OnSessionEstablished;
    SessionErrorFunct OnSessionError;

    /**
     * The key error callback function. This function is called when
     * a key error message is received.
     */
    KeyErrorMsgRcvdFunct OnKeyErrorMsgRcvd;

    void SetCASEAuthDelegate(WeaveCASEAuthDelegate *delegate)
    {
#if WEAVE_CONFIG_ENABLE_CASE_INITIATOR || WEAVE_CONFIG_ENABLE_CASE_RESPONDER
        mDefaultAuthDelegate = delegate;
#endif
    }

    void SetTAKEAuthDelegate(WeaveTAKEChallengerAuthDelegate *delegate)
    {
#if WEAVE_CONFIG_ENABLE_TAKE_INITIATOR
        mDefaultTAKEChallengerAuthDelegate = delegate;
#endif
    }

    void SetTAKETokenAuthDelegate(WeaveTAKETokenAuthDelegate *delegate)
    {
#if WEAVE_CONFIG_ENABLE_TAKE_RESPONDER
        mDefaultTAKETokenAuthDelegate = delegate;
#endif
    }

    void SetKeyExportDelegate(WeaveKeyExportDelegate *delegate)
    {
#if WEAVE_CONFIG_ENABLE_KEY_EXPORT_INITIATOR || WEAVE_CONFIG_ENABLE_KEY_EXPORT_RESPONDER
        mDefaultKeyExportDelegate = delegate;
#endif
    }

    // Determine whether Weave error code is a key error.
    bool IsKeyError(WEAVE_ERROR err);

    // Send key error message when correct key has not been found and the message cannot be decrypted.
    WEAVE_ERROR SendKeyErrorMsg(WeaveMessageInfo *rcvdMsgInfo, const IPPacketInfo *rcvdMsgPacketInfo, WeaveConnection *con, WEAVE_ERROR keyErr);

    void OnEncryptedMsgRcvd(uint16_t sessionKeyId, uint64_t peerNodeId, uint8_t encType);

#if WEAVE_CONFIG_USE_APP_GROUP_KEYS_FOR_MSG_ENC
    // Send message counter synchronization message.
    WEAVE_ERROR SendMsgCounterSyncResp(const WeaveMessageInfo *rcvdMsgInfo, const IPPacketInfo *rcvdMsgPacketInfo);

    // Send peer message counter synchronization request.
    WEAVE_ERROR SendSolitaryMsgCounterSyncReq(const WeaveMessageInfo *rcvdMsgInfo, const IPPacketInfo *rcvdMsgPacketInfo);

    // Handle message counter synchronization response message.
    void HandleMsgCounterSyncRespMsg(WeaveMessageInfo *msgInfo, PacketBuffer *msgBuf);
#endif

    WEAVE_ERROR CancelSessionEstablishment(void *reqState);

    void ReserveKey(uint64_t peerNodeId, uint16_t keyId);
    void ReleaseKey(uint64_t peerNodeId, uint16_t keyId);

private:
    enum Flags
    {
        kFlag_IdleSessionTimerRunning   = 0x01
    };

    ExchangeContext *mEC;
    WeaveConnection *mCon;
    union
    {
#if WEAVE_CONFIG_ENABLE_PASE_INITIATOR || WEAVE_CONFIG_ENABLE_PASE_RESPONDER
        WeavePASEEngine *mPASEEngine;
#endif
#if WEAVE_CONFIG_ENABLE_CASE_INITIATOR || WEAVE_CONFIG_ENABLE_CASE_RESPONDER
        WeaveCASEEngine *mCASEEngine;
#endif
#if WEAVE_CONFIG_ENABLE_TAKE_INITIATOR || WEAVE_CONFIG_ENABLE_TAKE_RESPONDER
        WeaveTAKEEngine *mTAKEEngine;
#endif
#if WEAVE_CONFIG_ENABLE_KEY_EXPORT_INITIATOR
        WeaveKeyExport *mKeyExport;
#endif
    };
    union
    {
        SessionEstablishedFunct mStartSecureSession_OnComplete;

        /**
         * The key export protocol complete callback function. This function is
         * called when the secret key export process is complete.
         */
        KeyExportCompleteFunct mStartKeyExport_OnComplete;
    };
    union
    {
        SessionErrorFunct mStartSecureSession_OnError;

        /**
         * The key export protocol error callback function. This function is
         * called when an error is encountered during key export process.
         */
        KeyExportErrorFunct mStartKeyExport_OnError;
    };
    union
    {
        void *mStartSecureSession_ReqState;
        void *mStartKeyExport_ReqState;
    };
#if WEAVE_CONFIG_ENABLE_PASE_RESPONDER
    uint32_t mPASERateLimiterTimeout;
    uint8_t mPASERateLimiterCount;
    void UpdatePASERateLimiter(WEAVE_ERROR err);
#endif
#if WEAVE_CONFIG_ENABLE_CASE_INITIATOR || WEAVE_CONFIG_ENABLE_CASE_RESPONDER
    WeaveCASEAuthDelegate *mDefaultAuthDelegate;
#endif
#if WEAVE_CONFIG_ENABLE_TAKE_INITIATOR
    WeaveTAKEChallengerAuthDelegate *mDefaultTAKEChallengerAuthDelegate;
#endif
#if WEAVE_CONFIG_ENABLE_TAKE_RESPONDER
    WeaveTAKETokenAuthDelegate *mDefaultTAKETokenAuthDelegate;
#endif
#if WEAVE_CONFIG_ENABLE_KEY_EXPORT_INITIATOR || WEAVE_CONFIG_ENABLE_KEY_EXPORT_RESPONDER
    WeaveKeyExportDelegate *mDefaultKeyExportDelegate;
#endif

    uint16_t        mSessionKeyId;
    WeaveAuthMode   mRequestedAuthMode;
    uint8_t         mEncType;
    System::Layer*  mSystemLayer;
    uint8_t         mFlags;

    void StartSessionTimer(void);
    void CancelSessionTimer(void);
    static void HandleSessionTimeout(System::Layer* aSystemLayer, void* aAppState, System::Error aError);

    void StartIdleSessionTimer(void);
    void StopIdleSessionTimer(void);
    static void HandleIdleSessionTimeout(System::Layer* aLayer, void* aAppState, System::Error aError);

    static void HandleUnsolicitedMessage(ExchangeContext *ec, const IPPacketInfo *pktInfo, const WeaveMessageInfo *msgInfo,
            uint32_t profileId, uint8_t msgType, PacketBuffer *msgBuf);

    void StartPASESession(void);
    void HandlePASESessionStart(ExchangeContext *ec, const IPPacketInfo *pktInfo, const WeaveMessageInfo *msgInfo, PacketBuffer *msgBuf);
    WEAVE_ERROR ProcessPASEInitiatorStep1(ExchangeContext *ec, PacketBuffer *msgBuf);
    WEAVE_ERROR SendPASEResponderReconfigure(void);
    WEAVE_ERROR SendPASEResponderStep1(void);
    WEAVE_ERROR SendPASEResponderStep2(void);
    WEAVE_ERROR SendPASEInitiatorStep1(uint32_t paseConfig);
    WEAVE_ERROR ProcessPASEResponderReconfigure(PacketBuffer *msgBuf, uint32_t &newConfig);
    WEAVE_ERROR ProcessPASEResponderStep1(PacketBuffer *msgBuf);
    WEAVE_ERROR ProcessPASEResponderStep2(PacketBuffer *msgBuf);
    WEAVE_ERROR SendPASEInitiatorStep2(void);
    WEAVE_ERROR ProcessPASEInitiatorStep2(PacketBuffer *msgBuf);
    WEAVE_ERROR SendPASEResponderKeyConfirm(void);
    WEAVE_ERROR ProcessPASEResponderKeyConfirm(PacketBuffer *msgBuf);
    static void HandlePASEMessageInitiator(ExchangeContext *ec, const IPPacketInfo *pktInfo, const WeaveMessageInfo *msgInfo,
            uint32_t profileId, uint8_t msgType, PacketBuffer *msgBuf);
    static void HandlePASEMessageResponder(ExchangeContext *ec, const IPPacketInfo *pktInfo, const WeaveMessageInfo *msgInfo,
            uint32_t profileId, uint8_t msgType, PacketBuffer *msgBuf);

    void StartCASESession(uint32_t config, uint32_t curveId);
    void HandleCASESessionStart(ExchangeContext *ec, const IPPacketInfo *pktInfo, const WeaveMessageInfo *msgInfo, PacketBuffer *msgBuf);
    static void HandleCASEMessageInitiator(ExchangeContext *ec, const IPPacketInfo *pktInfo, const WeaveMessageInfo *msgInfo,
            uint32_t profileId, uint8_t msgType, PacketBuffer *msgBuf);
    static void HandleCASEMessageResponder(ExchangeContext *ec, const IPPacketInfo *pktInfo, const WeaveMessageInfo *msgInfo,
            uint32_t profileId, uint8_t msgType, PacketBuffer *msgBuf);

    void StartTAKESession(bool encryptAuthPhase, bool encryptCommPhase, bool timeLimitedIK, bool sendChallengerId);
    void HandleTAKESessionStart(ExchangeContext *ec, const IPPacketInfo *pktInfo, const WeaveMessageInfo *msgInfo, PacketBuffer *msgBuf);
    WEAVE_ERROR SendTAKEIdentifyToken(uint8_t takeConfig, bool encryptAuthPhase, bool encryptCommPhase, bool timeLimitedIK, bool sendChallengerId);
    static void HandleTAKEMessageInitiator(ExchangeContext *ec, const IPPacketInfo *pktInfo, const WeaveMessageInfo *msgInfo,
            uint32_t profileId, uint8_t msgType, PacketBuffer *msgBuf);
    static void HandleTAKEMessageResponder(ExchangeContext *ec, const IPPacketInfo *pktInfo, const WeaveMessageInfo *msgInfo,
            uint32_t profileId, uint8_t msgType, PacketBuffer *msgBuf);
    WEAVE_ERROR ProcessTAKEIdentifyTokenResponse(const PacketBuffer *msgBuf);
    WEAVE_ERROR CreateTAKESecureSession(void);
    WEAVE_ERROR SendTAKEAuthenticateToken(void);
    WEAVE_ERROR ProcessTAKEAuthenticateToken(const PacketBuffer *msgBuf);
    WEAVE_ERROR SendTAKEAuthenticateTokenResponse(void);
    WEAVE_ERROR ProcessTAKEAuthenticateTokenResponse(const PacketBuffer *msgBuf);
    WEAVE_ERROR SendTAKEReAuthenticateToken(void);
    WEAVE_ERROR ProcessTAKEReAuthenticateToken(const PacketBuffer *msgBuf);
    WEAVE_ERROR SendTAKEReAuthenticateTokenResponse(void);
    WEAVE_ERROR ProcessTAKEReAuthenticateTokenResponse(const PacketBuffer *msgBuf);
    WEAVE_ERROR SendTAKETokenReconfigure(void);
    WEAVE_ERROR ProcessTAKETokenReconfigure(uint8_t& config, const PacketBuffer *msgBuf);
    WEAVE_ERROR FinishTAKESetUp(void);

    void HandleKeyErrorMsg(ExchangeContext *ec, PacketBuffer *msgBuf);

#if WEAVE_CONFIG_USE_APP_GROUP_KEYS_FOR_MSG_ENC
    WEAVE_ERROR NewMsgCounterSyncExchange(const WeaveMessageInfo *rcvdMsgInfo, const IPPacketInfo *rcvdMsgPacketInfo, ExchangeContext *& ec);
#endif
    WEAVE_ERROR NewSessionExchange(uint64_t peerNodeId, IPAddress peerAddr, uint16_t peerPort);
    WEAVE_ERROR HandleSessionEstablished(void);
    void HandleSessionComplete(void);
    void HandleSessionError(WEAVE_ERROR err, PacketBuffer *statusReportMsgBuf);
    static void HandleConnectionClosed(ExchangeContext *ec, WeaveConnection *con, WEAVE_ERROR conErr);

    static WEAVE_ERROR SendStatusReport(WEAVE_ERROR localError, ExchangeContext *ec);

    void HandleKeyExportRequest(ExchangeContext *ec, const IPPacketInfo *pktInfo, const WeaveMessageInfo *msgInfo, PacketBuffer *msgBuf);
    WEAVE_ERROR SendKeyExportRequest(uint8_t keyExportConfig, uint32_t keyId, bool signMessage);
    WEAVE_ERROR SendKeyExportResponse(WeaveKeyExport& keyExport, uint8_t msgType, const WeaveMessageInfo *msgInfo);
    static void HandleKeyExportMessageInitiator(ExchangeContext *ec, const IPPacketInfo *pktInfo, const WeaveMessageInfo *msgInfo,
                                                uint32_t profileId, uint8_t msgType, PacketBuffer *msgBuf);
    void HandleKeyExportError(WEAVE_ERROR err, PacketBuffer *statusReportMsgBuf);

#if WEAVE_CONFIG_ENABLE_RELIABLE_MESSAGING
    static void WRMPHandleAckRcvd(ExchangeContext *ec, void *msgCtxt);
    static void WRMPHandleSendError(ExchangeContext *ec, WEAVE_ERROR err, void *msgCtxt);
#endif // WEAVE_CONFIG_ENABLE_RELIABLE_MESSAGING

    void Reset(void);

    void AsyncNotifySecurityManagerAvailable();
    static void DoNotifySecurityManagerAvailable(System::Layer *systemLayer, void *appState, System::Error err);

    void ReserveSessionKey(WeaveSessionKey *sessionKey);
    void ReleaseSessionKey(WeaveSessionKey *sessionKey);
};

} // namespace Weave
} // namespace nl

#endif /* WEAVESECURITYMANAGER_H_ */
