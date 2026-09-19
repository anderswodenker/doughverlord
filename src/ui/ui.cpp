#include "ui.hpp"

#include <lvgl.h>

#include "../session.hpp"
#include "screens.hpp"

namespace {

enum class Screen { None, Home, Select, Ingredients, Timer };
Screen current = Screen::None;
bool   ingredients_preview = false;   // Zutaten vor dem Start (mit Los-Knopf)

}  // namespace

namespace ui {

void begin()
{
    if (session::active() || session::state() == session::State::Done) show_timer();
    else                                                                 show_home();
}

void show_home()
{
    current = Screen::Home;
    home::show();
}

void show_select()
{
    current = Screen::Select;
    select::show();
}

void show_ingredients(const recipe::Recipe &r, bool startable)
{
    current = Screen::Ingredients;
    ingredients_preview = startable;
    ingredients::show(r, startable);
}

void show_timer()
{
    current = Screen::Timer;
    timer::show();
}

void tick()
{
    static uint32_t last = 0;
    if (millis() - last < 1000) return;
    last = millis();

    // Zustand kann sich auch ohne Touch aendern (Timer laeuft ab, CLI):
    // Dashboard/Auswahl bei laufendem Teig ist falsch, Timer-Screen ohne Teig auch.
    const bool has_dough = session::active() || session::state() == session::State::Done;
    const bool preview   = current == Screen::Home || current == Screen::Select
                        || (current == Screen::Ingredients && ingredients_preview);
    if (has_dough && preview)                    { show_timer(); return; }
    if (!has_dough && current == Screen::Timer)  { show_home();  return; }
    if (!has_dough && current == Screen::Ingredients && !ingredients_preview) { show_home(); return; }

    switch (current) {
        case Screen::Home:        home::refresh();        break;
        case Screen::Select:      select::refresh();      break;
        case Screen::Ingredients: ingredients::refresh(); break;
        case Screen::Timer:       timer::refresh();       break;
        default: break;
    }
}

}  // namespace ui
