#pragma once
#include <Arduino.h>
#include <Wire.h>

void lsp(uint32_t ms);
void lsp_AUX(uint32_t ms);
bool readSHT35(TwoWire &w, float &t, float &h);
float calculateWBGT(float t, float h, float tg);
int16_t bankersRound10(float val);
int16_t round10(float val);
bool waitSerial(Stream &serialRef, uint32_t timeoutMs);