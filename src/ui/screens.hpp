// Interne Schnittstelle der einzelnen Screens; nach aussen zeigt ui.hpp.
#pragma once

#include "../recipe.hpp"

namespace ui {

namespace select      { void show(); void refresh(); }
namespace ingredients { void show(const recipe::Recipe &r, bool startable); void refresh(); }
namespace timer       { void show(); void refresh(); }

}  // namespace ui
