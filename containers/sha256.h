#ifndef __SHA256_H__
#define __SHA256_H__

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

struct Sha256Digest {
  using Byte = std::uint8_t;
  using Count = std::size_t;
  using HexValue = unsigned;

  static constexpr Count Size = 32;
  static constexpr Count HexPerByte = 2;
  static constexpr HexValue HexBase = 16;

  std::array<Byte, Size> bytes{};

  bool operator==(const Sha256Digest &) const = default;

  // Bitcoin muestra los hashes (txid, raiz) con los bytes invertidos
  static Sha256Digest from_display_hex(std::string_view hex) {
    Sha256Digest d;
    for (Count i = 0; i < Size; ++i) {
      const char *first = hex.data() + HexPerByte * i;
      HexValue value = 0;
      std::from_chars(first, first + HexPerByte, value, HexBase);
      d.bytes[Size - 1 - i] = Byte(value);
    }
    return d;
  }

  std::string display_hex() const {
    std::ostringstream os;
    for (auto it = bytes.rbegin(); it != bytes.rend(); ++it)
      os << std::hex << std::setw(HexPerByte) << std::setfill('0') << HexValue(*it);
    return os.str();
  }

  friend std::ostream &operator<<(std::ostream &os, const Sha256Digest &d) {
    auto flags = os.flags();
    for (auto b : d.bytes)
      os << std::hex << std::setw(HexPerByte) << std::setfill('0') << HexValue(b);
    os.flags(flags);
    return os;
  }
};

class Sha256 {
public:
  using Digest = Sha256Digest;
  using Byte = Digest::Byte;
  using Count = Digest::Count;
  using Word = std::uint32_t;
  using BitCount = std::uint64_t;
  using Shift = unsigned;

  static constexpr Count BlockSize = 64;
  static constexpr Count LengthSize = 8;
  static constexpr Count WordSize = sizeof(Word);
  static constexpr Count StateWords = Digest::Size / WordSize;
  static constexpr Count BlockWords = BlockSize / WordSize;
  static constexpr Count Rounds = BlockSize;
  static constexpr Shift BitsPerByte = 8;
  static constexpr Shift BitsPerWord = WordSize * BitsPerByte;
  static constexpr Byte PadMarker = 0x80;
  static constexpr Byte PadFill = 0x00;

  Sha256() { std::copy(std::begin(InitialState), std::end(InitialState), m_h); }

  void update(const void *data, Count len) {
    auto p = static_cast<const Byte *>(data);
    m_total += len;
    while (len) {
      Count n = std::min(len, BlockSize - m_fill);
      std::memcpy(m_buf + m_fill, p, n);
      m_fill += n;
      p += n;
      len -= n;
      if (m_fill == BlockSize) {
        compress(m_buf);
        m_fill = 0;
      }
    }
  }

  Digest finish() {
    BitCount bits = m_total * BitsPerByte;
    update(&PadMarker, 1);
    while (m_fill != BlockSize - LengthSize)
      update(&PadFill, 1);
    Byte len[LengthSize];
    for (Count i = 0; i < LengthSize; ++i)
      len[i] = Byte(bits >> (BitsPerByte * (LengthSize - 1 - i)));
    update(len, LengthSize);
    Digest out;
    for (Count i = 0; i < StateWords; ++i)
      for (Count j = 0; j < WordSize; ++j)
        out.bytes[WordSize * i + j] = Byte(m_h[i] >> (BitsPerByte * (WordSize - 1 - j)));
    return out;
  }

  static Digest hash(const void *data, Count len) {
    Sha256 h;
    h.update(data, len);
    return h.finish();
  }

  static Digest hash(std::string_view s) { return hash(s.data(), s.size()); }

private:
  // Cantidades de rotacion/desplazamiento de la especificacion (FIPS 180-4, seccion 4.1.2)
  struct Rotations {
    Shift first, second, third;
  };
  static constexpr Rotations BigSigma0{2, 13, 22};
  static constexpr Rotations BigSigma1{6, 11, 25};
  static constexpr Rotations SmallSigma0{7, 18, 3};
  static constexpr Rotations SmallSigma1{17, 19, 10};

  static constexpr Word InitialState[StateWords] = {
      0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

  static constexpr Word K[Rounds] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
      0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
      0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
      0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
      0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
      0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

  Word m_h[StateWords];
  Byte m_buf[BlockSize];
  Count m_fill = 0;
  BitCount m_total = 0;

  static Word rotr(Word x, Shift n) { return (x >> n) | (x << (BitsPerWord - n)); }

  static Word big_sigma(Word x, const Rotations &r) {
    return rotr(x, r.first) ^ rotr(x, r.second) ^ rotr(x, r.third);
  }

  static Word small_sigma(Word x, const Rotations &r) {
    return rotr(x, r.first) ^ rotr(x, r.second) ^ (x >> r.third);
  }

  static Word choose(Word e, Word f, Word g) { return (e & f) ^ (~e & g); }
  static Word majority(Word a, Word b, Word c) { return (a & b) ^ (a & c) ^ (b & c); }

  void compress(const Byte *block) {
    Word w[Rounds];
    for (Count i = 0; i < BlockWords; ++i) {
      w[i] = 0;
      for (Count j = 0; j < WordSize; ++j)
        w[i] = w[i] << BitsPerByte | block[WordSize * i + j];
    }
    for (Count i = BlockWords; i < Rounds; ++i)
      w[i] = w[i - 16] + small_sigma(w[i - 15], SmallSigma0) + w[i - 7] +
             small_sigma(w[i - 2], SmallSigma1);
    Word a = m_h[0], b = m_h[1], c = m_h[2], d = m_h[3], e = m_h[4], f = m_h[5],
         g = m_h[6], h = m_h[7];
    for (Count i = 0; i < Rounds; ++i) {
      Word t1 = h + big_sigma(e, BigSigma1) + choose(e, f, g) + K[i] + w[i];
      Word t2 = big_sigma(a, BigSigma0) + majority(a, b, c);
      h = g; g = f; f = e; e = d + t1;
      d = c; c = b; b = a; a = t1 + t2;
    }
    m_h[0] += a; m_h[1] += b; m_h[2] += c; m_h[3] += d;
    m_h[4] += e; m_h[5] += f; m_h[6] += g; m_h[7] += h;
  }
};

// shortcut: T debe ser string-like o trivialmente copiable (bytes crudos), definir otro Hasher para tipos con punteros
template <typename T> struct Sha256MerkleHasher {
  using Digest = Sha256::Digest;

  static constexpr Sha256::Byte LeafTag = 0x00;
  static constexpr Sha256::Byte NodeTag = 0x01;

  static Digest leaf(const T &data) {
    Sha256 h;
    h.update(&LeafTag, 1);
    if constexpr (std::is_convertible_v<const T &, std::string_view>) {
      std::string_view s = data;
      h.update(s.data(), s.size());
    } else {
      static_assert(std::is_trivially_copyable_v<T>);
      h.update(&data, sizeof(T));
    }
    return h.finish();
  }

  static Digest combine(const Digest &left, const Digest &right) {
    Sha256 h;
    h.update(&NodeTag, 1);
    h.update(left.bytes.data(), left.bytes.size());
    h.update(right.bytes.data(), right.bytes.size());
    return h.finish();
  }
};

// Merkle de Bitcoin: hojas = txid ya hasheados, SHA256 doble, y el nodo impar se duplica
struct BitcoinMerkleHasher {
  using Digest = Sha256::Digest;
  static constexpr bool duplicate_odd = true;

  static Digest leaf(const Digest &txid) { return txid; }

  static Digest combine(const Digest &left, const Digest &right) {
    Sha256 first;
    first.update(left.bytes.data(), left.bytes.size());
    first.update(right.bytes.data(), right.bytes.size());
    Digest once = first.finish();
    return Sha256::hash(once.bytes.data(), once.bytes.size());
  }
};

#endif // !__SHA256_H__
