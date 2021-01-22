// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "controllers/event_mgr.hpp"

/**
 * @brief Application Controller
 * 
 * Main controller responsible of event transmission to the
 * various controllers of the application.
 * 
 * They are:
 * 
 *   - DIR:    Books directory viewer and selection
 *   - PARAM:  Parameters viewer and selection
 *   - BOOK:   Book content viewer
 *   - OPTION: Application options viewer and edition
 */
class AppController
{
  public:
    /**
     * @brief 
     * 
     * Used to internally identify the controllers. 
     * 
     * LAST allows for the
     * selection of the last controller in charge before the current one.
     */
    enum class Ctrl { NONE, BOARD, OPTION, LAST };
    
    AppController();

    /**
     * @brief Start the application
     * 
     * Start the application, giving control to the DIR controller. 
     */
    void start();

    /**
     * @brief Set the controller object
     * 
     * This method will call the current controller *leave()* method then
     * call the selected controller *enter()* method.
     *  
     * @param new_ctrl The new controller to take control
     */
    void set_controller(Ctrl new_ctrl);

    /**
     * @brief Manage a Key Event
     * 
     * Called when a key is pressed by the user. The method is transfering
     * control to the current controller *key_event()* method.
     * 
     * @param key 
     */
    void key_event(EventMgr::KeyEvent key);

    void going_to_deep_sleep();
    void launch();

  private:
    static constexpr char const * TAG = "AppController";

    static const int LAST_COUNT = 4;
    Ctrl current_ctrl;
    Ctrl next_ctrl;
    Ctrl last_ctrl[LAST_COUNT]; ///< LIFO of last controllers in use
};

#if __APP_CONTROLLER__
  AppController app_controller;
#else
  extern AppController app_controller;
#endif
