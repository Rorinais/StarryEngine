#include"../core/application/Application.hpp"

int main() {
#ifdef _WIN32
	_putenv_s("VK_LAYER_PATH", "layers");
#endif 

	StarryEngine::Application app;
	app.run();

	return 0;
}