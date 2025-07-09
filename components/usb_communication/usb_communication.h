#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/speaker/speaker.h"
#include "esphome/components/microphone/microphone.h"
#include "esphome/components/i2s_audio/microphone/i2s_audio_microphone.h"
#include <cstdio>
#include <string>
#include <vector>

namespace esphome {
namespace usb_communication {

class USBCommunicationComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void mark_usb_activity();
  
  // Getters for current configuration
  std::string get_current_sensitivity() const { return current_sensitivity_; }
  std::string get_current_wake_word() const { return current_wake_word_; }
  int get_current_voice_phase() const { return current_voice_phase_; }
  
  // Audio control getters
  bool is_unmute_requested() { 
    if (unmute_requested_) {
      unmute_requested_ = false;
      return true;
    }
    return false;
  }
  bool is_volume_change_requested() {
    if (volume_change_requested_) {
      volume_change_requested_ = false;
      return true;
    }
    return false;
  }
  float get_requested_volume() const { return requested_volume_; }
  
  // Tone playback request getter
  bool is_tone_playback_requested() {
    if (tone_playback_requested_) {
      tone_playback_requested_ = false;
      return true;
    }
    return false;
  }
  
  // Setters for YAML to update current state
  void update_voice_phase(int phase) { current_voice_phase_ = phase; }
  
  // Audio playback trigger state
  bool should_play_audio() { 
    if (audio_trigger_pending_) {
      audio_trigger_pending_ = false;
      return true;
    }
    return false;
  }
  
  // USB audio streaming methods (replicating voice assistant interface)
  void set_speaker(speaker::Speaker *speaker) { 
    target_speaker_ = speaker; 
    ESP_LOGI("usb_communication", "Speaker reference set: %p", speaker);
  }
  void set_microphone(microphone::Microphone *microphone) {
    source_microphone_ = microphone;
    ESP_LOGI("usb_communication", "Microphone reference set: %p", microphone);
  }
  void start_audio_stream();
  void write_audio_chunk(const uint8_t *data, size_t length);
  void finish_audio_stream();
  bool has_audio_data() const { return usb_audio_buffer_size_ > 0; }
  void clear_audio_buffer() { 
    usb_audio_buffer_index_ = 0; 
    usb_audio_buffer_size_ = 0; 
  }
  
  // Microphone capture methods
  bool capture_microphone_data(std::vector<int16_t> &buffer, size_t samples_needed);
  void start_microphone_capture();
  void stop_microphone_capture();
  
  // Audio data injection (for receiving real microphone data)
  void inject_audio_data(const int16_t* samples, size_t sample_count);
  bool has_recent_audio_data() const;
  void get_latest_audio_data(std::vector<int16_t> &buffer, size_t samples_needed);

 protected:
  bool read_line_(std::string *line);
  void process_message_(const std::string &message);
  void process_config_(const std::string &message);
  void process_play_audio_(const std::string &message);
  void process_play_audio_compressed_(const std::string &message);
  void process_play_tone_(const std::string &message);
  void process_play_audio_chunk_(const std::string &message);
  void process_audio_data_chunk_(const std::string &message);
  void send_status_update_();
  void send_wake_word_options_();
  void send_response_(const char* response_type);
  void send_json_(const std::string &json);
  
 private:
  std::string input_buffer_;
  std::string current_wake_word_ = "Okay Nabu";
  std::string current_sensitivity_ = "Moderately sensitive";
  int current_voice_phase_ = 1;  // Default to idle phase
  bool audio_trigger_pending_ = false;
  
  // Audio control flags
  bool unmute_requested_ = false;
  bool volume_change_requested_ = false;
  float requested_volume_ = 0.85;
  bool tone_playback_requested_ = false;
  
  // Audio chunk management
  std::vector<std::vector<int>> audio_chunks_;
  int expected_total_chunks_ = 0;
  int received_chunks_ = 0;
  
  // USB Audio streaming buffer (replicating voice assistant architecture)
  static const size_t USB_AUDIO_BUFFER_SIZE = 16 * 1024; // 16KB like voice assistant
  uint8_t *usb_audio_buffer_;
  size_t usb_audio_buffer_index_;
  size_t usb_audio_buffer_size_;
  bool is_streaming_audio_;
  
  // Speaker reference for direct streaming
  speaker::Speaker *target_speaker_;
  
  // Microphone reference for audio capture
  microphone::Microphone *source_microphone_;
  bool is_capturing_audio_;
  std::vector<int16_t> microphone_buffer_;
  
  // Audio data injection for real microphone data
  std::vector<int16_t> injected_audio_buffer_;
  unsigned long last_audio_injection_time_;
  static const size_t MAX_INJECTED_AUDIO_BUFFER_SIZE = 1600; // 100ms at 16kHz
};

}  // namespace usb_communication
}  // namespace esphome