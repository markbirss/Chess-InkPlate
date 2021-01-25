// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "controllers/event_mgr.hpp"

class OptionController
{
  private:
    static constexpr char const * TAG = "OptionController";

    bool main_form_is_shown;
    bool chess_form_is_shown;
    bool books_refresh_needed;
    bool wait_for_key_after_wifi;
    
  public:
    OptionController() : main_form_is_shown(false), 
                        chess_form_is_shown(false),
                       books_refresh_needed(false), 
                    wait_for_key_after_wifi(false) { };
                         
    void key_event(EventMgr::KeyEvent key);
    void enter();
    void leave(bool going_to_deep_sleep = false);

    inline void  set_main_form_is_shown() { main_form_is_shown  = true; }
    inline void set_chess_form_is_shown() { chess_form_is_shown = true; }

    inline void set_wait_for_key_after_wifi() { 
      wait_for_key_after_wifi = true; 
      main_form_is_shown      = false;
      chess_form_is_shown     = false;
    }
};

#if __OPTION_CONTROLLER__
  OptionController option_controller;
#else
  extern OptionController option_controller;
#endif
