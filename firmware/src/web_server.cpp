#include "web_server.h"

#include <esp_heap_caps.h>

#include "gif_messages.h"
#include "gif_player.h"
#include "time_service.h"
#include "wifi_manager.h"

namespace {
constexpr size_t MAX_GIF_SIZE = 4 * 1024 * 1024;

const char WEB_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>T-Display Control</title><style>
:root{color-scheme:dark;--bg:#08090d;--panel:#151923;--line:#2a3040;--text:#f4f6fb;--muted:#9098aa;--accent:#a47fff;--ok:#64e6ad;--bad:#ff7d94}
*{box-sizing:border-box}body{margin:0;min-height:100vh;background:radial-gradient(circle at 80% 0,#282044 0,transparent 38%),var(--bg);color:var(--text);font:15px system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;display:grid;place-items:center;padding:24px}
main{width:min(720px,100%)}header{display:flex;align-items:center;justify-content:space-between;margin-bottom:26px}h1{font-size:clamp(27px,6vw,42px);letter-spacing:-.04em;margin:0}p{color:var(--muted);line-height:1.5}.status{display:flex;align-items:center;gap:8px;color:var(--muted);font-size:13px}.dot{width:9px;height:9px;border-radius:50%;background:var(--ok);box-shadow:0 0 16px var(--ok)}.card{background:color-mix(in srgb,var(--panel) 92%,transparent);border:1px solid var(--line);border-radius:24px;padding:24px;box-shadow:0 20px 70px #0007}.drop{border:1px dashed #566078;border-radius:18px;min-height:230px;display:grid;place-items:center;text-align:center;padding:22px;transition:.2s;background:#10141e}.drop.over,.drop.has-file{border-color:var(--accent);background:#1b172b}.drop input{display:none}.icon{font-size:37px;color:var(--accent)}button{border:0;border-radius:12px;padding:13px 17px;background:var(--accent);color:#100c18;font-weight:750;cursor:pointer}button.secondary{background:#2a3040;color:var(--text)}button:disabled{opacity:.45;cursor:not-allowed}.actions{display:flex;gap:10px;flex-wrap:wrap;margin-top:18px}.preview{display:none;max-width:100%;max-height:170px;border-radius:10px;margin:0 auto 12px}.filename{font-weight:650;overflow:hidden;text-overflow:ellipsis}.meter{height:8px;background:#292f3e;border-radius:99px;margin-top:20px;overflow:hidden;display:none}.meter i{display:block;height:100%;width:0;background:var(--accent);transition:width .15s}.message{min-height:24px;margin-top:16px;font-weight:600}.message.ok{color:var(--ok)}.message.bad{color:var(--bad)}.hint{font-size:12px;margin:18px 2px 0}@media(max-width:500px){body{padding:15px}.card{padding:16px;border-radius:19px}header{align-items:flex-start;gap:14px;flex-direction:column}}
</style></head><body><main><header><div><h1>T-Display Control</h1><p>Send a new animation to your local display.</p></div><div class="status"><i class="dot"></i><span id="status">Checking device</span></div></header>
<section class="card"><div class="drop" id="drop"><div><img class="preview" id="preview"><div class="icon">GIF</div><div class="filename" id="name">Drop an animated GIF here</div><p>Up to 4 MiB, stored securely in PSRAM.</p><button class="secondary" id="choose">Choose GIF</button><input id="file" type="file" accept="image/gif,.gif"></div></div><div class="meter" id="meter"><i id="bar"></i></div><div class="actions"><button id="send" disabled>Send Animation</button></div><div class="message" id="message"></div><p class="hint">This interface is available only from the same local network as the T-Display S3.</p></section></main><script>
const file=document.querySelector('#file'),drop=document.querySelector('#drop'),choose=document.querySelector('#choose'),send=document.querySelector('#send'),preview=document.querySelector('#preview'),name=document.querySelector('#name'),message=document.querySelector('#message'),meter=document.querySelector('#meter'),bar=document.querySelector('#bar'),status=document.querySelector('#status');let selected;
function pick(f){if(!f)return;if(f.size>4*1024*1024){show('GIF is larger than 4 MiB.','bad');return}if(f.type&&f.type!=='image/gif'&&!f.name.toLowerCase().endsWith('.gif')){show('Please choose a GIF file.','bad');return}selected=f;name.textContent=f.name;drop.classList.add('has-file');preview.src=URL.createObjectURL(f);preview.style.display='block';send.disabled=false;show('Ready to upload.','')}
function show(t,c){message.textContent=t;message.className='message '+c}choose.onclick=()=>file.click();file.onchange=()=>pick(file.files[0]);['dragenter','dragover'].forEach(e=>drop.addEventListener(e,x=>{x.preventDefault();drop.classList.add('over')}));['dragleave','drop'].forEach(e=>drop.addEventListener(e,x=>{x.preventDefault();drop.classList.remove('over')}));drop.ondrop=e=>pick(e.dataTransfer.files[0]);
send.onclick=()=>{if(!selected)return;send.disabled=true;meter.style.display='block';bar.style.width='0';show('Uploading...','');const xhr=new XMLHttpRequest();xhr.open('POST','/api/animation/upload');xhr.upload.onprogress=e=>{if(e.lengthComputable)bar.style.width=(e.loaded/e.total*100)+'%'};xhr.onload=()=>{try{const r=JSON.parse(xhr.responseText);if(xhr.status===200&&r.ok){show('Animation sent to the display.','ok')}else show(r.error||'Upload failed.','bad')}catch(_){show('Upload failed.','bad')}send.disabled=false};xhr.onerror=()=>{show('Device connection lost.','bad');send.disabled=false};const data=new FormData();data.append('animation',selected,selected.name);xhr.send(data)};
async function check(){try{const r=await fetch('/api/status');const d=await r.json();status.textContent=d.connected?'Device online':'Device offline'}catch(_){status.textContent='Device unavailable'}}check();setInterval(check,10000);
</script></body></html>
)HTML";

// Shown instead of the control page while the display is in Wi-Fi setup mode.
const char SETUP_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Wi-Fi setup</title><style>
:root{color-scheme:dark;--bg:#0e0c18;--panel:#1d1830;--field:#141022;--line:#3a3160;--text:#f4f6fb;--muted:#a99fcc;--accent:#a47fff;--ok:#64e6ad;--bad:#ff7d94}
*{box-sizing:border-box}body{margin:0;min-height:100vh;background:var(--bg);color:var(--text);font:16px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif;display:grid;place-items:center;padding:20px}
main{width:min(440px,100%)}h1{font-size:28px;letter-spacing:-.03em;margin:0 0 6px}p{color:var(--muted);margin:0 0 18px}
.card{background:var(--panel);border-radius:22px;padding:22px}label{display:grid;gap:6px;font-size:13px;color:var(--muted);margin-bottom:14px}
input{width:100%;background:var(--field);border:1px solid var(--line);border-radius:12px;padding:12px;color:var(--text);font:inherit}
input:focus-visible,button:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
button{width:100%;border:0;border-radius:12px;padding:13px;background:var(--accent);color:#100c18;font:inherit;font-weight:700;cursor:pointer}
button:disabled{opacity:.5}.nets{display:flex;flex-wrap:wrap;gap:8px;margin:-4px 0 14px}.nets button{width:auto;padding:6px 12px;background:#2c244a;color:var(--text);font-weight:500;font-size:14px}
.message{min-height:24px;margin-top:14px;font-weight:600}.ok{color:var(--ok)}.bad{color:var(--bad)}
</style></head><body><main><h1>Wi-Fi setup</h1><p>Choose your home Wi-Fi network. The display restarts and connects after you save.</p>
<form class="card" id="form"><label>Network name<input id="ssid" maxlength="32" autocomplete="off" autocapitalize="none" spellcheck="false" required></label>
<div class="nets" id="nets"></div>
<label>Password<input id="password" type="password" maxlength="63" autocomplete="off"></label>
<button id="save">Save and restart</button><div class="message" id="message"></div></form></main><script>
const $=i=>document.getElementById(i);function show(t,c){$('message').textContent=t;$('message').className='message '+(c||'')}
fetch('/api/wifi/scan').then(r=>r.json()).then(list=>{for(const name of list){const b=document.createElement('button');b.type='button';b.textContent=name;b.onclick=()=>{$('ssid').value=name;$('password').focus()};$('nets').appendChild(b)}}).catch(()=>{});
$('form').onsubmit=async e=>{e.preventDefault();const p=$('password').value;if(p.length>0&&p.length<8){show('Wi-Fi passwords have at least 8 characters.','bad');return}
$('save').disabled=true;show('Saving...');try{const r=await fetch('/api/wifi',{method:'POST',body:new URLSearchParams({ssid:$('ssid').value,password:p})});const d=await r.json();
if(d.ok)show('Saved. The display is restarting and will join that network.','ok');else{show(d.error||'Could not save.','bad');$('save').disabled=false}}catch(_){show('Lost contact with the display. Try again.','bad');$('save').disabled=false}};
</script></body></html>
)HTML";

// Minimal JSON string escaping for network names.
String jsonEscape(const String &value) {
  String result;
  for (size_t index = 0; index < value.length(); ++index) {
    const char character = value[index];
    if (character == '"' || character == '\\') {
      result += '\\';
      result += character;
    } else if (static_cast<uint8_t>(character) < 0x20) {
      result += ' ';
    } else {
      result += character;
    }
  }
  return result;
}
}

void WebServerService::begin(GifPlayer *player, GifMessageStore *messages,
                             TimeService *timeService, WifiManager *wifi) {
  player_ = player;
  messages_ = messages;
  timeService_ = timeService;
  wifi_ = wifi;
  if (xTaskCreatePinnedToCore(serverTask, "web_server", 8192, this, 1,
                              &taskHandle_, 0) != pdPASS) {
    Serial.println("Web server error: task creation failed");
    return;
  }
  Serial.println("Web server task started on port 80");
}

void WebServerService::update() {
  takeCompletedUpload();
}

void WebServerService::serverTask(void *context) {
  static_cast<WebServerService *>(context)->runServer();
}

void WebServerService::runServer() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/api/animation/upload", HTTP_POST,
             [this]() { handleUploadResult(); }, [this]() { handleUpload(); });
  server_.on("/api/wifi", HTTP_POST, [this]() { handleWifiSave(); });
  server_.on("/api/wifi/scan", HTTP_GET, [this]() { handleWifiScan(); });
  server_.onNotFound([this]() {
    if (wifi_ != nullptr && wifi_->setupMode()) {
      // Phones probe random addresses after joining; sending them here opens the setup page.
      server_.sendHeader("Location", "http://192.168.4.1/");
      server_.send(302, "text/plain", "");
      return;
    }
    server_.send(404, "application/json", "{\"error\":\"Not found\"}");
  });
  server_.begin();
  Serial.println("Web server started on port 80");
  for (;;) {
    server_.handleClient();
    if (wifi_ != nullptr) wifi_->processDns();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void WebServerService::handleRoot() {
  if (wifi_ != nullptr && wifi_->setupMode()) {
    server_.send_P(200, "text/html; charset=utf-8", SETUP_PAGE);
    return;
  }
  server_.send_P(200, "text/html; charset=utf-8", WEB_PAGE);
}

void WebServerService::handleWifiSave() {
  // Only reachable through the setup hotspot: once the display is on a network
  // this endpoint refuses, so nobody on that network can move it elsewhere.
  if (wifi_ == nullptr || !wifi_->setupMode()) {
    server_.send(403, "application/json", "{\"ok\":false,\"error\":\"Only available in setup mode\"}");
    return;
  }
  if (wifi_->saveCredentials(server_.arg("ssid"), server_.arg("password"))) {
    server_.send(200, "application/json", "{\"ok\":true}");
  } else {
    server_.send(400, "application/json",
                 "{\"ok\":false,\"error\":\"Check the network name and password\"}");
  }
}

void WebServerService::handleWifiScan() {
  String body = "[";
  if (wifi_ != nullptr && wifi_->setupMode()) {
    const int found = WiFi.scanNetworks();
    uint8_t listed = 0;
    for (int index = 0; index < found && listed < 12; ++index) {
      const String name = WiFi.SSID(index);
      if (name.length() == 0) continue;
      bool duplicate = false;
      for (int earlier = 0; earlier < index; ++earlier) {
        if (WiFi.SSID(earlier) == name) duplicate = true;
      }
      if (duplicate) continue;
      if (listed++ > 0) body += ',';
      body += '"' + jsonEscape(name) + '"';
    }
    WiFi.scanDelete();
  }
  body += "]";
  server_.send(200, "application/json", body);
}

void WebServerService::handleStatus() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  server_.send(200, "application/json", String("{\"connected\":") +
                                          (connected ? "true" : "false") + "}");
}

void WebServerService::handleUploadResult() {
  if (uploadResultOk_) {
    server_.send(200, "application/json", "{\"ok\":true}");
  } else {
    server_.send(400, "application/json",
                 String("{\"ok\":false,\"error\":\"") + uploadResultError_ + "\"}");
  }
  // A later request without a file part must not inherit this result.
  uploadResultOk_ = false;
  uploadResultError_ = "No GIF received";
}

void WebServerService::handleUpload() {
  HTTPUpload &upload = server_.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (uploadBuffer_ != nullptr) heap_caps_free(uploadBuffer_);
    uploadBuffer_ = nullptr;
    uploadSize_ = 0;
    uploadFailed_ = false;
    uploadError_ = "";
    lastProgressLog_ = 0;
    uploadResultOk_ = false;
    uploadResultError_ = "No GIF received";
    // The request's Content-Length covers the file plus a little multipart
    // framing, so it is a safe upper bound for the file itself. Sizing the
    // buffer from it keeps a small GIF from pinning a full 4 MiB of PSRAM.
    const int contentLength = server_.clientContentLength();
    uploadCapacity_ = contentLength > 0
                          ? min(static_cast<size_t>(contentLength), MAX_GIF_SIZE)
                          : MAX_GIF_SIZE;
    if (!psramFound() ||
        heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) < uploadCapacity_) {
      finishUpload(false, "Not enough PSRAM available");
      Serial.println("Web upload failed: insufficient PSRAM");
      return;
    }
    uploadBuffer_ = static_cast<uint8_t *>(
        heap_caps_malloc(uploadCapacity_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (uploadBuffer_ == nullptr) {
      finishUpload(false, "PSRAM allocation failed");
      Serial.println("Web upload failed: PSRAM allocation failed");
      return;
    }
    Serial.printf("Web upload started: %s\n", upload.filename.c_str());
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFailed_) return;
    if (uploadBuffer_ == nullptr || uploadSize_ + upload.currentSize > uploadCapacity_) {
      finishUpload(false, "GIF exceeds 4 MiB limit");
      Serial.println("Web upload failed: file too large");
      return;
    }
    memcpy(uploadBuffer_ + uploadSize_, upload.buf, upload.currentSize);
    uploadSize_ += upload.currentSize;
    if (uploadSize_ >= 6 &&
        (memcmp(uploadBuffer_, "GIF87a", 6) != 0 && memcmp(uploadBuffer_, "GIF89a", 6) != 0)) {
      finishUpload(false, "File is not a GIF");
      Serial.println("Web upload failed: invalid GIF signature");
      return;
    }
    if (uploadSize_ - lastProgressLog_ >= 256 * 1024) {
      lastProgressLog_ = uploadSize_;
      Serial.printf("Web upload progress: %u bytes\n", static_cast<unsigned int>(uploadSize_));
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!uploadFailed_ && uploadBuffer_ != nullptr && upload.totalSize == uploadSize_ &&
        uploadSize_ >= 6 &&
        (memcmp(uploadBuffer_, "GIF87a", 6) == 0 || memcmp(uploadBuffer_, "GIF89a", 6) == 0)) {
      // Hand back whatever the multipart framing over-reserved.
      if (uploadSize_ < uploadCapacity_) {
        uint8_t *shrunk = static_cast<uint8_t *>(heap_caps_realloc(
            uploadBuffer_, uploadSize_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (shrunk != nullptr) uploadBuffer_ = shrunk;
      }
      uint8_t *oldCompleted = nullptr;
      portENTER_CRITICAL(&bufferMux_);
      oldCompleted = const_cast<uint8_t *>(completedBuffer_);
      completedBuffer_ = uploadBuffer_;
      completedSize_ = uploadSize_;
      completedReady_ = true;
      portEXIT_CRITICAL(&bufferMux_);
      if (oldCompleted != nullptr) heap_caps_free(oldCompleted);
      uploadBuffer_ = nullptr;
      uploadResultOk_ = true;
      uploadResultError_ = "";
      Serial.printf("Web upload completed: %u bytes\n", static_cast<unsigned int>(uploadSize_));
    } else if (!uploadFailed_) {
      finishUpload(false, "Incomplete or invalid GIF upload");
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    finishUpload(false, "Upload aborted");
    Serial.println("Web upload failed: client aborted");
  }
}

void WebServerService::finishUpload(bool success, const char *error) {
  if (uploadBuffer_ != nullptr) {
    heap_caps_free(uploadBuffer_);
    uploadBuffer_ = nullptr;
  }
  uploadSize_ = 0;
  uploadFailed_ = !success;
  uploadResultOk_ = success;
  uploadError_ = error == nullptr ? "" : error;
  uploadResultError_ = uploadError_;
}

void WebServerService::takeCompletedUpload() {
  uint8_t *buffer = nullptr;
  size_t size = 0;
  portENTER_CRITICAL(&bufferMux_);
  if (completedReady_) {
    buffer = const_cast<uint8_t *>(completedBuffer_);
    size = completedSize_;
    completedBuffer_ = nullptr;
    completedSize_ = 0;
    completedReady_ = false;
  }
  portEXIT_CRITICAL(&bufferMux_);
  if (buffer == nullptr) return;
  if (player_ != nullptr && player_->acceptUploadedBuffer(buffer, size)) {
    if (messages_ != nullptr && timeService_ != nullptr) {
      messages_->add("local-upload", timeService_->messageTime());
    }
    Serial.println("Web upload applied as latest animation");
  } else {
    heap_caps_free(buffer);
    Serial.println("Web upload failed: player rejected GIF");
  }
}
