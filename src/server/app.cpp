#include "hypercache/server/app.hpp"
#include "hypercache/cache/lru_cache.hpp"
#include "hypercache/telemetry/telemetry.hpp"
#include "hypercache/similarity/similarity_engine.hpp"

#include "httplib.h"

#include <string>
#include <sstream>
#include <functional>

namespace hypercache::server {

namespace {

auto& get_cache() {
    static hypercache::cache::LruCache<std::string, std::string> cache(128);
    return cache;
}

auto& get_telemetry() {
    return hypercache::telemetry::TokenTelemetry::instance();
}

std::string json_telemetry() {
    std::ostringstream oss;
    oss << "{\"request_tokens\":" << get_telemetry().get_request_tokens()
        << ",\"streaming_tokens\":" << get_telemetry().get_streaming_tokens()
        << ",\"total_requests\":" << get_telemetry().get_total_requests()
        << "}";
    return oss.str();
}

std::string json_similarity(float sim) {
    std::ostringstream oss;
    oss << "{\"similarity\":" << sim << "}";
    return oss.str();
}

std::vector<float> parse_float_array(const std::string& json_str, const std::string& key) {
    std::vector<float> result;
    size_t pos = json_str.find("\"" + key + "\"");
    if (pos == std::string::npos) return result;
    pos = json_str.find('[', pos);
    if (pos == std::string::npos) return result;
    size_t end = json_str.find(']', pos);
    std::string arr = json_str.substr(pos + 1, end - pos - 1);
    std::istringstream iss(arr);
    std::string token;
    while (std::getline(iss, token, ',')) {
        token.erase(0, token.find_first_of("0123456789-."));
        token.erase(token.find_last_of("0123456789-.\"") + 1);
        if (!token.empty()) {
            result.push_back(std::stof(token));
        }
    }
    return result;
}

} // namespace

App::App(unsigned short port) : port_(port), running_(false) {}

App::~App() { stop(); }

void App::run() {
    running_ = true;
    httplib::Server svr;

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok","uptime":"running"})", "application/json");
    });

    svr.Get("/cache/:key", [&](const httplib::Request& req, httplib::Response& res) {
        auto& cache = get_cache();
        auto result = cache.get(req.matches[1]);
        get_telemetry().record_request(1);
        if (result.has_value()) {
            res.set_content(result.value(), "text/plain");
        } else {
            res.status = 404;
            res.set_content("Key not found", "text/plain");
        }
    });

    svr.Put("/cache/:key", [&](const httplib::Request& req, httplib::Response& res) {
        auto& cache = get_cache();
        cache.put(req.matches[1], req.body);
        get_telemetry().record_request(1);
        res.set_content("Cached", "text/plain");
    });

    svr.Delete("/cache/:key", [&](const httplib::Request&, httplib::Response& res) {
        auto& cache = get_cache();
        get_telemetry().record_request(1);
        res.set_content("Removed", "text/plain");
    });

    svr.Get("/cache", [&](const httplib::Request&, httplib::Response& res) {
        get_telemetry().record_request(1);
        std::ostringstream oss;
        oss << "{\"size\":" << get_cache().size() << "}";
        res.set_content(oss.str(), "application/json");
    });

    svr.Get("/telemetry", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(json_telemetry(), "application/json");
    });

    svr.Post("/similarity", [&](const httplib::Request& req, httplib::Response& res) {
        get_telemetry().record_request(1);
        try {
            auto lhs = parse_float_array(req.body, "lhs");
            auto rhs = parse_float_array(req.body, "rhs");
            if (lhs.empty() || rhs.empty() || lhs.size() != rhs.size()) {
                res.status = 400;
                res.set_content("Invalid vectors", "text/plain");
                return;
            }
            float sim = hypercache::similarity::SimilarityEngine::cosine_similarity(lhs, rhs);
            res.set_content(json_similarity(sim), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(e.what(), "text/plain");
        }
    });

    svr.Get("/stream", [&](const httplib::Request&, httplib::Response& res) {
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_chunked_content_provider(
            "text/event-stream",
            [&](size_t, httplib::DataSink& sink) -> bool {
                std::string d1 = "data: streaming started\n\n";
                std::string d2 = "data: token delta\n\n";
                std::string d3 = "data: streaming complete\n\n";
                sink.write(d1.data(), d1.size());
                get_telemetry().record_streaming(1);
                sink.write(d2.data(), d2.size());
                sink.write(d3.data(), d3.size());
                return true;
            });
    });

    svr.listen("0.0.0.0", port_);
}

void App::stop() {
    running_ = false;
}

} // namespace hypercache::server

int main() {
    hypercache::server::App app(18080);
    app.run();
    return 0;
}
