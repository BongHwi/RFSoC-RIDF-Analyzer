// export_waveforms.cpp - Export waveforms from rfsoc_ridf_analyzer TTree to TGraph objects

#include <algorithm>
#include <cstdlib>
#include <getopt.h>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1.h>
#include <TROOT.h>
#include <TSystem.h>
#include <TTree.h>

// Exit codes
constexpr int EXIT_OK = 0;
constexpr int EXIT_CLI_ERROR = 1;
constexpr int EXIT_FILE_ERROR = 2;
constexpr int EXIT_TREE_ERROR = 3;

void print_usage(const char *progname) {
  std::cout << "Usage: " << progname << " input.root [OPTIONS]\n"
            << "Options:\n"
            << "  -o, --output FILE   Output ROOT file (default: waveforms.root)\n"
            << "  -d, --imgdir DIR    Image output directory (default: input basename)\n"
            << "  --pdf               Export PDF images\n"
            << "  --png               Export PNG images\n"
            << "  -n, --maxevt N      Max events to process by unique evtn (-1 = all)\n"
            << "  -h, --help          Show this help\n";
}

void createDirIfNotExists(const std::string &path) {
  gSystem->mkdir(path.c_str(), kTRUE);
}

TGraph *makeGraph(Short_t *wf, Int_t nsample, const char *name, const char *title) {
  TGraph *g = new TGraph(nsample);
  g->SetName(name);
  g->SetTitle(title);
  for (int i = 0; i < nsample; i++) {
    g->SetPoint(i, i, wf[i]);
  }
  g->GetXaxis()->SetTitle("Sample");
  g->GetYaxis()->SetTitle("ADC");
  return g;
}

TCanvas *makeSummaryCanvas(std::map<int, TGraph *> &graphs, const char *name,
                           const char *title) {
  TCanvas *c = new TCanvas(name, title, 1200, 800);
  c->Divide(4, 2);
  for (int ch = 0; ch < 8; ch++) {
    c->cd(ch + 1);
    auto it = graphs.find(ch);
    if (it != graphs.end() && it->second != nullptr) {
      it->second->Draw("AL");
    }
  }
  return c;
}

void exportToImage(TCanvas *c, const std::string &path, const std::string &format) {
  std::string fullpath = path + "." + format;
  c->SaveAs(fullpath.c_str());
}

std::string getBasename(const std::string &path) {
  size_t lastSlash = path.find_last_of("/\\");
  std::string filename = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
  size_t lastDot = filename.find_last_of('.');
  return (lastDot == std::string::npos) ? filename : filename.substr(0, lastDot);
}

int main(int argc, char *argv[]) {
  std::string outfile = "waveforms.root";
  std::string imgdir;
  std::string infile;
  bool export_pdf = false;
  bool export_png = false;
  int maxevt = -1;

  static struct option long_options[] = {
      {"output", required_argument, 0, 'o'},
      {"imgdir", required_argument, 0, 'd'},
      {"pdf", no_argument, 0, 'P'},
      {"png", no_argument, 0, 'G'},
      {"maxevt", required_argument, 0, 'n'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  int opt;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "o:d:n:h", long_options, &option_index)) != -1) {
    switch (opt) {
    case 'o':
      outfile = optarg;
      break;
    case 'd':
      imgdir = optarg;
      break;
    case 'P':
      export_pdf = true;
      break;
    case 'G':
      export_png = true;
      break;
    case 'n':
      maxevt = std::atoi(optarg);
      break;
    case 'h':
      print_usage(argv[0]);
      return EXIT_OK;
    default:
      print_usage(argv[0]);
      return EXIT_CLI_ERROR;
    }
  }

  if (optind >= argc) {
    std::cerr << "Error: Input file required\n";
    print_usage(argv[0]);
    return EXIT_CLI_ERROR;
  }
  infile = argv[optind];

  if (imgdir.empty()) {
    imgdir = getBasename(infile);
  }

  gROOT->SetBatch(kTRUE);

  // Open input file
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

  // Setup branches
  Int_t evtn, segid, det, ch, nsample;
  Short_t wf[4096];
  tree->SetBranchAddress("evtn", &evtn);
  tree->SetBranchAddress("segid", &segid);
  tree->SetBranchAddress("det", &det);
  tree->SetBranchAddress("ch", &ch);
  tree->SetBranchAddress("nsample", &nsample);
  tree->SetBranchAddress("wf", wf);

  Long64_t nentries = tree->GetEntries();

  // Statistics
  int skipped_nsample = 0;
  int total_tgraphs = 0;
  int duplicate_entries = 0;
  int summary_canvases = 0;

  // Pass 1: Collect unique evtn values and build eventMap
  std::set<int> unique_evtn_set;
  for (Long64_t i = 0; i < nentries; i++) {
    tree->GetEntry(i);
    unique_evtn_set.insert(evtn);
  }

  // Sort and limit evtn values
  std::vector<int> selected_evtn(unique_evtn_set.begin(), unique_evtn_set.end());
  std::sort(selected_evtn.begin(), selected_evtn.end());
  if (maxevt > 0 && static_cast<int>(selected_evtn.size()) > maxevt) {
    selected_evtn.resize(maxevt);
  }
  std::set<int> selected_evtn_set(selected_evtn.begin(), selected_evtn.end());

  // Build eventMap: (evtn, segid, det, ch) -> vector of entry indices
  using Key = std::tuple<int, int, int, int>;
  std::map<Key, std::vector<Long64_t>> eventMap;

  for (Long64_t i = 0; i < nentries; i++) {
    tree->GetEntry(i);

    // Skip if not in selected events
    if (selected_evtn_set.find(evtn) == selected_evtn_set.end()) {
      continue;
    }

    // Validate nsample
    if (nsample <= 0 || nsample > 4096) {
      std::cerr << "Warning: Invalid nsample=" << nsample << " at entry " << i << ", skipping\n";
      skipped_nsample++;
      continue;
    }

    Key key = std::make_tuple(evtn, segid, det, ch);
    eventMap[key].push_back(i);
  }

  // Count duplicates
  int unique_keys = static_cast<int>(eventMap.size());
  for (const auto &kv : eventMap) {
    if (kv.second.size() > 1) {
      duplicate_entries += static_cast<int>(kv.second.size()) - 1;
    }
  }

  // Open output file
  TFile *fout = new TFile(outfile.c_str(), "RECREATE");
  if (!fout || fout->IsZombie()) {
    std::cerr << "Error: Cannot create output file: " << outfile << "\n";
    fin->Close();
    return EXIT_FILE_ERROR;
  }

  // Pass 2: Create TGraphs organized by (evtn, segid, det)
  // Group by (evtn, segid, det) for summary canvas creation
  using DetKey = std::tuple<int, int, int>;
  std::set<DetKey> detKeys;
  for (const auto &kv : eventMap) {
    detKeys.insert(std::make_tuple(std::get<0>(kv.first), std::get<1>(kv.first),
                                   std::get<2>(kv.first)));
  }

  for (const auto &detKey : detKeys) {
    int evt = std::get<0>(detKey);
    int seg = std::get<1>(detKey);
    int d = std::get<2>(detKey);

    // Create directory hierarchy
    std::string evtDir = Form("evt_%04d", evt);
    std::string segDir = Form("seg_%06d", seg);
    std::string detDir = Form("det_%02d", d);

    fout->cd();
    if (!fout->GetDirectory(evtDir.c_str())) {
      fout->mkdir(evtDir.c_str());
    }
    fout->cd(evtDir.c_str());
    if (!gDirectory->GetDirectory(segDir.c_str())) {
      gDirectory->mkdir(segDir.c_str());
    }
    gDirectory->cd(segDir.c_str());
    if (!gDirectory->GetDirectory(detDir.c_str())) {
      gDirectory->mkdir(detDir.c_str());
    }
    gDirectory->cd(detDir.c_str());

    TDirectory *targetDir = gDirectory;

    // Image directory: group by event/detector so segments are not fragmented into many folders.
    std::string detImgPath = imgdir + "/" + evtDir + "/" + detDir;
    if (export_pdf || export_png) {
      createDirIfNotExists(detImgPath);
    }

    // For summary canvas: store idx0 TGraph for each channel
    std::map<int, TGraph *> summaryGraphs;

    // Collect all channels that exist for this (evtn, segid, det)
    std::set<int> channels;
    for (const auto &kv : eventMap) {
      if (std::get<0>(kv.first) == evt && std::get<1>(kv.first) == seg &&
          std::get<2>(kv.first) == d) {
        channels.insert(std::get<3>(kv.first));
      }
    }

    // Process all channels for this (evtn, segid, det)
    for (int c : channels) {
      Key key = std::make_tuple(evt, seg, d, c);
      auto it = eventMap.find(key);

      const std::vector<Long64_t> &entries = it->second;
      for (size_t idx = 0; idx < entries.size(); idx++) {
        tree->GetEntry(entries[idx]);

        std::string gname = Form("wf_evt%04d_seg%06d_det%02d_ch%02d_idx%zu",
                                 evt, seg, d, c, idx);
        std::string gtitle = Form("Event %d Seg %d Det %d Ch %d Idx %zu",
                                  evt, seg, d, c, idx);

        TGraph *g = makeGraph(wf, nsample, gname.c_str(), gtitle.c_str());
        targetDir->cd();
        g->Write();
        total_tgraphs++;

        // Store idx0 for summary (only ch 0-7 for 2x4 grid)
        if (idx == 0 && c < 8) {
          summaryGraphs[c] = g;
        }

        // Export individual graph image
        if (export_pdf || export_png) {
          std::string chImgPath = detImgPath + "/" + Form("ch_%02d", c);
          createDirIfNotExists(chImgPath);

          TCanvas *gc = new TCanvas("gc", "", 800, 600);
          g->Draw("AL");
          if (export_pdf) {
            exportToImage(gc, chImgPath + "/" + gname, "pdf");
          }
          if (export_png) {
            exportToImage(gc, chImgPath + "/" + gname, "png");
          }
          delete gc;
        }
      }
    }

    // Create summary canvas (using idx0 graphs only)
    if (!summaryGraphs.empty()) {
      std::string sname = Form("summary_evt%04d_seg%06d_det%02d", evt, seg, d);
      std::string stitle = Form("Summary Event %d Seg %d Det %d", evt, seg, d);

      TCanvas *summary = makeSummaryCanvas(summaryGraphs, sname.c_str(), stitle.c_str());
      targetDir->cd();
      summary->Write();
      summary_canvases++;

      if (export_pdf || export_png) {
        if (export_pdf) {
          exportToImage(summary, detImgPath + "/" + sname, "pdf");
        }
        if (export_png) {
          exportToImage(summary, detImgPath + "/" + sname, "png");
        }
      }
      delete summary;
    }
  }

  fout->Close();
  fin->Close();

  // Print summary
  std::cout << "\nSummary:\n"
            << "  Events processed: " << selected_evtn.size() << " (unique evtn values)\n"
            << "  TTree entries scanned: " << nentries << "\n"
            << "  Entries skipped (invalid nsample): " << skipped_nsample << "\n"
            << "  Unique (evt,seg,det,ch) keys: " << unique_keys << "\n"
            << "  TGraph objects created: " << total_tgraphs << "\n"
            << "  Duplicate entries handled: " << duplicate_entries << "\n"
            << "  Summary canvases created: " << summary_canvases << "\n"
            << "\nOutput written to: " << outfile << "\n";

  if (export_pdf || export_png) {
    std::cout << "Images written to: " << imgdir << "/\n";
  }

  delete fin;
  delete fout;

  return EXIT_OK;
}
