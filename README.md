# GoGEN OS

Firmware pro ESP32-C3 Super Mini (sekundární 36V okruh GoGEN V2) + Android aplikace přes BLE.

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

### Pinout v2 (ESP32-C3 Super Mini, bez klaksonu)

| GPIO | Funkce | Poznámka |
|---|---|---|
| 0 | BAT_SENSE | ADC1_CH0, dělič baterie |
| 1 | BRAKE_IN | brzdová páka → GND, interní pull-up (obě varianty) |
| 2 | BTN_LIGHT | jen varianta `gogen_gpio`, strapping – nedrž při zapínání |
| 3 | LED pásky | Q7 low-side, PWM 5 kHz |
| 4 | Přední světlo | Q1 low-side |
| 5 | Zadní TAIL | HS1 high-side |
| 6 | Zadní BRAKE | HS2 high-side |
| 7 | Levé blinkry | Q4 low-side |
| 8 / 9 | I2C SDA / SCL | OLED 0x3C + PCF8574 0x20 |
| 10 | Pravé blinkry | Q5 low-side |
| 18 / 19 | USB | volné – konzole i nahrávání fungují v obou variantách |
| 20 | BTN_LEFT | jen `gogen_gpio` |
| 21 | BTN_RIGHT | jen `gogen_gpio`, zapoj přes 1 kΩ (UART TX při bootu) |

**Změny proti dokumentaci rev 0.1:** klakson (Q6, F8, GPIO3) je zrušený. LED pásky se přesunuly z GPIO1 na GPIO3. Brzda je na GPIO1 místo GPIO20/GPIO2. Tlačítka varianty GPIO jsou na 2/20/21 místo 20/21/18/19. Přední světlo, zadní světla a blinkry zůstaly na stejných pinech.

| Varianta | Vstupy |
|---|---|
| `gogen_pcf` (výchozí) | PCF8574 @0x20: P0 světla, P1 L, P2 P, P3 mód (P4–P7 volné) |
| `gogen_gpio` | světla GPIO2, L GPIO20, P GPIO21 (bez tlačítka mód) |

```bash
cd firmware
pio run -e gogen_pcf -t upload
pio device monitor        # příkazy: status, lights on, left, hazard, led 128, cal 41.8, pin 654321, help…
```

Nebo nahraj `gogen_pcf_factory.bin` z GitHub Actions na adresu `0x0` (např. přes ESP Web Flasher / `esptool.py write_flash 0x0 …`).

### Ovládání na řídítkách

| Tlačítko | Krátce | Dlouze (0,8 s) |
|---|---|---|
| Světla | přední + zadní zap/vyp | LED pásky zap/vyp |
| Levý / Pravý | blinkr (znovu = vyp, auto-vyp po 30 s) | výstražná světla |
| Mód (jen PCF) | další režim LED | výstražná světla |
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

1. **Dělič baterie:** 120k/8,2k dává při 42 V ≈ 2,69 V, ale ADC ESP32-C3 je přesný jen do ~2,5 V. Doporučuji **R2 = 7,5 kΩ** (42 V → 2,47 V) a v `config.h` změnit `BAT_R2_OHM` na `7500.0f`. Pak kalibruj multimetrem (`cal 41.8` nebo v aplikaci).
2. **GPIO varianta:** tlačítko Světla je na GPIO2 (strapping) – nedrž ho při zapínání. Tlačítko P na GPIO21 zapoj přes 1 kΩ.
3. **High-side moduly TAIL/BRAKE:** firmware předpokládá aktivní log. 1 (`OUT_ACTIVE_HIGH`). Ověř na konkrétním modulu.
4. Na I2C sběrnici musí být pull-upy (většina OLED modulů je má osazené).
