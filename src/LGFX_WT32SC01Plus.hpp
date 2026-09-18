// LovyanGFX-Konfiguration fuer das WT32-SC01 Plus.
//
// Display: ST7796 (320x480 portrait) an einem 8-Bit-Parallelbus (Intel 8080),
// kein SPI -- deshalb LovyanGFX statt TFT_eSPI.
// Touch:   FT6336U, I2C-kompatibel zum FT5x06-Treiber.
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX_WT32SC01Plus : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796    _panel;
  lgfx::Bus_Parallel8   _bus;
  lgfx::Light_PWM       _light;
  lgfx::Touch_FT5x06    _touch;

public:
  LGFX_WT32SC01Plus() {
    {
      auto cfg = _bus.config();
      cfg.freq_write = 40000000;
      cfg.pin_wr = 47;   // Write-Strobe
      cfg.pin_rd = -1;   // nicht verdrahtet
      cfg.pin_rs = 0;    // Data/Command
      cfg.pin_d0 = 9;
      cfg.pin_d1 = 46;
      cfg.pin_d2 = 3;
      cfg.pin_d3 = 8;
      cfg.pin_d4 = 18;
      cfg.pin_d5 = 17;
      cfg.pin_d6 = 16;
      cfg.pin_d7 = 15;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    {
      auto cfg = _panel.config();
      cfg.pin_cs           = -1;   // CS fest auf Masse
      cfg.pin_rst          = 4;
      cfg.pin_busy         = -1;
      cfg.panel_width      = 320;
      cfg.panel_height     = 480;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = false;  // RD-Leitung fehlt
      cfg.invert           = true;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = false;
      _panel.config(cfg);
    }

    {
      auto cfg = _light.config();
      cfg.pin_bl      = 45;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }

    {
      auto cfg = _touch.config();
      cfg.x_min           = 0;
      cfg.x_max           = 319;
      cfg.y_min           = 0;
      cfg.y_max           = 479;
      cfg.pin_int         = 7;
      cfg.pin_rst         = -1;   // teilt sich RST mit dem Panel
      cfg.bus_shared      = true;
      cfg.offset_rotation = 0;
      cfg.i2c_port        = 1;
      cfg.i2c_addr        = 0x38;
      cfg.pin_sda         = 6;
      cfg.pin_scl         = 5;
      cfg.freq            = 400000;
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }

    setPanel(&_panel);
  }
};
