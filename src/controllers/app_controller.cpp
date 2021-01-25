// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __APP_CONTROLLER__ 1
#include "controllers/app_controller.hpp"

#include "controllers/board_controller.hpp"
#include "controllers/option_controller.hpp"
#include "controllers/promotion_controller.hpp"
#include "controllers/event_mgr.hpp"
#include "screen.hpp"

AppController::AppController()
{
  current_ctrl = Ctrl::BOARD;
  for (int i = 0; i < LAST_COUNT; i++) {
    last_ctrl[i] = Ctrl::BOARD;
  }
}

void
AppController::start()
{
  current_ctrl = Ctrl::NONE;
  next_ctrl    = Ctrl::BOARD;

  #if CHESS_LINUX_BUILD
    launch();
    event_mgr.loop(); // Will start gtk. Will not return.
  #else
    while (true) {
      if (next_ctrl != Ctrl::NONE) launch();
      event_mgr.loop();
    }
  #endif
}

void 
AppController::set_controller(Ctrl new_ctrl) 
{
  next_ctrl = new_ctrl;
}

void AppController::launch()
{
  #if CHESS_LINUX_BUILD
    if (next_ctrl == Ctrl::NONE) return;
  #endif
  
  Ctrl the_ctrl = next_ctrl;
  next_ctrl = Ctrl::NONE;

  if (((the_ctrl == Ctrl::LAST) && (last_ctrl[0] != current_ctrl)) || (the_ctrl != current_ctrl)) {

    switch (current_ctrl) {
      case Ctrl::BOARD:         board_controller.leave(); break;
      case Ctrl::OPTION:       option_controller.leave(); break;
      case Ctrl::PROMOTION: promotion_controller.leave(); break;
      case Ctrl::NONE:
      case Ctrl::LAST:                                    break;
    }

    Ctrl tmp = current_ctrl;
    current_ctrl = (the_ctrl == Ctrl::LAST) ? last_ctrl[0] : the_ctrl;

    if (the_ctrl == Ctrl::LAST) {
      for (int i = 1; i < LAST_COUNT; i++) last_ctrl[i - 1] = last_ctrl[i];
      last_ctrl[LAST_COUNT - 1] = Ctrl::BOARD;
    }
    else {
      for (int i = 1; i < LAST_COUNT; i++) last_ctrl[i] = last_ctrl[i - 1];
      last_ctrl[0] = tmp;
    }

    switch (current_ctrl) {
      case Ctrl::BOARD:         board_controller.enter(); break;
      case Ctrl::OPTION:       option_controller.enter(); break;
      case Ctrl::PROMOTION: promotion_controller.enter(); break;
      case Ctrl::NONE:
      case Ctrl::LAST:                                    break;
    }
  }
}

void 
AppController::key_event(EventMgr::KeyEvent key)
{
  if (next_ctrl != Ctrl::NONE) launch();

  switch (current_ctrl) {
    case Ctrl::BOARD:         board_controller.key_event(key); break;
    case Ctrl::OPTION:       option_controller.key_event(key); break;
    case Ctrl::PROMOTION: promotion_controller.key_event(key); break;
    case Ctrl::NONE:
    case Ctrl::LAST:                                           break;
  }
}

void
AppController::going_to_deep_sleep()
{
  if (next_ctrl != Ctrl::NONE) launch();

  switch (current_ctrl) {
    case Ctrl::BOARD:      board_controller.leave(true); break;
    case Ctrl::OPTION:    option_controller.leave(true); break;
    case Ctrl::PROMOTION: option_controller.leave(true); break;
    case Ctrl::NONE:
    case Ctrl::LAST:                                     break;
  }
}