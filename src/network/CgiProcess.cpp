#include "../../include/network/CgiProcess.hpp"

#include "../../include/http/HttpStatus.hpp"
#include "../../include/utils/Utils.hpp"
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

long currentTimeMs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return static_cast<long>(tv.tv_sec) * 1000L + static_cast<long>(tv.tv_usec / 1000L);
}

std::string dirnameFromPath(const std::string& path)
{
    std::string::size_type slash = path.rfind('/');
    if (slash == std::string::npos)
        return ".";
    if (slash == 0)
        return "/";
    return path.substr(0, slash);
}

std::string basenameFromPath(const std::string& path)
{
    std::string::size_type slash = path.rfind('/');
    if (slash == std::string::npos)
        return path;
    return path.substr(slash + 1);
}

std::string parseHeaderKey(const std::string& line)
{
    std::string::size_type colon = line.find(':');
    if (colon == std::string::npos)
        return "";
    return Utils::trimCopy(line.substr(0, colon));
}

std::string parseHeaderValue(const std::string& line)
{
    std::string::size_type colon = line.find(':');
    if (colon == std::string::npos)
        return "";
    return Utils::trimCopy(line.substr(colon + 1));
}

bool parseSizeHeader(const std::string& value, std::size_t& out)
{
    std::string trimmed = Utils::trimCopy(value);
    if (trimmed.empty())
        return false;
    out = 0;
    for (std::size_t i = 0; i < trimmed.size(); ++i) {
        char c = trimmed[i];
        if (c < '0' || c > '9')
            return false;
        std::size_t digit = static_cast<std::size_t>(c - '0');
        if (out > (static_cast<std::size_t>(-1) - digit) / 10)
            return false;
        out = out * 10 + digit;
    }
    return true;
}

} // namespace

CgiProcess::CgiProcess()
    : status_code_(200)
    , stdin_fd_(-1)
    , stdout_fd_(-1)
    , pid_(-1)
    , headers_parsed_(false)
    , input_closed_(false)
    , output_closed_(false)
    , finished_(false)
    , failed_(false)
    , saw_body_separator_(false)
    , start_ms_(currentTimeMs())
{
}

CgiProcess::~CgiProcess()
{
    closeDescriptors();
}

void CgiProcess::setNonBlocking(int fd)
{
    Utils::makeNonBlocking(fd);
}

std::string CgiProcess::trimHeaderValue(const std::string& value)
{
    return Utils::trimCopy(value);
}

bool CgiProcess::parseStatusHeader(const std::string& value, int& status_code)
{
    std::string trimmed = Utils::trimCopy(value);
    if (trimmed.empty())
        return false;
    std::istringstream iss(trimmed);
    int code = 0;
    iss >> code;
    if (!iss || code < 100 || code > 599)
        return false;
    status_code = code;
    return true;
}

std::string CgiProcess::buildEnvString(const std::string& key, const std::string& value)
{
    return key + "=" + value;
}

void CgiProcess::closeInput()
{
    if (stdin_fd_ != -1) {
        Utils::closeFdSafe(stdin_fd_);
        stdin_fd_ = -1;
    }
}

void CgiProcess::closeOutput()
{
    if (stdout_fd_ != -1) {
        Utils::closeFdSafe(stdout_fd_);
        stdout_fd_ = -1;
    }
}

bool CgiProcess::start(const HttpRequest& request, const RequestContext& context, const std::string& script_path,
    const std::string& interpreter_path)
{
    request_ = request;
    context_ = context;
    script_path_ = script_path;
    interpreter_path_ = interpreter_path;
    status_code_ = 200;
    headers_.clear();
    input_buffer_ = request.body;
    output_buffer_.clear();
    headers_parsed_ = false;
    input_closed_ = false;
    output_closed_ = false;
    finished_ = false;
    failed_ = false;
    saw_body_separator_ = false;
    start_ms_ = currentTimeMs();

    int in_pipe[2] = { -1, -1 };
    int out_pipe[2] = { -1, -1 };
    if (pipe(in_pipe) == -1 || pipe(out_pipe) == -1) {
        if (in_pipe[0] != -1) {
            Utils::closeFdSafe(in_pipe[0]);
            Utils::closeFdSafe(in_pipe[1]);
        }
        return false;
    }

    pid_ = fork();
    if (pid_ == -1) {
        Utils::closeFdSafe(in_pipe[0]);
        Utils::closeFdSafe(in_pipe[1]);
        Utils::closeFdSafe(out_pipe[0]);
        Utils::closeFdSafe(out_pipe[1]);
        return false;
    }

    if (pid_ == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);

        std::string dir = dirnameFromPath(script_path_);
        if (chdir(dir.c_str()) == -1)
            _exit(1);

        std::vector<std::string> env_storage;
        env_storage.push_back(buildEnvString("GATEWAY_INTERFACE", "CGI/1.1"));
        env_storage.push_back(buildEnvString("REQUEST_METHOD", request_.method));
        env_storage.push_back(buildEnvString("SCRIPT_NAME", request_.path));
        env_storage.push_back(buildEnvString("SCRIPT_FILENAME", script_path_));
        env_storage.push_back(buildEnvString("QUERY_STRING", request_.query));
        env_storage.push_back(buildEnvString("REQUEST_URI", request_.target));
        env_storage.push_back(buildEnvString("SERVER_PROTOCOL", request_.version));
        env_storage.push_back(buildEnvString("REDIRECT_STATUS", "200"));
        env_storage.push_back(buildEnvString("DOCUMENT_ROOT", context_.root));
        if (context_.server != NULL) {
            std::ostringstream port;
            port << context_.server->port;
            env_storage.push_back(buildEnvString("SERVER_PORT", port.str()));
            env_storage.push_back(buildEnvString("SERVER_NAME", context_.server->server_name));
        }
        std::map<std::string, std::string>::const_iterator host_it = request_.headers.find("host");
        if (host_it != request_.headers.end())
            env_storage.push_back(buildEnvString("HTTP_HOST", host_it->second));
        std::map<std::string, std::string>::const_iterator type_it = request_.headers.find("content-type");
        if (type_it != request_.headers.end())
            env_storage.push_back(buildEnvString("CONTENT_TYPE", type_it->second));
        std::ostringstream len;
        len << request_.body.size();
        env_storage.push_back(buildEnvString("CONTENT_LENGTH", len.str()));
        if (!request_.path.empty())
            env_storage.push_back(buildEnvString("PATH_INFO", request_.path));

        std::vector<char*> envp;
        for (std::vector<std::string>::iterator it = env_storage.begin(); it != env_storage.end(); ++it)
            envp.push_back(const_cast<char*>(it->c_str()));
        envp.push_back(NULL);

        std::vector<std::string> argv_storage;
        argv_storage.push_back(interpreter_path_);
        argv_storage.push_back(basenameFromPath(script_path_));
        std::vector<char*> argv;
        for (std::vector<std::string>::iterator it = argv_storage.begin(); it != argv_storage.end(); ++it)
            argv.push_back(const_cast<char*>(it->c_str()));
        argv.push_back(NULL);

        execve(interpreter_path_.c_str(), &argv[0], &envp[0]);
        _exit(1);
    }

    stdin_fd_ = in_pipe[1];
    stdout_fd_ = out_pipe[0];
    Utils::closeFdSafe(in_pipe[0]);
    Utils::closeFdSafe(out_pipe[1]);
    setNonBlocking(stdin_fd_);
    setNonBlocking(stdout_fd_);
    if (input_buffer_.empty())
        closeInput();
    return true;
}

int CgiProcess::getReadFd() const { return stdout_fd_; }

int CgiProcess::getWriteFd() const { return stdin_fd_; }

pid_t CgiProcess::getPid() const { return pid_; }

const HttpRequest& CgiProcess::getRequest() const { return request_; }

bool CgiProcess::wantsRead() const
{
    return !output_closed_ && stdout_fd_ != -1;
}

bool CgiProcess::wantsWrite() const
{
    return !input_closed_ && stdin_fd_ != -1 && !input_buffer_.empty();
}

bool CgiProcess::onWritable()
{
    if (!wantsWrite())
        return true;

    ssize_t written = write(stdin_fd_, input_buffer_.c_str(), input_buffer_.size());
    if (written > 0) {
        input_buffer_.erase(0, static_cast<std::string::size_type>(written));
        if (input_buffer_.empty())
            closeInput();
        return true;
    }
    return true;
}

bool CgiProcess::parseCgiOutput()
{
    std::string::size_type header_end = output_buffer_.find("\r\n\r\n");
    std::size_t separator_len = 4;
    if (header_end == std::string::npos) {
        header_end = output_buffer_.find("\n\n");
        separator_len = 2;
    }

    if (header_end == std::string::npos) {
        if (!output_closed_)
            return false;
        headers_parsed_ = true;
        return true;
    }

    std::string header_block = output_buffer_.substr(0, header_end);
    std::string::size_type pos = 0;
    while (pos <= header_block.size()) {
        std::string::size_type end = header_block.find('\n', pos);
        std::string line = header_block.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (!line.empty()) {
            std::string key = parseHeaderKey(line);
            std::string value = parseHeaderValue(line);
            if (!key.empty()) {
                std::string lower_key = Utils::toLowerCopy(key);
                if (lower_key == "status") {
                    if (!parseStatusHeader(value, status_code_)) {
                        failed_ = true;
                        return false;
                    }
                } else {
                    if (lower_key == "content-type")
                        key = "Content-Type";
                    else if (lower_key == "content-length")
                        key = "Content-Length";
                    else if (lower_key == "location")
                        key = "Location";
                    else if (lower_key == "connection")
                        key = "Connection";
                    headers_[key] = trimHeaderValue(value);
                }
            }
        }
        if (end == std::string::npos)
            break;
        pos = end + 1;
    }

    std::string body = output_buffer_.substr(header_end + separator_len);
    std::map<std::string, std::string>::iterator content_length_it = headers_.find("Content-Length");
    if (content_length_it != headers_.end()) {
        std::size_t expected = 0;
        if (!parseSizeHeader(content_length_it->second, expected)) {
            failed_ = true;
            return false;
        }
        if (body.size() < expected) {
            if (!output_closed_)
                return false;
            failed_ = true;
            return false;
        }
        if (body.size() > expected)
            body.erase(expected);
        output_buffer_ = body;
    } else {
        output_buffer_ = body;
    }

    headers_parsed_ = true;
    return true;
}

bool CgiProcess::onReadable()
{
    if (!wantsRead())
        return true;

    char buffer[4096];
    for (;;) {
        ssize_t rd = read(stdout_fd_, buffer, sizeof(buffer));
        if (rd > 0) {
            output_buffer_.append(buffer, static_cast<std::string::size_type>(rd));
            continue;
        }
        if (rd == 0) {
            output_closed_ = true;
            closeOutput();
            break;
        }
        break;
    }

    if (!parseCgiOutput())
        return true;

    return true;
}

bool CgiProcess::reapIfFinished()
{
    if (pid_ == -1 || finished_)
        return finished_;

    int status = 0;
    pid_t ret = waitpid(pid_, &status, WNOHANG);
    if (ret == -1) {
        failed_ = true;
        finished_ = true;
        return true;
    }
    if (ret == 0)
        return false;

    pid_ = -1;
    finished_ = true;
    if (WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0)) {
        if (output_buffer_.empty())
            failed_ = true;
    }
    return true;
}

bool CgiProcess::timedOut(long now_ms, long timeout_ms) const
{
    if (timeout_ms <= 0)
        return false;
    return (now_ms - start_ms_) > timeout_ms;
}

void CgiProcess::abort()
{
    if (pid_ > 0) {
        kill(pid_, SIGKILL);
        waitpid(pid_, NULL, 0);
    }
    pid_ = -1;
    closeInput();
    closeOutput();
    finished_ = true;
}

void CgiProcess::closeDescriptors()
{
    closeInput();
    closeOutput();
}

bool CgiProcess::isFinished() const { return finished_; }

bool CgiProcess::hasFailed() const { return failed_; }

HttpResponse CgiProcess::buildResponse(bool keep_alive) const
{
    HttpResponse response;
    response.status_code = status_code_;
    response.body = output_buffer_;
    response.headers = headers_;
    response.headers["Connection"] = keep_alive ? "keep-alive" : "close";
    if (response.headers.find("Content-Length") == response.headers.end()) {
        std::ostringstream len;
        len << response.body.size();
        response.headers["Content-Length"] = len.str();
    }
    return response;
}
