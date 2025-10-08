#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>

enum class Level
{
    CRITICAL = 0,
    ERROR = 1,
    WARNING = 2,
    INFO = 3,
    DEBUG = 4
};

struct Sink
{
    virtual ~Sink() {}
    virtual bool write(const std::string &line) = 0;
    virtual void close() = 0;
};

class StreamSink : public Sink
{
public:
    explicit StreamSink(std::ostream &os) : os_(&os) {}
    bool write(const std::string &line) override
    {
        if (os_ == nullptr)
        {
            return false;
        }
        (*os_) << line << '\n';
        os_->flush();
        if (!(*os_))
        {
            return false;
        }
        return true;
    }
    void close() override {}

private:
    std::ostream *os_;
};

class FileSink : public Sink
{
public:
    FileSink(const std::string &path, bool append)
    {
        std::ios_base::openmode mode = std::ios::out;
        if (append)
        {
            mode = mode | std::ios::app;
        }
        else
        {
            mode = mode | std::ios::trunc;
        }
        file_.reset(new std::ofstream(path, mode));
        if (!file_ || !(*file_))
        {
            throw std::runtime_error("cannot open log file");
        }
    }
    bool write(const std::string &line) override
    {
        if (!file_ || !(*file_))
        {
            return false;
        }
        (*file_) << line << '\n';
        file_->flush();
        if (!(*file_))
        {
            return false;
        }
        return true;
    }
    void close() override
    {
        if (file_)
        {
            file_->close();
        }
    }

private:
    std::unique_ptr<std::ofstream> file_;
};

class Logger
{
public:
    class Builder
    {
    public:
        Builder() : level_(Level::INFO) {}
        Builder &set_level(Level level)
        {
            level_ = level;
            return *this;
        }
        Builder &add_stream(std::ostream &os)
        {
            std::shared_ptr<Sink> s(new StreamSink(os));
            sinks_.push_back(s);
            return *this;
        }
        Builder &add_file(const std::string &path, bool append)
        {
            std::shared_ptr<Sink> s(new FileSink(path, append));
            sinks_.push_back(s);
            return *this;
        }
        Logger build()
        {
            Logger lg;
            lg.level_ = level_;
            lg.sinks_ = sinks_;
            return lg;
        }

    private:
        Level level_;
        std::vector<std::shared_ptr<Sink>> sinks_;
    };

    Logger() : level_(Level::INFO), closed_(false) {}

    bool log(Level level, const std::string &message)
    {
        if (closed_)
        {
            return false;
        }
        if (!should_log(level))
        {
            return true;
        }
        std::string line = level_name(level);
        line += ": ";
        line += message;
        bool all_ok = true;
        for (std::size_t i = 0; i < sinks_.size(); ++i)
        {
            if (!sinks_[i]->write(line))
            {
                all_ok = false;
            }
        }
        return all_ok;
    }

    bool critical(const std::string &message)
    {
        return log(Level::CRITICAL, message);
    }
    bool error(const std::string &message)
    {
        return log(Level::ERROR, message);
    }
    bool warning(const std::string &message)
    {
        return log(Level::WARNING, message);
    }
    bool info(const std::string &message)
    {
        return log(Level::INFO, message);
    }
    bool debug(const std::string &message)
    {
        return log(Level::DEBUG, message);
    }

    void close()
    {
        if (closed_)
        {
            return;
        }
        for (std::size_t i = 0; i < sinks_.size(); ++i)
        {
            sinks_[i]->close();
        }
        sinks_.clear();
        closed_ = true;
    }

    Level level() const
    {
        return level_;
    }
    void set_level(Level level)
    {
        level_ = level;
    }

private:
    static std::string level_name(Level level)
    {
        if (level == Level::CRITICAL)
        {
            return std::string("CRITICAL");
        }
        if (level == Level::ERROR)
        {
            return std::string("ERROR");
        }
        if (level == Level::WARNING)
        {
            return std::string("WARNING");
        }
        if (level == Level::INFO)
        {
            return std::string("INFO");
        }
        return std::string("DEBUG");
    }
    bool should_log(Level level) const
    {
        int a = static_cast<int>(level);
        int b = static_cast<int>(level_);
        if (a <= b)
        {
            return true;
        }
        return false;
    }

    Level level_;
    std::vector<std::shared_ptr<Sink>> sinks_;
    bool closed_;
};

int main()
{
    try
    {
        Logger logger = Logger::Builder()
                            .set_level(Level::INFO)
                            .add_stream(std::cout)
                            .add_file("app.log", true)
                            .build();

        logger.critical("fatal condition");
        logger.error("recoverable error");
        logger.warning("potential issue");
        logger.info("regular info");
        logger.debug("hidden details");

        logger.close();
        return 0;
    }
    catch (const std::bad_alloc &e)
    {
        std::cerr << "Memory allocation failed: " << e.what() << "\n";
        return 2;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unknown error\n";
        return 1;
    }
}
