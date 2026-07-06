#include "HalPowerManager.h"

#include <Logging.h>
#include <WiFi.h>
#include <esp_sleep.h>

#include <cassert>

#include "HalGPIO.h"

HalPowerManager powerManager;  // Singleton instance

namespace {

constexpr uint8_t I2C_ADDR_MAX17048 = 0x36;
constexpr uint8_t MAX17048_REG_SOC = 0x04;

bool readMax17048Reg16(uint8_t reg, uint16_t* outValue) {
  Wire.beginTransmission(I2C_ADDR_MAX17048);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const uint8_t bytesRead = Wire.requestFrom(I2C_ADDR_MAX17048, static_cast<uint8_t>(2), static_cast<uint8_t>(true));
  if (bytesRead != 2) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }

  const uint8_t msb = Wire.read();
  const uint8_t lsb = Wire.read();
  *outValue = (static_cast<uint16_t>(msb) << 8) | lsb;
  return true;
}

uint16_t max17048SocToPercent(uint16_t rawSoc) {
  uint16_t percentage = (rawSoc + 128) / 256;  // SOC register unit is 1%/256.
  if (percentage > 100) {
    percentage = 100;
  }
  return percentage;
}

}  // namespace

void HalPowerManager::begin() {
  normalFreq = getCpuFrequencyMhz();
  modeMutex = xSemaphoreCreateMutex();
  assert(modeMutex != nullptr);

  uint16_t rawSoc = 0;
  if (readMax17048Reg16(MAX17048_REG_SOC, &rawSoc)) {
    _batteryGaugeAvailable = true;
    _batteryCachedPercent = max17048SocToPercent(rawSoc);
    _batteryLastPollMs = millis();
    LOG_INF("PWR", "MAX17048 fuel gauge detected: %u%%", _batteryCachedPercent);
  } else {
    _batteryGaugeAvailable = false;
    _batteryCachedPercent = 80;
    _batteryLastPollMs = millis();
    LOG_INF("PWR", "MAX17048 fuel gauge not detected, using fallback battery value");
  }
}

void HalPowerManager::setPowerSaving(bool enabled) {
  if (normalFreq <= 0) {
    return;  // invalid state
  }

  auto wifiMode = WiFi.getMode();
  if (wifiMode != WIFI_MODE_NULL) {
    // Wifi is active, force disabling power saving
    enabled = false;
  }

  // Note: We don't use mutex here to avoid too much overhead,
  // it's not very important if we read a slightly stale value for currentLockMode
  const LockMode mode = currentLockMode;

  if (mode == None && enabled && !isLowPower) {
    LOG_DBG("PWR", "Going to low-power mode");
    if (!setCpuFrequencyMhz(LOW_POWER_FREQ)) {
      LOG_DBG("PWR", "Failed to set CPU frequency = %d MHz", LOW_POWER_FREQ);
      return;
    }
    isLowPower = true;

  } else if ((!enabled || mode != None) && isLowPower) {
    LOG_DBG("PWR", "Restoring normal CPU frequency");
    if (!setCpuFrequencyMhz(normalFreq)) {
      LOG_DBG("PWR", "Failed to set CPU frequency = %d MHz", normalFreq);
      return;
    }
    isLowPower = false;
  }

  // Otherwise, no change needed
}

void HalPowerManager::startDeepSleep(HalGPIO& gpio) const {
  // Ensure that the power button has been released to avoid immediately turning back on if you're holding it
  while (gpio.isPressed(HalGPIO::BTN_POWER)) {
    delay(50);
    gpio.update();
  }

  pinMode(EPD_SCLK, INPUT);
  pinMode(EPD_MOSI, INPUT);
  pinMode(SPI_MISO, INPUT);
  pinMode(EPD_CS, INPUT);
  pinMode(SD_CS, INPUT);
  pinMode(EPD_DC, INPUT);
  pinMode(EPD_RST, INPUT);
  pinMode(EPD_BUSY, INPUT);
  digitalWrite(PERIPH_EN, LOW);

#ifdef ENABLE_SERIAL_LOG
  // Tear down HWCDC so the host sees a clean disconnect and the peripheral
  // doesn't hold power domains that interfere with USB-powered GPIO wake.
  // logSerial is the raw HWCDC reference; Serial is the MySerialImpl proxy
  // (which doesn't expose end()).
  logSerial.end();
#endif

  pinMode(InputManager::BUTTON_ADC_PIN_1, INPUT);
  pinMode(InputManager::BUTTON_ADC_PIN_2, INPUT);
  esp_sleep_enable_ext1_wakeup(InputManager::DEEP_SLEEP_WAKEUP_PIN_MASK, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

uint16_t HalPowerManager::getBatteryPercentage() const {
  const unsigned long now = millis();
  if (now - _batteryLastPollMs < BATTERY_POLL_MS) {
    return _batteryCachedPercent;
  }

  uint16_t rawSoc = 0;
  if (readMax17048Reg16(MAX17048_REG_SOC, &rawSoc)) {
    _batteryGaugeAvailable = true;
    _batteryCachedPercent = max17048SocToPercent(rawSoc);
  } else {
    if (_batteryGaugeAvailable) {
      LOG_DBG("PWR", "MAX17048 read failed, keeping cached battery value");
    }
    _batteryGaugeAvailable = false;
  }

  _batteryLastPollMs = now;
  return _batteryCachedPercent;
}

HalPowerManager::Lock::Lock() {
  xSemaphoreTake(powerManager.modeMutex, portMAX_DELAY);
  // Current limitation: only one lock at a time
  if (powerManager.currentLockMode != None) {
    LOG_ERR("PWR", "Lock already held, ignore");
    valid = false;
  } else {
    powerManager.currentLockMode = NormalSpeed;
    valid = true;
  }
  xSemaphoreGive(powerManager.modeMutex);
  if (valid) {
    // Immediately restore normal CPU frequency if currently in low-power mode
    powerManager.setPowerSaving(false);
  }
}

HalPowerManager::Lock::~Lock() {
  xSemaphoreTake(powerManager.modeMutex, portMAX_DELAY);
  if (valid) {
    powerManager.currentLockMode = None;
  }
  xSemaphoreGive(powerManager.modeMutex);
}
