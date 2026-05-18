#include "Application.h"

int main(int, char **) {

  Application app;

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
