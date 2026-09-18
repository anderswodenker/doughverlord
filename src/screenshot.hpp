// Screenshot ueber die serielle Konsole: der aktive LVGL-Screen wird in
// einen PSRAM-Puffer gerendert und als Base64 ausgegeben. Damit laesst
// sich die Oberflaeche vom Rechner aus pruefen, ohne aufs Display zu
// schauen -- tools/screenshot.py holt ihn ab und macht ein PNG daraus.
#pragma once

namespace screenshot {
void dump();
}
