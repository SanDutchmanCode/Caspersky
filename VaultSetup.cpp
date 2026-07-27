#include "VaultSetup.h"
#include <WiFi.h>
#include "FS.h"
#include "LittleFS.h"
#include <mbedtls/md.h>
#include "mbedtls/aes.h"
#include "mbedtls/gcm.h"

extern String getSHA256(String input);
extern String hardwareGCM(String data, bool encrypt, String decryptionKey = "", String storedData = "");

void setupVault() {
  if (!LittleFS.begin(true)) return;

  Serial.println("ENTER COFFEE:");
  while (!Serial.available());
  String coffee = Serial.readStringUntil('\n');
  coffee.trim(); 
  
  // The hash of the coffee is stored as the login trigger
  String coffeeHash = getSHA256(coffee);
  Serial.print("Saving hash: "); Serial.println(coffeeHash);

  Serial.println("ENTER EXTRACT:");
  while (!Serial.available());
  String extract = Serial.readStringUntil('\n');
  extract.trim();

  Serial.println("ENTER BEANS:");
  while (!Serial.available());
  String bean = Serial.readStringUntil('\n');
  bean.trim();
  
  // Encrypt the beans using the extract (together with the efuse MAC) as the key
  String encryptedBean = hardwareGCM(bean, true, extract);
  
  fs::File f = LittleFS.open("/vault.bin", "w");
  if (f) {
    f.println(coffeeHash);
    f.println(encryptedBean);
    f.close();
    Serial.println("Coffee is ground!");
  }
}