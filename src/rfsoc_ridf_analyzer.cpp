// rfsoc_ridf_analyzer.cpp - Standalone executable for RIDF waveform analysis

#include <cstdlib>
#include <algorithm>
#include <array>
#include <getopt.h>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <TApplication.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TH1.h>
#include <TLatex.h>
#include <TPad.h>
#include <TROOT.h>
#include <TSystem.h>
#include <TTree.h>

#include "RIDFParser.h"

void print_usage(const char *progname) {
  std::cout << "Usage: " << progname << " [OPTIONS] <input.ridf>" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  -o, --output FILE    Output ROOT file (default: rfsoc_ridf_analyzer_out.root)" << std::endl;
  std::cout << "  -n, --maxevt N       Maximum events to process (default: 10000)" << std::endl;
  std::cout << "  -b, --batch          Run in batch mode (no GUI)" << std::endl;
  std::cout << "  -a, --all            Draw all RFSoCs in one monitor canvas (GUI only)" << std::endl;
  std::cout << "                      GUI mode updates waveform monitor per event" << std::endl;
  std::cout << "                      (Enter: next event, q: quit monitor)" << std::endl;
  std::cout << "  -h, --help           Show this help message" << std::endl;
}

using DetectorWaveforms = std::array<std::vector<Short_t>, 8>;
using EventWaveforms = std::map<int, DetectorWaveforms>;

struct MonitorState {
  std::map<int, TCanvas *> det_canvases;
  std::map<int, std::array<TH1S *, 8>> det_hists;
  std::set<int> known_det_ids;
  TCanvas *all_canvas = nullptr;
  int all_canvas_cols = 0;
  int all_canvas_rows = 0;
  std::map<int, TPad *> all_det_outer_pads;
  std::map<int, TPad *> all_det_header_pads;
  std::map<int, TPad *> all_det_grid_pads;
  std::map<int, std::array<TPad *, 8>> all_det_channel_pads;
};

enum class MonitorLayoutMode {
  PerDetCanvas = 0,
  AllDetSingleCanvas = 1
};

void ensure_det_monitor_objects(MonitorState &monitor, int det) {
  if (monitor.det_canvases.find(det) == monitor.det_canvases.end()) {
    TCanvas *canvas = new TCanvas(Form("c_det%d", det), Form("RFSoC %d", det), 1200, 800);
    canvas->Divide(4, 2);
    monitor.det_canvases[det] = canvas;
  }

  if (monitor.det_hists.find(det) == monitor.det_hists.end()) {
    std::array<TH1S *, 8> hists{};
    hists.fill(nullptr);
    monitor.det_hists[det] = hists;
  }
}

void draw_no_data_pad(int det, int ch) {
  gPad->Clear();
  TLatex label;
  label.SetNDC(kTRUE);
  label.SetTextSize(0.08);
  label.DrawLatex(0.28, 0.56, Form("RFSoC %d ch %d", det, ch));
  label.DrawLatex(0.37, 0.40, "No data");
}

void fill_det_channel_pad(MonitorState &monitor, int det, int ch, const DetectorWaveforms *det_wfs) {
  if (det_wfs != nullptr && !det_wfs->at(ch).empty()) {
    std::vector<Short_t> const &samples = det_wfs->at(ch);
    TH1S *&hist = monitor.det_hists[det][ch];
    const int nsample = static_cast<int>(samples.size());

    if (hist == nullptr) {
      hist = new TH1S(Form("h_wf_det%d_ch%d", det, ch),
                      Form("RFSoC %d ch %d;Sample;ADC", det, ch), nsample, 0, nsample);
    } else if (hist->GetNbinsX() != nsample) {
      hist->SetBins(nsample, 0, nsample);
    }

    hist->SetTitle(Form("RFSoC %d ch %d;Sample;ADC", det, ch));
    hist->Reset();
    for (int i = 0; i < nsample; i++) {
      hist->SetBinContent(i + 1, samples[i]);
    }
    hist->SetStats(0);
    hist->Draw("hist");
  } else {
    draw_no_data_pad(det, ch);
  }
}

void update_event_monitor_per_det(MonitorState &monitor, const EventWaveforms &event_waveforms, int evtn) {
  for (const auto &pair : event_waveforms) {
    monitor.known_det_ids.insert(pair.first);
  }

  for (int det : monitor.known_det_ids) {
    ensure_det_monitor_objects(monitor, det);
    TCanvas *canvas = monitor.det_canvases[det];
    canvas->SetTitle(Form("RFSoC %d - Event %d", det, evtn));

    auto det_it = event_waveforms.find(det);
    const DetectorWaveforms *det_wfs = (det_it != event_waveforms.end()) ? &det_it->second : nullptr;

    for (int ch = 0; ch < 8; ch++) {
      canvas->cd(ch + 1);
      fill_det_channel_pad(monitor, det, ch, det_wfs);
    }

    canvas->Modified();
    canvas->Update();
  }

  gSystem->ProcessEvents();
}

void rebuild_all_canvas_layout(MonitorState &monitor, int cols, int rows) {
  monitor.all_canvas->Clear();
  monitor.all_canvas->Divide(cols, rows);
  monitor.all_canvas_cols = cols;
  monitor.all_canvas_rows = rows;
  monitor.all_det_outer_pads.clear();
  monitor.all_det_header_pads.clear();
  monitor.all_det_grid_pads.clear();
  monitor.all_det_channel_pads.clear();

  int det_index = 0;
  for (int det : monitor.known_det_ids) {
    monitor.all_canvas->cd(det_index + 1);
    TPad *outer_pad = static_cast<TPad *>(gPad);
    outer_pad->SetFillColor(0);
    outer_pad->SetLineColor(kBlack);
    outer_pad->SetLineWidth(3);
    outer_pad->SetFrameLineColor(kBlack);
    outer_pad->SetFrameLineWidth(2);
    outer_pad->SetBorderMode(0);
    outer_pad->SetMargin(0.0, 0.0, 0.0, 0.0);
    outer_pad->Clear();

    TPad *header_pad = new TPad(Form("det%d_header", det), "", 0.01, 0.87, 0.99, 0.99);
    header_pad->SetBorderMode(0);
    header_pad->SetFillColor(0);
    header_pad->SetMargin(0.0, 0.0, 0.0, 0.0);
    header_pad->Draw();
    header_pad->cd();

    TLatex det_label;
    det_label.SetNDC(kTRUE);
    det_label.SetTextSize(0.6);
    det_label.SetTextAlign(22);
    det_label.DrawLatex(0.5, 0.5, Form("RFSoC %d", det));

    outer_pad->cd();
    TPad *grid_pad = new TPad(Form("det%d_grid", det), "", 0.01, 0.02, 0.99, 0.86);
    grid_pad->SetBorderMode(0);
    grid_pad->SetFillColor(0);
    grid_pad->SetMargin(0.0, 0.0, 0.0, 0.0);
    grid_pad->Draw();
    grid_pad->Divide(4, 2, 0.001, 0.001);

    std::array<TPad *, 8> ch_pads{};
    ch_pads.fill(nullptr);
    for (int ch = 0; ch < 8; ch++) {
      grid_pad->cd(ch + 1);
      ch_pads[ch] = static_cast<TPad *>(gPad);
    }

    monitor.all_det_outer_pads[det] = outer_pad;
    monitor.all_det_header_pads[det] = header_pad;
    monitor.all_det_grid_pads[det] = grid_pad;
    monitor.all_det_channel_pads[det] = ch_pads;
    det_index++;
  }
}

void redraw_all_canvas_headers(MonitorState &monitor, int evtn) {
  if (monitor.known_det_ids.empty()) {
    return;
  }

  const int first_det = *monitor.known_det_ids.begin();
  for (int det : monitor.known_det_ids) {
    auto header_it = monitor.all_det_header_pads.find(det);
    if (header_it == monitor.all_det_header_pads.end() || header_it->second == nullptr) {
      continue;
    }

    TPad *header_pad = header_it->second;
    header_pad->cd();
    header_pad->Clear();

    TLatex det_label;
    det_label.SetNDC(kTRUE);
    det_label.SetTextSize(0.6);
    det_label.SetTextAlign(22);
    det_label.DrawLatex(0.5, 0.5, Form("RFSoC %d", det));

    if (det == first_det) {
      TLatex evt_label;
      evt_label.SetNDC(kTRUE);
      evt_label.SetTextSize(0.5);
      evt_label.SetTextAlign(12);
      evt_label.DrawLatex(0.02, 0.92, Form("Event %d", evtn));
    }
    header_pad->Modified();
  }
}

void update_event_monitor_all_canvas(MonitorState &monitor, const EventWaveforms &event_waveforms, int evtn) {
  for (const auto &pair : event_waveforms) {
    monitor.known_det_ids.insert(pair.first);
    if (monitor.det_hists.find(pair.first) == monitor.det_hists.end()) {
      std::array<TH1S *, 8> hists{};
      hists.fill(nullptr);
      monitor.det_hists[pair.first] = hists;
    }
  }

  const int ndet = static_cast<int>(monitor.known_det_ids.size());
  if (ndet <= 0) {
    return;
  }

  const int cols = std::min(3, ndet);
  const int rows = static_cast<int>(std::ceil(static_cast<double>(ndet) / cols));

  if (monitor.all_canvas == nullptr) {
    monitor.all_canvas = new TCanvas("c_all_det", "All RFSoCs", 1800, 1000);
  }

  const bool need_layout_rebuild =
      (monitor.all_canvas_cols != cols) || (monitor.all_canvas_rows != rows) ||
      (monitor.all_det_channel_pads.size() != monitor.known_det_ids.size());
  if (need_layout_rebuild) {
    rebuild_all_canvas_layout(monitor, cols, rows);
  }

  for (int det : monitor.known_det_ids) {
    auto det_it = event_waveforms.find(det);
    const DetectorWaveforms *det_wfs = (det_it != event_waveforms.end()) ? &det_it->second : nullptr;
    auto pad_it = monitor.all_det_channel_pads.find(det);
    if (pad_it == monitor.all_det_channel_pads.end()) {
      continue;
    }
    std::array<TPad *, 8> &ch_pads = pad_it->second;

    for (int ch = 0; ch < 8; ch++) {
      if (ch_pads[ch] == nullptr) {
        continue;
      }
      ch_pads[ch]->cd();
      fill_det_channel_pad(monitor, det, ch, det_wfs);
    }
    auto outer_it = monitor.all_det_outer_pads.find(det);
    if (outer_it != monitor.all_det_outer_pads.end() && outer_it->second != nullptr) {
      outer_it->second->Modified();
    }
  }

  redraw_all_canvas_headers(monitor, evtn);
  monitor.all_canvas->Modified();
  monitor.all_canvas->Update();
  gSystem->ProcessEvents();
}

void update_event_monitor(MonitorState &monitor, const EventWaveforms &event_waveforms,
                          MonitorLayoutMode layout_mode, int evtn) {
  if (layout_mode == MonitorLayoutMode::AllDetSingleCanvas) {
    update_event_monitor_all_canvas(monitor, event_waveforms, evtn);
  } else {
    update_event_monitor_per_det(monitor, event_waveforms, evtn);
  }
}

bool wait_for_monitor_input(int shown_evt_count, int evtn) {
  std::cout << "[Monitor] Shown event " << shown_evt_count << " (evtn=" << evtn
            << ")  Enter: next, q: quit > " << std::flush;

  std::string line;
  if (!std::getline(std::cin, line)) {
    std::cout << "\nInput stream closed. Stopping monitor." << std::endl;
    return false;
  }

  return !(line == "q" || line == "Q");
}

void run_analysis(const std::string &infile, int maxevt, const std::string &outfile,
                  bool enable_monitor, MonitorLayoutMode layout_mode) {
  RIDFParser *p = new RIDFParser();
  p->file(infile.c_str());

  TTree *tree = new TTree("wftree", "Waveform Tree");
  Int_t evtn, det, ch, nsample;
  Short_t wf[4096];
  Short_t wf_min, wf_max;
  Float_t wf_mean;

  tree->Branch("evtn", &evtn, "evtn/I");
  tree->Branch("det", &det, "det/I");
  tree->Branch("ch", &ch, "ch/I");
  tree->Branch("nsample", &nsample, "nsample/I");
  tree->Branch("wf", wf, "wf[nsample]/S");
  tree->Branch("wf_min", &wf_min, "wf_min/S");
  tree->Branch("wf_max", &wf_max, "wf_max/S");
  tree->Branch("wf_mean", &wf_mean, "wf_mean/F");

  TH1I *h_adc_dist = new TH1I("h_adc_dist", "ADC Distribution;ADC;Counts", 4096, -2048, 2048);
  TH1I *h_amplitude = new TH1I("h_amplitude", "Amplitude Distribution;Amplitude;Counts", 4096, 0, 4096);
  TH1I *h_nsample = new TH1I("h_nsample", "Number of Samples;Samples;Counts", 5000, 0, 5000);
  MonitorState monitor_state;

  int flag, seg, data[4];
  int total_segments = 0;
  int total_samples = 0;
  int skipped_ch_out_of_range = 0;
  int raw_evt_count = 0;
  int shown_evt_count = 0;
  bool stop_requested = false;

  std::cout << "Analysis start" << std::endl;

  while ((flag = p->nextevt(&evtn)) >= 0) {
    if (raw_evt_count >= maxevt || stop_requested)
      break;
    raw_evt_count++;
    if (flag)
      continue;
    shown_evt_count++;

    EventWaveforms event_waveforms;

    while (!p->nextseg(&seg)) {
      det = p->segdet(seg);
      ch = p->segfp(seg);
      total_segments++;

      int idx = 0;
      while (p->nextdata(seg, data) >= 0) {
        if (idx < 4096) {
          const Short_t raw = static_cast<Short_t>(data[3]);
          wf[idx++] = static_cast<Short_t>(raw >> 4);
        }
      }
      nsample = idx;
      total_samples += nsample;

      if (nsample == 0) {
        continue;
      }
      if (ch < 0 || ch > 7) {
        skipped_ch_out_of_range++;
        continue;
      }

      wf_min = 32767;
      wf_max = -32768;
      float sum = 0;

      for (int i = 0; i < nsample; i++) {
        if (wf[i] < wf_min)
          wf_min = wf[i];
        if (wf[i] > wf_max)
          wf_max = wf[i];
        sum += wf[i];
        h_adc_dist->Fill(wf[i]);
      }
      wf_mean = sum / nsample;

      int amplitude = wf_max - wf_min;
      h_amplitude->Fill(amplitude);
      h_nsample->Fill(nsample);

      tree->Fill();

      if (enable_monitor) {
        event_waveforms[det][ch].assign(wf, wf + nsample);
      }
    }

    if (enable_monitor) {
      update_event_monitor(monitor_state, event_waveforms, layout_mode, evtn);
      if (!wait_for_monitor_input(shown_evt_count, evtn)) {
        stop_requested = true;
      }
    }

    if ((shown_evt_count % 1000) == 0) {
      std::cout << "Processing shown event " << shown_evt_count << " (evtn=" << evtn << ")" << std::endl;
    }
  }

  p->close();
  std::cout << "Analysis done: " << shown_evt_count << " shown events ("
            << raw_evt_count << " raw events), " << total_segments
            << " segments, " << total_samples << " total samples, "
            << skipped_ch_out_of_range << " segments skipped (ch outside 0-7)" << std::endl;

  TFile *fout = new TFile(outfile.c_str(), "RECREATE");
  tree->Write();
  h_adc_dist->Write();
  h_amplitude->Write();
  h_nsample->Write();

  fout->Close();
  std::cout << "Output saved to " << outfile << std::endl;

  delete p;
  delete fout;
}

int main(int argc, char *argv[]) {
  std::string outfile = "rfsoc_ridf_analyzer_out.root";
  int maxevt = 10000;
  bool batch_mode = false;
  bool all_det_in_one_canvas = false;
  std::string infile;

  static struct option long_options[] = {{"output", required_argument, 0, 'o'},
                                          {"maxevt", required_argument, 0, 'n'},
                                          {"batch", no_argument, 0, 'b'},
                                          {"all", no_argument, 0, 'a'},
                                          {"help", no_argument, 0, 'h'},
                                          {0, 0, 0, 0}};

  int opt;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "o:n:bah", long_options, &option_index)) != -1) {
    switch (opt) {
    case 'o':
      outfile = optarg;
      break;
    case 'n':
      maxevt = std::atoi(optarg);
      break;
    case 'b':
      batch_mode = true;
      break;
    case 'a':
      all_det_in_one_canvas = true;
      break;
    case 'h':
      print_usage(argv[0]);
      return 0;
    default:
      print_usage(argv[0]);
      return 1;
    }
  }

  if (optind >= argc) {
    std::cerr << "Error: Input file required" << std::endl;
    print_usage(argv[0]);
    return 1;
  }
  infile = argv[optind];

  if (batch_mode && all_det_in_one_canvas) {
    std::cerr << "Warning: -a/--all is GUI-only and will be ignored in batch mode." << std::endl;
    all_det_in_one_canvas = false;
  }

  TApplication *app = nullptr;
  if (!batch_mode) {
    int root_argc = 1;
    char *root_argv[] = {argv[0], nullptr};
    app = new TApplication("rfsoc_ridf_analyzer", &root_argc, root_argv);
  } else {
    gROOT->SetBatch(kTRUE);
  }

  const MonitorLayoutMode layout_mode =
      all_det_in_one_canvas ? MonitorLayoutMode::AllDetSingleCanvas : MonitorLayoutMode::PerDetCanvas;
  run_analysis(infile, maxevt, outfile, !batch_mode, layout_mode);

  if (!batch_mode) {
    std::cout << "GUI monitor finished." << std::endl;
  }

  if (app != nullptr) {
    delete app;
  }

  return 0;
}
