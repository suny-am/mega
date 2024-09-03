#include "application.h"

int main()
{
	Application app;
	if (!app.onInit()) return 1;

#ifdef __EMSCRIPTEN__
	auto callback = [](void* arg)
		{
			Application* pApp = reinterpret_cast<Application*>(arg);
			pApp->onFrame();
		};
	emscripten_set_main_loop_arg(callback, &app, 0, true);
#else  // __EMSCRIPTEN__
	while (app.isRunning())
	{
		app.onFrame();
	}
#endif // __EMSCRIPTEN__
	return 0;
}