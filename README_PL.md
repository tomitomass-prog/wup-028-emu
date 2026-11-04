# DualSense -> BlueRetro (LOLIN ESP32) -> Wii Extension I2C -> ESP32-S2 Mini -> WUP-028 USB

STATUS: PROTOTYP DO KOMPILACJI I TESTOW, NIE ZWERYFIKOWANY NA SPRZECIE.

## Sprzet

* LOLIN ESP32 (klasyczny ESP32, nie S2/S3): oficjalny BlueRetro, wyjscie Wii-Ext, tryb GamePadAlt (Classic Controller z analogowymi triggerami).
* ESP32-S2 Mini: firmware z katalogu `s2_patch` podmieniajacy zrodlo wejscia adaptera loopj na Wii-Ext I2C.
* 2 rezystory podciagajace 2.2-4.7 kOhm do 3.3V (SDA i SCL), wspolna masa.
* Oba moduly zasilane niezaleznie z odpowiednich portow USB lub stabilnego 3.3V. NIE laczyc ze soba pinow 5V ani 3V3 z dwoch osobno zasilanych plytek.

## Polaczenia

LOLIN ESP32 (BlueRetro Wii-Ext) -> ESP32-S2 Mini
GPIO25 (SCL port 1) -> GPIO9 (SCL)
GPIO26 (SDA port 1) -> GPIO8 (SDA)
GND -> GND
SCL -> rezystor 2.2-4.7k -> 3.3V (szyna logiczna)
SDA -> rezystor 2.2-4.7k -> 3.3V (szyna logiczna)
ESP32-S2 USB-C -> Wii U USB

UWAGA: w BlueRetro port #1 Wii-Ext korzysta z IO25/IO26 wg oficjalnej tabeli pinow. Zweryfikuj, czy wybrany wariant firmware BlueRetro dziala na Twoim module LOLIN i czy port 1 jest aktywny. Oba MCU pracuja na logice 3.3V.

## BlueRetro na LOLIN

1. Nie piszemy zamiennika BlueRetro: zachowujemy oryginalny projekt, bo zawiera parowanie i obsluge DualSense.
2. Wgraj odmiane BlueRetro skonfigurowana jako Wii-Ext; dla uniwersalnego firmware sprawdz sposob wyboru systemu w dokumentacji BlueRetro.
3. W web config ustaw output #1 = GamePadAlt (oryginalny Classic Controller, analogowe triggery).
4. Sparuj DualSense: PS + Create i ustaw mapowanie przyciskow.
5. Na wyprowadzeniach IO25/IO26 dostepna ma byc emulowana magistrala Wii Extension slave pod adresem 0x52.

## Firmware S2 - kompilacja na komputerze z ESP-IDF

1. Pobierz z https://github.com/loopj/usb-gamecube-controller-adapter repozytorium ORAZ submoduly (`git clone --recursive ...`).
2. W katalogu repozytorium zastap `ports/esp32/main/main.c` plikiem `s2_patch/main.c`.
3. Skopiuj `s2_patch/bridge.c` do `ports/esp32/main/bridge.c`.
4. Zastap `ports/esp32/main/CMakeLists.txt` plikiem `s2_patch/CMakeLists.txt`.
5. Wejdz do `ports/esp32` i wykonaj:

   idf.py set-target esp32s2
   idf.py build
   idf.py -p PORT flash

Uzyj wersji ESP-IDF zgodnej z repozytorium i esp_tinyusb. Komponenty z projektu oryginalnego pozostaja w katalogu. Nie uzywaj gotowych obrazow ESP32-S3 na ESP32-S2!

## Testy

A. Podlacz S2 do komputera. Oczekiwane USB VID:PID = 057e:0337, produkt WUP-028. To dowodzi tylko enumeracji USB.
B. Sprawdz, czy ESP32-S2 wykrywa na I2C adres 0x52 i identyfikator Wii Classic (ostatni bajt 01). Jesli nie: sprawdz przewody, GND, zasilanie, pull-up oraz wariant BlueRetro.
C. Sprawdz w Dolphin lub narzedziu HID, czy stan przyciskow i osi sie zmienia.
D. Uruchom Twin Snakes jako WiiVC Inject; w konfiguracji zgodnej z Nintendont sprawdz wykrywanie portu i przyciskow. Nie zakladaj sukcesu bez testu.
E. Sprawdz L/R, precyzje osi, klik, Start, zmiane portu przy Psycho Mantis i zachowanie po rozlaczeniu DualSense.

## Ograniczenia

* To wersja testowa: brak przekazywania wibracji do DualSense; `ZL`, Home, Minus nie sa mapowane.
* Czestotliwosc odczytu I2C ok. 250 Hz, USB max ok. 250 Hz ze wzgledu na petle; deskryptor USB deklaruje interwal 1 ms.
* S2 nie ma Bluetooth; dlatego DualSense laczy sie z klasycznym ESP32.
* USB HID descriptor pochodzi z kodu loopj i moze wymagac zmian w zaleznosci od wersji TinyUSB/ESP-IDF.
* Nie przeprowadzono kompilacji ani testu fizycznego w tym srodowisku; wynik na WiiVC Inject jest niepotwierdzony.
* Kod jest szkicem integracyjnym dla wskazanej struktury upstream; po zmianie API moze wymagac dostosowania.

## Referencje

https://github.com/loopj/usb-gamecube-controller-adapter
https://github.com/darthcloud/BlueRetro
https://github.com/darthcloud/BlueRetro/wiki/BlueRetro-Core-Pinout-Specification
https://github.com/darthcloud/BlueRetro/wiki/BlueRetro-System-Specific-User-Manual
https://wiibrew.org/wiki/Classic_Controller
