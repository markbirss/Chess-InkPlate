// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __FORM_VIEWER__ 1
#include "viewers/form_viewer.hpp"

#include "models/fonts.hpp"
#include "models/ttf2.hpp"
#include "viewers/page.hpp"
#include "screen.hpp"

void
FormViewer::show(FormViewer::FormEntries  form_entries, 
                 int8_t                   size, 
                 const std::string      & bottom_msg)
{
  // Compute location of items on screen

  entries = form_entries;

  TTF * font                        = fonts.get(1);
  TTF::BitmapGlyph * glyph          = font->get_glyph('M', FONT_SIZE);
  uint8_t base_line_offset          = -glyph->yoff;
  uint8_t next_available_choice_loc = 0;

  line_height = font->get_line_height(FONT_SIZE);
  entry_count = size;

  // Compute width / height of individual choices and entry captions

  for (int i = 0; i < entry_count; i++) {
    // if (i < (entry_count - 1)) { // Don't do last entry (OK / CANCEL)
    // if (i < entry_count) { // Don't do last entry (OK / CANCEL)
      font->get_size(entries[i].caption, &entries_info[i].dim, FONT_SIZE);
    // }
    // else {
    //   entries_info[i].dim.width = entries_info[i].dim.height = 0;
    // }
    entries_info[i].first_choice_loc_idx = next_available_choice_loc;

    for (int j = 0, k = next_available_choice_loc; j < entries[i].choice_count; j++, k++) {
      font->get_size(
        entries[i].choices[j].caption, 
        &choice_loc[k].dim, 
        FONT_SIZE);
      if (entries[i].choices[j].value == *entries[i].value) {
        entries_info[i].choice_idx = k;
      }
    }
    next_available_choice_loc += entries[i].choice_count;
  }

  int16_t       current_ypos = TOP_YPOS + 10;
  const int16_t right_xpos   = Screen::WIDTH - 60;

  // Compute combined width / height

  // for (int i = 0; i < entry_count - 1; i++) {
  for (int i = 0; i < entry_count; i++) {

    int16_t choices_width  = 0;
    int16_t choices_height = 0;
    int16_t separator      = 0;
    int16_t last_height    = 0;

    if (entries[i].entry_type == FormEntryType::HORIZONTAL_CHOICES) {     
      for (int j = 0, k = entries_info[i].first_choice_loc_idx; j < entries[i].choice_count; j++, k++) {
        if (choices_height < choice_loc[k].dim.height) choices_height = choice_loc[k].dim.height;
        choices_width += choice_loc[k].dim.width + separator;
        separator = 20;
      }
    }
    else { // VERTICAL_CHOICES
      for (int j = 0, k = entries_info[i].first_choice_loc_idx; j < entries[i].choice_count; j++, k++) {
        if (choices_width < choice_loc[k].dim.width) choices_width = choice_loc[k].dim.width;
        choices_height += line_height;
        last_height     = choice_loc[k].dim.height;
      }
      choices_height += last_height - line_height;
    }
    if (all_choices_width < choices_width) all_choices_width = choices_width;
    entries_info[i].choices_height = choices_height;
  }

  // Compute xpos, ypos

  // for (int i = 0; i < entry_count - 1; i++) {
  for (int i = 0; i < entry_count; i++) {
    if (entries[i].entry_type == FormEntryType::HORIZONTAL_CHOICES) {     
      int16_t left_pos = right_xpos - all_choices_width - 10;
      for (int j = 0, k = entries_info[i].first_choice_loc_idx; j < entries[i].choice_count; j++, k++) {
        choice_loc[k].pos.x = left_pos;
        choice_loc[k].pos.y = current_ypos + 10;
        left_pos += choice_loc[k].dim.width + 20;
      }
    }
    else { // VERTICAL_CHOICES
      int16_t top_ypos = current_ypos + 10;
      for (int j = 0, k = entries_info[i].first_choice_loc_idx; j < entries[i].choice_count; j++, k++) {
        choice_loc[k].pos.x = right_xpos - all_choices_width - 10;
        choice_loc[k].pos.y = top_ypos;
        top_ypos += line_height;
      }
    }
    entries_info[i].pos.x = right_xpos - all_choices_width - 35 - entries_info[i].dim.width;
    entries_info[i].pos.y = current_ypos + 10;
    current_ypos         += entries_info[i].choices_height + 20;
  }

  current_ypos += 20;
  // int k = entries_info[entry_count - 1].first_choice_loc_idx;
  // int k = entries_info[entry_count].first_choice_loc_idx;
  // choice_loc[k].pos.y     = choice_loc[k + 1].pos.y = current_ypos;
  // choice_loc[k].pos.x     = (Screen::WIDTH >> 1) - choice_loc[k].dim.width - 20;
  // choice_loc[k + 1].pos.x = (Screen::WIDTH >> 1) + 20;
  // entries_info[entry_count - 1].choices_height = choice_loc[k].dim.height;
  // entries_info[entry_count].choices_height = choice_loc[k].dim.height;
  // last_choices_width = choice_loc[k].dim.width + choice_loc[k + 1].dim.width + 40;

  // Display the form

  Page::Format fmt = {
    .line_height_factor = 1.0,
    .font_index         =   1,
    .font_size          = FONT_SIZE,
    .indent             =   0,
    .margin_left        =   5,
    .margin_right       =   5,
    .margin_top         =   0,
    .margin_bottom      =   0,
    .screen_left        =  20,
    .screen_right       =  20,
    .screen_top         = TOP_YPOS,
    .screen_bottom      = BOTTOM_YPOS,
    .width              =   0,
    .height             =   0,
    .trim               = true,
    .pre                = false,
    .font_style         = Fonts::FaceStyle::NORMAL,
    .align              = Page::Align::LEFT,
    .text_transform     = Page::TextTransform::NONE
  };

  page.start(fmt);

  // The large rectangle into which the form will be drawn

  page.clear_region(
    Dim(Screen::WIDTH - 40, Screen::HEIGHT - fmt.screen_bottom - fmt.screen_top),
    Pos(20, TOP_YPOS));

  page.put_highlight(
    Dim(Screen::WIDTH - 44, Screen::HEIGHT - fmt.screen_bottom - fmt.screen_top - 4),
    Pos(22, TOP_YPOS + 2));

  // Show all captions (but the last one (OK / CANCEL))

  for (int i = 0; i < entry_count; i++) {
    // if (i < (entry_count - 1)) {
      page.put_str_at(
        entries[i].caption, 
        Pos(entries_info[i].pos.x, entries_info[i].pos.y + base_line_offset), 
        fmt);
    // }

    for (int j = 0, k = entries_info[i].first_choice_loc_idx; j < entries[i].choice_count; j++, k++) {
      page.put_str_at(
        entries[i].choices[j].caption,
        Pos(choice_loc[k].pos.x, choice_loc[k].pos.y + base_line_offset),
        fmt);
    }
  }

  fmt.screen_top = current_ypos + 40;
  page.set_limits(fmt);
  page.new_paragraph(fmt);
  if (!bottom_msg.empty()) {
    page.add_text(
      bottom_msg,
      fmt
    );
  }
  page.end_paragraph(fmt);

  // Highlight current values

  for (int i = 0; i < entry_count; i++) {
    int8_t k = entries_info[i].choice_idx;

    page.put_highlight(
      Dim(entries[i].entry_type == FormEntryType::HORIZONTAL_CHOICES ? choice_loc[k].dim.width + 10 : all_choices_width + 10,
          choice_loc[k].dim.height + 10),
      Pos(choice_loc[k].pos.x - 5, choice_loc[k].pos.y - 5));
  }

  // Set current entry, highlighting the set of choices for that entry

  current_entry_idx   = 0;
  entry_selection     = true;
  highlight_selection = false;

  page.put_highlight(
    Dim(all_choices_width + 20,
        entries_info[0].choices_height + 20),
    Pos(choice_loc[entries_info[0].first_choice_loc_idx].pos.x - 10,
        choice_loc[entries_info[0].first_choice_loc_idx].pos.y - 10));

  page.paint(false);
}

bool
FormViewer::event(EventMgr::KeyEvent key)
{
  int8_t old_entry_idx = current_entry_idx;
  int8_t old_choice_idx = 0;
  int8_t choice_idx = 0;

  bool completed = false;

  if (entry_selection) {
    switch (key) {
      case EventMgr::KeyEvent::DBL_PREV:
      case EventMgr::KeyEvent::PREV:
        current_entry_idx--;
        if (current_entry_idx < 0) {
          current_entry_idx = entry_count - 1;
        }
        break;
      case EventMgr::KeyEvent::DBL_NEXT:
      case EventMgr::KeyEvent::NEXT:
        current_entry_idx++;
        if (current_entry_idx >= entry_count) {
          current_entry_idx = 0;
        }
        break;
      case EventMgr::KeyEvent::SELECT:
        entry_selection = false;
        highlight_selection = true;
        break;
      case EventMgr::KeyEvent::NONE:
        return false;
      case EventMgr::KeyEvent::DBL_SELECT:
        completed = true;
        break;
    }
  }
  else {
    old_choice_idx = choice_idx = entries_info[current_entry_idx].choice_idx;

    switch (key) {
      case EventMgr::KeyEvent::DBL_PREV:
      case EventMgr::KeyEvent::PREV:
        choice_idx--;
        if (choice_idx < entries_info[current_entry_idx].first_choice_loc_idx) {
          choice_idx = entries_info[current_entry_idx].first_choice_loc_idx + entries[current_entry_idx].choice_count - 1;
        }
        break;
      case EventMgr::KeyEvent::DBL_NEXT:
      case EventMgr::KeyEvent::NEXT:
        choice_idx++;
        if (choice_idx >= entries_info[current_entry_idx].first_choice_loc_idx + entries[current_entry_idx].choice_count) {
          choice_idx = entries_info[current_entry_idx].first_choice_loc_idx;
        }
        break;
      case EventMgr::KeyEvent::SELECT:
        entry_selection = true;
        old_entry_idx = current_entry_idx;
        current_entry_idx++;
        if (current_entry_idx >= entry_count) {
          current_entry_idx = 0;
        }
        break;
      case EventMgr::KeyEvent::NONE:
        return false;
      case EventMgr::KeyEvent::DBL_SELECT:
        completed = true;
        break;
    }
  }

  Page::Format fmt = {
    .line_height_factor = 1.0,
    .font_index         =   1,
    .font_size          = FONT_SIZE,
    .indent             =   0,
    .margin_left        =   0,
    .margin_right       =   0,
    .margin_top         =   0,
    .margin_bottom      =   0,
    .screen_left        =  20,
    .screen_right       =  20,
    .screen_top         =  TOP_YPOS,
    .screen_bottom      =  BOTTOM_YPOS,
    .width              =   0,
    .height             =   0,
    .trim               = true,
    .pre                = false,
    .font_style         = Fonts::FaceStyle::NORMAL,
    .align              = Page::Align::LEFT,
    .text_transform     = Page::TextTransform::NONE
  };

  page.start(fmt);

  if (!completed) {

    if (entry_selection) {
//      if (current_entry_idx != old_entry_idx) {
        page.clear_highlight(
          // Dim(((old_entry_idx == entry_count - 1) ? last_choices_width : all_choices_width) + 20,
          Dim(all_choices_width + 20,
              entries_info[old_entry_idx].choices_height + 20),
          Pos(choice_loc[entries_info[old_entry_idx].first_choice_loc_idx].pos.x - 10,
              choice_loc[entries_info[old_entry_idx].first_choice_loc_idx].pos.y - 10));
        page.clear_highlight(
          // Dim(((old_entry_idx == entry_count - 1) ? last_choices_width : all_choices_width) + 22,
          Dim(all_choices_width + 22,
              entries_info[old_entry_idx].choices_height + 22),
          Pos(choice_loc[entries_info[old_entry_idx].first_choice_loc_idx].pos.x - 11,
              choice_loc[entries_info[old_entry_idx].first_choice_loc_idx].pos.y - 11));
        page.clear_highlight(
          // Dim(((old_entry_idx == entry_count - 1) ? last_choices_width : all_choices_width) + 24,
          Dim(all_choices_width + 24,
              entries_info[old_entry_idx].choices_height + 24),
          Pos(choice_loc[entries_info[old_entry_idx].first_choice_loc_idx].pos.x - 12,
              choice_loc[entries_info[old_entry_idx].first_choice_loc_idx].pos.y - 12));

        page.put_highlight(
          // Dim(((current_entry_idx == entry_count - 1) ? last_choices_width : all_choices_width) + 20,
          Dim(all_choices_width + 20,
              entries_info[current_entry_idx].choices_height + 20),
          Pos(choice_loc[entries_info[current_entry_idx].first_choice_loc_idx].pos.x - 10,
              choice_loc[entries_info[current_entry_idx].first_choice_loc_idx].pos.y - 10));
//      }
    }
    else {
      if (highlight_selection) {
        highlight_selection = false;
        page.put_highlight(
          // Dim(((current_entry_idx == entry_count - 1) ? last_choices_width : all_choices_width) + 22,
          Dim(all_choices_width + 22,
              entries_info[current_entry_idx].choices_height + 22),
          Pos(choice_loc[entries_info[current_entry_idx].first_choice_loc_idx].pos.x - 11,
              choice_loc[entries_info[current_entry_idx].first_choice_loc_idx].pos.y - 11));
        page.put_highlight(
          // Dim(((current_entry_idx == entry_count - 1) ? last_choices_width : all_choices_width) + 24,
          Dim(all_choices_width + 24,
              entries_info[current_entry_idx].choices_height + 24),
          Pos(choice_loc[entries_info[current_entry_idx].first_choice_loc_idx].pos.x - 12,
              choice_loc[entries_info[current_entry_idx].first_choice_loc_idx].pos.y - 12));
      }
      if (choice_idx != old_choice_idx) {
        entries_info[current_entry_idx].choice_idx = choice_idx;

        page.clear_highlight(
          Dim(entries[current_entry_idx].entry_type == FormEntryType::HORIZONTAL_CHOICES ? choice_loc[old_choice_idx].dim.width + 10 : all_choices_width + 10,
              choice_loc[old_choice_idx].dim.height + 10),
          Pos(choice_loc[old_choice_idx].pos.x - 5, choice_loc[old_choice_idx].pos.y - 5));

        page.put_highlight(
          Dim(entries[current_entry_idx].entry_type == FormEntryType::HORIZONTAL_CHOICES ? choice_loc[choice_idx].dim.width + 10 : all_choices_width + 10,
              choice_loc[choice_idx].dim.height + 10),
          Pos(choice_loc[choice_idx].pos.x - 5, choice_loc[choice_idx].pos.y - 5));
      }
    }

    page.paint(false);
  }
  else {
    for (int i = 0; i < entry_count; i++) {
      int8_t k = entries_info[i].choice_idx - entries_info[i].first_choice_loc_idx;
      *entries[i].value = entries[i].choices[k].value;
    }
    page.clear_region(
      Dim(Screen::WIDTH - 40, Screen::HEIGHT - fmt.screen_bottom - fmt.screen_top),
      Pos(20, TOP_YPOS));

    page.paint(false);
  }
  return completed;
}
