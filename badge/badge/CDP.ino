#include <ArduinoJson.h>

bool transmitSignedCoin(uint16_t myBadgeID, byte *csrPtr, int packetSize) {
  SignedCoin signedCoin;
  CoinSigningRequest *csr;
  size_t signatureLen = 0;
  char json[MAX_JSON_SIZE];
  
  csr = (CoinSigningRequest *)csrPtr;
  
  if (csr->coin.broadcasterID != myBadgeID) {
    Serial.print("Ignoring CSR, it was not intended for me and instead was for badge #");
    Serial.println(csr->coin.broadcasterID);
    return false;
  }

  signedCoin.csr = *csr;
  //signedCoin.signatureBroadcaster[0] = 0x02;
  sign(csrPtr, sizeof(CoinSigningRequest), signedCoin.signatureBroadcaster, &signatureLen);

  jsonifySignedCoin((byte *)&signedCoin, sizeof(signedCoin), json, MAX_JSON_SIZE);

  turnWiFiOnAndConnect();
  if (!submitSignedCoinToAPI(json)) // mother of all hacks, this should really be in main
    storeUnsentSignedCoinOnFS(csr->coin.CSRID, json); 
  else
    storeCompletedCoinOnFS(csr->coin.CSRID); 
  turnWiFiOff();

  Serial.print(F("Transmitting signed coin to badge #"));
  Serial.println(signedCoin.csr.coin.CSRID);
  LoRa.beginPacket();
  LoRa.write(CDP_SIGNEDCOIN_TYPE);
  LoRa.write((byte *)&signedCoin, sizeof(signedCoin));
  LoRa.endPacket();

  return true;
}

bool transmitCoinSigningRequest(uint16_t myBadgeID, byte *gbpPtr, int packetSize) {
  GlobalBroadcast *gbp;
  CoinSigningRequest csr;
  Coin coin;
  size_t signatureLen = 0;
  int packetRssi = LoRa.packetRssi();
  
  if ((packetSize - 1) != sizeof(GlobalBroadcast)) {  // subtract 1 to account for type byte
    Serial.print(F("GlobalBroadcast was an invalid length of "));
    Serial.println(packetSize);
    return false;
  }
    
  gbp = (GlobalBroadcast *)gbpPtr;

  if (packetRssi < CSR_RSSI_THRESHOLD) {
      Serial.print(F("Ignoring broadcast from badge #"));
      Serial.print(gbp->badgeID);
      Serial.print(F(" RSSI is too weak.. "));
      Serial.println(packetRssi);
      return false;
  } else {
      Serial.print(F("broadcast from badge #"));
      Serial.print(gbp->badgeID);
      Serial.print(F(" RSSI: "));
      Serial.println(packetRssi);
  }

  if (ifCoinExistsOnFS(gbp->badgeID)) {  // this is just getting stupid messy at this point
      Serial.print(F("Coin already exists ignoring broadcast from badge "));
      Serial.println(gbp->badgeID);
      return false;
  }
  
  coin.CSRID = myBadgeID;
  coin.broadcasterID = gbp->badgeID;
  csr.coin = coin;
  //csr.signatureCSR[0] = 0x01;
  sign((byte *)&csr.coin, sizeof(csr.coin), csr.signatureCSR, &signatureLen);
  
  Serial.print(F("Generating CSR for badge #"));
  Serial.println(gbp->badgeID);
  LoRa.beginPacket();
  LoRa.write(CDP_COINSIGNINGREQUEST_TYPE);
  LoRa.write((byte *)&csr, sizeof(csr));
  LoRa.endPacket();

  return true;
}

void transmitGlobalBroadcast(uint16_t myBadgeID) {
  GlobalBroadcast gbp;
  gbp.badgeID = myBadgeID;
  
  Serial.print(F("Transmitting broadcast with my badge #"));
  Serial.println(gbp.badgeID);
  LoRa.beginPacket();
  LoRa.write(CDP_GLOBALBROADCAST_TYPE);
  LoRa.write((byte *)&gbp, sizeof(gbp));
  LoRa.endPacket();
}

bool isValidGlobalBroadcast(byte *gbpPtr, int packetSize) {
  if ((packetSize - 1) != sizeof(GlobalBroadcast)) {  // subtract 1 to account for type byte
    Serial.print(F("GlobalBroadcast was an invalid length of "));
    Serial.println(packetSize);
    return false;
  }
  true;
}

bool isValidCoinSigningRequest(byte *csrPtr, int packetSize) {
  if ((packetSize - 1) != sizeof(CoinSigningRequest)) {  // subtract 1 to account for type byte
    Serial.print(F("CoinSigningRequest was an invalid length of "));
    Serial.println(packetSize);
    return false;
  }
  return true;
}

bool isValidSignedCoin(byte *scnPtr, int packetSize) {
  if ((packetSize - 1) != sizeof(SignedCoin)) {  // subtract 1 to account for type byte
    Serial.print(F("SignedCoin was an invalid length of "));
    Serial.println(packetSize);
    return false;
  }
  return true;
}

uint16_t getCSRIDFromBytes(byte *scnPtr, int packetSize) {
  // This entire function is a 1am mother fucking hack.
  SignedCoin *scn = (SignedCoin *)scnPtr;
  return scn->csr.coin.CSRID;
}

uint16_t getBroadcasterIDFromBytes(byte *scnPtr, int packetSize) {
  // This entire function is a 1am mother fucking hack.
  SignedCoin *scn = (SignedCoin *)scnPtr;
  return scn->csr.coin.broadcasterID;
}

bool jsonifySignedCoin(byte *scnPtr, int packetSize, char *buf, int bufsize) {
  SignedCoin *scn = (SignedCoin *)scnPtr;
  size_t olen1, olen2;
  unsigned char signatureCSRB64[BASE64_MAX_SIZE]; // this should be ciel(modulus_size / 3) * 4, but im being lazy
  unsigned char signatureBroadcasterB64[BASE64_MAX_SIZE]; // this should be ciel(modulus_size / 3) * 4, but im being lazy

  if (mbedtls_base64_encode(signatureCSRB64, BASE64_MAX_SIZE, &olen1, scn->csr.signatureCSR, CDP_MODULUS_SIZE) != 0 ) {
    Serial.println(F("Failed to convert CSR signature to base64"));
    return "";
  }
  if (mbedtls_base64_encode(signatureBroadcasterB64, BASE64_MAX_SIZE, &olen2, scn->signatureBroadcaster, CDP_MODULUS_SIZE) != 0) {
    Serial.println(F("Failed to convert broadcaster signature to base64"));
  }
  
  StaticJsonBuffer<MAX_JSON_SIZE> jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  root["CSRID"] = scn->csr.coin.CSRID;
  root["broadcasterID"] = scn->csr.coin.broadcasterID;
  root["signatureCSR"] = signatureCSRB64;
  root["signatureBroadcaster"] = signatureBroadcasterB64;
  root.printTo(buf, bufsize);
  return true;
}

