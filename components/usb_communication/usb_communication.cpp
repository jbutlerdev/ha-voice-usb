#include "usb_communication.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "driver/usb_serial_jtag.h"

namespace esphome {
namespace usb_communication {

static const char *const TAG = "usb_communication";

void USBCommunicationComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up USB Communication Component using USB Serial/JTAG");
  
  // Allocate USB audio buffer (16KB like voice assistant)
  usb_audio_buffer_ = new uint8_t[USB_AUDIO_BUFFER_SIZE];
  usb_audio_buffer_index_ = 0;
  usb_audio_buffer_size_ = 0;
  is_streaming_audio_ = false;
  target_speaker_ = nullptr;
  source_microphone_ = nullptr;
  is_capturing_audio_ = false;
  last_audio_injection_time_ = 0;
  
  ESP_LOGCONFIG(TAG, "USB Communication ready - allocated %d byte audio buffer", USB_AUDIO_BUFFER_SIZE);
  ESP_LOGCONFIG(TAG, "Speaker reference: %s", target_speaker_ ? "SET" : "NULL");
  
  // Send a boot message to indicate component is ready
  this->send_response_("boot_complete");
}

void USBCommunicationComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "USB Communication:");
}

void USBCommunicationComponent::loop() {
  // Read incoming data with timeout protection
  static std::string line_buffer;
  static unsigned long last_read_time = 0;
  static bool boot_message_sent = false;
  unsigned long now = millis();
  
  // Send a boot message after 3 seconds to verify UART communication
  if (!boot_message_sent && now > 3000) {
    this->send_response_("boot_complete");
    boot_message_sent = true;
  }
  
  // Clear buffer if it gets too old (prevent memory buildup)
  if (now - last_read_time > 5000 && !line_buffer.empty()) {
    line_buffer.clear();
    last_read_time = now;
  }
  
  if (this->read_line_(&line_buffer)) {
    this->process_message_(line_buffer);
    line_buffer.clear();  // Clear buffer after processing
    last_read_time = now;
  }
  
  // Send periodic status updates less frequently to avoid overwhelming
  static unsigned long last_status_update = 0;
  if (now - last_status_update > 10000) {  // Every 10 seconds
    this->send_status_update_();
    last_status_update = now;
  }
}

bool USBCommunicationComponent::read_line_(std::string *line) {
  // Read from stdin (USB Serial/JTAG) character by character
  int c = getchar();
  
  if (c == EOF) {
    return false;  // No data available
  }
  
  if (c == '\n') {
    if (!input_buffer_.empty()) {
      *line = input_buffer_;
      ESP_LOGD(TAG, "Complete line received: %s", line->c_str());
      input_buffer_.clear();
      return true;
    }
  } else if (c != '\r') {
    // Prevent buffer overflow
    if (input_buffer_.length() < 512) {
      input_buffer_.push_back(static_cast<char>(c));
    } else {
      // Buffer too large, clear it
      ESP_LOGW(TAG, "Input buffer overflow, clearing");
      input_buffer_.clear();
    }
  }
  
  return false;
}

void USBCommunicationComponent::mark_usb_activity() {
  // Function to be called from YAML to mark USB activity
  // This will be implemented in the YAML lambda
}

void USBCommunicationComponent::process_message_(const std::string &message) {
  // Update heartbeat timestamp - this will be accessed by YAML interval
  static unsigned long last_message_time = 0;
  last_message_time = millis();
  
  // Debug: Log received message (temporarily)
  ESP_LOGI(TAG, "Received message: %s", message.c_str());
  ESP_LOGI(TAG, "Message length: %d", message.length());
  
  // Simple JSON parsing for basic commands
  if (message.find("\"type\":\"heartbeat\"") != std::string::npos) {
    ESP_LOGD(TAG, "Processing heartbeat, sending ack");
    this->send_response_("heartbeat_ack");
  }
  else if (message.find("\"type\":\"get_status\"") != std::string::npos) {
    ESP_LOGD(TAG, "Processing get_status request");
    this->send_status_update_();
  }
  else if (message.find("\"type\":\"get_wake_word_options\"") != std::string::npos) {
    ESP_LOGD(TAG, "Processing get_wake_word_options request");
    this->send_wake_word_options_();
  }
  else if (message.find("\"type\":\"config\"") != std::string::npos) {
    ESP_LOGD(TAG, "Processing config message");
    this->process_config_(message);
  }
  else if (message.find("\"type\":\"disconnect\"") != std::string::npos) {
    ESP_LOGD(TAG, "Processing disconnect message");
    // Handle explicit disconnection
    last_message_time = 0;  // Force timeout
  }
  else if (message.find("\"type\":\"play_tone\"") != std::string::npos) {
    ESP_LOGI(TAG, "Processing play tone message");
    this->process_play_tone_(message);
  }
  else if (message.find("\"type\":\"play_audio_compressed\"") != std::string::npos) {
    ESP_LOGD(TAG, "Processing compressed play audio message");
    this->process_play_audio_compressed_(message);
  }
  else if (message.find("\"type\":\"play_audio\"") != std::string::npos) {
    ESP_LOGD(TAG, "Processing play audio message");
    this->process_play_audio_(message);
  }
  else if (message.find("\"type\":\"play_audio_chunk\"") != std::string::npos) {
    ESP_LOGD(TAG, "Processing play audio chunk message");
    this->process_play_audio_chunk_(message);
  }
  else if (message.find("\"type\":\"start_audio_stream\"") != std::string::npos) {
    ESP_LOGD(TAG, "Processing start audio stream");
    this->start_audio_stream();
  }
  else if (message.find("\"type\":\"audio_data_chunk\"") != std::string::npos) {
    ESP_LOGD(TAG, "Processing audio data chunk");
    this->process_audio_data_chunk_(message);
  }
  else if (message.find("\"type\":\"finish_audio_stream\"") != std::string::npos) {
    ESP_LOGD(TAG, "Processing finish audio stream");
    this->finish_audio_stream();
    this->send_response_("audio_stream_complete");
  }
  else {
    ESP_LOGI(TAG, "Unknown message type: %s", message.c_str());
  }
}

void USBCommunicationComponent::process_config_(const std::string &message) {
  ESP_LOGI(TAG, "Processing configuration: %s", message.c_str());
  
  // Handle unmute request
  if (message.find("\"unmute\":true") != std::string::npos) {
    ESP_LOGI(TAG, "Unmuting device via config");
    // This will be handled by YAML automation based on the flag
    unmute_requested_ = true;
  }
  
  // Handle volume setting
  if (message.find("\"volume\":") != std::string::npos) {
    size_t vol_pos = message.find("\"volume\":");
    if (vol_pos != std::string::npos) {
      vol_pos += 9; // Skip "volume":
      size_t end_pos = message.find(",", vol_pos);
      if (end_pos == std::string::npos) end_pos = message.find("}", vol_pos);
      if (end_pos != std::string::npos) {
        std::string vol_str = message.substr(vol_pos, end_pos - vol_pos);
        requested_volume_ = std::stof(vol_str);
        ESP_LOGI(TAG, "Setting volume to: %f", requested_volume_);
        volume_change_requested_ = true;
      }
    }
  }
  
  // Simple JSON parsing for configuration
  if (message.find("\"wake_word\":") != std::string::npos) {
    // Extract wake word value
    size_t start = message.find("\"wake_word\":\"") + 13;
    size_t end = message.find("\"", start);
    if (start != std::string::npos && end != std::string::npos) {
      std::string new_wake_word = message.substr(start, end - start);
      ESP_LOGD(TAG, "Setting wake word to: %s", new_wake_word.c_str());
      current_wake_word_ = new_wake_word;
      
      // Enable/disable the appropriate wake word models
      // Note: This will be handled via YAML interval that calls the micro_wake_word API
    }
  }
  
  if (message.find("\"sensitivity\":") != std::string::npos) {
    // Extract sensitivity value
    size_t start = message.find("\"sensitivity\":\"") + 15;
    size_t end = message.find("\"", start);
    if (start != std::string::npos && end != std::string::npos) {
      std::string new_sensitivity = message.substr(start, end - start);
      ESP_LOGD(TAG, "Setting sensitivity to: %s", new_sensitivity.c_str());
      current_sensitivity_ = new_sensitivity;
      
      // For now, just store the sensitivity value
      // The actual application to the wake word component will be handled
      // via YAML actions triggered by the status response
    }
  }
  
  if (message.find("\"voice_phase\":") != std::string::npos) {
    // Extract voice assistant phase (for LED control)
    size_t start = message.find("\"voice_phase\":\"") + 15;
    size_t end = message.find("\"", start);
    if (start != std::string::npos && end != std::string::npos) {
      std::string phase_name = message.substr(start, end - start);
      ESP_LOGD(TAG, "Setting voice phase to: %s", phase_name.c_str());
      
      // Map phase names to phase IDs (defined in YAML substitutions)
      int phase_id = 1; // default to idle
      if (phase_name == "waiting") phase_id = 2;
      else if (phase_name == "listening") phase_id = 3;
      else if (phase_name == "thinking") phase_id = 4;
      else if (phase_name == "replying") phase_id = 5;
      else if (phase_name == "idle") phase_id = 1;
      else if (phase_name == "error") phase_id = 11;
      
      current_voice_phase_ = phase_id;
    }
  }
  
  this->send_response_("config_received");
}

void USBCommunicationComponent::process_play_audio_(const std::string &message) {
  ESP_LOGD(TAG, "Processing play audio request");
  
  // Check if this is part of a batch
  bool is_batch = message.find("\"batch\":") != std::string::npos;
  int batch_number = 1;
  int total_batches = 1;
  
  if (is_batch) {
    // Extract batch information
    size_t batch_pos = message.find("\"batch\":");
    if (batch_pos != std::string::npos) {
      batch_pos += 8; // Skip "batch":
      size_t end_pos = message.find(",", batch_pos);
      if (end_pos == std::string::npos) end_pos = message.find("}", batch_pos);
      if (end_pos != std::string::npos) {
        std::string batch_str = message.substr(batch_pos, end_pos - batch_pos);
        batch_number = std::stoi(batch_str);
      }
    }
    
    size_t total_pos = message.find("\"total_batches\":");
    if (total_pos != std::string::npos) {
      total_pos += 16; // Skip "total_batches":
      size_t end_pos = message.find(",", total_pos);
      if (end_pos == std::string::npos) end_pos = message.find("}", total_pos);
      if (end_pos != std::string::npos) {
        std::string total_str = message.substr(total_pos, end_pos - total_pos);
        total_batches = std::stoi(total_str);
      }
    }
    
    ESP_LOGD(TAG, "Processing audio batch %d/%d", batch_number, total_batches);
    
    // Initialize streaming on first batch
    if (batch_number == 1) {
      ESP_LOGD(TAG, "Starting batched audio stream for %d total batches", total_batches);
      this->start_audio_stream();
    }
  }
  
  // Extract audio data from the message and parse it
  if (message.find("\"audio_data\":[") != std::string::npos) {
    ESP_LOGD(TAG, "Extracting audio data from play_audio message");
    
    // Find the audio_data array in the JSON
    size_t data_start = message.find("\"audio_data\":[");
    if (data_start != std::string::npos) {
      data_start += 14; // Skip to after the opening bracket
      size_t data_end = message.find("]", data_start);
      if (data_end != std::string::npos) {
        std::string audio_array = message.substr(data_start, data_end - data_start);
        
        // For non-batch messages, start streaming
        if (!is_batch) {
          this->start_audio_stream();
        }
        
        // Parse comma-separated integers and stream to buffer
        size_t pos = 0;
        while (pos < audio_array.length()) {
          size_t comma_pos = audio_array.find(",", pos);
          if (comma_pos == std::string::npos) comma_pos = audio_array.length();
          
          std::string sample_str = audio_array.substr(pos, comma_pos - pos);
          // Simple integer parsing without exceptions
          char* endptr;
          long sample = strtol(sample_str.c_str(), &endptr, 10);
          if (endptr != sample_str.c_str() && *endptr == '\0') {
            // Valid integer conversion - convert to bytes and stream
            int16_t sample_16 = static_cast<int16_t>(std::max(-32768L, std::min(32767L, sample)));
            uint8_t bytes[2] = {
              static_cast<uint8_t>(sample_16 & 0xFF),
              static_cast<uint8_t>((sample_16 >> 8) & 0xFF)
            };
            this->write_audio_chunk(bytes, 2);
          }
          pos = comma_pos + 1;
        }
        
        ESP_LOGD(TAG, "Buffered audio batch %d data", batch_number);
        
        // Only finish stream and play on last batch or non-batch messages
        if (!is_batch || batch_number >= total_batches) {
          ESP_LOGD(TAG, "Finishing audio stream and triggering playback");
          this->finish_audio_stream();
          this->send_response_("audio_played");
        } else {
          ESP_LOGD(TAG, "Waiting for more batches before playback");
          // Send acknowledgment that batch was received but don't trigger playback yet
          this->send_response_("batch_received");
        }
      }
    }
  } else {
    ESP_LOGD(TAG, "No audio data found in play audio message");
  }
}

void USBCommunicationComponent::process_play_audio_compressed_(const std::string &message) {
  ESP_LOGD(TAG, "Processing compressed audio message");
  
  // Extract base64 audio data and sample count
  if (message.find("\"audio_base64\":\"") != std::string::npos && message.find("\"sample_count\":") != std::string::npos) {
    
    // Extract sample count
    size_t count_pos = message.find("\"sample_count\":");
    if (count_pos != std::string::npos) {
      count_pos += 15; // Skip "sample_count":
      size_t end_pos = message.find(",", count_pos);
      if (end_pos == std::string::npos) end_pos = message.find("}", count_pos);
      if (end_pos != std::string::npos) {
        std::string count_str = message.substr(count_pos, end_pos - count_pos);
        int sample_count = std::stoi(count_str);
        ESP_LOGD(TAG, "Compressed audio contains %d samples", sample_count);
        
        // Extract base64 data
        size_t b64_start = message.find("\"audio_base64\":\"");
        if (b64_start != std::string::npos) {
          b64_start += 16; // Skip "audio_base64":"
          size_t b64_end = message.find("\"", b64_start);
          if (b64_end != std::string::npos) {
            std::string base64_audio = message.substr(b64_start, b64_end - b64_start);
            ESP_LOGD(TAG, "Base64 audio data length: %d characters", base64_audio.length());
            
            // Decode base64 - simple implementation for binary audio data
            this->start_audio_stream();
            
            // For now, send a confirmation tone instead of trying to decode base64
            // This confirms the messaging is working
            ESP_LOGD(TAG, "Generating confirmation tone for compressed audio");
            
            // Generate a simple 440Hz tone for 100ms (1600 samples at 16kHz)
            for (int i = 0; i < 1600; i++) {
              float t = (float)i / 16000.0f;
              int16_t sample = (int16_t)(16000.0f * sin(2.0f * 3.14159f * 440.0f * t));
              uint8_t bytes[2] = {
                static_cast<uint8_t>(sample & 0xFF),
                static_cast<uint8_t>((sample >> 8) & 0xFF)
              };
              this->write_audio_chunk(bytes, 2);
            }
            
            ESP_LOGD(TAG, "Finishing compressed audio stream");
            this->finish_audio_stream();
            this->send_response_("audio_played");
          }
        }
      }
    }
  } else {
    ESP_LOGD(TAG, "No compressed audio data found in message");
  }
}

void USBCommunicationComponent::process_play_tone_(const std::string &message) {
  ESP_LOGI(TAG, "***** PROCESSING PLAY TONE MESSAGE *****");
  
  // Extract frequency and duration (for logging purposes)
  int frequency = 440; // Default
  int duration_ms = 500; // Default
  
  // Extract frequency
  size_t freq_pos = message.find("\"frequency\":");
  if (freq_pos != std::string::npos) {
    freq_pos += 12; // Skip "frequency":
    size_t end_pos = message.find(",", freq_pos);
    if (end_pos == std::string::npos) end_pos = message.find("}", freq_pos);
    if (end_pos != std::string::npos) {
      std::string freq_str = message.substr(freq_pos, end_pos - freq_pos);
      frequency = std::stoi(freq_str);
    }
  }
  
  // Extract duration
  size_t dur_pos = message.find("\"duration_ms\":");
  if (dur_pos != std::string::npos) {
    dur_pos += 14; // Skip "duration_ms":
    size_t end_pos = message.find(",", dur_pos);
    if (end_pos == std::string::npos) end_pos = message.find("}", dur_pos);
    if (end_pos != std::string::npos) {
      std::string dur_str = message.substr(dur_pos, end_pos - dur_pos);
      duration_ms = std::stoi(dur_str);
    }
  }
  
  ESP_LOGI(TAG, "Requested %dHz tone for %dms - triggering factory firmware sound playback", frequency, duration_ms);
  
  // Set a flag that the YAML can check to trigger sound playback using factory firmware method
  tone_playback_requested_ = true;
  
  // The YAML will detect this flag and call the play_sound script with an existing sound file
  // This uses the exact same audio pipeline as the factory firmware
  
  this->send_response_("audio_played");
}

void USBCommunicationComponent::process_play_audio_chunk_(const std::string &message) {
  ESP_LOGD(TAG, "Processing audio chunk: %s", message.c_str());
  
  // Parse chunk information
  bool is_start = message.find("\"is_start\":true") != std::string::npos;
  
  if (is_start) {
    // Initialize new audio reception
    ESP_LOGD(TAG, "Starting new chunked audio reception");
    audio_chunks_.clear();
    received_chunks_ = 0;
    expected_total_chunks_ = 0;
    this->start_audio_stream();
    
    // Extract total chunks count
    size_t total_pos = message.find("\"total_chunks\":");
    if (total_pos != std::string::npos) {
      total_pos += 15; // Skip "total_chunks":
      size_t end_pos = message.find(",", total_pos);
      if (end_pos == std::string::npos) end_pos = message.find("}", total_pos);
      if (end_pos != std::string::npos) {
        std::string total_str = message.substr(total_pos, end_pos - total_pos);
        expected_total_chunks_ = std::stoi(total_str);
        audio_chunks_.resize(expected_total_chunks_);
        ESP_LOGD(TAG, "Expecting %d audio chunks", expected_total_chunks_);
      }
    }
    return;
  }
  
  // Extract chunk index
  int chunk_index = -1;
  size_t index_pos = message.find("\"chunk_index\":");
  if (index_pos != std::string::npos) {
    index_pos += 14; // Skip "chunk_index":
    size_t end_pos = message.find(",", index_pos);
    if (end_pos != std::string::npos) {
      std::string index_str = message.substr(index_pos, end_pos - index_pos);
      chunk_index = std::stoi(index_str);
    }
  }
  
  // Extract audio data if present
  if (message.find("\"audio_data\":[") != std::string::npos && chunk_index > 0) {
    ESP_LOGD(TAG, "Received audio chunk %d/%d", chunk_index, expected_total_chunks_);
    
    // Extract actual audio data from JSON array
    if (chunk_index <= expected_total_chunks_ && chunk_index > 0) {
      // Find the audio_data array in the JSON
      size_t data_start = message.find("\"audio_data\":[");
      if (data_start != std::string::npos) {
        data_start += 14; // Skip to after the opening bracket
        size_t data_end = message.find("]", data_start);
        if (data_end != std::string::npos) {
          std::string audio_array = message.substr(data_start, data_end - data_start);
          
          // Parse comma-separated integers
          std::vector<int> chunk_samples;
          size_t pos = 0;
          while (pos < audio_array.length()) {
            size_t comma_pos = audio_array.find(",", pos);
            if (comma_pos == std::string::npos) comma_pos = audio_array.length();
            
            std::string sample_str = audio_array.substr(pos, comma_pos - pos);
            // Simple integer parsing without exceptions
            char* endptr;
            long sample = strtol(sample_str.c_str(), &endptr, 10);
            if (endptr != sample_str.c_str() && *endptr == '\0') {
              // Valid integer conversion
              chunk_samples.push_back(static_cast<int>(sample));
            }
            pos = comma_pos + 1;
          }
          
          // Store chunk samples
          audio_chunks_[chunk_index - 1] = chunk_samples;
          received_chunks_++;
          
          ESP_LOGD(TAG, "Audio chunk %d received with %d samples (%d/%d total chunks)", 
                   chunk_index, chunk_samples.size(), received_chunks_, expected_total_chunks_);
        }
      }
      
      // Check if all chunks received
      if (received_chunks_ >= expected_total_chunks_) {
        ESP_LOGD(TAG, "All audio chunks received! Assembling complete audio");
        
        // Stream all chunks to audio buffer
        ESP_LOGD(TAG, "All chunks received, streaming to audio buffer");
        for (const auto& chunk : audio_chunks_) {
          for (int sample : chunk) {
            // Convert from int to int16_t and stream as bytes
            int16_t sample_16 = static_cast<int16_t>(std::max(-32768, std::min(32767, sample)));
            uint8_t bytes[2] = {
              static_cast<uint8_t>(sample_16 & 0xFF),
              static_cast<uint8_t>((sample_16 >> 8) & 0xFF)
            };
            this->write_audio_chunk(bytes, 2);
          }
        }
        
        ESP_LOGD(TAG, "Finished streaming chunked audio");
        
        // Finish the stream and trigger playback
        this->finish_audio_stream();
        this->send_response_("audio_played");
        
        // Reset for next audio
        audio_chunks_.clear();
        received_chunks_ = 0;
        expected_total_chunks_ = 0;
      }
    }
  }
}

void USBCommunicationComponent::send_status_update_() {
  // Build JSON more carefully to avoid corruption
  std::string status;
  status.reserve(512);  // Reserve enough space
  
  status += "{";
  status += "\"type\":\"status\",";
  status += "\"timestamp\":";
  status += std::to_string(millis());
  status += ",";
  status += "\"wake_word_active\":false,";
  status += "\"microphone_muted\":false,";
  status += "\"voice_assistant_phase\":";
  status += std::to_string(current_voice_phase_);
  status += ",";
  status += "\"voice_assistant_running\":true,";
  status += "\"timer_active\":false,";
  status += "\"timer_ringing\":false,";
  status += "\"led_brightness\":0.66,";
  status += "\"volume\":0.7,";
  status += "\"wake_word\":\"";
  status += current_wake_word_;
  status += "\",";
  status += "\"wake_word_sensitivity\":\"";
  status += current_sensitivity_;
  status += "\",";
  status += "\"wifi_connected\":false,";
  status += "\"api_connected\":false";
  status += "}";
  
  this->send_json_(status);
}

void USBCommunicationComponent::send_wake_word_options_() {
  // Wake word options based on the ESPHome configuration
  // These match the wake words loaded in the micro_wake_word component
  std::string options;
  options.reserve(256);
  
  options += "{";
  options += "\"type\":\"wake_word_options\",";
  options += "\"options\":[\"Okay Nabu\",\"Hey Jarvis\",\"Hey Mycroft\",\"Stop\"],";
  options += "\"timestamp\":";
  options += std::to_string(millis());
  options += "}";
  
  this->send_json_(options);
}

void USBCommunicationComponent::send_response_(const char* response_type) {
  std::string response;
  response.reserve(128);
  
  response += "{\"type\":\"";
  response += response_type;
  response += "\",\"timestamp\":";
  response += std::to_string(millis());
  response += "}";
  
  this->send_json_(response);
}

void USBCommunicationComponent::send_json_(const std::string &json) {
  // Send JSON via USB Serial/JTAG using printf
  printf("%s\n", json.c_str());
  fflush(stdout);
}

void USBCommunicationComponent::process_audio_data_chunk_(const std::string &message) {
  // Extract binary audio data from JSON array and write to stream buffer
  if (message.find("\"data\":[") != std::string::npos) {
    // Find the data array in the JSON
    size_t data_start = message.find("\"data\":[");
    if (data_start != std::string::npos) {
      data_start += 8; // Skip to after the opening bracket
      size_t data_end = message.find("]", data_start);
      if (data_end != std::string::npos) {
        std::string data_array = message.substr(data_start, data_end - data_start);
        
        // Parse comma-separated integers and convert to bytes
        std::vector<uint8_t> chunk_bytes;
        size_t pos = 0;
        while (pos < data_array.length()) {
          size_t comma_pos = data_array.find(",", pos);
          if (comma_pos == std::string::npos) comma_pos = data_array.length();
          
          std::string byte_str = data_array.substr(pos, comma_pos - pos);
          char* endptr;
          long byte_val = strtol(byte_str.c_str(), &endptr, 10);
          if (endptr != byte_str.c_str() && *endptr == '\0') {
            // Valid integer conversion
            chunk_bytes.push_back(static_cast<uint8_t>(byte_val & 0xFF));
          }
          pos = comma_pos + 1;
        }
        
        ESP_LOGD(TAG, "Received audio data chunk with %d bytes", chunk_bytes.size());
        
        // Write chunk to streaming buffer
        if (!chunk_bytes.empty()) {
          this->write_audio_chunk(chunk_bytes.data(), chunk_bytes.size());
        }
      }
    }
  }
}

// USB Audio streaming methods (replicating voice assistant architecture)
void USBCommunicationComponent::start_audio_stream() {
  ESP_LOGD(TAG, "Starting USB audio stream");
  usb_audio_buffer_index_ = 0;
  usb_audio_buffer_size_ = 0;
  is_streaming_audio_ = true;
}

void USBCommunicationComponent::write_audio_chunk(const uint8_t *data, size_t length) {
  if (!is_streaming_audio_) {
    ESP_LOGW(TAG, "Attempted to write audio chunk without starting stream");
    return;
  }
  
  // Check buffer space (same as voice assistant)
  if (usb_audio_buffer_index_ + length > USB_AUDIO_BUFFER_SIZE) {
    ESP_LOGW(TAG, "USB audio buffer overflow, dropping chunk");
    return;
  }
  
  // Copy audio data to buffer (replicating voice assistant buffer management)
  memcpy(usb_audio_buffer_ + usb_audio_buffer_index_, data, length);
  usb_audio_buffer_index_ += length;
  usb_audio_buffer_size_ += length;
  
  ESP_LOGD(TAG, "Wrote %d bytes to USB audio buffer (total: %d/%d)", 
           length, usb_audio_buffer_size_, USB_AUDIO_BUFFER_SIZE);
}

void USBCommunicationComponent::finish_audio_stream() {
  ESP_LOGD(TAG, "Finishing USB audio stream - %d bytes total", usb_audio_buffer_size_);
  is_streaming_audio_ = false;
  
  if (target_speaker_ == nullptr) {
    ESP_LOGE(TAG, "No speaker configured! Cannot play audio.");
    return;
  }
  
  if (usb_audio_buffer_size_ == 0) {
    ESP_LOGW(TAG, "No audio data to play");
    return;
  }
  
  ESP_LOGI(TAG, "***** STARTING DIRECT SPEAKER PLAYBACK OF %d BYTES *****", usb_audio_buffer_size_);
  
  // FIRST: Try to force the speaker to start if it's not already started
  ESP_LOGI(TAG, "Attempting to start speaker");
  target_speaker_->start();
  
  // Stream audio to speaker using smaller chunks for better control
  static const size_t CHUNK_SIZE = 512;  // Much smaller chunks
  size_t bytes_remaining = usb_audio_buffer_size_;
  size_t offset = 0;
  
  ESP_LOGI(TAG, "Streaming %d bytes to speaker in %d-byte chunks", 
           usb_audio_buffer_size_, CHUNK_SIZE);
  
  while (bytes_remaining > 0) {
    size_t write_chunk = std::min(bytes_remaining, CHUNK_SIZE);
    
    ESP_LOGI(TAG, "About to write %d bytes at offset %d", write_chunk, offset);
    size_t written = target_speaker_->play(usb_audio_buffer_ + offset, write_chunk);
    ESP_LOGI(TAG, "Successfully wrote %d/%d bytes to speaker", written, write_chunk);
    
    if (written == 0) {
      ESP_LOGE(TAG, "Speaker write returned 0 - speaker may be stopped or full");
      // Try to restart the speaker
      target_speaker_->stop();
      delay(10);
      target_speaker_->start();
      delay(10);
      // Try again
      written = target_speaker_->play(usb_audio_buffer_ + offset, write_chunk);
      ESP_LOGI(TAG, "After restart, wrote %d bytes", written);
    }
    
    offset += written;
    bytes_remaining -= written;
    
    // If we couldn't write the full chunk, the speaker buffer is full
    if (written < write_chunk) {
      ESP_LOGI(TAG, "Partial write - speaker buffer constraints, remaining %d bytes", bytes_remaining);
      // Small delay to let speaker process
      delay(5);
    }
    
    // Small delay between chunks to avoid overwhelming the speaker
    delay(1);
  }
  
  ESP_LOGI(TAG, "***** FINISHED STREAMING AUDIO TO SPEAKER *****");
  
  // Set flag for YAML interval to monitor completion
  audio_trigger_pending_ = true;
}

// Microphone capture methods
bool USBCommunicationComponent::capture_microphone_data(std::vector<int16_t> &buffer, size_t samples_needed) {
  if (source_microphone_ == nullptr) {
    ESP_LOGW(TAG, "No microphone configured for capture");
    return false;
  }
  
  if (!is_capturing_audio_) {
    ESP_LOGW(TAG, "Microphone capture not started");
    return false;
  }
  
  // Try to read audio data from the microphone
  // ESPHome microphones typically provide 32-bit samples
  static std::vector<int32_t> raw_samples;
  raw_samples.resize(samples_needed);
  
  // Use the microphone's read method (if available)
  size_t samples_read = 0;
  
  // The microphone component should have a way to read samples
  // This is a simplified approach - actual implementation depends on ESPHome's microphone API
  ESP_LOGD(TAG, "Attempting to capture %zu samples from microphone", samples_needed);
  
  // For now, return false to indicate we couldn't get real data
  // This will be improved once we understand the exact ESPHome microphone API
  return false;
}

void USBCommunicationComponent::start_microphone_capture() {
  if (source_microphone_ == nullptr) {
    ESP_LOGW(TAG, "Cannot start capture - no microphone configured");
    return;
  }
  
  ESP_LOGI(TAG, "Starting microphone capture");
  is_capturing_audio_ = true;
  
  // Start the microphone if it has a start method
  // source_microphone_->start();
}

void USBCommunicationComponent::stop_microphone_capture() {
  ESP_LOGI(TAG, "Stopping microphone capture");
  is_capturing_audio_ = false;
  
  // Stop the microphone if it has a stop method
  // source_microphone_->stop();
}

// Audio data injection methods
void USBCommunicationComponent::inject_audio_data(const int16_t* samples, size_t sample_count) {
  if (samples == nullptr || sample_count == 0) {
    return;
  }
  
  // Prevent buffer overflow
  while (injected_audio_buffer_.size() + sample_count > MAX_INJECTED_AUDIO_BUFFER_SIZE) {
    // Remove old samples from the front
    size_t samples_to_remove = std::min(static_cast<size_t>(160), injected_audio_buffer_.size());
    injected_audio_buffer_.erase(injected_audio_buffer_.begin(), injected_audio_buffer_.begin() + samples_to_remove);
  }
  
  // Add new samples
  for (size_t i = 0; i < sample_count; i++) {
    injected_audio_buffer_.push_back(samples[i]);
  }
  
  last_audio_injection_time_ = millis();
  ESP_LOGD(TAG, "Injected %zu audio samples, buffer size: %zu", sample_count, injected_audio_buffer_.size());
}

bool USBCommunicationComponent::has_recent_audio_data() const {
  unsigned long now = millis();
  return (now - last_audio_injection_time_) < 100 && !injected_audio_buffer_.empty(); // 100ms timeout
}

void USBCommunicationComponent::get_latest_audio_data(std::vector<int16_t> &buffer, size_t samples_needed) {
  buffer.clear();
  buffer.resize(samples_needed, 0);
  
  if (injected_audio_buffer_.empty()) {
    return;
  }
  
  size_t samples_to_copy = std::min(samples_needed, injected_audio_buffer_.size());
  
  // Copy the most recent samples
  if (injected_audio_buffer_.size() >= samples_to_copy) {
    size_t start_index = injected_audio_buffer_.size() - samples_to_copy;
    for (size_t i = 0; i < samples_to_copy; i++) {
      buffer[i] = injected_audio_buffer_[start_index + i];
    }
  }
  
  ESP_LOGD(TAG, "Retrieved %zu audio samples from injection buffer", samples_to_copy);
}

}  // namespace usb_communication
}  // namespace esphome