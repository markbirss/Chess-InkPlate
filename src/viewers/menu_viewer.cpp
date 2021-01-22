// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __MENU_VIEWER__ 1
#include "viewers/menu_viewer.hpp"

#include "models/fonts.hpp"
#include "models/ttf2.hpp"
#include "viewers/page.hpp"
#include "screen.hpp"
#include "controllers/app_controller.hpp"

void MenuViewer::show(MenuEntry * the_menu, uint8_t entry_index, bool clear_screen)
{
  TTF * font = fonts.get(1);

  line_height = font->get_line_height(12);
  text_height = line_height - font->get_descender_height(12); 

  font = fonts.get(0);
  TTF::BitmapGlyph * icon = font->get_glyph('A', 16);

  icon_height = icon->dim.height;

  icon_height   = icon->dim.height;
  icon_ypos     = 10 + icon_height;
  text_ypos     = icon_ypos + line_height + 10;
  region_height = text_ypos + 20;

  Page::Format fmt = {
    .line_height_factor =   1.0,
    .font_index         =     0,
    .font_size          =    16,
    .indent             =     0,
    .margin_left        =     0,
    .margin_right       =     0,
    .margin_top         =     0,
    .margin_bottom      =     0,
    .screen_left        =    10,
    .screen_right       =    10,
    .screen_top         =    10,
    .screen_bottom      =   100,
    .width              =     0,
    .height             =     0,
    .trim               =  true,
    .pre                = false,
    .font_style         = Fonts::FaceStyle::NORMAL,
    .align              = Page::Align::LEFT,
    .text_transform     = Page::TextTransform::NONE
  };

  page.start(fmt);

  page.clear_region(Dim{ Screen::WIDTH, region_height }, Pos{ 0, 0 });

  menu = the_menu;

  uint8_t idx = 0;

  Pos pos(10, icon_ypos);
  
  while ((idx < MAX_MENU_ENTRY) && (menu[idx].icon != Icon::END_MENU)) {

    char ch = icon_char[(int)menu[idx].icon];
    TTF::BitmapGlyph * glyph;
    glyph = font->get_glyph(ch, 16);

    entry_locs[idx].pos.x = pos.x;
    entry_locs[idx].pos.y = pos.y + glyph->yoff;
    entry_locs[idx].dim   = glyph->dim;

    page.put_char_at(ch, pos, fmt);
    pos.x += 60; // space between icons

    idx++;
  }
  
  max_index           = idx - 1;
  current_entry_index = entry_index;

  page.put_highlight(
    Dim(entry_locs[entry_index].dim.width + 8, entry_locs[entry_index].dim.height + 8), 
    Pos(entry_locs[entry_index].pos.x     - 4, entry_locs[entry_index].pos.y      - 4));

  fmt.font_index = 1;
  fmt.font_size  = 12;
  
  std::string txt = menu[entry_index].caption; 
  page.put_str_at(txt, Pos{ 10, text_ypos }, fmt);

  page.put_highlight(
    Dim(Screen::WIDTH - 20, 3), 
    Pos(10, region_height - 12));

  page.paint(clear_screen);
}

bool MenuViewer::event(EventMgr::KeyEvent key)
{
  uint8_t old_index = current_entry_index;

  Page::Format fmt = {
    .line_height_factor =   1.0,
    .font_index         =     0,
    .font_size          =    16,
    .indent             =     0,
    .margin_left        =     0,
    .margin_right       =     0,
    .margin_top         =     0,
    .margin_bottom      =     0,
    .screen_left        =    10,
    .screen_right       =    10,
    .screen_top         =    10,
    .screen_bottom      =     0,
    .width              =     0,
    .height             =     0,
    .trim               =  true,
    .pre                = false,
    .font_style         = Fonts::FaceStyle::NORMAL,
    .align              = Page::Align::LEFT,
    .text_transform     = Page::TextTransform::NONE
  };

  page.start(fmt);

  switch (key) {
    case EventMgr::KeyEvent::PREV:
      if (current_entry_index > 0) {
        current_entry_index--;
      }
      else {
        current_entry_index = max_index;
      }
      break;
    case EventMgr::KeyEvent::NEXT:
      if (current_entry_index < max_index) {
        current_entry_index++;
      }
      else {
        current_entry_index = 0;
      }
      break;
    case EventMgr::KeyEvent::DBL_PREV:
      return false;
    case EventMgr::KeyEvent::DBL_NEXT:
      return false;
    case EventMgr::KeyEvent::SELECT:
      if (menu[current_entry_index].func != nullptr) (*menu[current_entry_index].func)();
      return false;
    case EventMgr::KeyEvent::DBL_SELECT:
      return true;
    case EventMgr::KeyEvent::NONE:
      return false;
  }

  if (current_entry_index != old_index) {
    page.clear_highlight(
      Dim(entry_locs[old_index].dim.width + 8, entry_locs[old_index].dim.height + 8), 
      Pos(entry_locs[old_index].pos.x     - 4, entry_locs[old_index].pos.y      - 4));
      
    page.put_highlight(
      Dim(entry_locs[current_entry_index].dim.width + 8, entry_locs[current_entry_index].dim.height + 8),
      Pos(entry_locs[current_entry_index].pos.x     - 4, entry_locs[current_entry_index].pos.y      - 4));

    fmt.font_index = 1;
    fmt.font_size  = 12;
    

    page.clear_region(Dim(Screen::WIDTH, text_height), Pos(0, text_ypos - line_height));

    std::string txt = menu[current_entry_index].caption; 
    page.put_str_at(txt, Pos{ 10, text_ypos }, fmt);
  }

  page.paint(false);
  return false;
}
