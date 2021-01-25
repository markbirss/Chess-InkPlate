// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "controllers/event_mgr.hpp"

class MenuViewer
{
  public:
    static constexpr uint8_t MAX_MENU_ENTRY = 10;

    enum class Icon { RETURN, REVERT, REFRESH, BOOK, BOOK_LIST, MAIN_PARAMS, 
                      FONT_PARAMS, POWEROFF, WIFI, INFO,
                      W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
                      B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING, 
                      CHESS, END_MENU };

    char icon_char[23] = { '@', 'H', 'R', 'E', 'F', 'C', 
                           'A', 'Z', 'S', 'I', 
                           'f', 'a', 'b', 'c', 'd', 'e', 
                           'h', 'i', 'j', 'k', 'l', 'm', 'n' };
    struct MenuEntry {
      Icon icon;
      const char * caption;
      void (*func)();
    };
    void  show(MenuEntry * the_menu, uint8_t entry_index = 0, bool clear_screen = false);
    bool event(EventMgr::KeyEvent key);
    
  private:
    static constexpr char const * TAG = "MenuViewer";

    uint8_t  current_entry_index;
    uint8_t  max_index;
    uint16_t icon_height, 
             text_height, 
             line_height,
             region_height;
    int16_t  icon_ypos,
             text_ypos;

    struct EntryLoc {
      Pos pos;
      Dim dim;
    } entry_locs[MAX_MENU_ENTRY];
    
    MenuEntry * menu;
};

#if __MENU_VIEWER__
  MenuViewer menu_viewer;
#else
  extern MenuViewer menu_viewer;
#endif
