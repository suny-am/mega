#include "Application.h"

int main(int argc, char **argv) {

  Application app;
  // read mouse or trackball mode from argument and set it in the Application
  if (argc > 1) {
    std::string mode = argv[1];
    app.setMouseMode(mode == "trackpad" ? Application::MouseMode::Trackpad
                                        : Application::MouseMode::Mouse);
  }

  if (!app.onInit()) {
    return 1;
  }

#ifdef __EMSCRIPTEN__
  // Emscripten main loop
  auto callback = [](void *arg)
}
Application *pApp = reinterpret_cast<Application *>(arg);
pApp->OnFrame();
emscripten_set_main_loop_arg(callback, &app, 0, true);
#else
  while (app.isRunning()) {
    app.onFrame();
  }
#endif //  __EMSCRIPTEN__

app.onFinish();

return 0;
}
