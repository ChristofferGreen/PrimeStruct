#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace primec::text_filter {

// The origin of a byte that no source text produced: an inserted call name, parenthesis or type suffix.
constexpr std::uint32_t SynthesizedOrigin = 0xFFFFFFFFu;

// The text a filter reads and writes. A tracking `Text` remembers, for every byte, the offset in the user's source
// the byte was copied from (`SynthesizedOrigin` for a byte the filter wrote), so the offset map from the filtered
// text back to the source falls out of the rewrites with no per-rewrite bookkeeping: slices and appends of tracked
// text keep their origins, whatever the nesting. A text that does not track (the normal run) keeps no origins, so
// the fast path pays one branch per write.
//
// A `Text` is a `std::string` for every read (`find`, `compare`, `[]`, passing it as `const std::string &`). Writes
// go through the members below, which keep the origins in step; a write through a `std::string &` leaves the new
// bytes without an origin, which reads as synthesized.
class Text : public std::string {
public:
  Text() = default;
  Text(const std::string &text) : std::string(text) {
  }
  Text(std::string &&text) : std::string(std::move(text)) {
  }
  Text(const char *text) : std::string(text) {
  }
  Text(std::string_view text) : std::string(text) {
  }

  // `source` with every byte's origin set to its own offset (`track`), or without origins.
  static Text fromSource(const std::string &source, bool track) {
    Text text(source);
    if (track) {
      text.tracking_ = true;
      text.origins_.resize(source.size());
      for (std::size_t i = 0; i < source.size(); ++i) {
        text.origins_[i] = static_cast<std::uint32_t>(i);
      }
    }
    return text;
  }

  bool tracking() const {
    return tracking_;
  }

  // One origin per byte; a byte without an entry is synthesized.
  std::uint32_t originAt(std::size_t index) const {
    return tracking_ && index < origins_.size() ? origins_[index] : SynthesizedOrigin;
  }

  Text substr(std::size_t pos = 0, std::size_t count = npos) const {
    Text slice(std::string::substr(pos, count));
    if (tracking_) {
      slice.tracking_ = true;
      slice.origins_.resize(slice.size(), SynthesizedOrigin);
      for (std::size_t i = 0; i < slice.size(); ++i) {
        slice.origins_[i] = originAt(pos + i);
      }
    }
    return slice;
  }

  void clear() {
    std::string::clear();
    origins_.clear();
  }

  void reserve(std::size_t capacity) {
    std::string::reserve(capacity);
    if (tracking_) {
      origins_.reserve(capacity);
    }
  }

  // Bytes the filter wrote.
  Text &append(std::string_view text) {
    sync();
    std::string::append(text);
    if (tracking_) {
      origins_.resize(size(), SynthesizedOrigin);
    }
    return *this;
  }
  Text &append(const char *text) {
    return append(std::string_view(text));
  }
  Text &append(const std::string &text) {
    return append(std::string_view(text));
  }

  // Bytes copied from `other`, with their origins.
  Text &append(const Text &other) {
    adoptTracking(other);
    sync();
    std::string::append(other);
    if (tracking_) {
      for (std::size_t i = 0; i < other.size(); ++i) {
        origins_.push_back(other.originAt(i));
      }
    }
    return *this;
  }

  void push_back(char c) {
    append(std::string_view(&c, 1));
  }

  // One byte copied from `source`, with its origin.
  void pushCopy(const Text &source, std::size_t index) {
    adoptTracking(source);
    sync();
    std::string::push_back(source[index]);
    if (tracking_) {
      origins_.push_back(source.originAt(index));
    }
  }

  Text &insert(std::size_t pos, const std::string &text) {
    sync();
    std::string::insert(pos, text);
    if (tracking_) {
      origins_.insert(
          origins_.begin() + static_cast<std::ptrdiff_t>(pos), text.size(), SynthesizedOrigin);
    }
    return *this;
  }

  Text &erase(std::size_t pos = 0, std::size_t count = npos) {
    sync();
    const std::size_t before = size();
    std::string::erase(pos, count);
    if (tracking_) {
      const std::size_t removed = before - size();
      origins_.erase(origins_.begin() + static_cast<std::ptrdiff_t>(pos),
                     origins_.begin() + static_cast<std::ptrdiff_t>(pos + removed));
    }
    return *this;
  }

  // Takes the bytes and origins of `other`.
  void swap(Text &other) {
    std::string::swap(other);
    origins_.swap(other.origins_);
    std::swap(tracking_, other.tracking_);
  }

private:
  // A text that has not tracked so far (a default `Text` that a rewrite fills) starts to when it takes bytes from a
  // tracked one; the bytes it already holds are synthesized.
  void adoptTracking(const Text &like) {
    if (like.tracking_ && !tracking_) {
      tracking_ = true;
      origins_.assign(size(), SynthesizedOrigin);
    }
  }

  // Pads the origins of bytes written through the `std::string` interface.
  void sync() {
    if (tracking_ && origins_.size() != size()) {
      origins_.resize(size(), SynthesizedOrigin);
    }
  }

  bool tracking_ = false;
  std::vector<std::uint32_t> origins_;
};

} // namespace primec::text_filter
