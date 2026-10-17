# Audio dependencies — local integration

Exact binary and source revisions are recorded in `versions.lock`. The FFmpeg
artifact is a Windows x64 LGPL shared build with GPL and nonfree components
disabled. Its SHA256 also pins the codecs embedded in that artifact. Headers,
import libraries and DLLs come from the same archive; do not mix SDK versions.

The local setup script uses a dated release URL, validates hashes, checks source
commits, and patches two upstream tag references into their locked commit IDs.
It also adds SHA256 verification to the Opus and FLAC ExternalProject downloads.
Those changes are confined to the ignored dependency cache, not upstream repos.

Components linked by this integration:

| Component | Role | License files copied by collect-audio-notices.ps1 |
|---|---|---|
| FFmpeg | decoder, resampling, libavfilter DSP, R128 measurement | LGPL-3.0-or-later, bundled LICENSE.txt |
| OpenAL Soft 1.25.2 | device output, HRTF | COPYING, LICENSE-pffft |
| libiamf | IAMF decoding; invokes OAR internally | LICENSE, PATENTS |
| OAR / bundled OBR | binaural rendering | LICENSE, PATENTS; bundled source notices |
| Abseil | OBR support | Apache-2.0 LICENSE |
| Eigen | OBR linear algebra | COPYING.README, MPL2/BSD/MINPACK notices |
| PFFFT | OBR FFT | copyright/license header retained in notices |
| Opus 1.4 | IAMF Opus codec | COPYING |
| FLAC 1.4.2 | IAMF FLAC codec | COPYING.Xiph and other upstream copying notices |
| FDK-AAC | IAMF AAC codec | NOTICE (includes patent-related terms) |

GoogleTest is a pinned build/test dependency of the bundled OBR project; it is
not intentionally linked into Tonal's playback path. Qt and librespot retain the
project's existing notices.

This work is a local development build. The notices folder is an inventory, not
a completed distribution/commercial licensing assessment. A distributable build
must also preserve relevant notices for all dependencies embedded in the FFmpeg
binary, corresponding source/build information, and applicable replacement or
relinking requirements. Keep shared libraries replaceable. The FFmpeg archive
and dependency sources remain in `out/audio-deps` on this machine.
