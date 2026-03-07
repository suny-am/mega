#include "Application.h"

int main(int, char **) {

  Application app;

  if (!app.Initialize()) {
    return 1;
  }

#ifdef __EMSCRIPTEN__
  // Emscripten main loop
  auto callback = [](void *arg)
}
Application *pApp = reinterpret_cast<Application *>(arg);
pApp->MainLoop();
emscripten_set_main_loop_arg(callback, &app, 0, true);
#else
  while (app.isRunning()) {
    app.MainLoop();
  }
#endif //  __EMSCRIPTEN__

app.Terminate();

return 0;
}
