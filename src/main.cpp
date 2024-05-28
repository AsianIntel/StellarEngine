#include "core/app.hpp"

int main() {
    App app;
    app.initialize();
    app.run();
    app.shutdown();
    
    return 0;
}