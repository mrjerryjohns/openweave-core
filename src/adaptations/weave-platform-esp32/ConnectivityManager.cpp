#include <internal/WeavePlatformInternal.h>
#include <ConnectivityManager.h>
#include <internal/NetworkProvisioningServer.h>
#include <internal/NetworkInfo.h>
#include <Weave/Profiles/WeaveProfiles.h>
#include <Weave/Profiles/common/CommonProfile.h>
#include "esp_wifi.h"
#include <new>

using namespace ::nl;
using namespace ::nl::Weave;
using namespace ::nl::Weave::TLV;
using namespace ::nl::Weave::Profiles::Common;
using namespace ::nl::Weave::Profiles::NetworkProvisioning;
using namespace ::WeavePlatform::Internal;

using Profiles::kWeaveProfile_Common;
using Profiles::kWeaveProfile_NetworkProvisioning;

namespace WeavePlatform {

namespace {

enum
{
    kWiFiStationNetworkId  = 1
};

extern const char *ESPWiFiModeToStr(wifi_mode_t wifiMode);
extern WEAVE_ERROR ChangeESPWiFiMode(esp_interface_t intf, bool enabled);
extern WiFiSecurityType ESPWiFiAuthModeToWeaveWiFiSecurityType(wifi_auth_mode_t authMode);
extern int OrderESPScanResultsByRSSI(const void * _res1, const void * _res2);

} // namespace Internal


// ==================== ConnectivityManager Public Methods ====================

ConnectivityManager::WiFiStationMode ConnectivityManager::GetWiFiStationMode(void)
{
    if (mWiFiStationMode != kWiFiStationMode_ApplicationControlled)
    {
        bool autoConnect;
        mWiFiStationMode = (esp_wifi_get_auto_connect(&autoConnect) == ESP_OK && autoConnect)
            ? kWiFiStationMode_Enabled : kWiFiStationMode_Disabled;
    }
    return mWiFiStationMode;
}

bool ConnectivityManager::IsWiFiStationEnabled(void)
{
    return GetWiFiStationMode() == kWiFiStationMode_Enabled;
}

WEAVE_ERROR ConnectivityManager::SetWiFiStationMode(WiFiStationMode val)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    VerifyOrExit(val != kWiFiStationMode_NotSupported, err = WEAVE_ERROR_INVALID_ARGUMENT);

    if (val != kWiFiStationMode_ApplicationControlled)
    {
        bool autoConnect = (val == kWiFiStationMode_Enabled);
        err = esp_wifi_set_auto_connect(autoConnect);
        SuccessOrExit(err);

        SystemLayer.ScheduleWork(DriveStationState, NULL);
    }

    if (mWiFiStationMode != val)
    {
        ESP_LOGI(TAG, "Changing WiFi station mode: %s -> %s", WiFiStationModeToStr(mWiFiStationMode), WiFiStationModeToStr(val));
    }

    mWiFiStationMode = val;

exit:
    return err;
}

bool ConnectivityManager::IsWiFiStationProvisioned(void) const
{
    wifi_config_t stationConfig;
    return (esp_wifi_get_config(ESP_IF_WIFI_STA, &stationConfig) == ERR_OK && stationConfig.sta.ssid[0] != 0);
}

void ConnectivityManager::ClearWiFiStationProvision(void)
{
    if (mWiFiStationMode != kWiFiStationMode_ApplicationControlled)
    {
        wifi_config_t stationConfig;

        memset(&stationConfig, 0, sizeof(stationConfig));
        esp_wifi_set_config(ESP_IF_WIFI_STA, &stationConfig);

        SystemLayer.ScheduleWork(DriveStationState, NULL);
    }
}

uint32_t ConnectivityManager::GetWiFiStationNetworkId(void) const
{
    return kWiFiStationNetworkId;
}

WEAVE_ERROR ConnectivityManager::SetWiFiAPMode(WiFiAPMode val)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    VerifyOrExit(val != kWiFiAPMode_NotSupported, err = WEAVE_ERROR_INVALID_ARGUMENT);

    if (mWiFiAPMode != val)
    {
        ESP_LOGI(TAG, "Changing WiFi AP mode: %s -> %s", WiFiAPModeToStr(mWiFiAPMode), WiFiAPModeToStr(val));
    }

    mWiFiAPMode = val;

    SystemLayer.ScheduleWork(DriveAPState, NULL);

exit:
    return err;
}

void ConnectivityManager::DemandStartWiFiAP(void)
{
    if (mWiFiAPMode == kWiFiAPMode_OnDemand ||
        mWiFiAPMode == kWiFiAPMode_OnDemand_NoStationProvision)
    {
        mLastAPDemandTime = SystemLayer.GetSystemTimeMS();
        SystemLayer.ScheduleWork(DriveAPState, NULL);
    }
}

void ConnectivityManager::StopOnDemandWiFiAP(void)
{
    if (mWiFiAPMode == kWiFiAPMode_OnDemand ||
        mWiFiAPMode == kWiFiAPMode_OnDemand_NoStationProvision)
    {
        mLastAPDemandTime = 0;
        SystemLayer.ScheduleWork(DriveAPState, NULL);
    }
}

void ConnectivityManager::MaintainOnDemandWiFiAP(void)
{
    if (mWiFiAPMode == kWiFiAPMode_OnDemand ||
        mWiFiAPMode == kWiFiAPMode_OnDemand_NoStationProvision)
    {
        if (mWiFiAPState == kWiFiAPState_Activating || mWiFiAPState == kWiFiAPState_Active)
        {
            mLastAPDemandTime = SystemLayer.GetSystemTimeMS();
        }
    }
}

void ConnectivityManager::SetWiFiAPIdleTimeoutMS(uint32_t val)
{
    mWiFiAPIdleTimeoutMS = val;
    SystemLayer.ScheduleWork(DriveAPState, NULL);
}

// ==================== ConnectivityManager Platform Internal Methods ====================

WEAVE_ERROR ConnectivityManager::Init()
{
    WEAVE_ERROR err;

    mLastStationConnectFailTime = 0;
    mLastAPDemandTime = 0;
    mWiFiStationMode = kWiFiStationMode_Disabled;
    mWiFiStationState = kWiFiStationState_Disabled;
    mWiFiAPMode = kWiFiAPMode_Disabled;
    mWiFiAPState = kWiFiAPState_NotActive;
    mWiFiStationReconnectIntervalMS = WEAVE_PLATFORM_CONFIG_WIFI_STATION_RECONNECT_INTERVAL;
    mWiFiAPIdleTimeoutMS = WEAVE_PLATFORM_CONFIG_WIFI_AP_IDLE_TIMEOUT;
    mScanInProgress = false;

    // If there is no persistent station provision...
    if (!IsWiFiStationProvisioned())
    {
        // Switch to station mode temporarily so that the configuration can be changed.
        err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_wifi_set_mode() failed: %s", nl::ErrorStr(err));
        }

        // If the code has been compiled with a default WiFi station provision, configure that now.
        if (CONFIG_DEFAULT_WIFI_SSID[0] != 0)
        {
            ESP_LOGI(TAG, "Setting default WiFi station configuration (SSID: %s)", CONFIG_DEFAULT_WIFI_SSID);

            // Set a default station configuration.
            wifi_config_t wifiConfig;
            memset(&wifiConfig, 0, sizeof(wifiConfig));
            memcpy(wifiConfig.sta.ssid, CONFIG_DEFAULT_WIFI_SSID, strlen(CONFIG_DEFAULT_WIFI_SSID) + 1);
            memcpy(wifiConfig.sta.password, CONFIG_DEFAULT_WIFI_PASSWORD, strlen(CONFIG_DEFAULT_WIFI_PASSWORD) + 1);
            wifiConfig.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
            wifiConfig.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
            err = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifiConfig);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_wifi_set_config() failed: %s", nl::ErrorStr(err));
            }
            err = WEAVE_NO_ERROR;

            // Enable WiFi station mode.
            err = esp_wifi_set_auto_connect(true);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_wifi_set_auto_connect() failed: %s", nl::ErrorStr(err));
            }

            mWiFiStationMode = kWiFiStationMode_Enabled;
        }

        // Otherwise, ensure WiFi station mode is disabled.
        else
        {
            err = esp_wifi_set_auto_connect(false);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_wifi_set_auto_connect() failed: %s", nl::ErrorStr(err));
            }
            SuccessOrExit(err);
        }
    }

    // Disable both AP and STA mode.  The AP and station state machines will re-enable these as needed.
    err = esp_wifi_set_mode(WIFI_MODE_NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_mode() failed: %s", nl::ErrorStr(err));
    }
    SuccessOrExit(err);

    // Queue work items to bootstrap the AP and station state machines once the Weave event loop is running.
    err = SystemLayer.ScheduleWork(DriveStationState, NULL);
    SuccessOrExit(err);
    err = SystemLayer.ScheduleWork(DriveAPState, NULL);
    SuccessOrExit(err);

exit:
    return err;
}

NetworkProvisioningDelegate * ConnectivityManager::GetNetworkProvisioningDelegate()
{
    return &mNetProvDelegate;
}

void ConnectivityManager::OnPlatformEvent(const struct WeavePlatformEvent * event)
{
    WEAVE_ERROR err;

    // Handle ESP system events...
    if (event->Type == WeavePlatformEvent::kEventType_ESPSystemEvent)
    {
        switch(event->ESPSystemEvent.event_id) {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
            DriveStationState();
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_CONNECTED");
            if (mWiFiStationState == kWiFiStationState_Connecting)
            {
                ChangeWiFiStationState(kWiFiStationState_Connecting_Succeeded);
            }
            DriveStationState();
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
            if (mWiFiStationState == kWiFiStationState_Connecting)
            {
                ChangeWiFiStationState(kWiFiStationState_Connecting_Failed);
            }
            DriveStationState();
            break;
        case SYSTEM_EVENT_STA_STOP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_STOP");
            DriveStationState();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
            err = MessageLayer.RefreshEndpoints();
            if (err != WEAVE_NO_ERROR)
            {
                ESP_LOGE(TAG, "Error returned by MessageLayer.RefreshEndpoints(): %s", nl::ErrorStr(err));
            }
            break;
        case SYSTEM_EVENT_STA_LOST_IP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_LOST_IP");
            err = MessageLayer.RefreshEndpoints();
            if (err != WEAVE_NO_ERROR)
            {
                ESP_LOGE(TAG, "Error returned by MessageLayer.RefreshEndpoints(): %s", nl::ErrorStr(err));
            }
            break;
        case SYSTEM_EVENT_GOT_IP6:
            ESP_LOGI(TAG, "SYSTEM_EVENT_GOT_IP6");
            err = MessageLayer.RefreshEndpoints();
            if (err != WEAVE_NO_ERROR)
            {
                ESP_LOGE(TAG, "Error returned by MessageLayer.RefreshEndpoints(): %s", nl::ErrorStr(err));
            }
            break;
        case SYSTEM_EVENT_AP_START:
            ESP_LOGI(TAG, "SYSTEM_EVENT_AP_START");
            ChangeWiFiAPState(kWiFiAPState_Active);
            DriveAPState();
            break;
        case SYSTEM_EVENT_AP_STOP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STOP");
            ChangeWiFiAPState(kWiFiAPState_NotActive);
            DriveAPState();
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STACONNECTED");
            MaintainOnDemandWiFiAP();
            break;
        case SYSTEM_EVENT_SCAN_DONE:
            ESP_LOGI(TAG, "SYSTEM_EVENT_SCAN_DONE");
            mNetProvDelegate.HandleScanDone();
            break;
        default:
            break;
        }
    }
}

// ==================== ConnectivityManager Private Methods ====================

void ConnectivityManager::DriveStationState()
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    bool espSTAModeEnabled, stationConnected;

    // Refresh the cached station mode.
    GetWiFiStationMode();

    // Determine if STA mode is enabled in the ESP wifi layer.  If so, determine whether the station is
    // currently connected to an AP.
    {
        wifi_mode_t wifiMode;
        wifi_ap_record_t apInfo;
        espSTAModeEnabled = (esp_wifi_get_mode(&wifiMode) == ESP_OK && (wifiMode == WIFI_MODE_STA || wifiMode == WIFI_MODE_APSTA));
        stationConnected = (espSTAModeEnabled && esp_wifi_sta_get_ap_info(&apInfo) == ESP_OK);
    }

    // If STA mode is not enabled at the ESP wifi layer, enable it now, unless the WiFi station mode
    // is currently under application control.  Either way, wait until STA mode is enabled before
    // proceeding.
    if (!espSTAModeEnabled)
    {
        if (mWiFiStationMode != kWiFiStationMode_ApplicationControlled)
        {
            ChangeWiFiStationState(kWiFiStationState_Enabling);
            err = ChangeESPWiFiMode(ESP_IF_WIFI_STA, true);
            SuccessOrExit(err);
        }
        ExitNow();
    }

    // Advance the station state to NotConnected if it was previously Disabled or Enabling.
    if (mWiFiStationState == kWiFiStationState_Disabled ||
        mWiFiStationState == kWiFiStationState_Enabling)
    {
        ChangeWiFiStationState(kWiFiStationState_NotConnected);
    }

    // If the station interface is currently connected to an AP...
    if (stationConnected)
    {
        // Advance the station state to Connected if it was previously NotConnected or
        // a previously initiated connect attempt succeeded.
        if (mWiFiStationState == kWiFiStationState_NotConnected ||
            mWiFiStationState == kWiFiStationState_Connecting_Succeeded)
        {
            ChangeWiFiStationState(kWiFiStationState_Connected);
            ESP_LOGI(TAG, "WiFi station interface connected");
            mLastStationConnectFailTime = 0;
            OnStationConnected();
        }

        // If the WiFi station interface is no longer enabled, or no longer provisioned,
        // disconnect the station from the AP, unless the WiFi station mode is currently
        // under application control.
        if (mWiFiStationMode != kWiFiStationMode_ApplicationControlled &&
            (mWiFiStationMode != kWiFiStationMode_Enabled || !IsWiFiStationProvisioned()))
        {
            ESP_LOGI(TAG, "Disconnecting WiFi station interface");
            err = esp_wifi_disconnect();
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_wifi_disconnect() failed: %s", nl::ErrorStr(err));
            }
            SuccessOrExit(err);

            ChangeWiFiStationState(kWiFiStationState_Disconnecting);
        }
    }

    // Otherwise the station interface is NOT connected to an AP, so...
    else
    {
        uint64_t now = SystemLayer.GetSystemTimeMS();

        // Advance the station state to NotConnected if it was previously Connected or Disconnecting,
        // or if a previous initiated connect attempt failed.
        if (mWiFiStationState == kWiFiStationState_Connected ||
            mWiFiStationState == kWiFiStationState_Disconnecting ||
            mWiFiStationState == kWiFiStationState_Connecting_Failed)
        {
            WiFiStationState prevState = mWiFiStationState;
            ChangeWiFiStationState(kWiFiStationState_NotConnected);
            if (prevState != kWiFiStationState_Connecting_Failed)
            {
                ESP_LOGI(TAG, "WiFi station interface disconnected");
                mLastStationConnectFailTime = 0;
                OnStationDisconnected();
            }
            else
            {
                mLastStationConnectFailTime = now;
            }
        }

        // If the WiFi station interface is now enabled and provisioned (and by implication,
        // not presently under application control), AND the system is not in the process of
        // scanning, then...
        if (mWiFiStationMode == kWiFiStationMode_Enabled && IsWiFiStationProvisioned() && !mScanInProgress)
        {
            // Initiate a connection to the AP if we haven't done so before, or if enough
            // time has passed since the last attempt.
            if (mLastStationConnectFailTime == 0 || now >= mLastStationConnectFailTime + mWiFiStationReconnectIntervalMS)
            {
                ESP_LOGI(TAG, "Attempting to connect WiFi station interface");
                err = esp_wifi_connect();
                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "esp_wifi_connect() failed: %s", nl::ErrorStr(err));
                }
                SuccessOrExit(err);

                ChangeWiFiStationState(kWiFiStationState_Connecting);
            }

            // Otherwise arrange another connection attempt at a suitable point in the future.
            else
            {
                uint32_t timeToNextConnect = (uint32_t)((mLastStationConnectFailTime + mWiFiStationReconnectIntervalMS) - now);

                ESP_LOGI(TAG, "Next WiFi station reconnect in %" PRIu32 " ms", timeToNextConnect);

                err = SystemLayer.StartTimer(timeToNextConnect, DriveStationState, NULL);
                SuccessOrExit(err);
            }
        }
    }

exit:

    // If an error occurred and the station is not under the application control, disable it.
    if (err != WEAVE_NO_ERROR && mWiFiStationMode != kWiFiStationMode_ApplicationControlled)
    {
        SetWiFiStationMode(kWiFiStationMode_Disabled);
    }

    // Kick-off any pending network scan that might have been deferred due to the activity
    // of the WiFi station.
    mNetProvDelegate.StartPendingScan();
}

void ConnectivityManager::DriveAPState()
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    WiFiAPState targetState;
    uint32_t apTimeout = 0;

    if (mWiFiAPMode == kWiFiAPMode_Disabled)
    {
        targetState = kWiFiAPState_NotActive;
    }

    else if (mWiFiAPMode == kWiFiAPMode_Enabled)
    {
        targetState = kWiFiAPState_Active;
    }

    else if (mWiFiAPMode == kWiFiAPMode_OnDemand_NoStationProvision &&
             (!IsWiFiStationProvisioned() || GetWiFiStationMode() == kWiFiStationMode_Disabled))
    {
        targetState = kWiFiAPState_Active;
    }

    else if (mWiFiAPMode == kWiFiAPMode_OnDemand ||
             mWiFiAPMode == kWiFiAPMode_OnDemand_NoStationProvision)
    {
        uint64_t now = SystemLayer.GetSystemTimeMS();

        if (mLastAPDemandTime != 0 && now < (mLastAPDemandTime + mWiFiAPIdleTimeoutMS))
        {
            targetState = kWiFiAPState_Active;
            apTimeout = (uint32_t)((mLastAPDemandTime + mWiFiAPIdleTimeoutMS) - now);
        }
        else
        {
            targetState = kWiFiAPState_NotActive;
        }
    }
    else
    {
        targetState = kWiFiAPState_NotActive;
    }

    if (mWiFiAPState != targetState && mWiFiAPMode != kWiFiAPMode_ApplicationControlled)
    {
        if (targetState == kWiFiAPState_Active)
        {
            if (mWiFiAPState != kWiFiAPState_Activating)
            {
                err = ChangeESPWiFiMode(ESP_IF_WIFI_AP, true);
                SuccessOrExit(err);

                err = ConfigureWiFiAP();
                SuccessOrExit(err);

                ChangeWiFiAPState(kWiFiAPState_Activating);
            }
        }
        else
        {
            if (mWiFiAPState != kWiFiAPState_Deactivating)
            {
                err = ChangeESPWiFiMode(ESP_IF_WIFI_AP, false);
                SuccessOrExit(err);

                ChangeWiFiAPState(kWiFiAPState_Deactivating);
            }
        }
    }

    if (apTimeout != 0)
    {
        ESP_LOGI(TAG, "Next WiFi AP timeout in %" PRIu32 " ms", apTimeout);

        err = SystemLayer.StartTimer(apTimeout, DriveAPState, NULL);
        SuccessOrExit(err);
    }

exit:
    if (err != WEAVE_NO_ERROR)
    {
        SetWiFiAPMode(kWiFiAPMode_Disabled);
    }
}

WEAVE_ERROR ConnectivityManager::ConfigureWiFiAP()
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    wifi_config_t wifiConfig;

    memset(&wifiConfig, 0, sizeof(wifiConfig));
    err = ConfigurationMgr.GetWiFiAPSSID((char *)wifiConfig.ap.ssid, sizeof(wifiConfig.ap.ssid));
    SuccessOrExit(err);
    wifiConfig.ap.channel = WEAVE_PLATFORM_CONFIG_WIFI_AP_CHANNEL;
    wifiConfig.ap.authmode = WIFI_AUTH_OPEN;
    wifiConfig.ap.max_connection = WEAVE_PLATFORM_CONFIG_WIFI_AP_MAX_STATIONS;
    wifiConfig.ap.beacon_interval = WEAVE_PLATFORM_CONFIG_WIFI_AP_BEACON_INTERVAL;
    ESP_LOGI(TAG, "Configuring WiFi AP: SSID %s, channel %u", wifiConfig.ap.ssid, wifiConfig.ap.channel);
    err = esp_wifi_set_config(ESP_IF_WIFI_AP, &wifiConfig);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_config(ESP_IF_WIFI_AP) failed: %s", nl::ErrorStr(err));
    }
    SuccessOrExit(err);

exit:
    return err;
}

void ConnectivityManager::OnStationConnected()
{
    // TODO: alert other subsystems of connected state

    // Assign an IPv6 link local address to the station interface.
    tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA);
}

void ConnectivityManager::OnStationDisconnected()
{
    // TODO: alert other subsystems of disconnected state
}

void ConnectivityManager::ChangeWiFiStationState(WiFiStationState newState)
{
    if (mWiFiStationState != newState)
    {
        ESP_LOGI(TAG, "Changing WiFi station state: %s -> %s", WiFiStationStateToStr(mWiFiStationState), WiFiStationStateToStr(newState));
    }
    mWiFiStationState = newState;
}

void ConnectivityManager::ChangeWiFiAPState(WiFiAPState newState)
{
    if (mWiFiAPState != newState)
    {
        ESP_LOGI(TAG, "Changing WiFi AP state: %s -> %s", WiFiAPStateToStr(mWiFiAPState), WiFiAPStateToStr(newState));
    }
    mWiFiAPState = newState;
}

const char * ConnectivityManager::WiFiStationModeToStr(WiFiStationMode mode)
{
    switch (mode)
    {
    case kWiFiStationMode_NotSupported:
        return "NotSupported";
    case kWiFiStationMode_ApplicationControlled:
        return "AppControlled";
    case kWiFiStationMode_Enabled:
        return "Enabled";
    case kWiFiStationMode_Disabled:
        return "Disabled";
    default:
        return "(unknown)";
    }
}

const char * ConnectivityManager::WiFiStationStateToStr(WiFiStationState state)
{
    switch (state)
    {
    case kWiFiStationState_Disabled:
        return "Disabled";
    case kWiFiStationState_Enabling:
        return "Enabling";
    case kWiFiStationState_NotConnected:
        return "NotConnected";
    case kWiFiStationState_Connecting:
        return "Connecting";
    case kWiFiStationState_Connecting_Succeeded:
        return "Connecting_Succeeded";
    case kWiFiStationState_Connecting_Failed:
        return "Connecting_Failed";
    case kWiFiStationState_Connected:
        return "Connected";
    case kWiFiStationState_Disconnecting:
        return "Disconnecting";
    default:
        return "(unknown)";
    }
}

const char * ConnectivityManager::WiFiAPModeToStr(WiFiAPMode mode)
{
    switch (mode)
    {
    case kWiFiAPMode_NotSupported:
        return "NotSupported";
    case kWiFiAPMode_ApplicationControlled:
        return "AppControlled";
    case kWiFiAPMode_Disabled:
        return "Disabled";
    case kWiFiAPMode_Enabled:
        return "Enabled";
    case kWiFiAPMode_OnDemand:
        return "OnDemand";
    case kWiFiAPMode_OnDemand_NoStationProvision:
        return "OnDemand_NoStationProvision";
    default:
        return "(unknown)";
    }
}

const char * ConnectivityManager::WiFiAPStateToStr(WiFiAPState state)
{
    switch (state)
    {
    case kWiFiAPState_NotActive:
        return "NotActive";
    case kWiFiAPState_Activating:
        return "Activating";
    case kWiFiAPState_Active:
        return "Active";
    case kWiFiAPState_Deactivating:
        return "Deactivating";
    default:
        return "(unknown)";
    }
}

void ConnectivityManager::DriveStationState(nl::Weave::System::Layer * aLayer, void * aAppState, nl::Weave::System::Error aError)
{
    ConnectivityMgr.DriveStationState();
}

void ConnectivityManager::DriveAPState(nl::Weave::System::Layer * aLayer, void * aAppState, nl::Weave::System::Error aError)
{
    ConnectivityMgr.DriveAPState();
}

// ==================== ConnectivityManager::NetworkProvisioningDelegate Public Methods ====================

WEAVE_ERROR ConnectivityManager::NetworkProvisioningDelegate::HandleScanNetworks(uint8_t networkType)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    // Verify the expected network type.
    if (networkType != kNetworkType_WiFi)
    {
        err = NetworkProvisioningSvr.SendStatusReport(kWeaveProfile_NetworkProvisioning, kStatusCode_UnsupportedNetworkType);
        ExitNow();
    }

    // Reject the request if the application is currently in control of the WiFi station.
    if (RejectIfApplicationControlled(true))
    {
        ExitNow();
    }

    StartPendingScan();

exit:
    return err;
}

WEAVE_ERROR ConnectivityManager::NetworkProvisioningDelegate::HandleAddNetwork(PacketBuffer * networkInfoTLV)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    NetworkInfo netInfo;

    // Parse the supplied network configuration info.
    {
        TLV::TLVReader reader;
        reader.Init(networkInfoTLV);
        err = netInfo.Decode(reader);
        SuccessOrExit(err);
    }

    // Discard the request buffer.
    PacketBuffer::Free(networkInfoTLV);
    networkInfoTLV = NULL;

    // Reject the request if the application is currently in control of the WiFi station.
    if (RejectIfApplicationControlled(true))
    {
        ExitNow();
    }

    // Delegate to the ConnectivityManager to check the validity of the new WiFi station provision.
    // If the new provision is not acceptable, respond to the requestor with an appropriate
    // StatusReport.
    {
        uint32_t statusProfileId;
        uint16_t statusCode;
        err = ValidateWiFiStationProvision(netInfo, statusProfileId, statusCode);
        if (err != WEAVE_NO_ERROR)
        {
            err = NetworkProvisioningSvr.SendStatusReport(statusProfileId, statusCode, err);
            ExitNow();
        }
    }

    // Set the ESP WiFi station configuration.
    err = SetESPStationConfig(netInfo);
    SuccessOrExit(err);

    // Schedule a call to the ConnectivityManager's DriveStationState method to adjust the station
    // state based on the new provision.
    SystemLayer.ScheduleWork(ConnectivityMgr.DriveStationState, NULL);

    // Send an AddNetworkComplete message back to the requestor.
    NetworkProvisioningSvr.SendAddNetworkComplete(kWiFiStationNetworkId);

exit:
    PacketBuffer::Free(networkInfoTLV);
    return err;
}

WEAVE_ERROR ConnectivityManager::NetworkProvisioningDelegate::HandleUpdateNetwork(PacketBuffer * networkInfoTLV)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    NetworkInfo netInfo, netInfoUpdates;

    // Parse the supplied network configuration info.
    {
        TLV::TLVReader reader;
        reader.Init(networkInfoTLV);
        err = netInfoUpdates.Decode(reader);
        SuccessOrExit(err);
    }

    // Discard the request buffer.
    PacketBuffer::Free(networkInfoTLV);
    networkInfoTLV = NULL;

    // Reject the request if the application is currently in control of the WiFi station.
    if (RejectIfApplicationControlled(true))
    {
        ExitNow();
    }

    // If the network id field isn't present, immediately reply with an error.
    if (!netInfoUpdates.NetworkIdPresent)
    {
        err = NetworkProvisioningSvr.SendStatusReport(kWeaveProfile_NetworkProvisioning, kStatusCode_InvalidNetworkConfiguration);
        ExitNow();
    }

    // Verify that the specified network exists.
    if (!ConnectivityMgr.IsWiFiStationProvisioned() || netInfoUpdates.NetworkId != kWiFiStationNetworkId)
    {
        err = NetworkProvisioningSvr.SendStatusReport(kWeaveProfile_NetworkProvisioning, kStatusCode_UnknownNetwork);
        SuccessOrExit(err);
        ExitNow();
    }

    // Get the existing station provision.
    err = GetWiFiStationProvision(netInfo, true);
    SuccessOrExit(err);

    // Merge in the updated information.
    err = netInfoUpdates.MergeTo(netInfo);
    SuccessOrExit(err);

    // Check the validity of the updated station provision. If the new provision is not acceptable,
    // respond to the requestor with an appropriate StatusReport.
    {
        uint32_t statusProfileId;
        uint16_t statusCode;
        err = ValidateWiFiStationProvision(netInfo, statusProfileId, statusCode);
        if (err != WEAVE_NO_ERROR)
        {
            err = NetworkProvisioningSvr.SendStatusReport(statusProfileId, statusCode, err);
            ExitNow();
        }
    }

    // Set the ESP WiFi station configuration.
    err = SetESPStationConfig(netInfo);
    SuccessOrExit(err);

    // Schedule a call to the ConnectivityManager's DriveStationState method to adjust the station
    // state based on the new provision.
    SystemLayer.ScheduleWork(ConnectivityMgr.DriveStationState, NULL);

    // Tell the requestor we succeeded.
    err = NetworkProvisioningSvr.SendSuccessResponse();
    SuccessOrExit(err);

exit:
    PacketBuffer::Free(networkInfoTLV);
    return err;
}

WEAVE_ERROR ConnectivityManager::NetworkProvisioningDelegate::HandleRemoveNetwork(uint32_t networkId)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    wifi_config_t stationConfig;

    // Reject the request if the application is currently in control of the WiFi station.
    if (RejectIfApplicationControlled(true))
    {
        ExitNow();
    }

    // Verify that the specified network exists.
    if (!ConnectivityMgr.IsWiFiStationProvisioned() || networkId != kWiFiStationNetworkId)
    {
        err = NetworkProvisioningSvr.SendStatusReport(kWeaveProfile_NetworkProvisioning, kStatusCode_UnknownNetwork);
        SuccessOrExit(err);
        ExitNow();
    }

    // Clear the ESP WiFi station configuration.
    memset(&stationConfig, 0, sizeof(stationConfig));
    esp_wifi_set_config(ESP_IF_WIFI_STA, &stationConfig);

    // Schedule a call to the ConnectivityManager's DriveStationState method to adjust the station state.
    SystemLayer.ScheduleWork(DriveStationState, NULL);

    // Respond with a Success response.
    err = NetworkProvisioningSvr.SendSuccessResponse();
    SuccessOrExit(err);

exit:
    return err;
}

WEAVE_ERROR ConnectivityManager::NetworkProvisioningDelegate::HandleGetNetworks(uint8_t flags)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    NetworkInfo netInfo;
    PacketBuffer * respBuf = NULL;
    TLVWriter writer;
    uint8_t resultCount;
    const bool includeCredentials = (flags & kGetNetwork_IncludeCredentials) != 0;

    // Reject the request if the application is currently in control of the WiFi station.
    if (RejectIfApplicationControlled(true))
    {
        ExitNow();
    }

    respBuf = PacketBuffer::New();
    VerifyOrExit(respBuf != NULL, err = WEAVE_ERROR_NO_MEMORY);

    writer.Init(respBuf);

    err = GetWiFiStationProvision(netInfo, includeCredentials);
    if (err == WEAVE_NO_ERROR)
    {
        resultCount = 1;
    }
    else if (err == WEAVE_ERROR_INCORRECT_STATE)
    {
        resultCount = 0;
    }
    else
    {
        ExitNow();
    }

    err = NetworkInfo::EncodeArray(writer, &netInfo, resultCount);
    SuccessOrExit(err);

    err = writer.Finalize();
    SuccessOrExit(err);

    err = NetworkProvisioningSvr.SendGetNetworksComplete(resultCount, respBuf);
    respBuf = NULL;
    SuccessOrExit(err);

exit:
    PacketBuffer::Free(respBuf);
    return err;
}

WEAVE_ERROR ConnectivityManager::NetworkProvisioningDelegate::HandleEnableNetwork(uint32_t networkId)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    // Reject the request if the application is currently in control of the WiFi station.
    if (RejectIfApplicationControlled(true))
    {
        ExitNow();
    }

    // Verify that the specified network exists.
    if (!ConnectivityMgr.IsWiFiStationProvisioned() || networkId != kWiFiStationNetworkId)
    {
        err = NetworkProvisioningSvr.SendStatusReport(kWeaveProfile_NetworkProvisioning, kStatusCode_UnknownNetwork);
        SuccessOrExit(err);
        ExitNow();
    }

    // Tell the ConnectivityManager to enable the WiFi station interface.
    // Note that any effects of enabling the WiFi station interface (e.g. connecting to an AP) happen
    // asynchronously with this call.
    err = ConnectivityMgr.SetWiFiStationMode(ConnectivityManager::kWiFiStationMode_Enabled);
    SuccessOrExit(err);

    // Respond with a Success response.
    err = NetworkProvisioningSvr.SendSuccessResponse();
    SuccessOrExit(err);

exit:
    return err;
}

WEAVE_ERROR ConnectivityManager::NetworkProvisioningDelegate::HandleDisableNetwork(uint32_t networkId)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    // Reject the request if the application is currently in control of the WiFi station.
    if (RejectIfApplicationControlled(true))
    {
        ExitNow();
    }

    // Verify that the specified network exists.
    if (!ConnectivityMgr.IsWiFiStationProvisioned() || networkId != kWiFiStationNetworkId)
    {
        err = NetworkProvisioningSvr.SendStatusReport(kWeaveProfile_NetworkProvisioning, kStatusCode_UnknownNetwork);
        ExitNow();
    }

    // Tell the ConnectivityManager to disable the WiFi station interface.
    // Note that any effects of disabling the WiFi station interface (e.g. disconnecting from an AP) happen
    // asynchronously with this call.
    err = ConnectivityMgr.SetWiFiStationMode(ConnectivityManager::kWiFiStationMode_Disabled);
    SuccessOrExit(err);

    // Respond with a Success response.
    err = NetworkProvisioningSvr.SendSuccessResponse();
    SuccessOrExit(err);

exit:
    return err;
}

WEAVE_ERROR ConnectivityManager::NetworkProvisioningDelegate::HandleTestConnectivity(uint32_t networkId)
{
    return WEAVE_ERROR_NOT_IMPLEMENTED;
}

WEAVE_ERROR ConnectivityManager::NetworkProvisioningDelegate::HandleSetRendezvousMode(uint16_t rendezvousMode)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    // If any modes other than EnableWiFiRendezvousNetwork or EnableThreadRendezvous
    // were specified, fail with Common:UnsupportedMessage.
    if ((rendezvousMode & ~(kRendezvousMode_EnableWiFiRendezvousNetwork | kRendezvousMode_EnableThreadRendezvous)) != 0)
    {
        err = NetworkProvisioningSvr.SendStatusReport(kWeaveProfile_Common, kStatus_UnsupportedMessage);
        ExitNow();
    }

    // If EnableThreadRendezvous was requested, fail with NetworkProvisioning:UnsupportedNetworkType.
    if ((rendezvousMode & kRendezvousMode_EnableThreadRendezvous) != 0)
    {
        err = NetworkProvisioningSvr.SendStatusReport(kWeaveProfile_NetworkProvisioning, kStatusCode_UnsupportedNetworkType);
        ExitNow();
    }

    // Reject the request if the application is currently in control of the WiFi AP.
    if (RejectIfApplicationControlled(false))
    {
        ExitNow();
    }

    // If the request is to start the WiFi "rendezvous network" (a.k.a. the WiFi AP interface)...
    if (rendezvousMode != 0)
    {
        // If the AP interface has been expressly disabled by the application, fail with Common:NotAvailable.
        if (ConnectivityMgr.GetWiFiAPMode() == ConnectivityManager::kWiFiAPMode_Disabled)
        {
            err = NetworkProvisioningSvr.SendStatusReport(kWeaveProfile_Common, kStatus_NotAvailable);
            ExitNow();
        }

        // Otherwise, request the ConnectivityManager to demand start the WiFi AP interface.
        // If the interface is already active this will have no immediate effect, except if the
        // interface is in the "demand" mode, in which case this will serve to extend the
        // active time.
        ConnectivityMgr.DemandStartWiFiAP();
    }

    // Otherwise the request is to stop the WiFi rendezvous network, so request the ConnectivityManager
    // to stop the AP interface if it has been demand started.  This will have no effect if the
    // interface is already stopped, or if the application has expressly enabled the interface.
    else
    {
        ConnectivityMgr.StopOnDemandWiFiAP();
    }

    // Respond with a Success response.
    err = NetworkProvisioningSvr.SendSuccessResponse();
    SuccessOrExit(err);

exit:
    return err;
}

void ConnectivityManager::NetworkProvisioningDelegate::StartPendingScan()
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    wifi_scan_config_t scanConfig;

    // Do nothing if there's no ScanNetworks request pending, or if a scan is already in progress.
    if (NetworkProvisioningSvr.GetCurrentOp() != kMsgType_ScanNetworks || ConnectivityMgr.mScanInProgress)
    {
        return;
    }

    // Defer the scan if the WiFi station is in the process of connecting.
    if (ConnectivityMgr.mWiFiStationState == kWiFiStationState_Connecting)
    {
        return;
    }

    // Initiate an active scan using the default dwell times.  Configure the scan to return hidden networks.
    memset(&scanConfig, 0, sizeof(scanConfig));
    scanConfig.show_hidden = 1;
    scanConfig.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    err = esp_wifi_scan_start(&scanConfig, false);
    SuccessOrExit(err);

#if WEAVE_PLATFORM_CONFIG_WIFI_SCAN_COMPLETION_TIMEOUT
    // Arm timer in case we never get the scan done event.
    SystemLayer.StartTimer(WEAVE_PLATFORM_CONFIG_WIFI_SCAN_COMPLETION_TIMEOUT, HandleScanTimeOut, NULL);
#endif // WEAVE_PLATFORM_CONFIG_WIFI_SCAN_COMPLETION_TIMEOUT

    ConnectivityMgr.mScanInProgress = true;

exit:
    // If an error occurred, send a Internal Error back to the requestor.
    if (err != WEAVE_NO_ERROR)
    {
        NetworkProvisioningSvr.SendStatusReport(kWeaveProfile_Common, kStatus_InternalError, err);
    }
}

void ConnectivityManager::NetworkProvisioningDelegate::HandleScanDone()
{
    WEAVE_ERROR err;
    wifi_ap_record_t * scanResults = NULL;
    uint16_t scanResultCount;
    uint16_t encodedResultCount;
    PacketBuffer * respBuf = NULL;

    // If we receive a SCAN DONE event for a scan that we didn't initiate, ignore it.
    VerifyOrExit(ConnectivityMgr.mScanInProgress, err = WEAVE_ERROR_INCORRECT_STATE);

    ConnectivityMgr.mScanInProgress = false;

#if WEAVE_PLATFORM_CONFIG_WIFI_SCAN_COMPLETION_TIMEOUT
    // Cancel the scan timeout timer.
    SystemLayer.CancelTimer(HandleScanTimeOut, NULL);
#endif // WEAVE_PLATFORM_CONFIG_WIFI_SCAN_COMPLETION_TIMEOUT

    // Determine the number of scan results found.
    err = esp_wifi_scan_get_ap_num(&scanResultCount);
    SuccessOrExit(err);

    // Only return up to WEAVE_PLATFORM_CONFIG_MAX_SCAN_NETWORKS_RESULTS.
    scanResultCount = min(scanResultCount, (uint16_t)WEAVE_PLATFORM_CONFIG_MAX_SCAN_NETWORKS_RESULTS);

    // Allocate a buffer to hold the scan results array.
    scanResults = (wifi_ap_record_t *)malloc(scanResultCount * sizeof(wifi_ap_record_t));
    VerifyOrExit(scanResults != NULL, err = WEAVE_ERROR_NO_MEMORY);

    // Collect the scan results from the ESP WiFi driver.  Note that this also *frees*
    // the internal copy of the results.
    err = esp_wifi_scan_get_ap_records(&scanResultCount, scanResults);
    SuccessOrExit(err);

    // If the ScanNetworks request is still outstanding...
    if (NetworkProvisioningSvr.GetCurrentOp() == kMsgType_ScanNetworks)
    {
        nl::Weave::TLV::TLVWriter writer;
        TLVType outerContainerType;

        // Sort results by rssi.
        qsort(scanResults, scanResultCount, sizeof(*scanResults), OrderESPScanResultsByRSSI);

        // Allocate a packet buffer to hold the encoded scan results.
        respBuf = PacketBuffer::New(WEAVE_SYSTEM_CONFIG_HEADER_RESERVE_SIZE + 1);
        VerifyOrExit(respBuf != NULL, err = WEAVE_ERROR_NO_MEMORY);

        // Encode the list of scan results into the response buffer.  If the encoded size of all
        // the results exceeds the size of the buffer, encode only what will fit.
        writer.Init(respBuf, respBuf->AvailableDataLength() - 1);
        err = writer.StartContainer(AnonymousTag, kTLVType_Array, outerContainerType);
        SuccessOrExit(err);
        for (encodedResultCount = 0; encodedResultCount < scanResultCount; encodedResultCount++)
        {
            NetworkInfo netInfo;
            const wifi_ap_record_t & scanResult = scanResults[encodedResultCount];

            netInfo.Reset();
            memcpy(netInfo.WiFiSSID, scanResult.ssid, min(strlen((char *)scanResult.ssid) + 1, (size_t)NetworkInfo::kMaxWiFiSSIDLength));
            netInfo.WiFiSSID[NetworkInfo::kMaxWiFiSSIDLength] = 0;
            netInfo.WiFiMode = kWiFiMode_Managed;
            netInfo.WiFiRole = kWiFiRole_Station;
            netInfo.WiFiSecurityType = ESPWiFiAuthModeToWeaveWiFiSecurityType(scanResult.authmode);
            netInfo.WirelessSignalStrength = scanResult.rssi;

            {
                nl::Weave::TLV::TLVWriter savePoint = writer;
                err = netInfo.Encode(writer);
                if (err == WEAVE_ERROR_BUFFER_TOO_SMALL)
                {
                    writer = savePoint;
                    break;
                }
            }
            SuccessOrExit(err);
        }
        err = writer.EndContainer(outerContainerType);
        SuccessOrExit(err);
        err = writer.Finalize();
        SuccessOrExit(err);

        // Send the scan results to the requestor.  Note that this method takes ownership of the
        // buffer, success or fail.
        err = NetworkProvisioningSvr.SendNetworkScanComplete(encodedResultCount, respBuf);
        respBuf = NULL;
        SuccessOrExit(err);
    }

exit:
    PacketBuffer::Free(respBuf);

    // If an error occurred and we haven't yet responded, send a Internal Error back to the
    // requestor.
    if (err != WEAVE_NO_ERROR && NetworkProvisioningSvr.GetCurrentOp() == kMsgType_ScanNetworks)
    {
        NetworkProvisioningSvr.SendStatusReport(kWeaveProfile_Common, kStatus_InternalError, err);
    }

    // Schedule a call to the ConnectivityManager's DriveStationState method in case a station connect
    // attempt was deferred because the scan was in progress.
    SystemLayer.ScheduleWork(ConnectivityMgr.DriveStationState, NULL);
}

// ==================== ConnectivityManager::NetworkProvisioningDelegate Private Methods ====================

WEAVE_ERROR ConnectivityManager::NetworkProvisioningDelegate::GetWiFiStationProvision(NetworkInfo & netInfo, bool includeCredentials)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    wifi_config_t stationConfig;

    netInfo.Reset();

    err = esp_wifi_get_config(ESP_IF_WIFI_STA, &stationConfig);
    SuccessOrExit(err);

    VerifyOrExit(stationConfig.sta.ssid[0] != 0, err = WEAVE_ERROR_INCORRECT_STATE);

    netInfo.NetworkId = kWiFiStationNetworkId;
    netInfo.NetworkIdPresent = true;
    netInfo.NetworkType = kNetworkType_WiFi;
    memcpy(netInfo.WiFiSSID, stationConfig.sta.ssid, min(strlen((char *)stationConfig.sta.ssid) + 1, sizeof(netInfo.WiFiSSID)));
    netInfo.WiFiMode = kWiFiMode_Managed;
    netInfo.WiFiRole = kWiFiRole_Station;
    // TODO: this is broken
    switch (stationConfig.sta.threshold.authmode)
    {
    case WIFI_AUTH_OPEN:
        netInfo.WiFiSecurityType = kWiFiSecurityType_None;
        break;
    case WIFI_AUTH_WEP:
        netInfo.WiFiSecurityType = kWiFiSecurityType_WEP;
        break;
    case WIFI_AUTH_WPA_PSK:
        netInfo.WiFiSecurityType = kWiFiSecurityType_WPAPersonal;
        break;
    case WIFI_AUTH_WPA2_PSK:
        netInfo.WiFiSecurityType = kWiFiSecurityType_WPA2Personal;
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        netInfo.WiFiSecurityType = kWiFiSecurityType_WPA2MixedPersonal;
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        netInfo.WiFiSecurityType = kWiFiSecurityType_WPA2Enterprise;
        break;
    default:
        netInfo.WiFiSecurityType = kWiFiSecurityType_NotSpecified;
        break;
    }
    if (includeCredentials)
    {
        netInfo.WiFiKeyLen = min(strlen((char *)stationConfig.sta.password), sizeof(netInfo.WiFiKey));
        memcpy(netInfo.WiFiKey, stationConfig.sta.password, netInfo.WiFiKeyLen);
    }

exit:
    return err;
}

WEAVE_ERROR ConnectivityManager::NetworkProvisioningDelegate::ValidateWiFiStationProvision(
        const ::WeavePlatform::Internal::NetworkInfo & netInfo,
        uint32_t & statusProfileId, uint16_t & statusCode)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    if (netInfo.NetworkType != kNetworkType_WiFi)
    {
        ESP_LOGE(TAG, "ConnectivityManager: Unsupported WiFi station network type: %d", netInfo.NetworkType);
        statusProfileId = kWeaveProfile_NetworkProvisioning;
        statusCode = kStatusCode_UnsupportedNetworkType;
        ExitNow(err = WEAVE_ERROR_INVALID_ARGUMENT);
    }

    if (netInfo.WiFiSSID[0] == 0)
    {
        ESP_LOGE(TAG, "ConnectivityManager: Missing WiFi station SSID");
        statusProfileId = kWeaveProfile_NetworkProvisioning;
        statusCode = kStatusCode_InvalidNetworkConfiguration;
        ExitNow(err = WEAVE_ERROR_INVALID_ARGUMENT);
    }

    if (netInfo.WiFiMode != kWiFiMode_Managed)
    {
        if (netInfo.WiFiMode == kWiFiMode_NotSpecified)
        {
            ESP_LOGE(TAG, "ConnectivityManager: Missing WiFi station mode");
        }
        else
        {
            ESP_LOGE(TAG, "ConnectivityManager: Unsupported WiFi station mode: %d", netInfo.WiFiMode);
        }
        statusProfileId = kWeaveProfile_NetworkProvisioning;
        statusCode = kStatusCode_InvalidNetworkConfiguration;
        ExitNow(err = WEAVE_ERROR_INVALID_ARGUMENT);
    }

    if (netInfo.WiFiRole != kWiFiRole_Station)
    {
        if (netInfo.WiFiRole == kWiFiRole_NotSpecified)
        {
            ESP_LOGE(TAG, "ConnectivityManager: Missing WiFi station role");
        }
        else
        {
            ESP_LOGE(TAG, "ConnectivityManager: Unsupported WiFi station role: %d", netInfo.WiFiRole);
        }
        statusProfileId = kWeaveProfile_NetworkProvisioning;
        statusCode = kStatusCode_InvalidNetworkConfiguration;
        ExitNow(err = WEAVE_ERROR_INVALID_ARGUMENT);
    }

    if (netInfo.WiFiSecurityType != kWiFiSecurityType_None &&
        netInfo.WiFiSecurityType != kWiFiSecurityType_WEP &&
        netInfo.WiFiSecurityType != kWiFiSecurityType_WPAPersonal &&
        netInfo.WiFiSecurityType != kWiFiSecurityType_WPA2Personal &&
        netInfo.WiFiSecurityType != kWiFiSecurityType_WPA2Enterprise)
    {
        ESP_LOGE(TAG, "ConnectivityManager: Unsupported WiFi station security type: %d", netInfo.WiFiSecurityType);
        statusProfileId = kWeaveProfile_NetworkProvisioning;
        statusCode = kStatusCode_UnsupportedWiFiSecurityType;
        ExitNow(err = WEAVE_ERROR_INVALID_ARGUMENT);
    }

    if (netInfo.WiFiSecurityType != kWiFiSecurityType_None && netInfo.WiFiKeyLen == 0)
    {
        ESP_LOGE(TAG, "NetworkProvisioning: Missing WiFi Key");
        statusProfileId = kWeaveProfile_NetworkProvisioning;
        statusCode = kStatusCode_InvalidNetworkConfiguration;
        ExitNow(err = WEAVE_ERROR_INVALID_ARGUMENT);
    }

exit:
    return err;
}

WEAVE_ERROR ConnectivityManager::NetworkProvisioningDelegate::SetESPStationConfig(const ::WeavePlatform::Internal::NetworkInfo  & netInfo)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    wifi_mode_t wifiMode;
    wifi_config_t wifiConfig;
    bool restoreMode = false;

    // Inspect the current ESP wifi mode.  If the station interface is not enabled, enable it now.
    // The ESP wifi station interface must be enabled before esp_wifi_set_config(ESP_IF_WIFI_STA,...)
    // can be called.
    if (esp_wifi_get_mode(&wifiMode) == ESP_OK && (wifiMode != WIFI_MODE_STA && wifiMode != WIFI_MODE_APSTA))
    {
        err = ChangeESPWiFiMode(ESP_IF_WIFI_STA, true);
        SuccessOrExit(err);
        restoreMode = true;
    }

    // Initialize an ESP wifi_config_t structure based on the new provision information.
    memset(&wifiConfig, 0, sizeof(wifiConfig));
    memcpy(wifiConfig.sta.ssid, netInfo.WiFiSSID, min(strlen(netInfo.WiFiSSID) + 1, sizeof(wifiConfig.sta.ssid)));
    memcpy(wifiConfig.sta.password, netInfo.WiFiKey, min((size_t)netInfo.WiFiKeyLen, sizeof(wifiConfig.sta.password)));
    if (netInfo.WiFiSecurityType == kWiFiSecurityType_NotSpecified)
    {
        wifiConfig.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    }
    else
    {
        wifiConfig.sta.scan_method = WIFI_FAST_SCAN;
        wifiConfig.sta.threshold.rssi = 0;
        switch (netInfo.WiFiSecurityType)
        {
        case kWiFiSecurityType_None:
            wifiConfig.sta.threshold.authmode = WIFI_AUTH_OPEN;
            break;
        case kWiFiSecurityType_WEP:
            wifiConfig.sta.threshold.authmode = WIFI_AUTH_WEP;
            break;
        case kWiFiSecurityType_WPAPersonal:
            wifiConfig.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;
            break;
        case kWiFiSecurityType_WPA2Personal:
            wifiConfig.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            break;
        case kWiFiSecurityType_WPA2Enterprise:
            wifiConfig.sta.threshold.authmode = WIFI_AUTH_WPA2_ENTERPRISE;
            break;
        default:
            ExitNow(err = WEAVE_ERROR_INVALID_ARGUMENT);
        }
    }
    wifiConfig.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    // Configure the ESP WiFi interface.
    err = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifiConfig);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_config() failed: %s", nl::ErrorStr(err));
    }
    SuccessOrExit(err);

    ESP_LOGI(TAG, "WiFi station provision set (SSID: %s)", netInfo.WiFiSSID);

exit:
    if (restoreMode)
    {
        WEAVE_ERROR setModeErr = esp_wifi_set_mode(wifiMode);
        if (setModeErr != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_wifi_set_mode() failed: %s", nl::ErrorStr(setModeErr));
        }
    }
    return err;
}

bool ConnectivityManager::NetworkProvisioningDelegate::RejectIfApplicationControlled(bool station)
{
    bool isAppControlled = (station)
        ? ConnectivityMgr.IsWiFiStationApplicationControlled()
        : ConnectivityMgr.IsWiFiAPApplicationControlled();

    // Reject the request if the application is currently in control of the WiFi station.
    if (isAppControlled)
    {
        NetworkProvisioningSvr.SendStatusReport(kWeaveProfile_Common, kStatus_NotAvailable);
    }

    return isAppControlled;
}

#if WEAVE_PLATFORM_CONFIG_WIFI_SCAN_COMPLETION_TIMEOUT

void ConnectivityManager::NetworkProvisioningDelegate::HandleScanTimeOut(::nl::Weave::System::Layer * aLayer, void * aAppState, ::nl::Weave::System::Error aError)
{
    ESP_LOGE(TAG, "WiFi scan timed out");

    ConnectivityMgr.mScanInProgress = false;

    // If we haven't yet responded, send a Internal Error back to the requestor.
    if (NetworkProvisioningSvr.GetCurrentOp() == kMsgType_ScanNetworks)
    {
        NetworkProvisioningSvr.SendStatusReport(kWeaveProfile_Common, kStatus_InternalError, WEAVE_ERROR_TIMEOUT);
    }

    // Schedule a call to the ConnectivityManager's DriveStationState method in case a station connect
    // attempt was deferred because the scan was in progress.
    SystemLayer.ScheduleWork(ConnectivityMgr.DriveStationState, NULL);
}

#endif // WEAVE_PLATFORM_CONFIG_WIFI_SCAN_COMPLETION_TIMEOUT

// ==================== Local Utility Functions ====================

namespace {

const char *ESPWiFiModeToStr(wifi_mode_t wifiMode)
{
    switch (wifiMode)
    {
    case WIFI_MODE_NULL:
        return "NULL";
    case WIFI_MODE_STA:
        return "STA";
    case WIFI_MODE_AP:
        return "AP";
    case WIFI_MODE_APSTA:
        return "STA+AP";
    default:
        return "(unknown)";
    }
}

WEAVE_ERROR ChangeESPWiFiMode(esp_interface_t intf, bool enabled)
{
    WEAVE_ERROR err;
    wifi_mode_t curWiFiMode, targetWiFiMode;
    bool stationEnabled, apEnabled;

    VerifyOrExit(intf == ESP_IF_WIFI_STA || intf == ESP_IF_WIFI_AP, err = WEAVE_ERROR_INVALID_ARGUMENT);

    err = esp_wifi_get_mode(&curWiFiMode);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_get_mode() failed: %s", nl::ErrorStr(err));
    }
    SuccessOrExit(err);

    stationEnabled = (curWiFiMode == WIFI_MODE_STA || curWiFiMode == WIFI_MODE_APSTA);
    apEnabled = (curWiFiMode == WIFI_MODE_AP || curWiFiMode == WIFI_MODE_APSTA);

    if (intf == ESP_IF_WIFI_STA)
    {
        stationEnabled = enabled;
    }
    else
    {
        apEnabled = enabled;
    }

    targetWiFiMode = (stationEnabled)
        ? ((apEnabled) ? WIFI_MODE_APSTA : WIFI_MODE_STA)
        : ((apEnabled) ? WIFI_MODE_AP : WIFI_MODE_NULL);

    if (targetWiFiMode != curWiFiMode)
    {
        ESP_LOGI(TAG, "Changing ESP WiFi mode: %s -> %s", ESPWiFiModeToStr(curWiFiMode), ESPWiFiModeToStr(targetWiFiMode));

        err = esp_wifi_set_mode(targetWiFiMode);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_wifi_set_mode() failed: %s", nl::ErrorStr(err));
        }
        SuccessOrExit(err);
    }

exit:
    return err;
}

WiFiSecurityType ESPWiFiAuthModeToWeaveWiFiSecurityType(wifi_auth_mode_t authMode)
{
    switch (authMode)
    {
    case WIFI_AUTH_OPEN:
        return kWiFiSecurityType_None;
    case WIFI_AUTH_WEP:
        return kWiFiSecurityType_WEP;
    case WIFI_AUTH_WPA_PSK:
        return kWiFiSecurityType_WPAPersonal;
    case WIFI_AUTH_WPA2_PSK:
        return kWiFiSecurityType_WPA2Personal;
    case WIFI_AUTH_WPA_WPA2_PSK:
        return kWiFiSecurityType_WPA2MixedPersonal;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return kWiFiSecurityType_WPA2Enterprise;
    default:
        return kWiFiSecurityType_NotSpecified;
    }
}

int OrderESPScanResultsByRSSI(const void * _res1, const void * _res2)
{
    const wifi_ap_record_t * res1 = (const wifi_ap_record_t *) _res1;
    const wifi_ap_record_t * res2 = (const wifi_ap_record_t *) _res2;

    if (res1->rssi > res2->rssi)
    {
        return -1;
    }
    if (res1->rssi < res2->rssi)
    {
        return 1;
    }
    return 0;
}

} // unnamed namespace

} // namespace WeavePlatform
