#include "ui.hpp"

#include <lvgl.h>

#include "../display.hpp"
#include "../session.hpp"
#include "screens.hpp"

namespace {

enum class Screen { None, Home, Select, Ingredients, Timer, Alarm };
Screen current = Screen::None;
bool   ingredients_preview = false;   // Zutaten vor dem Start (mit Los-Knopf)

}  // namespace

namespace ui {

void begin()
{
    if (alarm_active())                                                       show_alarm();
    else if (session::active() || session::state() == session::State::Done)  show_timer();
    else                                                                      show_home();
}

bool alarm_active()
{
    const recipe::Step *s = session::current_step();
    return session::state() == session::State::Waiting && s && !s->offen;
}

void show_alarm()
{
    current = Screen::Alarm;
    display::set_brightness(display::BRIGHT_FULL);
    alarm::show();
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
    const bool alarm     = alarm_active();
    if (alarm && current != Screen::Alarm)       { show_alarm(); return; }
    if (!alarm && current == Screen::Alarm)      { has_dough ? show_timer() : show_home(); return; }
    if (has_dough && preview)                    { show_timer(); return; }
    if (!has_dough && current == Screen::Timer)  { show_home();  return; }
    if (!has_dough && current == Screen::Ingredients && !ingredients_preview) { show_home(); return; }

    // Nachts in der Kueche: nach 60 s ohne Beruehrung dimmen, Alarm bleibt hell.
    const bool dim = !alarm && display::idle_ms() > 60000;
    display::set_brightness(dim ? display::BRIGHT_DIM : display::BRIGHT_FULL);

    switch (current) {
        case Screen::Alarm:       alarm::refresh();       break;
        case Screen::Home:        home::refresh();        break;
        case Screen::Select:      select::refresh();      break;
        case Screen::Ingredients: ingredients::refresh(); break;
        case Screen::Timer:       timer::refresh();       break;
        default: break;
    }
}

}  // namespace ui
