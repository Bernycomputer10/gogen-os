# GoGEN OS

Firmware pro ESP-WROOM-32 / ESP32 DevKit (sekundární 36V okruh GoGEN V2) + Android aplikace přes BLE.

```
gogen-os/
├── firmware/          PlatformIO (Arduino core 2.0.17, NimBLE)
│   ├── include/config.h     piny, časování, BLE PIN
│   ├── include/protocol.h   BLE protokol (sdílený s aplikací)
│   └── src/                 vehicle · inputs · battery · display · ble_service · settings
├── android-app/       Capacitor 8 + React + @capacitor-community/bluetooth-le
└── .github/workflows/ CI: firmware .bin + debug APK
```

## Firmware

### Pinout v3 (ESP-WROOM-32, bez klaksonu)

| GPIO | Funkce | Poznámka |
|---|---|---|
| 34 | BAT_SENSE | ADC1_CH6, jen vstup, dělič baterie |
| 13 | BRAKE_IN | brzdová páka → GND, interní pull-up (obě varianty) |
| 25 | LED pásky | Q7 low-side, PWM 5 kHz |
| 26 | Přední světlo | Q1 low-side |
| 27 | Zadní TAIL | HS1 high-side |
| 33 | Zadní BRAKE | HS2 high-side |
| 32 | Levé blinkry | Q4 low-side |
| 23 | Pravé blinkry | Q5 low-side |
| 21 / 22 | I2C SDA / SCL | OLED 0x3C + PCF8574 0x20 |
| 4 / 18 / 19 / 14 | BTN Světla / L / P / Mód | jen varianta `gogen_gpio` |
| 1 / 3 | UART0 | konzole a nahrávání přes USB-UART převodník – nezapojovat |
| 16 / 17 | UART2 | rezerva: zadní jednotka / telemetrie KT |
| 35 / 36 / 39 | ADC1, jen vstup | rezerva (např. měření proudu) |

**Nepoužívat:** GPIO6–11 (interní flash), GPIO12 (strapping, při startu musí být LOW, jinak modul nenabootuje), GPIO0/2/5/15 (strapping).

**Změny proti pinoutu v2 (ESP32-C3):** všechny piny se přečíslovaly podle tabulky. Baterie musí být na ADC1 (GPIO32–39), protože ADC2 nejde používat současně s BLE. I2C je na výchozích pinech ESP32. Varianta `gogen_gpio` má nově i tlačítko Mód. Konzole jede přes UART0 a USB-UART převodník místo nativního USB.

| Varianta | Vstupy |
|---|---|
| `gogen_pcf` (výchozí) | PCF8574 @0x20: P0 světla, P1 L, P2 P, P3 mód (P4–P7 volné) |
| `gogen_gpio` | světla GPIO4, L GPIO18, P GPIO19, mód GPIO14 |

```bash
cd firmware
pio run -e gogen_pcf -t upload
pio device monitor        # příkazy: status, lights on, left, hazard, led 128, cal 41.8, pin 654321, help…
```

Pokud se nahrávání zasekne na „Connecting…“, podrž na desce tlačítko BOOT.

Nebo nahraj `gogen_pcf_factory.bin` z GitHub Actions na adresu `0x0` (např. přes ESP Web Flasher / `esptool.py write_flash 0x0 …`).

### Ovládání na řídítkách

| Tlačítko | Krátce | Dlouze (0,8 s) |
|---|---|---|
| Světla | přední + zadní zap/vyp | LED pásky zap/vyp |
| Levý / Pravý | blinkr (znovu = vyp, auto-vyp po 30 s) | výstražná světla |
| Mód | další režim LED | výstražná světla |
| Brzda | brzdové světlo (3× probliknutí) | – |

LED režimy: stálé, dýchání, stroboskop, „Brzda+“ (tlumené, při brzdě 100 %). Pod kritickým napětím (31 V) se LED pásky vypnou.

### Bezpečnost
- Po startu jsou všechny výstupy vypnuté ještě před inicializací čehokoli dalšího, běží watchdog 5 s.
- BLE vyžaduje párování s PINem (výchozí **123456** – změň v aplikaci → Nastavení → Zabezpečení).

## Android aplikace

```bash
cd android-app
npm install
npm run dev              # náhled v prohlížeči, tlačítko „Demo režim“
npm run cap:sync         # build + kopie do android/
cd android && ./gradlew assembleDebug
```

Bez lokálního Android SDK: pushni repo na GitHub → Actions → **Android APK** → artefakt `GoGEN-apk`.

Firmware a aplikace musí být stejné verze (protokol v2). Při prvním připojení Android zobrazí systémový dialog pro zadání PINu. Aplikace si pamatuje poslední zařízení a při ztrátě spojení se sama znovu připojuje.

## BLE protokol (v2)

Služba `a8f30001-4c1e-4b8e-9a52-6f6f67656e32`
- `…0002` STATE (read/notify, 20 B): výstupy (bez klaksonu), LED, napětí ×100, %, stavové bity, uptime, režim blinkru, tlačítka
- `…0003` CMD (write): `[cmd, arg…]` – viz `protocol.h`
- `…0004` CONFIG (read, 24 B)

## ⚠️ Poznámky k hardwaru (ověř před prvním zapojením)

1. **Dělič baterie:** firmware počítá s **120k / 7,5k** (42 V → 2,47 V, lineární oblast ADC ESP32). Pokud necháš 8,2 kΩ, změň v `config.h` `BAT_R2_OHM` na `8200.0f`. Mezi GPIO34 a GND dej 100 nF (ADC ESP32 je šumivé). Pak kalibruj multimetrem (`cal 41.8` nebo v aplikaci).
2. **Výstupy po resetu plavou:** GPIO23/25/26/27/32/33 nemají při startu definovanou úroveň. Na gate každého MOSFETu / vstup high-side modulu patří 10 kΩ pulldown.
3. **High-side moduly TAIL/BRAKE:** firmware předpokládá aktivní log. 1 (`OUT_ACTIVE_HIGH`). Ověř na konkrétním modulu.
4. Na I2C sběrnici musí být pull-upy (většina OLED modulů je má osazené).
5. **Napájení:** DevKit napájej 5 V na pin VIN/5V (z měniče), ne na 3V3. Modul při BLE vysílání bere špičky až ~250 mA.
