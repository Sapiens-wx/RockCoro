#pragma once
#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vector>

namespace rockcoro {

template <typename T, size_t BlockSize = 1024> class ChunkedVector {
private:
    std::vector<std::vector<T>> blocks_;
    size_t total_size_ = 0;

    std::pair<size_t, size_t> get_block_and_offset(size_t index) const
    {
        return {index / BlockSize, index % BlockSize};
    }

    void ensure_back_block()
    {
        if (blocks_.empty() || blocks_.back().size() == BlockSize) {
            blocks_.emplace_back();
            blocks_.back().reserve(BlockSize);
        }
    }

public:
    using value_type = T;
    using reference = T &;
    using const_reference = const T &;
    using size_type = size_t;
    using difference_type = ptrdiff_t;

    class iterator {
    private:
        ChunkedVector *container_ = nullptr;
        size_t index_ = 0;

    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T *;
        using reference = T &;

        iterator() = default;
        iterator(ChunkedVector *c, size_t i)
            : container_(c)
            , index_(i)
        {
        }

        reference operator*()
        {
            return (*container_)[index_];
        }
        const_reference operator*() const
        {
            return (*container_)[index_];
        }
        pointer operator->()
        {
            return &(*container_)[index_];
        }

        iterator &operator++()
        {
            ++index_;
            return *this;
        }
        iterator operator++(int)
        {
            auto t = *this;
            ++*this;
            return t;
        }
        iterator &operator--()
        {
            --index_;
            return *this;
        }
        iterator operator--(int)
        {
            auto t = *this;
            --*this;
            return t;
        }

        iterator &operator+=(difference_type n)
        {
            index_ += n;
            return *this;
        }
        iterator &operator-=(difference_type n)
        {
            index_ -= n;
            return *this;
        }

        iterator operator+(difference_type n) const
        {
            return {container_, index_ + n};
        }
        iterator operator-(difference_type n) const
        {
            return {container_, index_ - n};
        }

        difference_type operator-(const iterator &o) const
        {
            return static_cast<difference_type>(index_) - static_cast<difference_type>(o.index_);
        }

        reference operator[](difference_type n)
        {
            return (*container_)[index_ + n];
        }
        const_reference operator[](difference_type n) const
        {
            return (*container_)[index_ + n];
        }

        bool operator==(const iterator &o) const
        {
            return container_ == o.container_ && index_ == o.index_;
        }
        bool operator!=(const iterator &o) const
        {
            return !(*this == o);
        }
        bool operator<(const iterator &o) const
        {
            return index_ < o.index_;
        }
        bool operator>(const iterator &o) const
        {
            return index_ > o.index_;
        }
        bool operator<=(const iterator &o) const
        {
            return index_ <= o.index_;
        }
        bool operator>=(const iterator &o) const
        {
            return index_ >= o.index_;
        }
    };

    class const_iterator {
    private:
        const ChunkedVector *container_ = nullptr;
        size_t index_ = 0;

    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = const T *;
        using reference = const T &;

        const_iterator() = default;
        const_iterator(const ChunkedVector *c, size_t i)
            : container_(c)
            , index_(i)
        {
        }
        const_iterator(const iterator &it)
            : container_(it.container_)
            , index_(it.index_)
        {
        }

        reference operator*() const
        {
            return (*container_)[index_];
        }
        pointer operator->() const
        {
            return &(*container_)[index_];
        }

        const_iterator &operator++()
        {
            ++index_;
            return *this;
        }
        const_iterator operator++(int)
        {
            auto t = *this;
            ++*this;
            return t;
        }
        const_iterator &operator--()
        {
            --index_;
            return *this;
        }
        const_iterator operator--(int)
        {
            auto t = *this;
            --*this;
            return t;
        }

        const_iterator &operator+=(difference_type n)
        {
            index_ += n;
            return *this;
        }
        const_iterator &operator-=(difference_type n)
        {
            index_ -= n;
            return *this;
        }

        const_iterator operator+(difference_type n) const
        {
            return {container_, index_ + n};
        }
        const_iterator operator-(difference_type n) const
        {
            return {container_, index_ - n};
        }

        difference_type operator-(const const_iterator &o) const
        {
            return static_cast<difference_type>(index_) - static_cast<difference_type>(o.index_);
        }

        reference operator[](difference_type n) const
        {
            return (*container_)[index_ + n];
        }

        bool operator==(const const_iterator &o) const
        {
            return container_ == o.container_ && index_ == o.index_;
        }
        bool operator!=(const const_iterator &o) const
        {
            return !(*this == o);
        }
        bool operator<(const const_iterator &o) const
        {
            return index_ < o.index_;
        }
        bool operator>(const const_iterator &o) const
        {
            return index_ > o.index_;
        }
        bool operator<=(const const_iterator &o) const
        {
            return index_ <= o.index_;
        }
        bool operator>=(const const_iterator &o) const
        {
            return index_ >= o.index_;
        }
    };

    ChunkedVector() = default;

    reference operator[](size_t index)
    {
        assert(index < total_size_);
        auto [b, o] = get_block_and_offset(index);
        return blocks_[b][o];
    }

    const_reference operator[](size_t index) const
    {
        assert(index < total_size_);
        auto [b, o] = get_block_and_offset(index);
        return blocks_[b][o];
    }

    reference at(size_t index)
    {
        if (index >= total_size_)
            throw std::out_of_range("out of range");
        return (*this)[index];
    }

    const_reference at(size_t index) const
    {
        if (index >= total_size_)
            throw std::out_of_range("out of range");
        return (*this)[index];
    }

    reference front()
    {
        return (*this)[0];
    }
    const_reference front() const
    {
        return (*this)[0];
    }

    reference back()
    {
        return (*this)[total_size_ - 1];
    }
    const_reference back() const
    {
        return (*this)[total_size_ - 1];
    }

    iterator begin() noexcept
    {
        return {this, 0};
    }
    iterator end() noexcept
    {
        return {this, total_size_};
    }

    const_iterator begin() const noexcept
    {
        return {this, 0};
    }
    const_iterator end() const noexcept
    {
        return {this, total_size_};
    }

    bool empty() const
    {
        return total_size_ == 0;
    }
    size_t size() const
    {
        return total_size_;
    }
    size_t block_count() const noexcept
    {
        return blocks_.size();
    }

    void push_back(const T &v)
    {
        ensure_back_block();
        blocks_.back().push_back(v);
        ++total_size_;
    }

    void push_back(T &&v)
    {
        ensure_back_block();
        blocks_.back().push_back(std::move(v));
        ++total_size_;
    }

    template <typename... Args> void emplace_back(Args &&...args)
    {
        ensure_back_block();
        blocks_.back().emplace_back(std::forward<Args>(args)...);
        ++total_size_;
    }

    void pop_back()
    {
        if (empty())
            return;

        blocks_.back().pop_back();
        --total_size_;

        if (blocks_.back().empty()) {
            blocks_.pop_back();
        }
    }

    void clear()
    {
        blocks_.clear();
        total_size_ = 0;
    }

    void shrink_to_fit()
    {
        for (auto &b : blocks_) {
            b.shrink_to_fit();
        }
        blocks_.shrink_to_fit();
    }

    void resize(size_type n, const T &value = T())
    {
        if (n == total_size_)
            return;

        // shrink
        if (n < total_size_) {
            size_t new_block_count = (n + BlockSize - 1) / BlockSize;

            // shrink last block first
            if (!blocks_.empty()) {
                size_t last_block_idx = n / BlockSize;
                size_t last_block_size = n % BlockSize;

                blocks_[last_block_idx].resize(last_block_size);
            }

            // remove extra blocks
            if (new_block_count < blocks_.size()) {
                blocks_.resize(new_block_count);
            }

            total_size_ = n;
            return;
        }

        // grow
        size_t old_size = total_size_;
        size_t new_block_count = (n + BlockSize - 1) / BlockSize;

        // ensure enough blocks
        if (blocks_.size() < new_block_count) {
            blocks_.resize(new_block_count);
        }

        // fill existing last block
        size_t old_block = old_size / BlockSize;
        size_t old_offset = old_size % BlockSize;

        if (old_block < blocks_.size()) {
            auto &b = blocks_[old_block];
            size_t fill_in_block = std::min(BlockSize - old_offset, n - old_size);

            b.resize(old_offset + fill_in_block, value);

            old_size += fill_in_block;
        }

        // fill full blocks in bulk
        while (old_size + BlockSize <= n) {
            size_t bidx = old_size / BlockSize;
            blocks_[bidx].resize(BlockSize, value);
            old_size += BlockSize;
        }

        // final partial block
        if (old_size < n) {
            size_t bidx = old_size / BlockSize;
            blocks_[bidx].resize(n % BlockSize, value);
        }

        total_size_ = n;
    }
};

} // namespace rockcoro