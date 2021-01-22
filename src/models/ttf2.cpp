// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define _TTF_ 1
#include "models/ttf2.hpp"
#include "viewers/msg_viewer.hpp"

#include "screen.hpp"
#include "alloc.hpp"

#include <iostream>
#include <ostream>
#include <sys/stat.h>

FT_Library TTF::library{ nullptr };

TTF::TTF(const std::string & filename)
{
  face = nullptr;

  if (library == nullptr) {
    int error = FT_Init_FreeType(& library);
    if (error) {
      LOG_E("An error occurred during FreeType library initialization.");
      std::abort();
    }
  }

  set_font_face_from_file(filename);
  memory_font  = nullptr;
  current_size = -1;
}

TTF::TTF(unsigned char * buffer, int32_t buffer_size)
{
  face = nullptr;
  
  if (library == nullptr) {
    int error = FT_Init_FreeType(& library);
    if (error) {
      LOG_E("An error occurred during FreeType library initialization.");
    }
  }

  set_font_face_from_memory(buffer, buffer_size);
  current_size = -1;
}

TTF::~TTF()
{
  if (face != nullptr) clear_face();
}

void
TTF::add_buff_to_byte_pool()
{
  BytePool * pool = (BytePool *) allocate(BYTE_POOL_SIZE);
  if (pool == nullptr) {
    LOG_E("Unable to allocated memory for bytes pool.");
    msg_viewer.out_of_memory("ttf pool allocation");
  }
  byte_pools.push_front(pool);

  byte_pool_idx = 0;
}

uint8_t * 
TTF::byte_pool_alloc(uint16_t size)
{
  if (size > BYTE_POOL_SIZE) {
    LOG_E("Byte Pool Size NOT BIG ENOUGH!!!");
    std::abort();
  }
  if (byte_pools.empty() || (byte_pool_idx + size) > BYTE_POOL_SIZE) {
    LOG_D("Adding new Byte Pool buffer.");
    add_buff_to_byte_pool();
  }

  uint8_t * buff = &(*byte_pools.front())[byte_pool_idx];
  byte_pool_idx += size;

  return buff;
}

void
TTF::clear_face()
{
  clear_cache();
  if (face != nullptr) FT_Done_Face(face);
  face = nullptr;
  free(memory_font);
  
  current_size = -1;
}

void
TTF::clear_cache()
{
  for (auto const & entry : cache) {
    for (auto const & glyph : entry.second) {
      bitmap_glyph_pool.deleteElement(glyph.second);      
    }
  }

  for (auto * buff : byte_pools) {
    free(buff);
  }
  byte_pools.clear();
  
  cache.clear();
  cache.reserve(50);
}

TTF::BitmapGlyph *
TTF::get_glyph(int32_t charcode, int16_t glyph_size)
{
  return get_glyph_internal(charcode, glyph_size);
}

TTF::BitmapGlyph *
TTF::get_glyph_internal(int32_t charcode, int16_t glyph_size)
{
  int error;

  Glyphs::iterator git;

  if (face == nullptr) return nullptr;

  GlyphsCache::iterator cache_it = cache.find(glyph_size);

  bool found = (cache_it != cache.end()) &&
               ((git = cache_it->second.find(charcode)) != cache_it->second.end());

  if (found) {
    return git->second;
  }
  else {
    if (current_size != glyph_size) set_font_size(glyph_size);

    int glyph_index = FT_Get_Char_Index(face, charcode);
    if (glyph_index == 0) {
      LOG_E("Charcode not found in face: %d, font_index: %d", charcode, fonts_cache_index);
      return nullptr;
    }
    else {
      error = FT_Load_Glyph(
            face,             /* handle to face object */
            glyph_index,      /* glyph index           */
            FT_LOAD_DEFAULT); /* load flags            */
      if (error) {
        LOG_E("Unable to load glyph for charcode: %d", charcode);
        return nullptr;
      }
    }

    BitmapGlyph * glyph = bitmap_glyph_pool.newElement();

    if (glyph == nullptr) {
      LOG_E("Unable to allocate memory for glyph.");
      msg_viewer.out_of_memory("glyph allocation");
    }

    FT_GlyphSlot slot = face->glyph;

    glyph->root       = this;
    glyph->dim.width  = slot->metrics.width  >> 6;
    glyph->dim.height = slot->metrics.height >> 6;


    if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
      if (screen.get_pixel_resolution() == Screen::PixelResolution::ONE_BIT) {
        error = FT_Render_Glyph(face->glyph,            // glyph slot
                                FT_RENDER_MODE_MONO);   // render mode
      }
      else {
        error = FT_Render_Glyph(face->glyph,            // glyph slot
                                FT_RENDER_MODE_NORMAL); // render mode
      }
      if (error) {
        LOG_E("Unable to render glyph for charcode: %d error: %d", charcode, error);
        return nullptr;
      }
    }

    glyph->pitch      = slot->bitmap.pitch;
    glyph->dim.height = slot->bitmap.rows;
    glyph->dim.width  = slot->bitmap.width;

    int32_t size = glyph->pitch * glyph->dim.height;

    if (size > 0) {
      glyph->buffer = byte_pool_alloc(size);

      if (glyph->buffer == nullptr) {
        LOG_E("Unable to allocate memory for glyph.");
        msg_viewer.out_of_memory("glyph allocation");
      }
      // else {
      //   LOG_D("Allocated %d bytes for glyph.", size)
      // }

      memcpy(glyph->buffer, slot->bitmap.buffer, size);
    }
    else {
      glyph->buffer = nullptr;
    }

    glyph->xoff    =  slot->bitmap_left;
    glyph->yoff    = -slot->bitmap_top;
    glyph->advance =  slot->advance.x >>  6;

    // std::cout << "Glyph: " <<
    //   " w:"  << glyph->dim.width <<
    //   " bw:" << slot->bitmap.width <<
    //   " h:"  << glyph->dim.height <<
    //   " br:" << slot->bitmap.rows <<
    //   " p:"  << glyph->pitch <<
    //   " x:"  << glyph->xoff <<
    //   " y:"  << glyph->yoff <<
    //   " a:"  << glyph->advance << std::endl;

    cache[current_size][charcode] = glyph;
    return glyph;
  }
}

bool 
TTF::set_font_size(int16_t size)
{
  int error = FT_Set_Char_Size(
          face,                 // handle to face object
          0,                    // char_width in 1/64th of points
          size * 64,            // char_height in 1/64th of points
          Screen::RESOLUTION,   // horizontal device resolution
          Screen::RESOLUTION);  // vertical device resolution

  if (error) {
    LOG_E("Unable to set font size.");
    return false;
  }

  current_size = size;
  return true;
}

bool 
TTF::set_font_face_from_file(const std::string font_filename)
{
  LOG_D("set_font_face_from_file() ...");

  FILE * font_file;
  if ((font_file = fopen(font_filename.c_str(), "r")) == nullptr) {
    LOG_E("set_font_face_from_file: Unable to open font file '%s'", font_filename.c_str());
    perror("System msg");
    return false;
  }
  else {
    uint8_t * buffer;

    struct stat stat_buf;
    fstat(fileno(font_file), &stat_buf);
    int32_t length = stat_buf.st_size;
    
    LOG_D("Font File Length: %d", length);

    buffer = (uint8_t *) allocate(length + 1);

    if (buffer == nullptr) {
      LOG_E("Unable to allocate font buffer: %d", (int32_t) (length + 1));
      msg_viewer.out_of_memory("font buffer allocation");
    }

    if (fread(buffer, length, 1, font_file) != 1) {
      LOG_E("set_font_face_from_file: Unable to read file content");
      fclose(font_file);
      free(buffer);
      return false;
    }

    fclose(font_file);

    buffer[length] = 0;

    return set_font_face_from_memory(buffer, length);
  }
}

bool 
TTF::set_font_face_from_memory(unsigned char * buffer, int32_t buffer_size)
{
  if (face != nullptr) clear_face();

  int error = FT_New_Memory_Face(library, (const FT_Byte *) buffer, buffer_size, 0, &face);
  if (error) {
    LOG_E("The memory font format is unsupported or is broken.");
    return false;
  }

  memory_font = buffer;
  return true;
}

void
TTF::get_size(const char * str, Dim * dim, int16_t glyph_size)
{
  int16_t max_up   = 0;
  int16_t max_down = 0;

  dim->width  = 0;

  while (*str) {
    BitmapGlyph * glyph = get_glyph_internal(*str++, glyph_size);
    if (glyph) {
      dim->width += glyph->advance;

      int16_t up   = -glyph->yoff;
      int16_t down =  glyph->dim.height + glyph->yoff;
    
      if (up   > max_up  ) max_up   = up;
      if (down > max_down) max_down = down;
    }
  }

  dim->height = max_up + max_down;
}