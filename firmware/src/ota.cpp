#include "ota.h"
#include "config.h"

#if FEATURE_OTA
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include "display.h"

namespace ota {

static WebServer *srv = nullptr;
static bool on = false, writing = false, rebootPending = false;
static uint32_t startedAt = 0, rebootAt = 0;
static char apName[32], apPass[16];
static String lastError;

static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="cs"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>GoGEN OTA</title>
<style>body{font:16px system-ui;margin:0;padding:24px;background:#0b0f14;color:#e6edf3}
.c{max-width:420px;margin:auto}h1{font-size:22px}input,button{font:inherit;width:100%;margin:8px 0;padding:12px;border-radius:8px}
button{background:#2f81f7;color:#fff;border:0}progress{width:100%;height:18px}.m{color:#8b949e;font-size:14px}</style>
<div class="c"><h1>GoGEN OS – aktualizace</h1><p class="m">Verze %VER%. Nahraj soubor <b>firmware.bin</b> pro ESP32
(z GitHub Actions nebo <code>.pio/build/gogen_pcf/</code>). Koloběžka musí stát.</p>
<input type="file" id="f" accept=".bin"><button onclick="up()">Nahrát</button><progress id="p" max="100" value="0"></progress>
<p id="s" class="m"></p></div><script>
function up(){const f=document.getElementById('f').files[0];if(!f)return;const x=new XMLHttpRequest(),d=new FormData();
d.append('fw',f);x.upload.onprogress=e=>{document.getElementById('p').value=e.loaded/e.total*100};
x.onload=()=>{document.getElementById('s').textContent=x.responseText};x.onerror=()=>{document.getElementById('s').textContent='Spojení přerušeno'};
x.open('POST','/update');x.send(d);document.getElementById('s').textContent='Nahrávám…'}</script></html>)HTML";

static void onRoot() {
  String p = FPSTR(PAGE);
  p.replace("%VER%", String(GOGEN_FW_MAJOR) + "." + String(GOGEN_FW_MINOR));
  srv->send(200, "text/html; charset=utf-8", p);
}

static void onUploadDone() {
  bool ok = !Update.hasError() && lastError.isEmpty();
  srv->send(ok ? 200 : 500, "text/plain; charset=utf-8",
            ok ? "Hotovo – GoGEN se restartuje." : ("Chyba: " + lastError));
  if (ok) { rebootPending = true; rebootAt = millis() + 1500; }
}

static void onUpload() {
  HTTPUpload &u = srv->upload();
  if (u.status == UPLOAD_FILE_START) {
    lastError = "";
    writing = true;
    display::message("Aktualizace", "NAHRAVAM");
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) lastError = Update.errorString();
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (lastError.isEmpty() && Update.write(u.buf, u.currentSize) != u.currentSize)
      lastError = Update.errorString();
  } else if (u.status == UPLOAD_FILE_END) {
    if (lastError.isEmpty() && !Update.end(true)) lastError = Update.errorString();
    writing = false;
    display::message(lastError.isEmpty() ? "Aktualizace OK" : "Aktualizace", lastError.isEmpty() ? "RESTART" : "CHYBA");
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    lastError = "přerušeno";
    writing = false;
  }
}

bool start(const char *bleName, uint32_t passkey) {
  if (on) return true;
  snprintf(apName, sizeof(apName), "%s-OTA", bleName);
  snprintf(apPass, sizeof(apPass), "gg%06u", (unsigned)passkey);   // WPA2 potřebuje ≥ 8 znaků
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(apName, apPass)) { WiFi.mode(WIFI_OFF); return false; }
  srv = new WebServer(80);
  srv->on("/", HTTP_GET, onRoot);
  srv->on("/update", HTTP_POST, onUploadDone, onUpload);
  srv->begin();
  on = true;
  startedAt = millis();
  return true;
}

void stop() {
  if (!on) return;
  if (writing) Update.abort();
  srv->stop();
  delete srv;
  srv = nullptr;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  on = writing = false;
}

void handle(uint32_t now) {
  if (!on) return;
  srv->handleClient();
  if (rebootPending && (int32_t)(now - rebootAt) >= 0) ESP.restart();
  if (!writing && now - startedAt > OTA_TIMEOUT_S * 1000UL) stop();
}

bool active() { return on; }
bool busy() { return writing; }
const char *ssid() { return apName; }
const char *password() { return apPass; }

}  // namespace ota

#else
namespace ota {
bool start(const char *, uint32_t) { return false; }
void stop() {}
void handle(uint32_t) {}
bool active() { return false; }
bool busy() { return false; }
const char *ssid() { return ""; }
const char *password() { return ""; }
}
#endif
