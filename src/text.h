#ifndef TEXT_H
#define TEXT_H
static char *text_last(const char *text, char character) {
  const char *last = 0;
  do {
    if (*text == character)
      last = text;
  } while (*text++);
  return (char *)last;
}
#endif
