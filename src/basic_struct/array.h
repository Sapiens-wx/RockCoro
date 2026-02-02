#pragma once
namespace rockcoro {

template <typename T, size_t CAPACITY> class Array {
public:
    Array()
        : size_(0)
    {
    }

    explicit Array(size_t n)
        : size_(n)
    {
        assert(n <= CAPACITY);
        for (size_t i = 0; i < n; ++i)
            new (data_ + i) T();
    }

    ~Array()
    {
        destroy_all();
    }

    Array(const Array &other)
        : size_(other.size_)
    {
        for (size_t i = 0; i < size_; ++i)
            new (data_ + i) T(other.data_[i]);
    }

    Array &operator=(const Array &other)
    {
        if (this == &other)
            return *this;

        destroy_all();
        size_ = other.size_;

        for (size_t i = 0; i < size_; ++i)
            new (data_ + i) T(other.data_[i]);

        return *this;
    }

    Array(Array &&other) noexcept
        : size_(other.size_)
    {
        for (size_t i = 0; i < size_; ++i)
            new (data_ + i) T(std::move(other.data_[i]));

        other.destroy_all();
        other.size_ = 0;
    }

    Array &operator=(Array &&other) noexcept
    {
        if (this == &other)
            return *this;

        destroy_all();
        size_ = other.size_;

        for (size_t i = 0; i < size_; ++i)
            new (data_ + i) T(std::move(other.data_[i]));

        other.destroy_all();
        other.size_ = 0;

        return *this;
    }

    T &operator[](size_t i)
    {
        assert(i < size_);
        return data_[i];
    }

    const T &operator[](size_t i) const
    {
        assert(i < size_);
        return data_[i];
    }

    // ---------- 容量 ----------
    size_t size() const
    {
        return size_;
    }

    constexpr size_t capacity() const
    {
        return CAPACITY;
    }

    bool empty() const
    {
        return size_ == 0;
    }

    // ---------- 修改 ----------
    void push_back(const T &value)
    {
        assert(size_ < CAPACITY);
        new (data_ + size_) T(value);
        ++size_;
    }

    void push_back(T &&value)
    {
        assert(size_ < CAPACITY);
        new (data_ + size_) T(std::move(value));
        ++size_;
    }

    template <typename... Args> T &emplace_back(Args &&...args)
    {
        assert(size_ < CAPACITY);
        new (data_ + size_) T(std::forward<Args>(args)...);
        return data_[size_++];
    }

    void pop_back()
    {
        assert(size_ > 0);
        data_[--size_].~T();
    }

    void clear()
    {
        destroy_all();
        size_ = 0;
    }

private:
    alignas(T) unsigned char buffer_[sizeof(T) * CAPACITY];
    T *data_ = reinterpret_cast<T *>(buffer_);
    size_t size_;

    void destroy_all()
    {
        for (size_t i = 0; i < size_; ++i)
            data_[i].~T();
    }
};
} // namespace rockcoro