#ifndef MODULES_VIDEO_CODING_HISTOGRAM_H_
#define MODULES_VIDEO_CODING_HISTOGRAM_H_

#include <cstddef>
#include <vector>

namespace xrtc {
namespace video_coding {
class Histogram {
 public:
  // A discrete histogram where every bucket with range [0, num_buckets).
  // Values greater or equal to num_buckets will be placed in the last bucket.
  Histogram(size_t num_buckets, size_t max_num_values);

  // Add a value to the histogram. If there already is max_num_values in the
  // histogram then the oldest value will be replaced with the new value.
  void Add(size_t value);

  // Calculates how many buckets have to be summed in order to accumulate at
  // least the given probability.
  size_t InverseCdf(float probability) const;

  // How many values that make up this histogram.
  size_t NumValues() const;

 private:
  // A circular buffer that holds the values that make up the histogram.
  std::vector<size_t> values_;
  std::vector<size_t> buckets_;
  size_t index_;
};

}  // namespace video_coding
}  // namespace xrtc

#endif  // MODULES_VIDEO_CODING_HISTOGRAM_H_
