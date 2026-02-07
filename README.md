# RFSoC RIDF Analyzer

RIBF DAQ RIDF data converter and monitor tool based on ROOT.

## Requirements

- Linux environment
- CMake 3.16+
- C++17 compiler (g++)
- ROOT (default path: `/opt/root`)

If ROOT is installed in a custom location, set one of:

- `ROOTSYS` environment variable
- CMake option: `-DROOT_DIR=/path/to/root`

## Project Layout

```
RFSoC-RIDF-Analyzer/
├── CMakeLists.txt
├── include/        # headers
├── src/            # sources
├── lib/            # built shared library (generated)
├── bin/            # built executable (generated)
├── build/          # cmake build directory (generated)
└── README.md
```

## Source Distribution Policy

This repository is managed for source-code distribution.

- Track source and build configs: `CMakeLists.txt`, `include/`, `src/`, `README.md`
- Exclude generated outputs: `build/`, `bin/`, `lib/`
- `.gitignore` already includes these exclusions

## Build

```bash
cd /home/blim/epic/RFSoC/RFSoC-RIDF-Analyzer
mkdir -p build
cd build
cmake ..
cmake --build . -j$(nproc)
```

With custom ROOT path:

```bash
cd /home/blim/epic/RFSoC/RFSoC-RIDF-Analyzer/build
cmake -DROOT_DIR=/opt/root ..
cmake --build . -j$(nproc)
```

## Run

### Help

```bash
/home/blim/epic/RFSoC/RFSoC-RIDF-Analyzer/bin/rfsoc_ridf_analyzer --help
/home/blim/epic/RFSoC/RFSoC-RIDF-Analyzer/bin/export_waveforms --help
```

### Batch mode (recommended for servers)

```bash
cd /home/blim/epic/RFSoC/RFSoC-RIDF-Analyzer
./bin/rfsoc_ridf_analyzer -b -n 1000 data.ridf
```

### GUI mode

```bash
cd /home/blim/epic/RFSoC/RFSoC-RIDF-Analyzer
./bin/rfsoc_ridf_analyzer -n 1000 data.ridf
```

### CLI options

- `-o, --output FILE`: output ROOT file (default: `rfsoc_ridf_analyzer_out.root`)
- `-n, --maxevt N`: maximum number of events (default: `10000`)
- `-b, --batch`: run without GUI
- `-a, --all`: GUI mode only, draw all detectors in a single monitor canvas
- `-h, --help`: show help

## Export Waveforms (`export_waveforms`)

`export_waveforms` reads `wftree` from an `rfsoc_ridf_analyzer` output ROOT file and exports:

- Per-channel `TGraph` objects into a structured ROOT file
- Optional per-graph and summary images (`--pdf`, `--png`)

### Basic usage

```bash
cd /home/blim/epic/RFSoC/RFSoC-RIDF-Analyzer
./bin/export_waveforms rfsoc_ridf_analyzer_out.root
```

### Example with image export

```bash
cd /home/blim/epic/RFSoC/RFSoC-RIDF-Analyzer
./bin/export_waveforms rfsoc_ridf_analyzer_out.root -o waveforms.root --png -n 200
```

### CLI options

- `-o, --output FILE`: output ROOT file (default: `waveforms.root`)
- `-d, --imgdir DIR`: image output directory (default: input basename)
- `--pdf`: export images in PDF format
- `--png`: export images in PNG format
- `-n, --maxevt N`: max unique `evtn` values to process (`-1` = all)
- `-h, --help`: show help

### Output structure

Output ROOT file directory hierarchy:

- `evt_XXXX/seg_XXXXXX/det_XX/`
- Graph name format: `wf_evt%04d_seg%06d_det%02d_ch%02d_idx%zu`
- Summary canvas per detector group: `summary_evt%04d_seg%06d_det%02d`

## Output Validation

Check ROOT output content:

```bash
cd /home/blim/epic/RFSoC/RFSoC-RIDF-Analyzer
root -l -b -q -e 'TFile f("rfsoc_ridf_analyzer_out.root"); f.ls(); TTree *t=(TTree*)f.Get("wftree"); if(t) t->Print();'
```

Expected objects include:

- `wftree`
- `h_adc_dist`
- `h_amplitude`
- `h_nsample`
- `h_wf_seg*` histograms (per segment)

## Clean/Rebuild

```bash
cd /home/blim/epic/RFSoC/RFSoC-RIDF-Analyzer
rm -rf build
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

## Notes

- Old ROOT macro workflow was migrated to executable `src/rfsoc_ridf_analyzer.cpp`.
