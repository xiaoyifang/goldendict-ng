#pragma once

namespace Help {

enum class section {
  main_website,
  ui_fulltextserch,
  ui_headwords,
  ui_popup,
  manage_sources,
  manage_groups
};

void openHelpWebpage( section sec = section::main_website );

} // namespace Help
