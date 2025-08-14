#pragma once
#include <vector>

template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(std::size_t n) : data_(n), n_(n) {}
    void push(T v) { data_[idx_] = v; idx_ = (idx_ + 1) % n_; filled_ = filled_ || idx_ == 0; }
    std::size_t size() const { return filled_ ? n_ : idx_; }
    T average() const {
        if (size() == 0) return T{};
        T sum{};
        for (std::size_t i = 0; i < size(); ++i) sum += data_[i];
        return sum / static_cast<T>(size());
    }
private:
    std::vector<T> data_;
    std::size_t n_{};
    std::size_t idx_{};
    bool filled_{};
};
