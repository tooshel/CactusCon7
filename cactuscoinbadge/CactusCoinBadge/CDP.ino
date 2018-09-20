typedef struct GlobalBroadcast {
  uint16_t badgeID;  
};

typedef struct Coin {
  uint16_t CSRID;
  uint16_t broadcasterID;
};

typedef struct CoinSigningRequest {
  Coin coin;
  byte signatureCSR[CDP_MODULUS_SIZE];
};

typedef struct SignedCoin {
  CoinSigningRequest csr;
  byte signatureBroadcaster[CDP_MODULUS_SIZE];
};

int transmitSignedCoin(uint16_t myBadgeID, byte *csrPtr, int packetSize) {
  if ((packetSize - 1) != sizeof(CoinSigningRequest)) {  // subtract 1 to account for type byte
    Serial.print("CoinSigningRequest was an invalid length of ");
    Serial.println(packetSize);
    return -1;
  }
  
  CoinSigningRequest *csr = (CoinSigningRequest *)csrPtr;
  
  if (csr->coin.broadcasterID != myBadgeID) {
    Serial.print("Ignoring CSR, it was intended for badge #");
    Serial.print(csr->coin.broadcasterID);
    Serial.print(" and my badge is #");
    Serial.println(myBadgeID);
    return -1;
  }
  
  SignedCoin signedCoin;
  signedCoin.csr = *csr;
  //signedCoin.signatureBroadcaster = 
  
  Serial.print("Signing CSR for Badge #");
  Serial.println(csr->coin.CSRID);
  LoRa.beginPacket();
  LoRa.write(CDP_SIGNEDCOIN_TYPE);
  LoRa.write((byte *)&signedCoin, sizeof(signedCoin));
  LoRa.endPacket();
}

int transmitCoinSigningRequest(uint16_t myBadgeID, byte *gbpPtr, int packetSize) {
  int packetRssi = LoRa.packetRssi();
  if (packetRssi < CSR_RSSI_THRESHOLD) {
      Serial.print("Ignoring broadcast RSSI (");
      Serial.print(packetRssi);
      Serial.println(") below threshold");
      return -1;
  }
  
  if ((packetSize - 1) != sizeof(GlobalBroadcast)) {  // subtract 1 to account for type byte
    Serial.print("GlobalBroadcast was an invalid length of ");
    Serial.println(packetSize);
    return -1;
  }
    
  GlobalBroadcast *gbp = (GlobalBroadcast *)gbpPtr;
  Coin coin;
  coin.CSRID = myBadgeID;
  coin.broadcasterID;
  CoinSigningRequest csr;
  csr.coin = coin;
  // csr->signatureCSR = we'll just leak some random bits for now
  
  Serial.print("Generating CSR for Badge #");
  Serial.println(gbp->badgeID);
  LoRa.beginPacket();
  LoRa.write(CDP_COINSIGNINGREQUEST_TYPE);
  LoRa.write((byte *)&csr, sizeof(csr));
  LoRa.endPacket();
}

void transmitGlobalBroadcast(uint16_t myBadgeID) {
  GlobalBroadcast gbp;
  gbp.badgeID = myBadgeID;
  
  Serial.println("Transmitting broadcast");
  LoRa.beginPacket();
  LoRa.write(CDP_GLOBALBROADCAST_TYPE);
  LoRa.write((byte *)&gbp, sizeof(gbp));
  LoRa.endPacket();
}



