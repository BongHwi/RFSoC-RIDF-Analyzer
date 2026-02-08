#ifndef WAVEFORM_ANALYSIS_H
#define WAVEFORM_ANALYSIS_H

#include <array>
#include <map>
#include <optional>
#include <string>

enum class SignalPolarity { Positive = 1, Negative = -1 };

struct ConfigNode {
  std::optional<bool> enabled;
  std::optional<double> sample_rate_ns;
  std::optional<SignalPolarity> polarity;
  std::optional<int> baseline_start;
  std::optional<int> baseline_end;
  std::optional<int> ma_window_size;
  std::optional<bool> dcfd_enabled;
  std::optional<int> dcfd_delay;
  std::optional<std::string> cfd_store_mode;
  std::optional<std::string> dcfd_store_mode;
  std::optional<int> cfd_target_percent;
  std::optional<int> dcfd_target_percent;
  std::optional<bool> store_cfd_array;
  std::optional<bool> store_dcfd_array;
  std::optional<double> dcfd_fraction;
};

struct DetectorConfigNode {
  ConfigNode detector;
  std::map<int, ConfigNode> channels;
};

struct AnalysisConfig {
  ConfigNode global;
  ConfigNode default_detector;
  std::map<int, DetectorConfigNode> detectors;
};

struct ResolvedAnalysisParams {
  bool enabled = true;
  double sample_rate_ns = 2.0;
  SignalPolarity polarity = SignalPolarity::Negative;
  int baseline_start = 0;
  int baseline_end = 50;
  int ma_window_size = 1;
  bool dcfd_enabled = false;
  int dcfd_delay = 3;
  std::string cfd_store_mode = "array";
  std::string dcfd_store_mode = "single";
  int cfd_target_percent = 50;
  int dcfd_target_percent = 30;
};

struct WaveformAnalysisResult {
  float baseline = 0.0f;
  float baseline_rms = 0.0f;
  float amplitude = 0.0f;
  int peak_sample = -1;
  float peak_time_ns = -1.0f;
  float cfd_time_ns = -1.0f;
  float dcfd_time_ns = -1.0f;
  std::array<float, 9> cfd_times{};
  std::array<float, 9> dcfd_times{};
  float risetime = 0.0f;
  bool valid = false;

  WaveformAnalysisResult() {
    cfd_times.fill(-1.0f);
    dcfd_times.fill(-1.0f);
  }
};

AnalysisConfig makeDefaultAnalysisConfig();
bool writeTemplateConfig(const std::string &path, std::string *error_message);
bool loadAnalysisConfig(const std::string &path, AnalysisConfig &config, std::string *error_message);

ResolvedAnalysisParams resolveAnalysisParams(const AnalysisConfig &config, int det, int ch);
bool validateBaselineRange(const ResolvedAnalysisParams &params, int nsample);

WaveformAnalysisResult analyzeWaveform(const short *wf, int nsample, const ResolvedAnalysisParams &params);

#endif
