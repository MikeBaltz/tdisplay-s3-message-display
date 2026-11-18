#include "weather_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "app_config.h"

namespace {
// ISRG Root X1 validates the Let's Encrypt certificate used by Open-Meteo.
const char OPEN_METEO_ROOT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";
}

void WeatherService::begin() {
  data_ = WeatherData{};
  lastAttempt_ = 0;
  lastSuccess_ = 0;
  fetching_ = false;
  taskHandle_ = nullptr;
}

void WeatherService::setLocation(float latitude, float longitude) {
  pendingLatitude_ = latitude;
  pendingLongitude_ = longitude;
  locationPending_ = true;
}

void WeatherService::update(bool wifiConnected) {
  // The URL is only rebuilt while no fetch is running, because the fetch task reads it.
  if (locationPending_ && !fetching_) {
    locationPending_ = false;
    url_ = String(WEATHER_URL_BASE) + "&latitude=" + String(pendingLatitude_, 4) +
           "&longitude=" + String(pendingLongitude_, 4);
    data_ = WeatherData{};
    data_.updatedAt = millis() | 1;  // lets the screen notice the old reading is gone
    lastAttempt_ = 0;
    lastSuccess_ = 0;
  }
  if (!wifiConnected || fetching_ || url_.length() == 0) return;

  const uint32_t now = millis();
  const bool neverSucceeded = lastSuccess_ == 0;
  const bool refreshDue = !neverSucceeded && now - lastSuccess_ >= 900000;
  const bool retryDue = lastAttempt_ == 0 || now - lastAttempt_ >= 60000;
  if ((!neverSucceeded && !refreshDue) || !retryDue) return;

  lastAttempt_ = now;
  fetching_ = true;
  if (xTaskCreatePinnedToCore(fetchTask, "weather", 8192, this, 1, &taskHandle_, 0) != pdPASS) {
    fetching_ = false;
    taskHandle_ = nullptr;
    Serial.println("Weather worker task creation failed");
  }
}

void WeatherService::fetchTask(void *context) {
  WeatherService *service = static_cast<WeatherService *>(context);
  WeatherData result;
  if (service->fetch(result)) {
    service->data_ = result;
    service->lastSuccess_ = millis();
    Serial.println("Weather updated");
  } else {
    Serial.println("Weather update failed; cached data retained");
  }
  service->fetching_ = false;
  service->taskHandle_ = nullptr;
  vTaskDelete(nullptr);
}

bool WeatherService::fetch(WeatherData &result) {
  WiFiClientSecure client;
  client.setCACert(OPEN_METEO_ROOT_CA);
  HTTPClient http;
  if (!http.begin(client, url_)) {
    Serial.println("Weather HTTP begin failed");
    return false;
  }
  http.setTimeout(5000);
  const int status = http.GET();
  Serial.printf("Weather HTTP status: %d\n", status);
  if (status != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();
  DynamicJsonDocument document(4096);
  const DeserializationError error = deserializeJson(document, payload);
  if (error) {
    Serial.print("Weather JSON parse error: ");
    Serial.println(error.c_str());
    return false;
  }

  JsonObject current = document["current"];
  JsonObject daily = document["daily"];
  JsonArray minimums = daily["temperature_2m_min"].as<JsonArray>();
  JsonArray maximums = daily["temperature_2m_max"].as<JsonArray>();
  if (current.isNull() || daily.isNull() || current["temperature_2m"].isNull() ||
      current["weather_code"].isNull() || minimums.isNull() || maximums.isNull() ||
      minimums.size() == 0 || maximums.size() == 0 || minimums[0].isNull() ||
      maximums[0].isNull()) {
    Serial.println("Weather JSON missing expected fields");
    return false;
  }

  result.valid = true;
  result.currentTemperature = current["temperature_2m"].as<float>();
  result.minimumTemperature = daily["temperature_2m_min"][0].as<float>();
  result.maximumTemperature = daily["temperature_2m_max"][0].as<float>();
  result.weatherCode = current["weather_code"].as<int>();
  // Optional: older responses without the field are treated as daytime.
  result.isDay = (current["is_day"] | 1) != 0;
  result.updatedAt = millis();
  return true;
}

const WeatherData &WeatherService::data() const { return data_; }

String WeatherService::condition() const {
  if (!data_.valid) return "--";
  // WMO weather interpretation codes, as returned by Open-Meteo.
  const int code = data_.weatherCode;
  if (code == 0) return "Clear";
  if (code == 1) return "Mostly clear";
  if (code == 2) return "Partly cloudy";
  if (code == 3) return "Cloudy";
  if (code <= 48) return "Fog";
  if (code <= 57) return "Drizzle";
  if (code <= 67) return "Rain";
  if (code <= 77) return "Snow";
  if (code <= 82) return "Showers";
  if (code <= 86) return "Snow";
  return "Storm";
}
