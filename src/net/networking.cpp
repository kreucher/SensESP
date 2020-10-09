#include "networking.h"

#include "sensesp.h"
#include "system/led_blinker.h"

// Wifi config portal timeout (seconds). The smaller the value, the faster
// the device will attempt to reconnect. If set too small, it might
// become impossible to actually configure the Wifi settings in the captive
// portal.
#ifndef WIFI_CONFIG_PORTAL_TIMEOUT
#define WIFI_CONFIG_PORTAL_TIMEOUT 180
#endif

bool should_save_config = false;

void save_config_callback() { should_save_config = true; }

Networking::Networking(String config_path, String ssid, String password,
                       String hostname)
    : Configurable{config_path} {
  this->hostname = new ObservableValue<String>(hostname);

  preset_ssid = ssid;
  preset_password = password;
  preset_hostname = hostname;

  if (!ssid.isEmpty()) {
    debugI("Using hard-coded SSID %s and password", ssid.c_str());
    this->ap_ssid = ssid;
    this->ap_password = password;
  } else {
    load_configuration();
  }
  server = new AsyncWebServer(80);
  dns = new DNSServer();
  wifi_manager = new AsyncWiFiManager(server, dns);

  WiFi.onEvent(
      [this](WiFiEvent_t event, WiFiEventInfo_t info) {
        debugI("Got ip address of Device: %s",
               IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
        connection_callback(true);
      },
      SYSTEM_EVENT_STA_GOT_IP);

  WiFi.onEvent(
      [this](WiFiEvent_t event, WiFiEventInfo_t info) {
        debugI("Connected to WiFi, SSID: %s (signal: %d)", info.connected.ssid,
               WiFi.RSSI());
      },
      SYSTEM_EVENT_STA_CONNECTED);  // if we are connected to Wifi
  WiFi.onEvent(
      [this](WiFiEvent_t event, WiFiEventInfo_t info) {
        auto reason = get_disconnected_reason(info.disconnected.reason);

        debugD("WiFi disconnected. Reason=%s.",
               reason.c_str());  // info.disconnected.reason);

        connection_callback(false);
      },
      SYSTEM_EVENT_STA_DISCONNECTED);
}

void Networking::check_connection() {
  if (!offline && WiFi.status() != WL_CONNECTED) {
    // if connection is lost, simply restart
    auto wifiStatus = WiFi.status();
    debugD("WiFi status: %d", wifiStatus);
    if (wifiStatus != WL_CONNECTED) {
      setup_saved_ssid();
    }
  }
}

void Networking::setup(std::function<void(bool)> connection_cb) {
  connection_callback = connection_cb;

  if (ap_ssid != "" && ap_password != "") {
    setup_saved_ssid();
  }
  if (useWifiManager && ap_ssid == "" && WiFi.status() != WL_CONNECTED) {
    setup_wifi_manager();
  }

  app.onRepeat(5000, std::bind(&Networking::check_connection, this));
}

void Networking::setup_saved_ssid() {
  WiFi.mode(WIFI_STA);
  auto status = WiFi.begin(ap_ssid.c_str(), ap_password.c_str());
  debugI("WiFi begin result=%d", status);
}

void Networking::setup_wifi_manager() {
  should_save_config = false;

  // set config save notify callback
  wifi_manager->setSaveConfigCallback(save_config_callback);

  wifi_manager->setConfigPortalTimeout(WIFI_CONFIG_PORTAL_TIMEOUT);

#ifdef SERIAL_DEBUG_DISABLED
  wifi_manager->setDebugOutput(false);
#endif
  AsyncWiFiManagerParameter custom_hostname("hostname",
                                            "Set ESP Device custom hostname",
                                            this->hostname->get().c_str(), 20);
  wifi_manager->addParameter(&custom_hostname);

  // Create a unique SSID for configuring each SensESP Device
  String config_ssid = this->hostname->get();
  config_ssid = "Configure " + config_ssid;
  const char* pconfig_ssid = config_ssid.c_str();

  if (!wifi_manager->autoConnect(pconfig_ssid)) {
    debugE("Failed to connect to wifi and config timed out. Restarting...");
    ESP.restart();
  }

  debugI("Connected to wifi,");
  debugI("IP address of Device: %s", WiFi.localIP().toString().c_str());
  connection_callback(true);

  if (should_save_config) {
    String new_hostname = custom_hostname.getValue();
    debugI("Got new custom hostname: %s", new_hostname.c_str());
    this->hostname->set(new_hostname);
    this->ap_ssid = WiFi.SSID();
    debugI("Got new SSID and password: %s", ap_ssid.c_str());
    this->ap_password = WiFi.psk();
    save_configuration();
    debugW("Restarting in 500ms");
    app.onDelay(500, []() { ESP.restart(); });
  }
}

ObservableValue<String>* Networking::get_hostname() { return this->hostname; }


static const char SCHEMA_PREFIX[] PROGMEM = R"({
"type": "object",
"properties": {
)";

String get_property_row(String key, String title, bool readonly) {
  String readonly_title = "";
  String readonly_property = "";

  if (readonly) {
    readonly_title = " (readonly)";
    readonly_property = ",\"readOnly\":true";
  }

  return "\"" + key + "\":{\"title\":\"" + title + readonly_title + "\"," +
         "\"type\":\"string\"" + readonly_property + "}";
}

String Networking::get_config_schema() {
  String schema;
  // If hostname is not set by SensESPAppBuilder::set_hostname() in main.cpp,
  // then preset_hostname will be "SensESP", and should not be read-only in the
  // Config UI. If preset_hostname is not "SensESP", then it was set in main.cpp, so
  // it should be read-only.
  bool hostname_preset = preset_hostname != "SensESP";
  bool wifi_preset = preset_ssid != "";
  return String(FPSTR(SCHEMA_PREFIX)) +
         get_property_row("hostname", "ESP device hostname", hostname_preset) +
         "," +
         get_property_row("ap_ssid", "Wifi Access Point SSID", wifi_preset) +
         "," +
         get_property_row("ap_password", "Wifi Access Point Password",
                          wifi_preset) +
         "}}";
}

void Networking::get_configuration(JsonObject& root) {

  root["hostname"] = this->hostname->get();
  root["ap_ssid"] = this->ap_ssid;
  root["ap_password"] = this->ap_password;
}

bool Networking::set_configuration(const JsonObject& config) {
  if (!config.containsKey("hostname")) {
    return false;
  }

  if (preset_hostname == "SensESP") {
    this->hostname->set(config["hostname"].as<String>());
  }

  if (preset_ssid == "") {
    debugW("Using saved SSID and password");
    this->ap_ssid = config["ap_ssid"].as<String>();
    this->ap_password = config["ap_password"].as<String>();
  }
  return true;
}

void Networking::reset_settings() {
  ap_ssid = preset_ssid;
  ap_password = preset_password;

  save_configuration();
  wifi_manager->resetSettings();
}

void Networking::set_offline(bool offline) {
  this->offline = offline;

  debugI("Setting offline parameter to %d", offline);

  if (offline) {
    WiFi.mode(WIFI_OFF);
    connection_callback(false);
  }
}

String Networking::get_disconnected_reason(uint8_t reason) {
  if (reason == WIFI_REASON_NO_AP_FOUND) {
    return String("no AP found");
  } else if (reason == WIFI_REASON_NOT_ASSOCED ||
             reason == WIFI_REASON_NOT_AUTHED) {
    return String("rejected or bad password");
  } else {
    return String((int)reason);
  }
}