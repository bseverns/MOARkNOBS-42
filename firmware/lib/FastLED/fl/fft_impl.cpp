#include "fl/fft_impl.h"
#include "fl/fft.h"
#include "fl/audio.h"
#include "fl/span.h"
#include "fl/string.h"
#include "fl/unused.h"

// Minimal FFT shim. The real backend lived in `third_party/cq_kernel`,
// but we kicked that dependency to the curb. This stub keeps the headers
// happy and returns silence for anyone still poking the API.

namespace fl {

class FFTContext {
 public:
  FFTContext(int samples, int bands, float fmin, float fmax, int sample_rate) {
    FASTLED_UNUSED(samples);
    FASTLED_UNUSED(bands);
    FASTLED_UNUSED(fmin);
    FASTLED_UNUSED(fmax);
    FASTLED_UNUSED(sample_rate);
  }

  fl::size sampleSize() const { return 0; }
  fl::string info() const { return fl::string(); }
};

FFTImpl::FFTImpl(const FFT_Args &args) {
  FASTLED_UNUSED(args);
  mContext.reset(new FFTContext(0, 0, 0.0f, 0.0f, 0));
}

FFTImpl::~FFTImpl() = default;

fl::size FFTImpl::sampleSize() const {
  return mContext ? mContext->sampleSize() : 0;
}

fl::string FFTImpl::info() const {
  return mContext ? mContext->info() : fl::string();
}

FFTImpl::Result FFTImpl::run(const AudioSample &sample, FFTBins *out) {
  FASTLED_UNUSED(sample);
  return run(span<const i16>(), out);
}

FFTImpl::Result FFTImpl::run(span<const i16> sample, FFTBins *out) {
  FASTLED_UNUSED(sample);
  if (out) {
    out->clear();
  }
  return FFTImpl::Result(true, "no-op fft");
}

} // namespace fl

