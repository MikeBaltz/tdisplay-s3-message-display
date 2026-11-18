#include "gif_player.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>

#include "app_config.h"

namespace {
#if 0
constexpr size_t MAX_GIF_SIZE = 4 * 1024 * 1024;
constexpr uint32_t DOWNLOAD_RETRY_MS = 60000;
constexpr uint16_t GIF_BACKGROUND = 0x0841;
constexpr uint16_t GIF_TEXT = 0xFFFF;

const char OPEN_MEDIA_ROOT_CA[] PROGMEM = R"EOF(
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

// media.giphy.com currently chains to GlobalSign Atlas R46.
const char GIF_ROOT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFWjCCA0KgAwIBAgISEdK7udcjGJ5AXwqdLdDfJWfRMA0GCSqGSIb3DQEBDAUA
MEYxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9iYWxTaWduIG52LXNhMRwwGgYD
VQQDExNHbG9iYWxTaWduIFJvb3QgUjQ2MB4XDTE5MDMyMDAwMDAwMFoXDTQ2MDMy
MDAwMDAwMFowRjELMAkGA1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYt
c2ExHDAaBgNVBAMTE0dsb2JhbFNpZ24gUm9vdCBSNDYwggIiMA0GCSqGSIb3DQEB
AQUAA4ICDwAwggIKAoICAQCsrHQy6LNl5brtQyYdpokNRbopiLKkHWPd08EsCVeJ
OaFV6Wc0dwxu5FUdUiXSE2te4R2pt32JMl8Nnp8semNgQB+msLZ4j5lUlghYruQG
vGIFAha/r6gjA7aUD7xubMLL1aa7DOn2wQL7Id5m3RerdELv8HQvJfTqa1VbkNud
316HCkD7rRlr+/fKYIje2sGP1q7Vf9Q8g+7XFkyDRTNrJ9CG0Bwta/OrffGFqfUo
0q3v84RLHIf8E6M6cqJaESvWJ3En7YEtbWaBkoe0G1h6zD8K+kZPTXhc+CtI4wSE
y132tGqzZfxCnlEmIyDLPRT5ge1lFgBPGmSXZgjPjHvjK8Cd+RTyG/FWaha/LIWF
cj2JwJ7Vd8+c6h2e0l2V7N8QqN0J8fGdYpV4q7N3L5n5J6kQ8r6d7yM6w8H1c5P
v4H2J1m6M8x0G7z4Q5n6R2p8S3d9F1h4L7j0N5p2T8v3X6z9B1d4F7h0J3l6N
8p2R5t7V0x3Z6b9D1f4H7j0L3n6P9r2T5v8X1z4B7d0F3h6J9l2N5p8R1t4V
7x0Z3b6D9f2H5j8L1n4P7r0T3v6X9z2B5d8F1h4J7l0N3p6R9t2V5x8Z1b4D
7f0H3j6L9n2P5r8T1v4X7z0B3d6F9h2J5l8N1p4R7t0V3x6Z9b2D5f8H1j4L
7n0P3r6T9v2X5z8B1d4F7h0J3l6N9p2R5t8V1x4Z7b0D3f6H9j2L5n8P1r4T
7v0X3z6B9d2F5h8J1l4N7p0R3t6V9x2Z5b8D1f4H7j0L3n6P9r2T5v8X1z4B
7d0F3h6J9l2N5p8R1t4V7x0Z3b6D9f2H5j8L1n4P7r0T3v6X9z2B5d8F1h4J
7l0N3p6R9t2V5x8Z1b4D7f0H3j6L9n2P5r8T1v4X7z0B3d6F9h2J5l8N1p4R
7t0V3x6Z9b2D5f8H1j4L7n0P3r6T9v2X5z8B1d4F7h0J3l6N9p2R5t8V1x4Z
7b0D3f6H9j2L5n8P1r4T7v0X3z6B9d2F5h8J1l4N7p0R3t6V9x2Z5b8D1f4H
7j0L3n6P9r2T5v8X1z4B7d0F3h6J9l2N5p8R1t4V7x0Z3b6D9f2H5j8L1n4P
7r0T3v6X9z2B5d8F1h4J7l0N3p6R9t2V5x8Z1b4D7f0H3j6L9n2P5r8T1v4X
7z0B3d6F9h2J5l8N1p4R7t0V3x6Z9b2D5f8H1j4L7n0P3r6T9v2X5z8B1d4F
7h0J3l6N9p2R5t8V1x4Z7b0D3f6H9j2L5n8P1r4T7v0X3z6B9d2F5h8J1l4N
7p0R3t6V9x2Z5b8D1f4H7j0L3n6P9r2T5v8X1z4B7d0F3h6J9l2N5p8R1t4V
7x0Z3b6D9f2H5j8L1n4P7r0T3v6X9z2B5d8F1h4J7l0N3p6R9t2V5x8Z1b4D
7f0H3j6L9n2P5r8T1v4X7z0B3d6F9h2J5l8N1p4R7t0V3x6Z9b2D5f8H1j4L
7n0P3r6T9v2X5z8B1d4F7h0J3l6N9p2R5t8V1x4Z7b0D3f6H9j2L5n8P1r4T
7v0X3z6B9d2F5h8J1l4N7p0R3t6V9x2Z5b8D1f4H7j0L3n6P9r2T5v8X1z4B
7d0F3h6J9l2N5p8R1t4V7x0Z3b6D9f2H5j8L1n4P7r0T3v6X9z2B5d8F1h4J
7l0N3p6R9t2V5x8Z1b4D7f0H3j6L9n2P5r8T1v4X7z0B3d6F9h2J5l8N1p4R
7t0V3x6Z9b2D5f8H1j4L7n0P3r6T9v2X5z8B1d4F7h0J3l6N9p2R5t8V1x4Z
7b0D3f6H9j2L5n8P1r4T7v0X3z6B9d2F5h8J1l4N7p0R3t6V9x2Z5b8D1f4H
7j0L3n6P9r2T5v8X1z4B7d0F3h6J9l2N5p8R1t4V7x0Z3b6D9f2H5j8L1n4P
7r0T3v6X9z2B5d8F1h4J7l0N3p6R9t2V5x8Z1b4D7f0H3j6L9n2P5r8T1v4X
7z0B3d6F9h2J5l8N1p4R7t0V3x6Z9b2D5f8H1j4L7n0P3r6T9v2X5z8B1d4F
7h0J3l6N9p2R5t8V1x4Z7b0D3f6H9j2L5n8P1r4T7v0X3z6B9d2F5h8J1l4N
7p0R3t6V9x2Z5b8D1f4H7j0L3n6P9r2T5v8X1z4B7d0F3h6J9l2N5p8R1t4V
7x0Z3b6D9f2H5j8L1n4P7r0T3v6X9z2B5d8F1h4J7l0N3p6R9t2V5x8Z1b4D
7f0H3j6L9n2P5r8T1v4X7z0B3d6F9h2J5l8N1p4R7t0V3x6Z9b2D5f8H1j4L
7n0P3r6T9v2X5z8B1d4F7h0J3l6N9p2R5t8V1x4Z7b0D3f6H9j2L5n8P1r4T
7v0X3z6B9d2F5h8J1l4N7p0R3t6V9x2Z5b8D1f4H7j0L3n6P9r2T5v8X1z4B
7d0F3h6J9l2N5p8R1t4V7x0Z3b6D9f2H5j8L1n4P7r0T3v6X9z2B5d8F1h4J
7l0N3p6R9t2V5x8Z1b4D7f0H3j6L9n2P5r8T1v4X7z0B3d6F9h2J5l8N1p4R
7t0V3x6Z9b2D5f8H1j4L7n0P3r6T9v2X5z8B1d4F7h0J3l6N9p2R5t8V1x4Z
7b0D3f6H9j2L5n8P1r4T7v0X3z6B9d2F5h8J1l4N7p0R3t6V9x2Z5b8D1f4H
7j0L3n6P9r2T5v8X1z4B7d0F3h6J9l2N5p8R1t4V7x0Z3b6D9f2H5j8L1n4P
7r0T3v6X9z2B5d8F1h4J7l0N3p6R9t2V5x8Z1b4D7f0H3j6L9n2P5r8T1v4X
7z0B3d6F9h2J5l8N1p4R7t0V3x6Z9b2D5f8H1j4L7n0P3r6T9v2X5z8B1d4F
-----END CERTIFICATE-----
)EOF";

#endif
constexpr size_t MAX_GIF_SIZE = 4 * 1024 * 1024;
constexpr uint32_t DOWNLOAD_RETRY_MS = 60000;
constexpr uint8_t MAX_DOWNLOAD_ATTEMPTS = 3;
constexpr uint16_t GIF_BACKGROUND = 0x0841;
constexpr uint16_t GIF_TEXT = 0xFFFF;
const char GIF_GLOBALSIGN_ROOT_R46[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFWjCCA0KgAwIBAgISEdK7udcjGJ5AXwqdLdDfJWfRMA0GCSqGSIb3DQEBDAUA
MEYxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9iYWxTaWduIG52LXNhMRwwGgYD
VQQDExNHbG9iYWxTaWduIFJvb3QgUjQ2MB4XDTE5MDMyMDAwMDAwMFoXDTQ2MDMy
MDAwMDAwMFowRjELMAkGA1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYt
c2ExHDAaBgNVBAMTE0dsb2JhbFNpZ24gUm9vdCBSNDYwggIiMA0GCSqGSIb3DQEB
AQUAA4ICDwAwggIKAoICAQCsrHQy6LNl5brtQyYdpokNRbopiLKkHWPd08EsCVeJ
OaFV6Wc0dwxu5FUdUiXSE2te4R2pt32JMl8Nnp8semNgQB+msLZ4j5lUlghYruQG
vGIFAha/r6gjA7aUD7xubMLL1aa7DOn2wQL7Id5m3RerdELv8HQvJfTqa1VbkNud
316HCkD7rRlr+/fKYIje2sGP1q7Vf9Q8g+7XFkyDRTNrJ9CG0Bwta/OrffGFqfUo
0q3v84RLHIf8E6M6cqJaESvWJ3En7YEtbWaBkoe0G1h6zD8K+kZPTXhc+CtI4wSE
y132tGqzZfxCnlEmIyDLPRT5ge1lFgBPGmSXZgjPjHvjK8Cd+RTyG/FWaha/LIWF
zXg4mutCagI0GIMXTpRW+LaCtfOW3T3zvn8gdz57GSNrLNRyc0NXfeD412lPFzYE
+cCQYDdF3uYM2HSNrpyibXRdQr4G9dlkbgIQrImwTDsHTUB+JMWKmIJ5jqSngiCN
I/onccnfxkF0oE32kRbcRoxfKWMxWXEM2G/CtjJ9++ZdU6Z+Ffy7dXxd7Pj2Fxzs
x2sZy/N78CsHpdlseVR2bJ0cpm4O6XkMqCNqo98bMDGfsVR7/mrLZqrcZdCinkqa
ByFrgY/bxFn63iLABJzjqls2k+g9vXqhnQt2sQvHnf3PmKgGwvgqo6GDoLclcqUC
4wIDAQABo0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNV
HQ4EFgQUA1yrc4GHqMywptWU4jaWSf8FmSwwDQYJKoZIhvcNAQEMBQADggIBAHx4
7PYCLLtbfpIrXTncvtgdokIzTfnvpCo7RGkerNlFo048p9gkUbJUHJNOxO97k4Vg
JuoJSOD1u8fpaNK7ajFxzHmuEajwmf3lH7wvqMxX63bEIaZHU1VNaL8FpO7XJqti
2kM3S+LGteWygxk6x9PbTZ4IevPuzz5i+6zoYMzRx6Fcg0XERczzF2sUyQQCPtIk
pnnpHs6i58FZFZ8d4kuaPp92CC1r2LpXFNqD6v6MVenQTqnMdzGxRBF6XLE+0xRF
FRhiJBPSy03OXIPBNvIQtQ6IbbjhVp+J3pZmOUdkLG5NrmJ7v2B0GbhWrJKsFjLt
rWhV/pi60zTe9Mlhww6G9kuEYO4Ne7UyWHmRVSyBQ7N0H3qqJZ4d16GLuc1CLgSk
ZoNNiTW2bKg2SnkheCLQQrzRQDGQob4Ez8pn7fXwgNNgyYMqIgXQBztSvwyeqiv5
u+YfjyW6hY0XHgL+XVAEV8/+LbzvXMAaq7afJMbfc2hIkCwU9D9SGuTSyxTDYWnP
4vkYxboznxSjBF25cfe1lNj2M8FawTSLfJvdkzrnE6JwYZ+vj+vYxXX4M2bUdGc6
N3ec592kD3ZDZopD8p/7DEJ4Y9HiD2971KE9dJeFt0g5QdYg/NA6s/rob8SKunE3
vouXsXgxT7PntgMTzlSdriVZzH81Xwj3QEUxeCp6
-----END CERTIFICATE-----
)EOF";
}

GifPlayer *GifPlayer::instance_ = nullptr;

void GifPlayer::begin(TFT_eSPI *display) {
  display_ = display;
  instance_ = this;
  gif_.begin(BIG_ENDIAN_PIXELS);
  Serial.printf("GIF PSRAM total: %u bytes, free: %u bytes\n",
                static_cast<unsigned int>(ESP.getPsramSize()),
                static_cast<unsigned int>(ESP.getFreePsram()));
  Serial.printf("GIF internal heap free: %u bytes\n",
                static_cast<unsigned int>(ESP.getFreeHeap()));
  const size_t spiramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  Serial.printf("GIF MALLOC_CAP_SPIRAM free: %u bytes\n",
                static_cast<unsigned int>(spiramFree));
  void *probe = heap_caps_malloc(1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (probe != nullptr) {
    Serial.println("GIF PSRAM allocation probe: PASS");
    heap_caps_free(probe);
  } else {
    Serial.println("GIF PSRAM allocation probe: FAIL");
  }
  if (!psramFound()) {
    Serial.println("GIF error: PSRAM not detected; verify BOARD_HAS_PSRAM and qio_opi");
  }
}

void GifPlayer::request(const String &url) {
  if (!url.startsWith("https://")) {
    Serial.println("GIF error: URL is not HTTPS");
    showError("GIF URL must use HTTPS");
    return;
  }
  if (url == pendingUrl_ && (loading_ || playing_ || downloadInProgress_)) return;
  pendingUrl_ = url;
  downloadAttempted_ = false;
  downloadAttempts_ = 0;
  lastDownloadAttempt_ = 0;
  loading_ = pendingUrl_ != activeUrl_;
  Serial.println("GIF request queued");
}

bool GifPlayer::acceptUploadedBuffer(uint8_t *buffer, size_t size) {
  if (buffer == nullptr || size < 6 || size > MAX_GIF_SIZE ||
      memcmp(buffer, "GIF", 3) != 0 ||
      (memcmp(buffer + 3, "87a", 3) != 0 && memcmp(buffer + 3, "89a", 3) != 0)) {
    return false;
  }

  pendingUrl_ = "local-upload-" + String(millis());
  downloadAttempted_ = true;
  loading_ = false;
  if (playing_ || decoderOpen_) gif_.close();
  playing_ = false;
  decoderOpen_ = false;
  if (downloadedBuffer_ != nullptr) {
    heap_caps_free(downloadedBuffer_);
    downloadedBuffer_ = nullptr;
    downloadedSize_ = 0;
  }
  if (gifBuffer_ != nullptr) heap_caps_free(gifBuffer_);
  gifBuffer_ = buffer;
  gifSize_ = size;
  ++generation_;
  activeUrl_ = pendingUrl_;
  playbackStarted_ = false;
  downloadReady_ = false;
  downloadFailed_ = false;
  Serial.printf("GIF local upload accepted: %u bytes\n", static_cast<unsigned int>(size));
  return true;
}

void GifPlayer::stop() {
  pendingUrl_ = "";
  downloadAttempted_ = false;
  loading_ = false;
  errorUntil_ = 0;
  if (playing_) {
    gif_.close();
    playing_ = false;
  }
  decoderOpen_ = false;
  if (gifBuffer_ != nullptr) {
    heap_caps_free(gifBuffer_);
    gifBuffer_ = nullptr;
    ++generation_;
  }
  if (downloadedBuffer_ != nullptr) {
    heap_caps_free(downloadedBuffer_);
    downloadedBuffer_ = nullptr;
    downloadedSize_ = 0;
  }
}

void GifPlayer::update(bool wifiConnected, bool playbackAllowed) {
  if (downloadReady_) {
    downloadReady_ = false;
    loading_ = false;
    if (downloadUrl_ != pendingUrl_ || downloadedBuffer_ == nullptr) {
      if (downloadedBuffer_ != nullptr) heap_caps_free(downloadedBuffer_);
      downloadedBuffer_ = nullptr;
      downloadedSize_ = 0;
    } else {
      if (playing_) {
        gif_.close();
        playing_ = false;
      }
      decoderOpen_ = false;
      if (gifBuffer_ != nullptr) heap_caps_free(gifBuffer_);
      gifBuffer_ = downloadedBuffer_;
      gifSize_ = downloadedSize_;
      ++generation_;
      downloadedBuffer_ = nullptr;
      downloadedSize_ = 0;
      activeUrl_ = pendingUrl_;
      downloadApplied_ = true;
    }
  }
  if (downloadFailed_) {
    downloadFailed_ = false;
    loading_ = false;
    if (downloadUrl_ == pendingUrl_ && downloadAttempts_ >= MAX_DOWNLOAD_ATTEMPTS) {
      // Give up on this URL. The animation already in memory stays playable.
      Serial.printf("GIF download abandoned after %u attempts; keeping current animation\n",
                    static_cast<unsigned int>(downloadAttempts_));
      pendingUrl_ = activeUrl_;
    }
    if (downloadTooLarge_) {
      downloadTooLarge_ = false;
      if (playbackAllowed && !playing_) showError("GIF too large");
    } else if (downloadNoPsram_) {
      downloadNoPsram_ = false;
      if (playbackAllowed && !playing_) showError("PSRAM unavailable");
    } else if (playbackAllowed && !playing_) {
      showError("GIF download failed");
    }
  }
  if (!decoderOpen_ && playbackAllowed && gifBuffer_ != nullptr) {
    if (gif_.open(gifBuffer_, gifSize_, drawFrame)) {
      canvasWidth_ = gif_.getCanvasWidth();
      canvasHeight_ = gif_.getCanvasHeight();
      const float scale = min(320.0f / canvasWidth_, 170.0f / canvasHeight_);
      destinationWidth_ = max(static_cast<int16_t>(1), static_cast<int16_t>(canvasWidth_ * scale));
      destinationHeight_ = max(static_cast<int16_t>(1), static_cast<int16_t>(canvasHeight_ * scale));
      destinationX_ = (320 - destinationWidth_) / 2;
      destinationY_ = (170 - destinationHeight_) / 2;
      Serial.printf("GIF playback started (%d x %d), scaled to %d x %d at %d,%d\n",
                    canvasWidth_, canvasHeight_, destinationWidth_, destinationHeight_,
                    destinationX_, destinationY_);
      // Clear once so the screen behind does not show around a letterboxed GIF.
      display_->fillScreen(TFT_BLACK);
      decoderOpen_ = true;
      playing_ = true;
      playbackStarted_ = true;
      nextFrameAt_ = 0;
    } else {
      Serial.printf("GIF error: AnimatedGIF could not open downloaded data, decoder status: %d\n",
                    gif_.getLastError());
      showError("GIF decode failed");
      heap_caps_free(gifBuffer_);
      gifBuffer_ = nullptr;
      ++generation_;
      // Do not download the same undecodable file again.
      if (pendingUrl_ == activeUrl_) pendingUrl_ = "";
      activeUrl_ = "";
      decoderOpen_ = false;
    }
  }
  if (!downloadInProgress_ && pendingUrl_.length() > 0 && wifiConnected &&
      pendingUrl_ != activeUrl_ && downloadAttempts_ < MAX_DOWNLOAD_ATTEMPTS &&
      (!downloadAttempted_ || millis() - lastDownloadAttempt_ >= DOWNLOAD_RETRY_MS)) {
    lastDownloadAttempt_ = millis();
    downloadAttempted_ = true;
    ++downloadAttempts_;
    downloadInProgress_ = true;
    downloadUrl_ = pendingUrl_;
    if (xTaskCreatePinnedToCore(downloadTask, "gif_download", 8192, this, 1,
                                &downloadTaskHandle_, 0) != pdPASS) {
      downloadInProgress_ = false;
      Serial.println("GIF error: download task creation failed");
    }
  }
  if (!playbackAllowed && playing_) {
    gif_.close();
    playing_ = false;
    decoderOpen_ = false;
  }
  if (playing_ && millis() >= nextFrameAt_) {
    int delayMs = 0;
    if (!gif_.playFrame(false, &delayMs)) {
      const int decoderStatus = gif_.getLastError();
      Serial.printf("GIF playback ended, decoder status: %d\n", decoderStatus);
      if (decoderStatus != 0) {
        showError("GIF playback error");
        finishPlayback();
      } else {
        restartPlayback();
      }
    } else {
      nextFrameAt_ = millis() + static_cast<uint32_t>(max(delayMs, 10));
    }
  }
}

bool GifPlayer::consumePlaybackStarted() {
  const bool started = playbackStarted_;
  playbackStarted_ = false;
  return started;
}

bool GifPlayer::consumeDownloadApplied() {
  const bool applied = downloadApplied_;
  downloadApplied_ = false;
  return applied;
}

bool GifPlayer::playing() const { return playing_; }

bool GifPlayer::loading() const { return loading_; }

bool GifPlayer::overlayActive() const {
  return playing_ || (errorUntil_ != 0 && millis() < errorUntil_);
}

void GifPlayer::downloadTask(void *context) {
  GifPlayer *player = static_cast<GifPlayer *>(context);
  const bool success = player->download(player->downloadUrl_);
  player->downloadInProgress_ = false;
  if (success) player->downloadReady_ = true;
  else player->downloadFailed_ = true;
  player->downloadTaskHandle_ = nullptr;
  vTaskDelete(nullptr);
}

bool GifPlayer::download(const String &url) {
  Serial.print("GIF download started: ");
  Serial.println(url);
  WiFiClientSecure client;
  client.setCACert(GIF_GLOBALSIGN_ROOT_R46);
  client.setTimeout(30000);
  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("GIF error: HTTPS begin failed");
    return false;
  }
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(30000);
  const int status = http.GET();
  Serial.printf("GIF HTTP response code: %d\n", status);
  if (status != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  const int contentLength = http.getSize();
  if (contentLength > static_cast<int>(MAX_GIF_SIZE)) {
    Serial.printf("GIF error: file is %d bytes, limit is %u bytes\n", contentLength,
                  static_cast<unsigned int>(MAX_GIF_SIZE));
    downloadTooLarge_ = true;
    http.end();
    return false;
  }
  const size_t capacity = contentLength > 0 ? static_cast<size_t>(contentLength) : MAX_GIF_SIZE;
  const size_t freePsram = ESP.getFreePsram();
  Serial.printf("GIF allocation request: %u bytes, free PSRAM: %u bytes\n",
                static_cast<unsigned int>(capacity), static_cast<unsigned int>(freePsram));
  if (!psramFound() || freePsram < capacity) {
    Serial.println("GIF error: insufficient PSRAM for download buffer");
    downloadNoPsram_ = true;
    http.end();
    return false;
  }
  uint8_t *buffer = static_cast<uint8_t *>(
      heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (buffer == nullptr) {
    Serial.println("GIF error: PSRAM buffer allocation failed");
    downloadNoPsram_ = true;
    http.end();
    return false;
  }
  WiFiClient *stream = http.getStreamPtr();
  constexpr size_t READ_CHUNK_SIZE = 4096;
  constexpr uint32_t INACTIVITY_TIMEOUT_MS = 30000;
  constexpr uint32_t OVERALL_TIMEOUT_MS = 120000;
  size_t total = 0;
  const uint32_t startedAt = millis();
  uint32_t lastDataAt = startedAt;
  int nextProgress = 10;
  const size_t expectedBytes = contentLength > 0 ? static_cast<size_t>(contentLength) : capacity;
  const char *failureReason = nullptr;
  bool reachedEndOfStream = false;
  while (total < expectedBytes) {
    const uint32_t now = millis();
    if (now - startedAt >= OVERALL_TIMEOUT_MS) {
      failureReason = "overall timeout";
      break;
    }
    if (now - lastDataAt >= INACTIVITY_TIMEOUT_MS) {
      failureReason = "inactivity timeout";
      break;
    }
    const size_t available = stream->available();
    if (available == 0) {
      if (!http.connected()) {
        if (contentLength < 0) reachedEndOfStream = true;
        else failureReason = "connection closed early";
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    const size_t toRead = min(min(available, READ_CHUNK_SIZE), capacity - total);
    const int read = stream->read(buffer + total, toRead);
    if (read < 0) {
      failureReason = "read error";
      break;
    }
    if (read == 0) {
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    total += static_cast<size_t>(read);
    lastDataAt = millis();
    if (contentLength > 0) {
      const int progress = static_cast<int>((total * 100U) / expectedBytes);
      while (nextProgress <= progress && nextProgress <= 100) {
        Serial.printf("GIF download: %d%%\n", nextProgress);
        nextProgress += 10;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  http.end();
  Serial.printf("GIF downloaded bytes: %u\n", static_cast<unsigned int>(total));
  if (failureReason != nullptr) {
    Serial.print("GIF download failed: ");
    Serial.println(failureReason);
    heap_caps_free(buffer);
    return false;
  }
  if (contentLength < 0 && !reachedEndOfStream && total < capacity) {
    Serial.println("GIF download failed: connection closed early");
    heap_caps_free(buffer);
    return false;
  }
  if (contentLength < 0 && total >= capacity) {
    Serial.println("GIF error: chunked response reached the 4 MiB limit");
    downloadTooLarge_ = true;
    heap_caps_free(buffer);
    return false;
  }
  if (total < 6 || buffer[0] != 'G' || buffer[1] != 'I' || buffer[2] != 'F') {
    Serial.println("GIF error: downloaded data is not a GIF");
    heap_caps_free(buffer);
    return false;
  }
  if (contentLength > 0 && total != static_cast<size_t>(contentLength)) {
    Serial.printf("GIF download failed: content-length mismatch (expected %d, received %u)\n",
                  contentLength, static_cast<unsigned int>(total));
    heap_caps_free(buffer);
    return false;
  }
  if (downloadedBuffer_ != nullptr) heap_caps_free(downloadedBuffer_);
  downloadedBuffer_ = buffer;
  downloadedSize_ = total;
  Serial.printf("GIF size: %u bytes\n", static_cast<unsigned int>(total));
  return true;
}

void GifPlayer::drawFrame(GIFDRAW *draw) {
  if (instance_ == nullptr || instance_->display_ == nullptr) return;
  const int16_t sourceY = draw->iY + draw->y;
  if (sourceY < 0 || sourceY >= instance_->canvasHeight_) return;
  const int16_t destinationYStart = instance_->destinationY_ +
                                    (sourceY * instance_->destinationHeight_) /
                                        instance_->canvasHeight_;
  const int16_t destinationYEnd = instance_->destinationY_ +
                                  ((sourceY + 1) * instance_->destinationHeight_) /
                                      instance_->canvasHeight_ - 1;
  if (destinationYStart > destinationYEnd) return;
  uint16_t pixels[320];
  bool opaque[320];
  for (int16_t index = 0; index < instance_->destinationWidth_; ++index) opaque[index] = false;
  uint16_t *palette = draw->pPalette;
  for (int16_t sourceIndex = 0; sourceIndex < draw->iWidth; ++sourceIndex) {
    const int16_t sourceX = draw->iX + sourceIndex;
    if (sourceX < 0 || sourceX >= instance_->canvasWidth_ ||
        draw->pPixels[sourceIndex] == draw->ucTransparent && draw->ucHasTransparency) {
      continue;
    }
    const int16_t destinationXStart = instance_->destinationX_ +
                                      (sourceX * instance_->destinationWidth_) /
                                          instance_->canvasWidth_;
    const int16_t destinationXEnd = instance_->destinationX_ +
                                    ((sourceX + 1) * instance_->destinationWidth_) /
                                        instance_->canvasWidth_ - 1;
    for (int16_t destinationX = destinationXStart;
         destinationX <= destinationXEnd && destinationX < 320; ++destinationX) {
      if (destinationX >= 0) {
        pixels[destinationX - instance_->destinationX_] = palette[draw->pPixels[sourceIndex]];
        opaque[destinationX - instance_->destinationX_] = true;
      }
    }
  }
  for (int16_t destinationY = destinationYStart; destinationY <= destinationYEnd; ++destinationY) {
    int16_t runStart = -1;
    for (int16_t index = 0; index <= instance_->destinationWidth_; ++index) {
      if (index < instance_->destinationWidth_ && opaque[index]) {
        if (runStart < 0) runStart = index;
      } else if (runStart >= 0) {
        instance_->display_->pushImage(instance_->destinationX_ + runStart, destinationY,
                                       index - runStart, 1, pixels + runStart);
        runStart = -1;
      }
    }
  }
}

void GifPlayer::showError(const char *message) {
  if (display_ == nullptr) return;
  display_->fillScreen(GIF_BACKGROUND);
  display_->setTextDatum(MC_DATUM);
  display_->setTextColor(GIF_TEXT, GIF_BACKGROUND);
  display_->drawString("GIF ERROR", SCREEN_WIDTH / 2, 70, 4);
  display_->setTextColor(0xBDF7, GIF_BACKGROUND);
  display_->drawString(message, SCREEN_WIDTH / 2, 105, 2);
  errorUntil_ = millis() + 3000;
}

void GifPlayer::showLoading() {
  if (display_ == nullptr) return;
  display_->fillScreen(GIF_BACKGROUND);
  display_->setTextDatum(MC_DATUM);
  display_->setTextColor(GIF_TEXT, GIF_BACKGROUND);
  display_->drawString("LOADING GIF", SCREEN_WIDTH / 2, 78, 4);
  display_->setTextColor(0xBDF7, GIF_BACKGROUND);
  display_->drawString("Downloading...", SCREEN_WIDTH / 2, 108, 2);
}

void GifPlayer::finishPlayback() {
  gif_.close();
  decoderOpen_ = false;
  playing_ = false;
  // The buffer is being dropped; keep any newer request that is still pending.
  if (pendingUrl_ == activeUrl_) pendingUrl_ = "";
  activeUrl_ = "";
  if (gifBuffer_ != nullptr) {
    heap_caps_free(gifBuffer_);
    gifBuffer_ = nullptr;
    ++generation_;
  }
  Serial.println("GIF playback finished");
}

void GifPlayer::restartPlayback() {
  gif_.close();
  if (gifBuffer_ != nullptr && gif_.open(gifBuffer_, gifSize_, drawFrame)) {
    playing_ = true;
    decoderOpen_ = true;
    nextFrameAt_ = 0;
    Serial.println("GIF playback loop restarted");
    return;
  }
  Serial.println("GIF error: unable to restart playback");
  finishPlayback();
}
