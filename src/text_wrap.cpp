#include "text_wrap.h"
#include <Arduino.h>
#include <string.h>

// Safe append helpers (avoid snprintf truncation warnings)
static bool appendChar(char* dst, size_t cap, char c)
{
  size_t len = strlen(dst);
  if (len + 1 >= cap)
    return false;
  dst[len] = c;
  dst[len + 1] = '\0';
  return true;
}

static bool appendStr(char* dst, size_t cap, const char* s)
{
  size_t len = strlen(dst);
  size_t add = strlen(s);
  if (len + add >= cap)
    return false;
  memcpy(dst + len, s, add + 1);
  return true;
}

static bool isSpaceByte(uint8_t b)
{
  return b == ' ' || b == '\t' || b == '\n' || b == '\r';
}

// Splits UTF-8 text by ASCII spaces into "words" (keeps UTF-8 letters intact).
// For Russian text it’s enough (spaces are ASCII).
void drawWrappedUTF8(U8G2& u8g2,
                     int x, int y,
                     int w, int h,
                     int lineH,
                     const char* utf8)
{
  if (!utf8 || !*utf8)
    return;

  const int maxLines = (lineH > 0) ? (h / lineH) : 0;
  if (maxLines <= 0)
    return;

  char line[192]; // current line
  char word[96];  // current word
  line[0] = '\0';
  word[0] = '\0';

  int lineIdx = 0;
  int baseline = y;

  auto flushLine = [&]()
  {
    if (line[0] == '\0')
      return;
    if (lineIdx >= maxLines)
      return;
    u8g2.drawUTF8(x, baseline, line);
    baseline += lineH;
    lineIdx++;
    line[0] = '\0';
  };

  const char* p = utf8;
  while (*p && lineIdx < maxLines)
  {
    // Skip multiple spaces
    while (*p && isSpaceByte((uint8_t)*p))
      p++;

    // Extract next word (bytes until next ASCII space)
    word[0] = '\0';
    while (*p && !isSpaceByte((uint8_t)*p))
    {
      // copy byte (UTF-8 safe: we copy raw bytes)
      if (!appendChar(word, sizeof(word), *p))
        break;
      p++;
    }
    if (word[0] == '\0')
      break;

    // Try to place word into current line
    char test[192];
    test[0] = '\0';
    appendStr(test, sizeof(test), line);

    if (test[0] != '\0')
      appendChar(test, sizeof(test), ' ');
    appendStr(test, sizeof(test), word);

    const int px = u8g2.getUTF8Width(test);

    if (px <= w)
    {
      // Accept
      strcpy(line, test);
    }
    else
    {
      // New line
      flushLine();

      // If a single word is wider than the box, hard-break by bytes
      if (u8g2.getUTF8Width(word) > w)
      {
        // Put as much as fits into one line
        char chunk[192];
        chunk[0] = '\0';

        const char* q = word;
        while (*q)
        {
          // Try adding next byte
          char tmp[192];
          tmp[0] = '\0';
          appendStr(tmp, sizeof(tmp), chunk);
          appendChar(tmp, sizeof(tmp), *q);

          if (u8g2.getUTF8Width(tmp) > w)
            break;
          strcpy(chunk, tmp);
          q++;
        }

        if (chunk[0] != '\0')
        {
          u8g2.drawUTF8(x, baseline, chunk);
          baseline += lineH;
          lineIdx++;
        }

        // Remaining part (best effort): continue as new word
        line[0] = '\0';
        if (*q)
        {
          // Put remainder back into line if possible
          strncpy(line, q, sizeof(line) - 1);
          line[sizeof(line) - 1] = '\0';
        }
      }
      else
      {
        // word fits on empty line
        strncpy(line, word, sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';
      }
    }
  }

  // Flush last line
  if (lineIdx < maxLines)
    flushLine();
}
