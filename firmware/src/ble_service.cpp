#include "ble_service.h"
#include <NimBLEDevice.h>
#include "config.h"
#include "protocol.h"
#include "settings.h"

namespace ble {

struct CmdMsg { uint8_t len; uint8_t data[16]; };

static QueueHandle_t queue = nullptr;
static CommandHandler handler = nullptr;
static NimBLEServer *server = nullptr;
static NimBLECharacteristic *chState = nullptr, *chCmd = nullptr, *chCfg = nullptr;
static NimBLECharacteristic *chTelem = nullptr, *chCharge = nullptr;
static volatile bool isSecured = false;
static char devName[20];

class ServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *s, ble_gap_conn_desc *desc) override {
    // rozumné parametry spojení: 15–30 ms, timeout 4 s
    s->updateConnParams(desc->conn_handle, 12, 24, 0, 400);
#if !BLE_SECURE
    isSecured = true;
#endif
    NimBLEDevice::startAdvertising();  // umožní druhé zařízení
  }
  void onDisconnect(NimBLEServer *s) override {
    if (s->getConnectedCount() == 0) isSecured = !BLE_SECURE;
    NimBLEDevice::startAdvertising();
  }
  uint32_t onPassKeyRequest() override { return settings::passkey(); }
  bool onConfirmPIN(uint32_t) override { return true; }
  void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
    isSecured = desc->sec_state.encrypted && desc->sec_state.authenticated;
    if (!isSecured) NimBLEDevice::getServer()->disconnect(desc->conn_handle);
  }
};

class CmdCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c) override {
    NimBLEAttValue v = c->getValue();
    if (v.length() == 0 || !queue) return;
    CmdMsg m;
    m.len = min((size_t)sizeof(m.data), (size_t)v.length());
    memcpy(m.data, v.data(), m.len);
    xQueueSend(queue, &m, 0);
  }
};

void begin(CommandHandler h) {
  handler = h;
  queue = xQueueCreate(16, sizeof(CmdMsg));

  uint64_t mac = ESP.getEfuseMac();
  snprintf(devName, sizeof(devName), "%s%02X%02X", BLE_NAME_PREFIX,
           (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));

  NimBLEDevice::init(devName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setMTU(185);

  uint32_t readP = NIMBLE_PROPERTY::READ, writeP = NIMBLE_PROPERTY::WRITE;
#if BLE_SECURE
  NimBLEDevice::setSecurityAuth(true, true, true);          // bonding, MITM, SC
  NimBLEDevice::setSecurityPasskey(settings::passkey());
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);   // PIN zadává telefon
  readP |= NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN;
  writeP |= NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN;
#else
  isSecured = true;
#endif

  server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCB());

  NimBLEService *svc = server->createService(GOGEN_SVC_UUID);
  chState = svc->createCharacteristic(GOGEN_STATE_UUID, readP | NIMBLE_PROPERTY::NOTIFY);
  chCmd = svc->createCharacteristic(GOGEN_CMD_UUID, writeP | NIMBLE_PROPERTY::WRITE_NR);
  chCfg = svc->createCharacteristic(GOGEN_CFG_UUID, readP);
  chTelem = svc->createCharacteristic(GOGEN_TELEM_UUID, readP | NIMBLE_PROPERTY::NOTIFY);
  chCharge = svc->createCharacteristic(GOGEN_CHARGE_UUID, readP | NIMBLE_PROPERTY::NOTIFY);
  chCmd->setCallbacks(new CmdCB());
  svc->start();

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(GOGEN_SVC_UUID);
  adv->setScanResponse(true);
  adv->setName(devName);
  adv->start();
}

void process() {
  if (!queue) return;
  CmdMsg m;
  while (xQueueReceive(queue, &m, 0) == pdTRUE)
    if (handler) handler(m.data, m.len);
}

void publishState(const uint8_t *buf, size_t len) {
  if (!chState) return;
  chState->setValue(buf, len);
  if (server->getConnectedCount() > 0) chState->notify();
}

void publishConfig(const uint8_t *buf, size_t len) {
  if (chCfg) chCfg->setValue(buf, len);
}

void publishTelemetry(const uint8_t *buf, size_t len) {
  if (!chTelem) return;
  chTelem->setValue(buf, len);
  if (server->getConnectedCount() > 0) chTelem->notify();
}

void publishCharge(const uint8_t *buf, size_t len) {
  if (!chCharge) return;
  chCharge->setValue(buf, len);
  if (server->getConnectedCount() > 0) chCharge->notify();
}

bool connected() { return server && server->getConnectedCount() > 0; }
bool secured() { return isSecured; }
void deleteBonds() { NimBLEDevice::deleteAllBonds(); }
const char *name() { return devName; }

}  // namespace ble
