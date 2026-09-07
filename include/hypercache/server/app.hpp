#pragma once

#include <string>
#include <cstdint>

namespace hypercache::server {

class App {
public:
    explicit App(unsigned short port = 18080);
    ~App();

    void run();
    void stop();

private:
    unsigned short port_;
    bool running_;
};

} // namespace hypercache::server
