#include "SpeakerDriver.h"

#include <cmath>

namespace {

constexpr gpio_num_t kI2sBclk = GPIO_NUM_48;
constexpr gpio_num_t kI2sWordSelect = GPIO_NUM_38;
constexpr gpio_num_t kI2sData = GPIO_NUM_47;
constexpr uint32_t kSampleRate = 16000;
constexpr float kTwoPi = 6.28318530718f;

}  // namespace

bool SpeakerDriver::begin() {
  i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  if (i2s_new_channel(&channelConfig, &txChannel_, nullptr) != ESP_OK) return false;

  i2s_std_config_t standardConfig = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRate),
    .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT,
      I2S_SLOT_MODE_STEREO
    ),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = kI2sBclk,
      .ws = kI2sWordSelect,
      .dout = kI2sData,
      .din = I2S_GPIO_UNUSED,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
      },
    },
  };
  if (i2s_channel_init_std_mode(txChannel_, &standardConfig) != ESP_OK ||
      i2s_channel_enable(txChannel_) != ESP_OK) {
    i2s_del_channel(txChannel_);
    txChannel_ = nullptr;
    return false;
  }
  ready_ = xTaskCreate(taskEntry, "speaker", 3072, this, 1, &task_) == pdPASS;
  return ready_;
}

void SpeakerDriver::confirmWeight() {
  if (enabled_ && ready_ && task_ != nullptr) xTaskNotify(task_,1,eSetBits);
}

void SpeakerDriver::testTone() {
  confirmWeight();
}

void SpeakerDriver::taskEntry(void *argument) {
  static_cast<SpeakerDriver *>(argument)->taskLoop();
}

void SpeakerDriver::taskLoop() {
  while (true) {
    uint32_t event=0;
    xTaskNotifyWait(0,UINT32_MAX,&event,portMAX_DELAY);
    if (enabled_) { if(event&2) { playTone(330,220,30); vTaskDelay(pdMS_TO_TICKS(80)); playTone(330,220,30); } else playConfirmation(); }
  }
}

void SpeakerDriver::playConfirmation() {
  playTone(880, 105, 34);
  vTaskDelay(pdMS_TO_TICKS(35));
  playTone(1320, 150, 40);
}

void SpeakerDriver::playTone(uint16_t frequency, uint16_t durationMs, uint8_t amplitude) {
  if (txChannel_ == nullptr) return;
  constexpr size_t kFrames = 128;
  int16_t samples[kFrames * 2];
  const uint32_t totalFrames = kSampleRate * durationMs / 1000;
  uint32_t writtenFrames = 0;
  while (writtenFrames < totalFrames) {
    const size_t frameCount = min<uint32_t>(kFrames, totalFrames - writtenFrames);
    for (size_t frame = 0; frame < frameCount; ++frame) {
      const float phase = kTwoPi * frequency * (writtenFrames + frame) / kSampleRate;
      const int16_t value = static_cast<int16_t>(sinf(phase) * amplitude * 256);
      samples[frame * 2] = value;
      samples[frame * 2 + 1] = value;
    }
    size_t bytesWritten = 0;
    i2s_channel_write(
      txChannel_,
      samples,
      frameCount * 2 * sizeof(int16_t),
      &bytesWritten,
      pdMS_TO_TICKS(50)
    );
    writtenFrames += frameCount;
  }
  memset(samples, 0, sizeof(samples));
  size_t bytesWritten = 0;
  i2s_channel_write(txChannel_, samples, sizeof(samples), &bytesWritten, pdMS_TO_TICKS(50));
}

void SpeakerDriver::alert() { if(enabled_&&ready_&&task_) xTaskNotify(task_,2,eSetBits); }
