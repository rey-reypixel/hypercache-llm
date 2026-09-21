#pragma once

namespace hypercache::server {

class App {
public:
    explicit App(unsigned short port);
    ~App();

    void run();
    void stop();

private:
    unsigned short port_;
    bool running_;
};

} // namespace hypercache::server