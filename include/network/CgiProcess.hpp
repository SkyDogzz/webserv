#ifndef CGIPROCESS_HPP
#define CGIPROCESS_HPP

#include "../config/LocationConfig.hpp"
#include "../core/Router.hpp"
#include "../http/HttpRequest.hpp"
#include "../http/HttpResponse.hpp"
#include <map>
#include <string>
#include <sys/types.h>

class CgiProcess {
public:
    CgiProcess();
    ~CgiProcess();

    bool start(const HttpRequest& request, const RequestContext& context, const std::string& script_path,
        const std::string& interpreter_path);

    int getReadFd() const;
    int getWriteFd() const;
    pid_t getPid() const;
    const HttpRequest& getRequest() const;

    bool wantsRead() const;
    bool wantsWrite() const;
    bool onReadable();
    bool onWritable();
    bool reapIfFinished();
    bool timedOut(long now_ms, long timeout_ms) const;
    void abort();
    void closeDescriptors();

    bool isFinished() const;
    bool hasFailed() const;

    HttpResponse buildResponse(bool keep_alive) const;

private:
    HttpRequest request_;
    RequestContext context_;
    std::string script_path_;
    std::string interpreter_path_;
    std::string input_buffer_;
    std::string output_buffer_;
    std::map<std::string, std::string> headers_;
    int status_code_;
    int stdin_fd_;
    int stdout_fd_;
    pid_t pid_;
    bool headers_parsed_;
    bool input_closed_;
    bool output_closed_;
    bool finished_;
    bool failed_;
    bool saw_body_separator_;
    long start_ms_;

    bool parseCgiOutput();
    void closeInput();
    void closeOutput();
    static std::string trimHeaderValue(const std::string& value);
    static bool parseStatusHeader(const std::string& value, int& status_code);
    static void setNonBlocking(int fd);
    static std::string buildEnvString(const std::string& key, const std::string& value);
};

#endif
