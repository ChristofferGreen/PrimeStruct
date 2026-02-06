#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static const char kData[] =
    "{\"users\":[{\"id\":1,\"name\":\"alice\",\"score\":12},"
    "{\"id\":2,\"name\":\"bob\",\"score\":7},"
    "{\"id\":3,\"name\":\"carol\",\"score\":42}],"
    "\"active\":true,\"count\":3,"
    "\"misc\":{\"neg\":-7,\"escape\":\"a\\n\\\"b\\\"\",\"unicode\":\"\\u0041\",\"zip\":90210},"
    "\"tags\":[\"a\",\"b\",\"c\"]}";

static inline bool isDigit(uint8_t ch) { return ch >= (uint8_t)'0' && ch <= (uint8_t)'9'; }

int main(void) {
  const int64_t outer = 20000;
  const int64_t len = (int64_t)(sizeof(kData) - 1);

  int64_t tokenTotal = 0;
  int64_t numberCount = 0;
  int64_t numberSum = 0;
  int64_t stringBytes = 0;

  for (int64_t o = 0; o < outer; ++o) {
    int64_t localTokens = 0;
    int64_t localNumberCount = 0;
    int64_t localNumberSum = 0;
    int64_t localStringBytes = 0;

    bool inString = false;
    bool escape = false;
    int32_t unicodeDigits = 0;
    bool inNumber = false;
    int64_t sign = 1;
    int64_t value = 0;

    for (int64_t i = 0; i < len; ++i) {
      uint8_t ch = (uint8_t)kData[i];

      if (inString) {
        if (unicodeDigits > 0) {
          unicodeDigits -= 1;
          continue;
        }
        if (escape) {
          escape = false;
          if (ch == (uint8_t)'u') {
            unicodeDigits = 4;
          }
          localStringBytes += 1;
          continue;
        }
        if (ch == (uint8_t)'\\') {
          escape = true;
          continue;
        }
        if (ch == (uint8_t)'"') {
          inString = false;
          continue;
        }
        localStringBytes += 1;
        continue;
      }

      if (inNumber) {
        if (isDigit(ch)) {
          value = value * 10 + (int64_t)(ch - (uint8_t)'0');
          continue;
        }
        localNumberSum += sign * value;
        localNumberCount += 1;
        inNumber = false;
        sign = 1;
        value = 0;
      }

      if (ch == (uint8_t)'"') {
        inString = true;
      } else if (ch == (uint8_t)'-') {
        inNumber = true;
        sign = -1;
        value = 0;
      } else if (isDigit(ch)) {
        inNumber = true;
        sign = 1;
        value = (int64_t)(ch - (uint8_t)'0');
      } else if (ch == (uint8_t)'{' || ch == (uint8_t)'}' || ch == (uint8_t)'[' || ch == (uint8_t)']' ||
                 ch == (uint8_t)':' || ch == (uint8_t)',') {
        localTokens += 1;
      }
    }

    if (inNumber) {
      localNumberSum += sign * value;
      localNumberCount += 1;
    }

    tokenTotal += localTokens;
    numberCount += localNumberCount;
    numberSum += localNumberSum;
    stringBytes += localStringBytes;
  }

  const int64_t output = numberSum + (tokenTotal * 1000000000LL) + (numberCount * 1000000LL) + stringBytes;
  printf("%lld\n", (long long)output);
  return 0;
}

