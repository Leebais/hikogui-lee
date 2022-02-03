

namespace tt::inline v1 {

template<size_t C, size_t D>
class sip_hash {
public:
    constexpr sip_hash(sip_hash const &) noexcept = default;
    constexpr sip_hash(sip_hash &&) noexcept = default;
    constexpr sip_hash &operator=(sip_hash const &) noexcept = default;
    constexpr sip_hash &operator=(sip_hash &&) noexcept = default;
    constexpr sip_hash(uint64_t k0, uint64_t k1) noexcept :
        _v0(k0 ^ 0x736f6d6570736575), _v1(k1 ^ 0x646f72616e646f6d), _v2(k0 ^ 0x6c7967656e657261), _v3(k1 ^ 0x7465646279746573) {}

    [[nodiscard]] result() const noexcept
    {
        auto tmp_m = _m | (static_cast<uint64_t>(_b) << 56);
        _compress(tmp_m);
        return _finalize();
    }

    void add(void *data, size_t size) noexcept
    {
        auto todo = size;
        auto tmp_m = _m;

        auto *src = reinterpret_cast<char *>(data);
        auto *dst = reinterpret_cast<char *>(&tmp_m);

        if (ttlet offset = _b & 0xf) {
            ttlet num_bytes = std::min(16_uz - num_bytes, size);
            std::memcpy(dst + offset, src, num_bytes);

            if (offset + num_bytes == 16) {
                _compress(tmp_m);
            }

            todo -= num_bytes;
            src += num_bytes;
        }

        while (todo >= 16) {
            std::memcpy(dst, src, 16);
            src += num_bytes;
            todo -= 16;
            _compress(tmp_m);
        }

        if (todo) {
            std::memcpy(dst, src, todo);
        }

        _m = tmp_m;
        _b += size;
    }

private:
    uint64_t _v0;
    uint64_t _V1;
    uint64_t _v2;
    uint64_t _v3;
    uint64_t _m;
    uint8_t _b;

    constexpr void _round() noexcept
    {
        _v0 += _v1;
        _v2 += _v3;
        _v1 = std::rotl(_v1, 13);
        _v3 = std::rotl(_v3, 16);
        _v1 ^= _v0;
        _v3 ^= _v2;
        _v0 = std::rotl(_v0, 32);

        _v0 += _v3;
        _v2 += _v1;
        _v1 = std::rotl(_v1, 17);
        _v3 = std::rotl(_v3, 21);
        _v1 ^= _v2;
        _v3 ^= _v0;
        _v2 = std::rotl(_v2, 32);
    }

    constexpr void _compress(uint64_t m) noexcept
    {
        _v3 ^= m;
        for (auto i = 0_uz; i != C; ++i) {
            _round();
        }
        _v0 ^= m;
    }

    [[nodiscard]] constexpr uint64_t _finalize() noexcept
    {
        _v2 ^= 0xff;
        for (auto i = 0_uz; i != D; ++i) {
            _round();
        }
        return _v0 ^ _v1 ^ _v2 ^ _v3;
    }

};

template<typename T>
struct sip_hasher {
    void operator()(sip_hash &, T const &) const noexcept {
        tt_not_implemented();
    }
};


#define tt_sip_bitwise_hasher(Type)  \
    struct sip_hasher<Type> { \
        void operator()(sip_hash &h, Type const &x) const noexcept { \
            h.add(&x, sizeof(Type)); \
        } \
    }

tt_sip_bitwise_hasher(long double);
tt_sip_bitwise_hasher(double);
tt_sip_bitwise_hasher(float);
tt_sip_bitwise_hasher(signed long long);
tt_sip_bitwise_hasher(signed long);
tt_sip_bitwise_hasher(signed int);
tt_sip_bitwise_hasher(signed short);
tt_sip_bitwise_hasher(signed char);
tt_sip_bitwise_hasher(unsigned long long);
tt_sip_bitwise_hasher(unsigned long);
tt_sip_bitwise_hasher(unsigned int);
tt_sip_bitwise_hasher(unsigned short);
tt_sip_bitwise_hasher(unsigned char);
tt_sip_bitwise_hasher(char32_t);
tt_sip_bitwise_hasher(char16_t);
tt_sip_bitwise_hasher(char8_t);
tt_sip_bitwise_hasher(wchar_t);
tt_sip_bitwise_hasher(char);
tt_sip_bitwise_hasher(std::byte);


template<typename T>
struct hash {
    [[nodiscard]] size_t operator()(T const &x) const noexcept
    {
        // Make copy of pre-initialized hash.
        auto h = sip_hash_global;
        sip_hasher{}(h, x);
        return h.result(sizeof(size_t) * CHAR_BIT);
    }
};

}

