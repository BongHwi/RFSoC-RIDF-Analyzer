#include "WaveformAnalysis.h"

#include <algorithm>
#include <cmath>
#include <fstream>
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

void parseConfigNode(const json &node, ConfigNode &cfg_node) {
  if (node.contains("enabled")) {
    cfg_node.enabled = node.at("enabled").get<bool>();
  }
  if (node.contains("sample_rate_ns")) {
    cfg_node.sample_rate_ns = node.at("sample_rate_ns").get<double>();
  }
  if (node.contains("polarity")) {
    cfg_node.polarity = parsePolarity(node.at("polarity").get<std::string>());
  }
  if (node.contains("baseline_start")) {
    cfg_node.baseline_start = node.at("baseline_start").get<int>();
  }
  if (node.contains("baseline_end")) {
    cfg_node.baseline_end = node.at("baseline_end").get<int>();
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
      parseConfigNode(j.at("global"), config.global);
    }

    if (j.contains("detectors")) {
      const json &detectors = j.at("detectors");
      if (detectors.contains("default")) {
        parseConfigNode(detectors.at("default"), config.default_detector);
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
        parseConfigNode(it.value(), det_node.detector);
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
            parseConfigNode(ch_it.value(), ch_node);
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
  out.risetime = quietNaN();

  if (!params.enabled || wf == nullptr || nsample <= 0 || !validateBaselineRange(params, nsample)) {
    out.baseline = quietNaN();
    out.baseline_rms = quietNaN();
    out.amplitude = quietNaN();
    out.peak_time_ns = quietNaN();
    out.valid = false;
    return out;
  }

  float baseline = 0.0f;
  float baseline_rms = 0.0f;
  if (!computeBaseline(wf, nsample, params.baseline_start, params.baseline_end, baseline, baseline_rms)) {
    out.baseline = quietNaN();
    out.baseline_rms = quietNaN();
    out.amplitude = quietNaN();
    out.peak_time_ns = quietNaN();
    out.valid = false;
    return out;
  }

  const double sign = (params.polarity == SignalPolarity::Negative) ? -1.0 : 1.0;
  std::vector<double> normalized;
  normalized.reserve(nsample);
  for (int i = 0; i < nsample; i++) {
    normalized.push_back((static_cast<double>(wf[i]) - baseline) * sign);
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
    out.cfd_times[i] = computeCFDTime(normalized, peak_idx, threshold, params.sample_rate_ns);
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
  out.peak_time_ns = static_cast<float>(peak_idx * params.sample_rate_ns);
  out.valid = true;
  return out;
}
