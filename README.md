# GoGEN OS 3

Firmware pro **ESP-WROOM-32 / ESP32 DevKit**, který řídí celý sekundární 36V okruh GoGEN V2, a k němu Android aplikace přes BLE.
Jedna deska obsluhuje světla, blinkry, LED pásky, brzdové světlo, rychloměr, dobíjení z předního kola i přední pohon.

```
gogen-os/
├── firmware/                PlatformIO (Arduino core 2.0.17, NimBLE)
│   ├── include/             config.h (piny), protocol.h (BLE), *_logic.h (logika bez Arduina)
│   ├── src/                 moduly – viz tabulka níže
│   └── tools/               host_tests (testy logiky na PC), arduino-build.sh (build bez PlatformIO)
├── android-app/             Capacitor 8 + React + @capacitor-community/bluetooth-le
└── .github/workflows/       CI: testy, firmware (.bin), debug APK
```

| Modul | Co dělá |
|---|---|
| `vehicle` | světla vyp/zap/**auto**, blinkry, výstražná světla, brzdové světlo (páka i akcelerometr), LED pásky |
| `inputs` | ovladač PCF8574 nebo GPIO, krátký/dlouhý stisk, kombinace L+P = vypnout |
| `battery` | napětí a % Lime-S 10S, kalibrace, detekce vadného děliče |
| `sensors` | BH1750 (světlo), MPU6050 (zpomalení), MCP4725 (plyn předního motoru) – vše volitelné, hledá se samo |
| `speed` | rychloměr z hallů zadního a předního motoru, výlet, celkové km (ukládá se) |
| `charge` | dobíjení z předního kola: zapne při 25 %, vypne při 95 %, ochrany |
| `drive` | přední pohon: podíl plynu + PI regulace otáček na zadní kolo, ochrany |
| `power` | napájecí latch, vypnutí po nečinnosti, ochrana článků při vybití |
| `ota` | aktualizace firmwaru přes Wi-Fi (bez kabelu) |
| `display` | OLED: rychlost, výlet, baterie, blinkry, stav |
| `ble_service` | BLE s PINem: STATE, TELEMETRY, CMD, CONFIG |

## Pinout v4 (jedna ESP-WROOM-32)

| GPIO | Signál | Připojeno k |
|---|---|---|
| 34 | BAT_SENSE | dělič **120k / 7,5k** + 100 nF |
| 35 | GEN_SENSE | dělič **270k / 10k** + 100 nF, BAT54 na 3V3 (napětí generátoru) |
| 36 | THROTTLE_IN | plyn primáru přes **lineární optočlen** (např. IL300) |
| 39 | rezerva | ADC1, jen vstup (např. měření proudu) |
| 13 | BRAKE_IN | brzdová páka → GND (BAT54 při sdílení s KT), probouzí ze spánku |
| 25 | LED | Q7 low-side PWM – LED pásky |
| 26 | FRONT | Q1 low-side – přední světlo |
| 27 | TAIL | HS1 high-side – koncové světlo |
| 33 | BRAKE | HS2 high-side – brzdové světlo |
| 32 / 23 | LEFT / RIGHT | Q4 / Q5 low-side – blinkry (přední + zadní paralelně) |
| 21 / 22 | SDA / SCL | PCF8574 0x20, OLED 0x3C, BH1750 0x23, MPU6050 0x68, MCP4725 0x60 |
| 16 | PWR_HOLD | napájecí latch (HIGH = drží, LOW = vypne) |
| 17 | CHG_EN | relé/MOSFET nabíjecí větve |
| 4 | REAR_SPEED | hall zadního motoru přes optočlen, pull-up 10k na 3V3 |
| 14 | FRONT_SPEED | hall předního motoru – **dělič 5 V → 3,3 V** (10k/20k) |
| 18 | PHASE_EN | relé fází předního motoru |
| 19 | FRONT_BRAKE | HIGH = brzdový vstup předního kontroléru aktivní (přes NPN na GND) |
| 1 / 3 | UART0 | konzole a nahrávání – nezapojovat |

- **Nepoužívat:** GPIO6–11 (flash), GPIO12 (při startu musí být LOW, jinak modul nenabootuje), GPIO0/2/5/15.
- **GPIO36/39** dávají při běhu ADC a Wi-Fi falešné krátké pulzy (errata ESP32). Jsou proto jen analogové, rychloměr je na GPIO4/14.
- **Všechny výstupy po resetu plavou.** Na každý gate nebo vstup modulu patří 10 kΩ pulldown. Díky tomu se po pádu nebo resetu ESP rozepne i relé fází.
- Varianta `gogen_gpio` má tlačítka na GPIO4/18/19/14, takže nemá rychloměr ani přední pohon.

## Firmware

```bash
cd firmware
pio run -e gogen_pcf -t upload
pio device monitor        # příkazy: status, settings, set <klíč> <hodnota>, help…
```

Bez PlatformIO jde firmware nahrát i jako `gogen_pcf_factory.bin` z GitHub Actions (artefakt **GoGEN-firmware**) na adresu `0x0` přes ESP Web Flasher nebo `esptool.py write_flash 0x0 …`. Další verze se pak dají nahrávat přes Wi-Fi (viz níže) souborem `gogen_pcf_firmware.bin`.

Testy logiky na PC: `cd firmware/tools/host_tests && g++ -std=c++17 -I../../include test_logic.cpp -o t && ./t`.

### Ovládání na řídítkách

| Tlačítko | Krátce | Dlouze (0,8 s) |
|---|---|---|
| Světla | vyp → zap → auto → vyp (auto jen s BH1750) | LED pásky zap/vyp |
| Levý / Pravý | blinkr (znovu = vyp, auto-vyp po 30 s) | výstražná světla |
| Mód | další režim LED | **přední pohon zap/vyp** (když je povolený), jinak výstražná světla |
| Levý + Pravý 2 s | **vypnout GoGEN** | |
| Brzda | brzdové světlo (3× probliknutí), přední pohon na klidový plyn | |

### Automatická světla a brzdové světlo
- **Auto světla:** pod 40 lx svítí do 2 s, nad 120 lx zhasnou až po 10 s. Proto nezhasínají pod stromy. Když senzor zmizí, světla svítí.
- **Brzdové světlo z MPU6050:** zpomalení nad 0,15 g po dobu 120 ms. Sklon kopce se průběžně vynuluje. Nad 0,45 g brzdové světlo bliká 4×/s. MPU musí být **pevně na rámu** (ne na řídítkách) a v nastavení je potřeba vybrat osu ve směru jízdy.

### Dobíjení z předního kola
Řetězec: generátor → usměrňovač → **měnič CC/CV nastavený na 42,0 V** → relé CHG_EN → dioda → B+.
ESP nabíjení jen povoluje. Ochranu proti přebití dál drží měnič a BMS.

| Stav | Kdy |
|---|---|
| Čeká | baterie nad 25 % |
| Nabíjí | od ≤ 25 % do 95 % nebo 41,5 V |
| Chce nabíjet | kolo stojí, generátor mimo 20–58 V, pohání přední motor, pauza relé 10 s |
| Porucha (ruční reset) | baterie ≥ 42,2 V, 10 min bez nárůstu napětí, víc než 6 h nabíjení |

Konzole: `chg`, `chg auto|off|force|reset`. Prahy v nastavení `chg_start` a `chg_stop`.

### Přední pohon
Plyn jezdce (z primáru přes optočlen) jde do ESP. To posílá přednímu kontroléru přes MCP4725 podíl plynu (výchozí 60 %) a PI regulátorem dorovnává otáčky předního kola na zadní.

Ochrany:
- Výstup **nikdy nepřekročí plyn jezdce.**
- **Brzda** okamžitě nastaví klidový plyn a aktivuje brzdový vstup předního kontroléru.
- **Nad `drv_max_kmh`** (výchozí 25 km/h) se fáze odpojí a připojí se znovu až o 5 km/h níž. Je to ochrana proti dobíjení 36V baterie 48V motorem.
- **Prokluz** předního kola (o 25 % + 3 km/h rychlejší než zadní) stáhne plyn na nulu.
- **Poruchy s ručním resetem:** zmizí DAC, signál plynu vypadne za jízdy, přední kolo se rozběhne bez plynu.
- Relé fází se spíná a rozpíná jen s klidovým plynem. Při zapnutí musí být plyn puštěný.
- Pod 20 % baterie se pohon vypne. Zapnutý pohon vypne nabíjení.

**Uvedení do provozu (v tomto pořadí):**
1. `drv dacinit` – uloží 0 V jako výchozí hodnotu MCP4725. Po výpadku napájení pak DAC nezačne s plynem.
2. Plyn puštěný: `drv cal min`. Plný plyn: `drv cal max`. Pak `save`. Totéž jde z aplikace v Nastavení.
3. Multimetrem na výstupu DAC zkontroluj klidové napětí (0,8 V) a plný plyn (4,2 V). Hodnoty uprav v `dac_idle`, `dac_start` a `dac_max`.
4. `set drv_allowed 1`, `save`. Pohon zapneš dlouhým stiskem tlačítka Mód nebo v aplikaci. Po každém startu je vypnutý.
5. První jízdy dělej s koly ve vzduchu a s nízkým `drv_assist`.

MCP4725 napájený z 5 V potřebuje na I2C **převodník úrovní** (BSS138), protože 3,3V logika ESP mu nestačí.

### Napájení a vypnutí
- `PWR_HOLD` drží latch. GoGEN se vypne z aplikace, kombinací L+P na 2 s, po 15 min nečinnosti (`auto_off_min`, 0 = nikdy) nebo když baterie klesne pod 30,0 V na 30 s.
- Bez latche (nebo při napájení z USB) ESP usne a probudí ho stisk brzdy. DevKit ale ve spánku bere kvůli regulátoru a USB převodníku řádově mA, proto je latch doporučený.
- Při vypnutí se uloží ujeté km. Ukládají se také každých 500 m.

### Aktualizace přes Wi-Fi
V aplikaci *Nastavení → Systém → Aktualizace přes Wi-Fi* nebo v konzoli `ota`. Koloběžka musí stát a přední pohon se vypne.
1. Připoj telefon nebo PC k síti **GoGEN-xxxx-OTA**. Heslo je `gg` + BLE PIN, ve výchozím stavu `gg123456`.
2. Otevři **http://192.168.4.1** a nahraj `gogen_pcf_firmware.bin`.
3. ESP se restartuje. Přístupový bod se sám vypne po 10 minutách.

### Nastavení
Všechna nastavení jsou v aplikaci a v konzoli: `settings` vypíše klíče, rozsahy a výchozí hodnoty, `set <název> <hodnota>` změní hodnotu, `save` ji uloží.
Firmware 3.0 má novou strukturu nastavení, takže **po aktualizaci z 2.x se obnoví výchozí hodnoty.** Kalibraci baterie a PIN je pak potřeba zadat znovu. Ujeté km tovární reset nemaže.

### Bezpečnost
- Po startu jsou všechny výstupy vypnuté ještě před čímkoli jiným a DAC jde hned na 0 V. Watchdog má 2 s.
- BLE vyžaduje párování s PINem (výchozí **123456**, změň ho v aplikaci → Nastavení → Zabezpečení).

## Android aplikace

```bash
cd android-app
npm install
npm run dev              # náhled v prohlížeči, tlačítko „Demo režim“
npm run cap:sync         # build + kopie do android/
cd android && ./gradlew assembleDebug
```

Bez lokálního Android SDK: pushni repo na GitHub → Actions → **Build** → artefakt `GoGEN-apk`, nebo použij GoGEN devbox (`gogen build`).
Aplikace 3.0 potřebuje firmware 3.x (protokol v3). Se starším firmwarem napíše, že je potřeba ho aktualizovat.

## BLE protokol v3

Služba `a8f30001-4c1e-4b8e-9a52-6f6f67656e32`
- `…0002` STATE (read/notify, 20 B): výstupy, LED, napětí, %, stavové bity, uptime, blinkr, tlačítka, režim světel, rychlost, stav nabíjení a pohonu
- `…0003` CMD (write): `[cmd, arg…]`
- `…0004` CONFIG (read): verze + 34 hodnot u16 (klíč *k* na offsetu 4 + 2·(k−1)) + hw příznaky
- `…0005` TELEMETRY (read/notify, 32 B): důvody a poruchy nabíjení a pohonu, plyn, otáčky obou kol, napětí generátoru, lux, zpomalení, km

Podrobnosti jsou v `firmware/include/protocol.h`, zrcadlem je `android-app/src/ble/protocol.ts`.

## ⚠️ Před prvním zapojením ověř
1. **High-side moduly TAIL/BRAKE:** firmware předpokládá aktivní log. 1 (`OUT_ACTIVE_HIGH`).
2. **I2C:** jeden pár pull-upů 4,7 kΩ na 3V3. Většina modulů je má osazené, při pěti modulech některé odpájej.
3. **Napájení DevKitu:** 5 V z měniče na pin VIN/5V, ne na 3V3. Při BLE a Wi-Fi bere modul špičky až ~400 mA.
4. **Hally:** výstupy hallů předního kontroléru bývají vytažené na 5 V, takže před GPIO14 patří dělič. Zadní hall z primáru jen přes optočlen (galvanické oddělení).
5. **Obvod kola a počet pulzů na otáčku** (`wheel_mm`, `rear_ppr`, `front_ppr`) zkontroluj podle tachometru KT displeje.
