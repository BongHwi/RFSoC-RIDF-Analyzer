#include "WaveformAnalysis.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

#include "nlohmann/json.hpp"

namespace {

using json = nlohmann::json;

SignalPolarity parsePolarity(const std::string &value) {
  if (value == "positive") {
    return SignalPolarity::Positive;
  }
  return SignalPolarity::Negative;
}

std::string polarityToString(SignalPolarity polarity) {
  return (polarity == SignalPolarity::Positive) ? "positive" : "negative";
}

void parseConfigNode(const json &node, ConfigNode &cfg_node, const std::string &context = "global") {
  if (node.contains("enabled")) {
    const auto &v = node.at("enabled");
    if (v.is_boolean()) {
      cfg_node.enabled = v.get<bool>();
    } else {
      std::cerr << "Warning [" << context << "]: enabled must be boolean, using default\n";
    }
  }
  if (node.contains("sample_rate_ns")) {
    const auto &v = node.at("sample_rate_ns");
    if (v.is_number()) {
      cfg_node.sample_rate_ns = v.get<double>();
    } else {
      std::cerr << "Warning [" << context << "]: sample_rate_ns must be number, using default\n";
    }
  }
  if (node.contains("polarity")) {
    const auto &v = node.at("polarity");
    if (v.is_string()) {
      cfg_node.polarity = parsePolarity(v.get<std::string>());
    } else {
      std::cerr << "Warning [" << context << "]: polarity must be string, using default\n";
    }
  }
  if (node.contains("baseline_start")) {
    const auto &v = node.at("baseline_start");
    if (v.is_number_integer()) {
      cfg_node.baseline_start = v.get<int>();
    } else {
      std::cerr << "Warning [" << context << "]: baseline_start must be integer, using default\n";
    }
  }
  if (node.contains("baseline_end")) {
    const auto &v = node.at("baseline_end");
    if (v.is_number_integer()) {
      cfg_node.baseline_end = v.get<int>();
    } else {
      std::cerr << "Warning [" << context << "]: baseline_end must be integer, using default\n";
    }
  }
  if (node.contains("ma_window_size")) {
    const auto &v = node.at("ma_window_size");
    if (v.is_number_integer()) {
      cfg_node.ma_window_size = v.get<int>();
    } else {
      std::cerr << "Warning [" << context << "]: ma_window_size must be integer, using default\n";
    }
  }
  if (node.contains("dcfd_enabled")) {
    const auto &v = node.at("dcfd_enabled");
    if (v.is_boolean()) {
      cfg_node.dcfd_enabled = v.get<bool>();
    } else {
      std::cerr << "Warning [" << context << "]: dcfd_enabled must be boolean, using default\n";
    }
  }
  if (node.contains("dcfd_delay")) {
    const auto &v = node.at("dcfd_delay");
    if (v.is_number_integer()) {
      cfg_node.dcfd_delay = v.get<int>();
    } else {
      std::cerr << "Warning [" << context << "]: dcfd_delay must be integer, using default\n";
    }
  }
  if (node.contains("dcfd_fraction")) {
    const auto &v = node.at("dcfd_fraction");
    if (v.is_number()) {
      cfg_node.dcfd_fraction = v.get<double>();
    } else {
      std::cerr << "Warning [" << context << "]: dcfd_fraction must be number, using default\n";
    }
  }
}

void applyNode(const ConfigNode &node, ResolvedAnalysisParams &params) {
  if (node.enabled.has_value()) {
    params.enabled = node.enabled.value();
  }
  if (node.sample_rate_ns.has_value()) {
    params.sample_rate_ns = node.sample_rate_ns.value();
  }
  if (node.polarity.has_value()) {
    params.polarity = node.polarity.value();
  }
  if (node.baseline_start.has_value()) {
    params.baseline_start = node.baseline_start.value();
  }
  if (node.baseline_end.has_value()) {
    params.baseline_end = node.baseline_end.value();
  }
  if (node.ma_window_size.has_value()) {
    params.ma_window_size = node.ma_window_size.value();
  }
  if (node.dcfd_enabled.has_value()) {
    params.dcfd_enabled = node.dcfd_enabled.value();
  }
  if (node.dcfd_delay.has_value()) {
    params.dcfd_delay = node.dcfd_delay.value();
  }
  if (node.dcfd_fraction.has_value()) {
    params.dcfd_fraction = node.dcfd_fraction.value();
  }
}

bool sanitizeAnalysisParams(ResolvedAnalysisParams &params) {
  if (params.sample_rate_ns <= 0.0) {
    return false;
  }

  if (params.ma_window_size < 1) {
    params.ma_window_size = 1;
  } else if (params.ma_window_size > 1 && (params.ma_window_size % 2) == 0) {
    params.ma_window_size += 1;
  }

  if (params.dcfd_delay < 1) {
    params.dcfd_delay = 1;
  }

  constexpr double kFractionMin = 0.01;
  constexpr double kFractionMax = 0.99;
  if (params.dcfd_fraction < kFractionMin) {
    params.dcfd_fraction = kFractionMin;
  } else if (params.dcfd_fraction > kFractionMax) {
    params.dcfd_fraction = kFractionMax;
  }

  return true;
}

bool computeBaseline(const short *wf, int nsample, int start, int end, float &baseline, float &rms) {
  if (wf == nullptr || nsample <= 0 || start < 0 || end > nsample || start >= end) {
    return false;
  }

  const int count = end - start;
  double sum = 0.0;
  for (int i = start; i < end; i++) {
    sum += wf[i];
  }
  const double mean = sum / static_cast<double>(count);

  double sqsum = 0.0;
  for (int i = start; i < end; i++) {
    const double d = static_cast<double>(wf[i]) - mean;
    sqsum += d * d;
  }

  baseline = static_cast<float>(mean);
  rms = static_cast<float>(std::sqrt(sqsum / static_cast<double>(count)));
  return true;
}

int findPeakIndex(const std::vector<double> &normalized, double &amplitude) {
  if (normalized.empty()) {
    amplitude = 0.0;
    return -1;
  }

  auto it = std::max_element(normalized.begin(), normalized.end());
  amplitude = *it;
  return static_cast<int>(std::distance(normalized.begin(), it));
}

std::vector<double> applyMovingAverage(const std::vector<double> &input, int window_size) {
  if (window_size <= 1 || input.empty()) {
    return input;
  }

  const int n = static_cast<int>(input.size());
  std::vector<double> output(n);
  const int half_window = window_size / 2;

  for (int i = 0; i < n; i++) {
    const int start = std::max(0, i - half_window);
    const int end = std::min(n, i + half_window + 1);
    double sum = 0.0;
    for (int j = start; j < end; j++) {
      sum += input[j];
    }
    output[i] = sum / static_cast<double>(end - start);
  }
  return output;
}

float computeCFDTime(const std::vector<double> &normalized, int peak_idx, double threshold, double sample_rate_ns) {
  if (peak_idx <= 0 || peak_idx >= static_cast<int>(normalized.size())) {
    return -1.0f;
  }

  for (int i = peak_idx; i > 0; i--) {
    const double v0 = normalized[i - 1];
    const double v1 = normalized[i];
    const bool crossing = (v0 < threshold) && (v1 >= threshold);
    if (!crossing) {
      continue;
    }

    const double denom = v1 - v0;
    if (std::fabs(denom) < 1e-12) {
      return static_cast<float>(i * sample_rate_ns);
    }

    const double frac = (threshold - v0) / denom;
    const double sample_pos = static_cast<double>(i - 1) + frac;
    return static_cast<float>(sample_pos * sample_rate_ns);
  }

  return -1.0f;
}

float computeDCFDTime(const std::vector<double> &normalized, int baseline_end, int peak_idx, int delay,
                      double fraction, double sample_rate_ns) {
  const int n = static_cast<int>(normalized.size());
  const int search_start = std::max(baseline_end, delay);
  const int search_end = std::min(peak_idx, n - 1);

  if (search_start >= search_end) {
    return -1.0f;
  }

  for (int i = search_start; i < search_end; i++) {
    const double y_i = normalized[i] * fraction - normalized[i - delay];
    const double y_ip1 = normalized[i + 1] * fraction - normalized[i + 1 - delay];
    if (y_i > 0.0 && y_ip1 <= 0.0) {
      const double denom = y_i - y_ip1;
      if (std::fabs(denom) < 1e-12) {
        return static_cast<float>(i * sample_rate_ns);
      }
      const double frac_pos = y_i / denom;
      return static_cast<float>((static_cast<double>(i) + frac_pos) * sample_rate_ns);
    }
  }
  return -1.0f;
}

float quietNaN() { return std::numeric_limits<float>::quiet_NaN(); }

} // namespace

AnalysisConfig makeDefaultAnalysisConfig() {
  AnalysisConfig config;
  config.global.enabled = true;
  config.global.sample_rate_ns = 2.0;
  config.global.polarity = SignalPolarity::Negative;
  config.global.baseline_start = 0;
  config.global.baseline_end = 50;

  config.default_detector.enabled = true;
  config.default_detector.polarity = SignalPolarity::Negative;
  config.default_detector.baseline_start = 0;
  config.default_detector.baseline_end = 50;
  return config;
}

bool writeTemplateConfig(const std::string &path, std::string *error_message) {
  json j;
  j["_comment"] = "RFSoC Waveform Analysis Configuration";
  j["global"] = {
      {"sample_rate_ns", 2.0},
      {"polarity", "negative"},
      {"baseline_start", 0},
      {"baseline_end", 50},
      {"ma_window_size", 1},
      {"dcfd_enabled", false},
      {"dcfd_delay", 3},
      {"dcfd_fraction", 0.3},
  };
  j["detectors"]["default"] = {
      {"enabled", true},
      {"polarity", "negative"},
      {"baseline_start", 0},
      {"baseline_end", 50},
  };
  j["detectors"]["1"] = {
      {"polarity", "positive"},
      {"baseline_start", 10},
      {"baseline_end", 60},
      {"channels",
       {
           {"0", {{"baseline_start", 5}, {"baseline_end", 55}}},
           {"2", {{"enabled", false}}},
       }},
  };

  std::ofstream ofs(path);
  if (!ofs) {
    if (error_message != nullptr) {
      *error_message = "Cannot open template output file: " + path;
    }
    return false;
  }

  ofs << j.dump(2) << "\n";
  return true;
}

bool loadAnalysisConfig(const std::string &path, AnalysisConfig &config, std::string *error_message) {
  config = makeDefaultAnalysisConfig();

  std::ifstream ifs(path);
  if (!ifs) {
    if (error_message != nullptr) {
      *error_message = "Cannot open config file: " + path;
    }
    return false;
  }

  json j;
  try {
    ifs >> j;
  } catch (const std::exception &e) {
    if (error_message != nullptr) {
      *error_message = std::string("JSON parse failed: ") + e.what();
    }
    return false;
  }

  try {
    if (j.contains("global")) {
      parseConfigNode(j.at("global"), config.global, "global");
    }

    if (j.contains("detectors")) {
      const json &detectors = j.at("detectors");
      if (detectors.contains("default")) {
        parseConfigNode(detectors.at("default"), config.default_detector, "detectors.default");
      }

      for (auto it = detectors.begin(); it != detectors.end(); ++it) {
        if (it.key() == "default") {
          continue;
        }

        int det_id = 0;
        try {
          det_id = std::stoi(it.key());
        } catch (...) {
          continue;
        }

        DetectorConfigNode det_node;
        parseConfigNode(it.value(), det_node.detector, "detectors." + it.key());
        if (it.value().contains("channels")) {
          const json &channels = it.value().at("channels");
          for (auto ch_it = channels.begin(); ch_it != channels.end(); ++ch_it) {
            int ch_id = 0;
            try {
              ch_id = std::stoi(ch_it.key());
            } catch (...) {
              continue;
            }
            ConfigNode ch_node;
            parseConfigNode(ch_it.value(), ch_node,
                            "detectors." + it.key() + ".channels." + ch_it.key());
            det_node.channels[ch_id] = ch_node;
          }
        }
        config.detectors[det_id] = det_node;
      }
    }
  } catch (const std::exception &e) {
    if (error_message != nullptr) {
      *error_message = std::string("Invalid config schema: ") + e.what();
    }
    return false;
  }

  return true;
}

ResolvedAnalysisParams resolveAnalysisParams(const AnalysisConfig &config, int det, int ch) {
  ResolvedAnalysisParams params;
  params.enabled = true;
  params.sample_rate_ns = 2.0;
  params.polarity = SignalPolarity::Negative;
  params.baseline_start = 0;
  params.baseline_end = 50;
  params.ma_window_size = 1;
  params.dcfd_enabled = false;
  params.dcfd_delay = 3;
  params.dcfd_fraction = 0.3;

  applyNode(config.global, params);
  applyNode(config.default_detector, params);

  auto det_it = config.detectors.find(det);
  if (det_it != config.detectors.end()) {
    applyNode(det_it->second.detector, params);
    auto ch_it = det_it->second.channels.find(ch);
    if (ch_it != det_it->second.channels.end()) {
      applyNode(ch_it->second, params);
    }
  }

  return params;
}

bool validateBaselineRange(const ResolvedAnalysisParams &params, int nsample) {
  return (params.baseline_start >= 0) && (params.baseline_start < params.baseline_end) &&
         (params.baseline_end <= nsample);
}

WaveformAnalysisResult analyzeWaveform(const short *wf, int nsample, const ResolvedAnalysisParams &params) {
  WaveformAnalysisResult out;
  out.cfd_times.fill(-1.0f);
  out.dcfd_time_ns = -1.0f;
  out.risetime = quietNaN();

  ResolvedAnalysisParams safe_params = params;
  if (!sanitizeAnalysisParams(safe_params)) {
    out.baseline = quietNaN();
    out.baseline_rms = quietNaN();
    out.amplitude = quietNaN();
    out.peak_time_ns = quietNaN();
    out.valid = false;
    return out;
  }

  if (!safe_params.enabled || wf == nullptr || nsample <= 0 || !validateBaselineRange(safe_params, nsample)) {
    out.baseline = quietNaN();
    out.baseline_rms = quietNaN();
    out.amplitude = quietNaN();
    out.peak_time_ns = quietNaN();
    out.valid = false;
    return out;
  }

  float baseline = 0.0f;
  float baseline_rms = 0.0f;
  if (!computeBaseline(wf, nsample, safe_params.baseline_start, safe_params.baseline_end, baseline,
                       baseline_rms)) {
    out.baseline = quietNaN();
    out.baseline_rms = quietNaN();
    out.amplitude = quietNaN();
    out.peak_time_ns = quietNaN();
    out.valid = false;
    return out;
  }

  const double sign = (safe_params.polarity == SignalPolarity::Negative) ? -1.0 : 1.0;
  std::vector<double> normalized;
  normalized.reserve(nsample);
  for (int i = 0; i < nsample; i++) {
    normalized.push_back((static_cast<double>(wf[i]) - baseline) * sign);
  }
  if (safe_params.ma_window_size > 1) {
    normalized = applyMovingAverage(normalized, safe_params.ma_window_size);
  }

  double amplitude = 0.0;
  const int peak_idx = findPeakIndex(normalized, amplitude);
  if (peak_idx < 0 || amplitude <= 0.0) {
    out.baseline = baseline;
    out.baseline_rms = baseline_rms;
    out.amplitude = 0.0f;
    out.peak_sample = peak_idx;
    out.peak_time_ns = -1.0f;
    out.valid = false;
    return out;
  }

  static constexpr std::array<int, 9> kCFDPercents = {10, 20, 30, 40, 50, 60, 70, 80, 90};
  for (size_t i = 0; i < kCFDPercents.size(); i++) {
    const double threshold = amplitude * (static_cast<double>(kCFDPercents[i]) / 100.0);
    out.cfd_times[i] = computeCFDTime(normalized, peak_idx, threshold, safe_params.sample_rate_ns);
  }

  if (safe_params.dcfd_enabled && peak_idx > 0) {
    out.dcfd_time_ns =
        computeDCFDTime(normalized, safe_params.baseline_end, peak_idx, safe_params.dcfd_delay,
                        safe_params.dcfd_fraction, safe_params.sample_rate_ns);
  }

  const float cfd10 = out.cfd_times[0];
  const float cfd90 = out.cfd_times[8];
  if (cfd10 >= 0.0f && cfd90 >= 0.0f) {
    out.risetime = cfd90 - cfd10;
  }

  out.baseline = baseline;
  out.baseline_rms = baseline_rms;
  out.amplitude = static_cast<float>(amplitude);
  out.peak_sample = peak_idx;
  out.peak_time_ns = static_cast<float>(peak_idx * safe_params.sample_rate_ns);
  out.valid = true;
  return out;
}
