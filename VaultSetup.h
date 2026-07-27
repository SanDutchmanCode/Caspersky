#include "VaultSetup.h" // Voeg deze toe bovenin je code

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Roep hier eenmalig de setup aan om je /vault.bin te genereren op de ESP32.
  // Zodra je kluis erop staat, kun je deze regel weglaten of uitcommentariëren!
  setupVault(); 

  // Rest van je normale setup...
}