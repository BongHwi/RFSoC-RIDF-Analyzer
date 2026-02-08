#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <getopt.h>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <TDirectory.h>
#include <TFile.h>
#include <TGraph.h>
#include <TROOT.h>
#include <TAxis.h>
#include <TCanvas.h>
#include <TLegend.h>
#include <TLine.h>
#include <TMarker.h>
#include <TTree.h>

#include "WaveformAnalysis.h"

namespace {

constexpr int EXIT_OK = 0;
constexpr int EXIT_CLI_ERROR = 1;
constexpr int EXIT_FILE_ERROR = 2;
constexpr int EXIT_TREE_ERROR = 3;
constexpr int EXIT_CONFIG_ERROR = 4;

struct EntryKey {
  int evtn = 0;
  int det = 0;
  int ch = 0;

  bool operator<(const EntryKey &other) const {
    return std::tie(evtn, det, ch) < std::tie(other.evtn, other.det, other.ch);
  }
};

void print_usage(const char *progname) {
  std::cout << "Usage: " << progname << " <input.root> [OPTIONS]\n"
            << "Options:\n"
            << "  -o, --output FILE       Output ROOT file (default: analysis_out.root)\n"
            << "  -c, --config FILE       JSON config file\n"
            << "  --generate-template     Generate template config and exit\n"
            << "  -w, --save-waveform     Save baseline-corrected waveform as TGraph\n"
            << "  -n, --maxevt N          Max events to process by unique evtn (-1 = all)\n"
            << "  -b, --batch             Run in batch mode (disable ROOT GUI)\n"
            << "  -h, --help              Show this help\n";
}

void ensureOutputDirectory(TFile *fout, int evtn, int det) {
  const std::string evt_dir = Form("evt_%04d", evtn);
  const std::string det_dir = Form("det_%02d", det);

  fout->cd();
  if (!fout->GetDirectory(evt_dir.c_str())) {
    fout->mkdir(evt_dir.c_str());
  }
  fout->cd(evt_dir.c_str());
  if (!gDirectory->GetDirectory(det_dir.c_str())) {
    gDirectory->mkdir(det_dir.c_str());
  }
  gDirectory->cd(det_dir.c_str());
}

bool hasThreeSigmaSignal(const short *wf, int nsample, float baseline, float baseline_rms) {
  if (wf == nullptr || nsample <= 0 || !(baseline_rms > 0.0f)) {
    return false;
  }

  const double threshold = 3.0 * static_cast<double>(baseline_rms);
  for (int i = 0; i < nsample; i++) {
    const double dev = std::fabs(static_cast<double>(wf[i]) - baseline);
    if (dev >= threshold) {
      return true;
    }
  }
  return false;
}

TCanvas *buildWaveformCanvas(const short *wf, int nsample, const ResolvedAnalysisParams &params,
                             const WaveformAnalysisResult &result, const std::string &name,
                             const std::string &title) {
  TCanvas *c = new TCanvas(name.c_str(), title.c_str(), 1100, 700);

  TGraph *g = new TGraph(nsample);
  g->SetName((name + "_raw").c_str());
  g->SetTitle(title.c_str());

  double ymin = std::numeric_limits<double>::max();
  double ymax = std::numeric_limits<double>::lowest();
  for (int i = 0; i < nsample; i++) {
    const double x = static_cast<double>(i) * params.sample_rate_ns;
    const double y = static_cast<double>(wf[i]);
    ymin = std::min(ymin, y);
    ymax = std::max(ymax, y);
    g->SetPoint(i, x, y);
  }
  if (ymin == ymax) {
    ymin -= 1.0;
    ymax += 1.0;
  }

  c->cd();
  g->SetLineColor(kBlack);
  g->SetLineWidth(2);
  g->Draw("AL");
  g->GetXaxis()->SetTitle("Time (ns)");
  g->GetYaxis()->SetTitle("ADC");

  TLegend *legend = new TLegend(0.62, 0.60, 0.90, 0.90);
  legend->SetBorderSize(0);
  legend->SetFillStyle(0);
  legend->AddEntry(g, "Raw waveform", "l");

  TLine *baseline_line = new TLine(0.0, result.baseline,
                                   static_cast<double>(nsample - 1) * params.sample_rate_ns,
                                   result.baseline);
  baseline_line->SetLineColor(kBlue + 2);
  baseline_line->SetLineStyle(2);
  baseline_line->SetLineWidth(2);
  baseline_line->Draw("same");
  legend->AddEntry(baseline_line, Form("Baseline = %.2f, #sigma = %.2f", result.baseline, result.baseline_rms),
                   "l");

  const bool overlay_cfd =
      hasThreeSigmaSignal(wf, nsample, result.baseline, result.baseline_rms) && result.valid;
  if (overlay_cfd) {
    static constexpr int kPercents[5] = {10, 30, 50, 70, 90};
    static constexpr int kIndices[5] = {0, 2, 4, 6, 8};
    static constexpr int kColors[5] = {kRed + 1, kMagenta + 1, kOrange + 7, kGreen + 2, kCyan + 2};
    static constexpr int kMarkers[5] = {20, 21, 22, 23, 29};

    const double raw_sign = (params.polarity == SignalPolarity::Negative) ? -1.0 : 1.0;
    const double thr_sigma = 3.0 * static_cast<double>(result.baseline_rms);
    legend->AddEntry((TObject *)nullptr, Form("|wf-baseline| >= 3#sigma (%.2f ADC)", thr_sigma), "");

    for (int i = 0; i < 5; i++) {
      const float t_ns = result.cfd_times[kIndices[i]];
      if (t_ns < 0.0f) {
        continue;
      }
      const double y_thr =
          static_cast<double>(result.baseline) +
          raw_sign * static_cast<double>(result.amplitude) * (static_cast<double>(kPercents[i]) / 100.0);

      TLine *vline = new TLine(t_ns, ymin, t_ns, ymax);
      vline->SetLineColor(kColors[i]);
      vline->SetLineStyle(3);
      vline->SetLineWidth(2);
      vline->Draw("same");

      TMarker *mk = new TMarker(t_ns, y_thr, kMarkers[i]);
      mk->SetMarkerColor(kColors[i]);
      mk->SetMarkerSize(1.2);
      mk->Draw("same");

      legend->AddEntry(vline, Form("CFD%d = %.2f ns", kPercents[i], t_ns), "l");
    }
  } else {
    legend->AddEntry((TObject *)nullptr, "No 3#sigma pulse: waveform only", "");
  }

  legend->Draw();
  c->Modified();
  c->Update();
  return c;
}

} // namespace

int main(int argc, char *argv[]) {
  std::string outfile = "analysis_out.root";
  std::string infile;
  std::string config_path;
  bool generate_template = false;
  bool save_waveform = false;
  bool batch_mode = false;
  int maxevt = -1;

  static struct option long_options[] = {{"output", required_argument, 0, 'o'},
                                          {"config", required_argument, 0, 'c'},
                                          {"generate-template", no_argument, 0, 't'},
                                          {"save-waveform", no_argument, 0, 'w'},
                                          {"maxevt", required_argument, 0, 'n'},
                                          {"batch", no_argument, 0, 'b'},
                                          {"help", no_argument, 0, 'h'},
                                          {0, 0, 0, 0}};

  int opt = 0;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "o:c:wn:bh", long_options, &option_index)) != -1) {
    switch (opt) {
    case 'o':
      outfile = optarg;
      break;
    case 'c':
      config_path = optarg;
      break;
    case 't':
      generate_template = true;
      break;
    case 'w':
      save_waveform = true;
      break;
    case 'n':
      maxevt = std::atoi(optarg);
      break;
    case 'b':
      batch_mode = true;
      break;
    case 'h':
      print_usage(argv[0]);
      return EXIT_OK;
    default:
      print_usage(argv[0]);
      return EXIT_CLI_ERROR;
    }
  }

  if (batch_mode) {
    gROOT->SetBatch(kTRUE);
  }

  if (generate_template) {
    const std::string template_path =
        config_path.empty() ? "analyze_waveforms_template.json" : config_path;
    std::string err;
    if (!writeTemplateConfig(template_path, &err)) {
      std::cerr << "Error: " << err << "\n";
      return EXIT_CONFIG_ERROR;
    }
    std::cout << "Template config generated: " << template_path << "\n";
    return EXIT_OK;
  }

  if (optind >= argc) {
    std::cerr << "Error: Input file required\n";
    print_usage(argv[0]);
    return EXIT_CLI_ERROR;
  }
  infile = argv[optind];

  AnalysisConfig config = makeDefaultAnalysisConfig();
  if (!config_path.empty()) {
    std::string err;
    if (!loadAnalysisConfig(config_path, config, &err)) {
      std::cerr << "Error: " << err << "\n";
      return EXIT_CONFIG_ERROR;
    }
  }

  TFile *fin = TFile::Open(infile.c_str(), "READ");
  if (!fin || fin->IsZombie()) {
    std::cerr << "Error: Cannot open input file: " << infile << "\n";
    return EXIT_FILE_ERROR;
  }

  TTree *tree = dynamic_cast<TTree *>(fin->Get("wftree"));
  if (!tree) {
    std::cerr << "Error: TTree 'wftree' not found in " << infile << "\n";
    fin->Close();
    return EXIT_TREE_ERROR;
  }

  Int_t evtn = 0, det = 0, ch = 0, nsample = 0;
  Short_t wf[4096];
  tree->SetBranchAddress("evtn", &evtn);
  tree->SetBranchAddress("det", &det);
  tree->SetBranchAddress("ch", &ch);
  tree->SetBranchAddress("nsample", &nsample);
  tree->SetBranchAddress("wf", wf);

  const Long64_t nentries = tree->GetEntries();

  std::set<int> unique_evtn_set;
  for (Long64_t i = 0; i < nentries; i++) {
    tree->GetEntry(i);
    unique_evtn_set.insert(evtn);
  }

  std::vector<int> selected_evtn(unique_evtn_set.begin(), unique_evtn_set.end());
  std::sort(selected_evtn.begin(), selected_evtn.end());
  if (maxevt > 0 && static_cast<int>(selected_evtn.size()) > maxevt) {
    selected_evtn.resize(maxevt);
  }
  const std::set<int> selected_evtn_set(selected_evtn.begin(), selected_evtn.end());

  std::map<EntryKey, Long64_t> entry_map;
  int skipped_nsample = 0;
  int skipped_ch_out_of_range = 0;
  int duplicate_entries = 0;
  for (Long64_t i = 0; i < nentries; i++) {
    tree->GetEntry(i);
    if (selected_evtn_set.find(evtn) == selected_evtn_set.end()) {
      continue;
    }
    if (nsample <= 0 || nsample > 4096) {
      skipped_nsample++;
      continue;
    }
    if (ch < 0 || ch > 7) {
      skipped_ch_out_of_range++;
      continue;
    }
    EntryKey key{evtn, det, ch};
    if (entry_map.find(key) != entry_map.end()) {
      duplicate_entries++;
    }
    entry_map[key] = i; // last-wins
  }

  TFile *fout = new TFile(outfile.c_str(), "RECREATE");
  if (!fout || fout->IsZombie()) {
    std::cerr << "Error: Cannot create output file: " << outfile << "\n";
    fin->Close();
    return EXIT_FILE_ERROR;
  }

  TTree *analysis_tree = new TTree("analysis_tree", "Waveform analysis results");

  Int_t out_evtn = 0, out_det = 0, out_ch = 0, out_nsample = 0;
  Float_t out_baseline = 0.0f, out_baseline_rms = 0.0f, out_amplitude = 0.0f;
  Int_t out_peak_sample = -1;
  Float_t out_peak_time_ns = -1.0f;
  Float_t out_cfd_time_ns = -1.0f;
  Float_t out_cfd10 = -1.0f, out_cfd20 = -1.0f, out_cfd30 = -1.0f, out_cfd40 = -1.0f, out_cfd50 = -1.0f;
  Float_t out_cfd60 = -1.0f, out_cfd70 = -1.0f, out_cfd80 = -1.0f, out_cfd90 = -1.0f;
  Float_t out_dcfd_time_ns = -1.0f;
  Float_t out_dcfd10 = -1.0f, out_dcfd20 = -1.0f, out_dcfd30 = -1.0f, out_dcfd40 = -1.0f,
          out_dcfd50 = -1.0f;
  Float_t out_dcfd60 = -1.0f, out_dcfd70 = -1.0f, out_dcfd80 = -1.0f, out_dcfd90 = -1.0f;
  Float_t out_risetime = 0.0f;
  Bool_t out_valid = false;

  analysis_tree->Branch("evtn", &out_evtn, "evtn/I");
  analysis_tree->Branch("det", &out_det, "det/I");
  analysis_tree->Branch("ch", &out_ch, "ch/I");
  analysis_tree->Branch("nsample", &out_nsample, "nsample/I");
  analysis_tree->Branch("baseline", &out_baseline, "baseline/F");
  analysis_tree->Branch("baseline_rms", &out_baseline_rms, "baseline_rms/F");
  analysis_tree->Branch("amplitude", &out_amplitude, "amplitude/F");
  analysis_tree->Branch("peak_sample", &out_peak_sample, "peak_sample/I");
  analysis_tree->Branch("peak_time_ns", &out_peak_time_ns, "peak_time_ns/F");
  analysis_tree->Branch("cfd_time_ns", &out_cfd_time_ns, "cfd_time_ns/F");
  analysis_tree->Branch("cfd10", &out_cfd10, "cfd10/F");
  analysis_tree->Branch("cfd20", &out_cfd20, "cfd20/F");
  analysis_tree->Branch("cfd30", &out_cfd30, "cfd30/F");
  analysis_tree->Branch("cfd40", &out_cfd40, "cfd40/F");
  analysis_tree->Branch("cfd50", &out_cfd50, "cfd50/F");
  analysis_tree->Branch("cfd60", &out_cfd60, "cfd60/F");
  analysis_tree->Branch("cfd70", &out_cfd70, "cfd70/F");
  analysis_tree->Branch("cfd80", &out_cfd80, "cfd80/F");
  analysis_tree->Branch("cfd90", &out_cfd90, "cfd90/F");
  analysis_tree->Branch("dcfd_time_ns", &out_dcfd_time_ns, "dcfd_time_ns/F");
  analysis_tree->Branch("dcfd10", &out_dcfd10, "dcfd10/F");
  analysis_tree->Branch("dcfd20", &out_dcfd20, "dcfd20/F");
  analysis_tree->Branch("dcfd30", &out_dcfd30, "dcfd30/F");
  analysis_tree->Branch("dcfd40", &out_dcfd40, "dcfd40/F");
  analysis_tree->Branch("dcfd50", &out_dcfd50, "dcfd50/F");
  analysis_tree->Branch("dcfd60", &out_dcfd60, "dcfd60/F");
  analysis_tree->Branch("dcfd70", &out_dcfd70, "dcfd70/F");
  analysis_tree->Branch("dcfd80", &out_dcfd80, "dcfd80/F");
  analysis_tree->Branch("dcfd90", &out_dcfd90, "dcfd90/F");
  analysis_tree->Branch("risetime", &out_risetime, "risetime/F");
  analysis_tree->Branch("valid", &out_valid, "valid/O");

  int analyzed_count = 0;
  int invalid_count = 0;
  int disabled_count = 0;
  int saved_canvases = 0;
  int processed_unique_events = 0;
  int last_evtn = std::numeric_limits<int>::min();

  for (const auto &kv : entry_map) {
    tree->GetEntry(kv.second);
    const EntryKey &key = kv.first;
    if (key.evtn != last_evtn) {
      last_evtn = key.evtn;
      processed_unique_events++;
      if ((processed_unique_events % 1000) == 0) {
        std::cout << "Processing event " << processed_unique_events
                  << " / " << selected_evtn.size()
                  << " (evtn=" << key.evtn << ")" << std::endl;
      }
    }
    const ResolvedAnalysisParams params = resolveAnalysisParams(config, key.det, key.ch);
    const WaveformAnalysisResult result = analyzeWaveform(wf, nsample, params);

    out_evtn = key.evtn;
    out_det = key.det;
    out_ch = key.ch;
    out_nsample = nsample;
    out_baseline = result.baseline;
    out_baseline_rms = result.baseline_rms;
    out_amplitude = result.amplitude;
    out_peak_sample = result.peak_sample;
    out_peak_time_ns = result.peak_time_ns;
    out_cfd_time_ns = result.cfd_time_ns;
    out_cfd10 = result.cfd_times[0];
    out_cfd20 = result.cfd_times[1];
    out_cfd30 = result.cfd_times[2];
    out_cfd40 = result.cfd_times[3];
    out_cfd50 = result.cfd_times[4];
    out_cfd60 = result.cfd_times[5];
    out_cfd70 = result.cfd_times[6];
    out_cfd80 = result.cfd_times[7];
    out_cfd90 = result.cfd_times[8];
    out_dcfd_time_ns = result.dcfd_time_ns;
    out_dcfd10 = result.dcfd_times[0];
    out_dcfd20 = result.dcfd_times[1];
    out_dcfd30 = result.dcfd_times[2];
    out_dcfd40 = result.dcfd_times[3];
    out_dcfd50 = result.dcfd_times[4];
    out_dcfd60 = result.dcfd_times[5];
    out_dcfd70 = result.dcfd_times[6];
    out_dcfd80 = result.dcfd_times[7];
    out_dcfd90 = result.dcfd_times[8];
    out_risetime = result.risetime;
    out_valid = result.valid;
    analysis_tree->Fill();

    analyzed_count++;
    if (!params.enabled) {
      disabled_count++;
    } else if (!result.valid) {
      invalid_count++;
    }

    if (save_waveform) {
      ensureOutputDirectory(fout, key.evtn, key.det);
      const std::string cname =
          Form("canvas_evt%04d_det%02d_ch%02d", key.evtn, key.det, key.ch);
      const std::string ctitle =
          Form("Evt %d Det %d Ch %d | amp=%.2f cfd%d=%.2fns valid=%d", key.evtn, key.det, key.ch,
               result.amplitude, params.cfd_target_percent, result.cfd_time_ns,
               static_cast<int>(result.valid));
      TCanvas *c = buildWaveformCanvas(wf, nsample, params, result, cname, ctitle);
      c->Write();
      saved_canvases++;
      delete c;
    }
  }

  fout->cd();
  analysis_tree->Write();
  fout->Close();
  fin->Close();

  std::cout << "\nSummary:\n"
            << "  Unique events selected: " << selected_evtn.size() << "\n"
            << "  Input entries scanned: " << nentries << "\n"
            << "  Unique (evtn,det,ch) analyzed: " << analyzed_count << "\n"
            << "  Duplicate entries overwritten (last-wins): " << duplicate_entries << "\n"
            << "  Entries skipped (invalid nsample): " << skipped_nsample << "\n"
            << "  Entries skipped (ch outside 0-7): " << skipped_ch_out_of_range << "\n"
            << "  Disabled by config: " << disabled_count << "\n"
            << "  Invalid analysis results: " << invalid_count << "\n"
            << "  Saved waveform canvases: " << saved_canvases << "\n"
            << "Output written to: " << outfile << "\n";

  delete fin;
  delete fout;
  return EXIT_OK;
}
