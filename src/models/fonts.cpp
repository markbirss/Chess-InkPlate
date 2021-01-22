// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __FONTS__ 1
#include "models/fonts.hpp"
#include "models/config.hpp"
#include "viewers/msg_viewer.hpp"
#include "alloc.hpp"

#include <algorithm>

static const char * font_names[7] = {
  "CASE",
  "COND",
  "LEIP",
  "LUCE",
  "MARR",
  "MAYA",
  "MVAL"
};

Fonts::Fonts()
{
  #if USE_EPUB_FONTS
    font_cache.reserve(20);
  #else
    font_cache.reserve(4);
  #endif
}

bool Fonts::setup()
{
  FontEntry font_entry;

  LOG_D("Fonts initialization");

  clear(true);

  std::string def         = "Default";
  std::string draw        = "Drawings";
  std::string ches        = "Chess ";

  std::string drawings    = FONTS_FOLDER "/drawings.otf";
  
  std::string normal      = std::string(FONTS_FOLDER "/CrimsonPro-Regular.otf"   );
  std::string bold        = std::string(FONTS_FOLDER "/CrimsonPro-Bold.otf"      );
  std::string italic      = std::string(FONTS_FOLDER "/CrimsonPro-Italic.otf"    );
  std::string bold_italic = std::string(FONTS_FOLDER "/CrimsonPro-BoldItalic.otf");
  
  
  if (!add(draw, FaceStyle::NORMAL,      drawings   )) return false;
  if (!add(def,  FaceStyle::NORMAL,      normal     )) return false;
  if (!add(def,  FaceStyle::BOLD,        bold       )) return false;
  if (!add(def,  FaceStyle::ITALIC,      italic     )) return false;
  if (!add(def,  FaceStyle::BOLD_ITALIC, bold_italic)) return false; 

  for (int i = 0; i < 7; i++) {
    ches[5] = '1' + i;
    std::string chess       = std::string(FONTS_FOLDER "/").append(font_names[i]).append("FONT.TTF");
    if (!add(ches, FaceStyle::NORMAL, chess)) return false;
  }
  
  return true;
}

Fonts::~Fonts()
{
  for (auto & entry : font_cache) {
    delete entry.font;
  }
  font_cache.clear();
}

void
Fonts::clear(bool all)
{
  // LOG_D("Fonts Clear!");
  // Keep the first 5 fonts as they are reused. Caches will be cleared.
  #if USE_EPUB_FONTS
    int i = 0;
    for (auto & entry : font_cache) {
      if (all || (i >= 5)) delete entry.font;
      else entry.font->clear_cache();
      i++;
    }
    font_cache.resize(all ? 0 : 5);
    font_cache.reserve(20);
  #endif
}

void
Fonts::clear_glyph_caches()
{
  for (auto & entry : font_cache) {
    entry.font->clear_cache();
  }
}

int16_t
Fonts::get_index(const std::string & name, FaceStyle style)
{
  int16_t idx = 0;

  for (auto & entry : font_cache) {
    if ((entry.name.compare(name) == 0) && 
        (entry.style == style)) return idx;
    idx++;
  }
  
  return -1;
}

bool 
Fonts::add(const std::string & name, 
           FaceStyle style,
           const std::string & filename)
{
  // If the font is already loaded, return promptly
  for (auto & font : font_cache) {
    if ((name.compare(font.name) == 0) && 
        (font.style == style)) return true;
  }

  FontEntry f;
  if ((f.font = new TTF(filename))) {
    if (f.font->ready()) {
      f.name                    = name;
      f.font->fonts_cache_index = font_cache.size();
      f.style                   = style;
      font_cache.push_back(f);

      LOG_D("Font %s added to cache at index %d and style %d.",
        f.name.c_str(), 
        f.font->fonts_cache_index,
        (int)f.style);
      return true;
    }
    else {
      delete f.font;
    }
  }
  else {
    LOG_E("Unable to allocate memory.");
    // msg_viewer.out_of_memory("font allocation");
  }

  return false;
}

bool 
Fonts::add(const std::string & name, 
           FaceStyle           style,
           unsigned char *     buffer,
           int32_t             size)
{
  // If the font is already loaded, return promptly
  for (auto & font : font_cache) {
    if ((name.compare(font.name) == 0) && 
        (font.style == style)) return true;
  }

  FontEntry f;

  if ((f.font = new TTF(buffer, size))) {
    if (f.font->ready()) {
      f.name                    = name;
      f.font->fonts_cache_index = font_cache.size();
      f.style                   = style;
      font_cache.push_back(f);

      LOG_D("Font %s added to cache at index %d and style %d.",
        f.name.c_str(), 
        f.font->fonts_cache_index,
        (int)f.style);
      return true;
    }
    else {
      delete f.font;
    }
  }
  else {
    LOG_E("Unable to allocate memory.");
    // msg_viewer.out_of_memory("font allocation");
  }

  return false;
}

Fonts::FaceStyle
Fonts::adjust_font_style(FaceStyle style, FaceStyle font_style, FaceStyle font_weight) const
{
  if (font_style == FaceStyle::ITALIC) { 
    // NORMAL -> ITALIC
    // BOLD -> BOLD_ITALIC
    // ITALIC (no change)
    // BOLD_ITALIC (no change)
    if      (style == FaceStyle::NORMAL) style = FaceStyle::ITALIC;
    else if (style == FaceStyle::BOLD  ) style = FaceStyle::BOLD_ITALIC;
  }
  else if (font_style == FaceStyle::NORMAL) { 
    // NORMAL
    // BOLD
    // ITALIC -> NORMAL
    // BOLD_ITALIC -> BOLD
    if      (style == FaceStyle::BOLD_ITALIC) style = FaceStyle::BOLD;
    else if (style == FaceStyle::ITALIC     ) style = FaceStyle::NORMAL;
  }
  if (font_weight == FaceStyle::BOLD) { 
    // NORMAL -> BOLD
    // BOLD
    // ITALIC -> BOLD_ITALIC
    // BOLD_ITALIC
    if      (style == FaceStyle::ITALIC) style = FaceStyle::BOLD_ITALIC;
    else if (style == FaceStyle::NORMAL) style = FaceStyle::BOLD;
  }
  else if (font_weight == FaceStyle::NORMAL) { 
    // NORMAL
    // BOLD -> NORMAL
    // ITALIC
    // BOLD_ITALIC -> ITALIC
    if      (style == FaceStyle::BOLD       ) style = FaceStyle::NORMAL;
    else if (style == FaceStyle::BOLD_ITALIC) style = FaceStyle::ITALIC;
  }

  return style;
}