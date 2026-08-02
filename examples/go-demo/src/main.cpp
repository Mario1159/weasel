#include <wsl/app.hpp>
#include <wsl/rsc/project_loader.hpp>

class go_demo_app : public wsl::app {
public:
    go_demo_app() : wsl::app("go-demo", 1280, 720, 
#ifdef WSL_RESOURCE_PATH
        WSL_RESOURCE_PATH
#else
        "."
#endif
    ) {}

protected:
    void on_init() override {
        set_project_path("wslpro.json");
    }
};

int main(int argc, char** argv) {
    go_demo_app app;
    return app.run();
}
