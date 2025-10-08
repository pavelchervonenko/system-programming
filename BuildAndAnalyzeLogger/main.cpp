#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

struct tcp_traffic_pkg
{
    in_addr_t src_addr;
    in_port_t src_port;
    in_addr_t dst_addr;
    in_port_t dst_port;
    size_t sz;

    tcp_traffic_pkg() : src_addr(0), src_port(0), dst_addr(0), dst_port(0), sz(0) {}
    tcp_traffic_pkg(in_addr_t saddr, in_port_t sport, in_addr_t daddr, in_port_t dport, size_t size)
        : src_addr(saddr), src_port(sport), dst_addr(daddr), dst_port(dport), sz(size) {}
};

enum class EventType
{
    Connect,
    Send,
    Recv,
    Disconnect
};

struct TcpEvent
{
    EventType type;
    tcp_traffic_pkg pkg;
    bool abrupt;

    TcpEvent() : type(EventType::Connect), pkg(), abrupt(false) {}
    TcpEvent(EventType t, const tcp_traffic_pkg &p, bool ab) : type(t), pkg(p), abrupt(ab) {}
};

class Logger
{
public:
    enum class Level
    {
        Info,
        Warn,
        Error,
        Debug
    };

    void set_level(Level lvl)
    {
        std::lock_guard<std::mutex> lk(m_);
        level_ = lvl;
    }

    void info(const std::string &msg)
    {
        log(Level::Info, "INFO", msg);
    }

    void warn(const std::string &msg)
    {
        log(Level::Warn, "WARN", msg);
    }

    void error(const std::string &msg)
    {
        log(Level::Error, "ERROR", msg);
    }

    void debug(const std::string &msg)
    {
        log(Level::Debug, "DEBUG", msg);
    }

private:
    std::mutex m_;
    Level level_ = Level::Info;

    bool enabled(Level msg)
    {
        if (level_ == Level::Debug)
        {
            return true;
        }
        if (level_ == Level::Info)
        {
            if (msg == Level::Info)
            {
                return true;
            }
            else if (msg == Level::Warn)
            {
                return true;
            }
            else if (msg == Level::Error)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        if (level_ == Level::Warn)
        {
            if (msg == Level::Warn)
            {
                return true;
            }
            else if (msg == Level::Error)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        if (level_ == Level::Error)
        {
            if (msg == Level::Error)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        return false;
    }

    void log(Level lvl, const char *tag, const std::string &msg)
    {
        std::lock_guard<std::mutex> lk(m_);
        if (enabled(lvl))
        {
            auto now = std::chrono::system_clock::now();
            auto tt = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
            localtime_r(&tt, &tm);
            std::cout << std::setfill('0')
                      << "[" << (tm.tm_year + 1900) << "-"
                      << std::setw(2) << (tm.tm_mon + 1) << "-"
                      << std::setw(2) << tm.tm_mday << " "
                      << std::setw(2) << tm.tm_hour << ":"
                      << std::setw(2) << tm.tm_min << ":"
                      << std::setw(2) << tm.tm_sec << "] "
                      << tag << ": " << msg << std::endl;
        }
    }
};

template <typename T>
class BoundedQueue
{
public:
    explicit BoundedQueue(size_t cap) : capacity_(cap), closed_(false)
    {
        if (capacity_ == 0)
        {
            throw std::invalid_argument("capacity must be positive");
        }
    }

    bool push(const T &value)
    {
        std::unique_lock<std::mutex> lk(m_);
        while (!closed_ && q_.size() >= capacity_)
        {
            not_full_.wait(lk);
        }
        if (closed_)
        {
            return false;
        }
        q_.push(value);
        not_empty_.notify_one();
        return true;
    }

    bool pop(T &out)
    {
        std::unique_lock<std::mutex> lk(m_);
        while (q_.empty() && !closed_)
        {
            not_empty_.wait(lk);
        }
        if (q_.empty())
        {
            return false;
        }
        out = std::move(q_.front());
        q_.pop();
        not_full_.notify_one();
        return true;
    }

    void close()
    {
        std::lock_guard<std::mutex> lk(m_);
        closed_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }

private:
    std::mutex m_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::queue<T> q_;
    size_t capacity_;
    bool closed_;
};

struct PortStats
{
    std::map<uint16_t, size_t> bytes_out;
    std::map<uint16_t, size_t> bytes_in;
};

struct PeerStats
{
    size_t bytes_out;
    size_t bytes_in;
    PortStats ports;

    PeerStats() : bytes_out(0), bytes_in(0), ports() {}
};

struct IpStats
{
    size_t total_sent;
    size_t total_recv;
    size_t connections;
    std::map<uint32_t, PeerStats> peers;

    IpStats() : total_sent(0), total_recv(0), connections(0), peers() {}
};

class Analyzer
{
public:
    Analyzer() : done_(false) {}

    void consume(const TcpEvent &ev)
    {
        if (ev.type == EventType::Connect)
        {
            on_connect(ev.pkg);
        }
        else if (ev.type == EventType::Send)
        {
            on_send(ev.pkg);
        }
        else if (ev.type == EventType::Recv)
        {
            on_recv(ev.pkg);
        }
        else
        {
            on_disconnect(ev.pkg, ev.abrupt);
        }
    }

    void mark_done()
    {
        std::lock_guard<std::mutex> lk(m_);
        done_ = true;
    }

    bool is_done() const
    {
        return done_;
    }

    std::optional<IpStats> get_ip_stats(uint32_t ip) const
    {
        std::lock_guard<std::mutex> lk(m_);
        auto it = stats_.find(ip);
        if (it == stats_.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    std::map<uint32_t, IpStats> snapshot_all() const
    {
        std::lock_guard<std::mutex> lk(m_);
        return stats_;
    }

private:
    mutable std::mutex m_;
    std::map<uint32_t, IpStats> stats_;
    bool done_;

    static uint32_t key_addr(in_addr_t a)
    {
        return static_cast<uint32_t>(a);
    }

    static uint16_t key_port(in_port_t p)
    {
        return ntohs(p);
    }

    void on_connect(const tcp_traffic_pkg &p)
    {
        std::lock_guard<std::mutex> lk(m_);
        uint32_t s = key_addr(p.src_addr);
        uint32_t d = key_addr(p.dst_addr);
        IpStats &from = stats_[s];
        IpStats &to = stats_[d];
        from.connections += 1;
        to.connections += 1;
        PeerStats &ps = from.peers[d];
        (void)ps;
    }

    void on_send(const tcp_traffic_pkg &p)
    {
        std::lock_guard<std::mutex> lk(m_);
        uint32_t s = key_addr(p.src_addr);
        uint32_t d = key_addr(p.dst_addr);
        IpStats &from = stats_[s];
        IpStats &to = stats_[d];
        from.total_sent += p.sz;
        to.total_recv += p.sz;
        PeerStats &ps = from.peers[d];
        ps.bytes_out += p.sz;
        uint16_t dp = key_port(p.dst_port);
        size_t before = 0;
        auto it = ps.ports.bytes_out.find(dp);
        if (it != ps.ports.bytes_out.end())
        {
            before = it->second;
        }
        ps.ports.bytes_out[dp] = before + p.sz;
    }

    void on_recv(const tcp_traffic_pkg &p)
    {
        std::lock_guard<std::mutex> lk(m_);
        uint32_t s = key_addr(p.src_addr);
        uint32_t d = key_addr(p.dst_addr);
        IpStats &from = stats_[s];
        IpStats &to = stats_[d];
        from.total_recv += p.sz;
        to.total_sent += p.sz;
        PeerStats &ps = to.peers[s];
        ps.bytes_in += p.sz;
        uint16_t sp = key_port(p.src_port);
        size_t before = 0;
        auto it = ps.ports.bytes_in.find(sp);
        if (it != ps.ports.bytes_in.end())
        {
            before = it->second;
        }
        ps.ports.bytes_in[sp] = before + p.sz;
    }

    void on_disconnect(const tcp_traffic_pkg &p, bool)
    {
        std::lock_guard<std::mutex> lk(m_);
        uint32_t s = key_addr(p.src_addr);
        uint32_t d = key_addr(p.dst_addr);
        IpStats &from = stats_[s];
        IpStats &to = stats_[d];
        (void)from;
        (void)to;
    }
};

class Coordinator
{
public:
    explicit Coordinator(size_t capacity) : queue_(capacity) {}

    void add_analyzer(std::shared_ptr<Analyzer> a)
    {
        analyzers_.push_back(a);
    }

    BoundedQueue<TcpEvent> &queue()
    {
        return queue_;
    }

    IpStats query_ip(uint32_t ip)
    {
        IpStats result;
        for (auto &a : analyzers_)
        {
            auto part = a->get_ip_stats(ip);
            if (part.has_value())
            {
                const IpStats &s = part.value();
                result.total_sent += s.total_sent;
                result.total_recv += s.total_recv;
                result.connections += s.connections;
                for (const auto &kv : s.peers)
                {
                    auto it = result.peers.find(kv.first);
                    if (it == result.peers.end())
                    {
                        result.peers[kv.first] = kv.second;
                    }
                    else
                    {
                        it->second.bytes_out += kv.second.bytes_out;
                        it->second.bytes_in += kv.second.bytes_in;
                        for (const auto &pp : kv.second.ports.bytes_out)
                        {
                            size_t cur = 0;
                            auto jt = it->second.ports.bytes_out.find(pp.first);
                            if (jt != it->second.ports.bytes_out.end())
                            {
                                cur = jt->second;
                            }
                            it->second.ports.bytes_out[pp.first] = cur + pp.second;
                        }
                        for (const auto &pp : kv.second.ports.bytes_in)
                        {
                            size_t cur = 0;
                            auto jt = it->second.ports.bytes_in.find(pp.first);
                            if (jt != it->second.ports.bytes_in.end())
                            {
                                cur = jt->second;
                            }
                            it->second.ports.bytes_in[pp.first] = cur + pp.second;
                        }
                    }
                }
            }
        }
        return result;
    }

    std::map<uint32_t, IpStats> merge_all()
    {
        std::map<uint32_t, IpStats> merged;
        for (auto &a : analyzers_)
        {
            auto snap = a->snapshot_all();
            for (auto &kv : snap)
            {
                IpStats &dst = merged[kv.first];
                dst.total_sent += kv.second.total_sent;
                dst.total_recv += kv.second.total_recv;
                dst.connections += kv.second.connections;
                for (const auto &peer : kv.second.peers)
                {
                    PeerStats &p = dst.peers[peer.first];
                    p.bytes_out += peer.second.bytes_out;
                    p.bytes_in += peer.second.bytes_in;
                    for (const auto &pp : peer.second.ports.bytes_out)
                    {
                        size_t cur = 0;
                        auto it = p.ports.bytes_out.find(pp.first);
                        if (it != p.ports.bytes_out.end())
                        {
                            cur = it->second;
                        }
                        p.ports.bytes_out[pp.first] = cur + pp.second;
                    }
                    for (const auto &pp : peer.second.ports.bytes_in)
                    {
                        size_t cur = 0;
                        auto it = p.ports.bytes_in.find(pp.first);
                        if (it != p.ports.bytes_in.end())
                        {
                            cur = it->second;
                        }
                        p.ports.bytes_in[pp.first] = cur + pp.second;
                    }
                }
            }
        }
        return merged;
    }

private:
    BoundedQueue<TcpEvent> queue_;
    std::vector<std::shared_ptr<Analyzer>> analyzers_;
};

static std::string ip_to_str(uint32_t ip_be)
{
    in_addr a;
    a.s_addr = ip_be;
    char buf[INET_ADDRSTRLEN];
    const char *res = inet_ntop(AF_INET, &a, buf, sizeof(buf));
    if (res == nullptr)
    {
        return std::string("0.0.0.0");
    }
    return std::string(buf);
}

struct Options
{
    int producers;
    int consumers;
    size_t events;
    size_t capacity;

    Options() : producers(2), consumers(2), events(20000), capacity(1024) {}
};

static bool parse_int(const char *s, long long &out)
{
    try
    {
        std::string v(s);
        size_t pos = 0;
        long long x = std::stoll(v, &pos, 10);
        if (pos != v.size())
        {
            return false;
        }
        out = x;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

static Options parse_cli(int argc, char *argv[])
{
    Options opt;
    int i = 1;
    while (i < argc)
    {
        std::string a(argv[i]);
        if (a == "--producers")
        {
            if (i + 1 >= argc)
            {
                throw std::invalid_argument("missing value for --producers");
            }
            long long v = 0;
            if (!parse_int(argv[i + 1], v))
            {
                throw std::invalid_argument("invalid --producers");
            }
            if (v < 1)
            {
                throw std::invalid_argument("producers must be >= 1");
            }
            opt.producers = static_cast<int>(v);
            i += 2;
        }
        else if (a == "--consumers")
        {
            if (i + 1 >= argc)
            {
                throw std::invalid_argument("missing value for --consumers");
            }
            long long v = 0;
            if (!parse_int(argv[i + 1], v))
            {
                throw std::invalid_argument("invalid --consumers");
            }
            if (v < 1)
            {
                throw std::invalid_argument("consumers must be >= 1");
            }
            opt.consumers = static_cast<int>(v);
            i += 2;
        }
        else if (a == "--events")
        {
            if (i + 1 >= argc)
            {
                throw std::invalid_argument("missing value for --events");
            }
            long long v = 0;
            if (!parse_int(argv[i + 1], v))
            {
                throw std::invalid_argument("invalid --events");
            }
            if (v < 1)
            {
                throw std::invalid_argument("events must be >= 1");
            }
            opt.events = static_cast<size_t>(v);
            i += 2;
        }
        else if (a == "--capacity")
        {
            if (i + 1 >= argc)
            {
                throw std::invalid_argument("missing value for --capacity");
            }
            long long v = 0;
            if (!parse_int(argv[i + 1], v))
            {
                throw std::invalid_argument("invalid --capacity");
            }
            if (v < 1)
            {
                throw std::invalid_argument("capacity must be >= 1");
            }
            opt.capacity = static_cast<size_t>(v);
            i += 2;
        }
        else
        {
            throw std::invalid_argument("unknown option: " + a);
        }
    }
    return opt;
}

static in_addr_t rand_ip(std::mt19937 &rng)
{
    std::uniform_int_distribution<uint32_t> oct(1, 254);
    uint32_t a = oct(rng);
    uint32_t b = oct(rng);
    uint32_t c = oct(rng);
    uint32_t d = oct(rng);
    uint32_t host = (a << 24) | (b << 16) | (c << 8) | d;
    uint32_t be = htonl(host);
    return static_cast<in_addr_t>(be);
}

static in_port_t rand_port(std::mt19937 &rng)
{
    std::uniform_int_distribution<int> d(1024, 65535);
    uint16_t p = static_cast<uint16_t>(d(rng));
    return htons(p);
}

static size_t rand_size(std::mt19937 &rng)
{
    std::uniform_int_distribution<int> d(64, 1500);
    return static_cast<size_t>(d(rng));
}

static TcpEvent make_event(std::mt19937 &rng)
{
    std::uniform_int_distribution<int> et(0, 99);
    int v = et(rng);
    EventType t = EventType::Send;
    if (v < 10)
    {
        t = EventType::Connect;
    }
    else if (v < 55)
    {
        t = EventType::Send;
    }
    else if (v < 95)
    {
        t = EventType::Recv;
    }
    else
    {
        t = EventType::Disconnect;
    }
    bool abrupt = false;
    if (t == EventType::Disconnect)
    {
        std::uniform_int_distribution<int> b(0, 9);
        int r = b(rng);
        if (r == 0)
        {
            abrupt = true;
        }
    }
    tcp_traffic_pkg pkg(rand_ip(rng), rand_port(rng), rand_ip(rng), rand_port(rng), rand_size(rng));
    return TcpEvent(t, pkg, abrupt);
}

int run_app(int argc, char *argv[])
{
    Options opt = parse_cli(argc, argv);

    Logger log;
    log.set_level(Logger::Level::Info);

    Coordinator coord(opt.capacity);

    std::vector<std::shared_ptr<Analyzer>> analyzers;
    int ci = 0;
    while (ci < opt.consumers)
    {
        analyzers.push_back(std::make_shared<Analyzer>());
        coord.add_analyzer(analyzers.back());
        ci += 1;
    }

    std::vector<std::thread> consumers;
    for (std::shared_ptr<Analyzer> a : analyzers)
    {
        consumers.emplace_back([&coord, a]()
                               {
            try {
                TcpEvent ev;
                bool ok = true;
                ok = coord.queue().pop(ev);
                while (ok) {
                    a->consume(ev);
                    ok = coord.queue().pop(ev);
                }
                a->mark_done();
            } catch (...) {
                a->mark_done();
            } });
    }

    std::atomic<size_t> produced{0};
    std::vector<std::thread> producers;
    int pi = 0;
    while (pi < opt.producers)
    {
        producers.emplace_back([&coord, &log, &produced, total = opt.events, seed = std::random_device{}() + static_cast<unsigned>(pi)]()
                               {
            try {
                std::mt19937 rng(seed);
                bool keep = true;
                while (keep) {
                    size_t cur = produced.fetch_add(1);
                    if (cur >= total) {
                        break;
                    }
                    TcpEvent ev = make_event(rng);
                    bool pushed = coord.queue().push(ev);
                    if (!pushed) {
                        break;
                    }
                    if (ev.type == EventType::Connect) {
                        log.debug("connect");
                    } else if (ev.type == EventType::Send) {
                        log.debug("send");
                    } else if (ev.type == EventType::Recv) {
                        log.debug("recv");
                    } else {
                        if (ev.abrupt) {
                            log.debug("disconnect abrupt");
                        } else {
                            log.debug("disconnect");
                        }
                    }
                }
            } catch (...) {
            } });
        pi += 1;
    }

    for (auto &t : producers)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    coord.queue().close();

    for (auto &t : consumers)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    auto merged = coord.merge_all();

    if (!merged.empty())
    {
        auto it = merged.begin();
        uint32_t ip = it->first;
        IpStats live = coord.query_ip(ip);
        std::cout << "LIVE " << ip_to_str(ip) << " sent=" << live.total_sent << " recv=" << live.total_recv << " conn=" << live.connections << std::endl;
    }

    size_t printed = 0;
    for (const auto &kv : merged)
    {
        if (printed >= 10)
        {
            break;
        }
        std::cout << ip_to_str(kv.first)
                  << " sent=" << kv.second.total_sent
                  << " recv=" << kv.second.total_recv
                  << " conn=" << kv.second.connections
                  << std::endl;
        size_t peers_printed = 0;
        for (const auto &p : kv.second.peers)
        {
            if (peers_printed >= 3)
            {
                break;
            }
            std::cout << "  peer " << ip_to_str(p.first)
                      << " out=" << p.second.bytes_out
                      << " in=" << p.second.bytes_in
                      << std::endl;
            peers_printed += 1;
        }
        printed += 1;
    }

    return 0;
}

int main(int argc, char *argv[]) noexcept
{
    try
    {
        return run_app(argc, argv);
    }
    catch (const std::bad_alloc &e)
    {
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
        return 2;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unknown error" << std::endl;
        return 1;
    }
}
