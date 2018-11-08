// oldprocs - list/restart processes/services whose executable/shared libraries
//            have changed
//
// 2018, Georg Sauthoff <mail@gms.tf>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <ixxx/util.hh>
#include <ixxx/posix.hh>
#include <ixxx/ansi.hh>
#include <ixxx/sys_error.hh>

#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <set>
#include <map>
#include <deque>
#include <utility>
#include <stdexcept>
#include <memory>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

struct Args {
    bool restart{false};
    bool print_pid{false};
    bool check_dm{true};

    set<string> display_managers;

    void parse(int argc, char **argv);
    void help(ostream &o, const char *argv0);
};
void Args::parse(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            help(cout, argv[0]);
            exit(0);
        } else if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--pid")) {
            print_pid = true;
        } else if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--restart")) {
            restart = true;
        } else if (!strcmp(argv[i], "--no-check-dm")) {
            check_dm = false;
        } else {
            cerr << "Unknown argument: " << argv[1] << '\n';
            help(cerr, argv[0]);
            exit(1);
        }
    }
}
void Args::help(ostream &o, const char *argv0)
{
    o << "Usage: " << argv0 << " [--restart]\n\n"
        "oldprocs - list and/or restart processes whose executable/libraries"
        "           were updated\n\n";
    o << "optional arguments:\n"
         "  -h, --help            show this help message and exit\n"
         "  --no-check-dm         don't care whether a service like gdm/sddm\n"
         "                        has active session (those sessions are terminated\n"
        "                         during a service restart)\n"
         "  --pid, -p             print pids\n"
         "  --restart, -r         automatically restart systemd services\n"
         "\n"
         "GPL-3.0-or-later, 2018, Georg Sauthoff\n"
         "https://github.com/gsauthof/utility\n\n"
         ;
}

static bool is_num(const char *s)
{
    for (const char *x = s; *x; ++x)
        if (*x < '0' || *x > '9')
            return false;
    return true;
}

static time_t link_ctime(const std::string &name)
{
    struct stat st;
    ixxx::posix::lstat(name, &st);
    return st.st_ctime;
}
#if 0
static pair<time_t, uid_t> link_ctime_uid(const std::string &name)
{
    struct stat st;
    ixxx::posix::lstat(name, &st);
    return make_pair(st.st_ctime, st.st_uid);
}
#endif
static time_t file_ctime(const char *name)
{
    struct stat st;
    ixxx::posix::stat(name, &st);
    return st.st_ctime;
}
static uid_t file_uid(const std::string &name)
{
    struct stat st;
    ixxx::posix::stat(name, &st);
    return st.st_uid;
}

class Maps_Reader {
    public:
      Maps_Reader(const string &filename, const char *exe);
      ~Maps_Reader();
      Maps_Reader(const Maps_Reader&) =delete;
      Maps_Reader &operator=(const Maps_Reader&) =delete;
      const char *next();
    private:
      ixxx::util::File f_;
      char *line_;
      size_t n_;
      const char *exe_;
};
Maps_Reader::Maps_Reader(const string &filename, const char *exe)
    :
        f_(filename, "r"),
        line_(static_cast<char*>(ixxx::ansi::malloc(1024))),
        n_(1024),
        exe_(exe)
{
}
Maps_Reader::~Maps_Reader()
{
    free(line_);
}
const char *Maps_Reader::next()
{
    for (;;) {
        auto r = ixxx::posix::getline(&line_, &n_, f_);
        if (r == -1)
            return nullptr;
        char *p = static_cast<char*>(memchr(line_, '\n', r));
        if (!p)
            return nullptr;
        *p = 0;
        --r;
        const char xmark[] = " r-xp ";
        char *e = static_cast<char*>(memmem(line_, r, xmark, sizeof xmark - 1));
        if (!e)
            continue;
        p = static_cast<char*>(memchr(line_, '/', r));
        if (!p)
            continue;
        if (!strcmp(p, exe_))
            continue;
        *e = 0;
        return line_;
    }
}

static bool is_deleted(const char *s, size_t l)
{
    const char del_mark[] = " (deleted)";
    if (l >= sizeof del_mark - 1) {
        if (!memcmp(s + (l - (sizeof del_mark - 1)), del_mark, sizeof del_mark - 1))
            return true;
    }
    return false;
}

class Proc_Reader {
    public:

        Proc_Reader();
        enum State { DONE, OK, EXE_DELETED, LIB_DELETED,
            EXE_CTIME_MISMATCH, LIB_CTIME_MISMATCH };
        State next();
        pid_t pid() const;
        const char *pid_c_str() const;
        const char *exe() const;
        uid_t uid() const;
    private:
        State state_ {OK};
        ixxx::util::Directory proc;
        const struct dirent *d{nullptr};
        array<char, 4096> a;
        string path;
        string path2;
        mutable uid_t uid_{0};
};
Proc_Reader::Proc_Reader()
    :
        proc("/proc"),
        path("/proc/"),
        path2("/proc/")
{
}
uid_t Proc_Reader::uid() const
{
    if (!uid_) {
        auto l = path.rfind('/');
        auto p(path.substr(0, l));
        uid_ = file_uid(p);
    }
    return uid_;
}
pid_t Proc_Reader::pid() const
{
    return atol(d->d_name);
}
const char *Proc_Reader::pid_c_str() const
{
    return d->d_name;
}
const char *Proc_Reader::exe() const
{
    return a.data();
}
Proc_Reader::State Proc_Reader::next()
{
    for (;;) {
        d = proc.read();
        if (!d)
            return DONE;
        if (!is_num(d->d_name))
            continue;
        try {
            path.resize(6);
            path += d->d_name;
            size_t path_len = path.size();
            path += "/exe";
            auto l = ixxx::posix::readlink(path, a);
            //cout << path << " -> " << a.data() << '\n';
            if (is_deleted(a.data(), l)) {
                //cout << "    Warning: exe is deleted\n";
                return EXE_DELETED;
            }

            // the uid of the /exe might be != than the actual uid
            auto x = link_ctime(path);
            uid_ = 0;
            auto y = file_ctime(a.data());
            if (x < y) {
                //cout << "    Warning: executable updated after process start!\n";
                return EXE_CTIME_MISMATCH;
            }

            path.resize(path_len);
            path += "/maps";
            path2.resize(6);
            path2 += d->d_name;
            path2 += "/map_files/";
            size_t path2_len = path2.size();
            Maps_Reader maps(path, a.data());
            while (auto m = maps.next()) {
                //cout << "  => " << m << '\n';
                array<char, 4096> b;
                path2.resize(path2_len);
                path2 += m;
                auto l = ixxx::posix::readlink(path2, b);
                //cout << "  => " << b.data() << '\n';
                if (is_deleted(b.data(), l)) {
                    //cout << "    Warning: library deleted!\n";
                    return LIB_DELETED;
                }
                auto x = link_ctime(path2);
                auto y = file_ctime(b.data());
                if (x < y) {
                    //cout << "    Warning: library updated after process start!\n";
                    return LIB_CTIME_MISMATCH;
                }
            }
        } catch (const ixxx::readlink_error &e) {
            if (e.code() == EACCES)
                continue;
        }
        return OK;
    }
}

enum class Service {
    UNKNOWN,
    SYSTEMD,
    YES,
    SESSION
};
ostream &operator<<(ostream &o, Service s)
{
    switch (s) {
        case Service::UNKNOWN: o << "UNKNOWN"; break;
        case Service::SYSTEMD: o << "SYSTEMD"; break;
        case Service::YES: o << "YES"; break;
        case Service::SESSION: o << "SESSION"; break;
    }
    return o;
}

static bool starts_with(const char *begin, const char *end,
        const char *b, const char *e)
{
    size_t n = end-begin;
    size_t m = e-b;
    if (m > n)
        return false;
    return equal(begin, begin+m, b, e);
}
static bool ends_with(const char *begin, const char *end,
        const char *b, const char *e)
{
    size_t n = end-begin;
    size_t m = e-b;
    if (m > n)
        return false;
    return equal(end - m, end, b, e);
}

static pair<Service, string> get_service(const char *pid_str)
{
    string filename("/proc/");
    filename += pid_str;
    filename += "/cgroup";
    ixxx::util::FD fd(filename, O_RDONLY);
    array<char, 8*1024> a;
    size_t n = ixxx::util::read_all(fd, a);
    const char *begin = a.data();
    const char *end = begin + n;
    const char name_mark[] = "1:name=systemd:/";
    const char *p = search(begin, end, name_mark, name_mark + sizeof name_mark - 1);
    if (p == end || (p > begin && *(p-1) != '\n' ) )
        return make_pair(Service::UNKNOWN, string());
    p += sizeof name_mark - 1;
    end = find(p, end, '\n');
    const char service_mark[] = ".service";
    if (ends_with(p, end, service_mark, service_mark + sizeof service_mark - 1)) {
        p = static_cast<const char*>(memrchr(p-1, '/', end-(p-1)));
        return make_pair(Service::YES, string(p+1, end));
    }
    const char init_mark[] = "/init.scope";
    if (ends_with(p-1, end, init_mark, init_mark + sizeof init_mark - 1)) {
        return make_pair(Service::SYSTEMD, string());
    }
    const char scope_mark[] = ".scope";
    if (ends_with(p, end, scope_mark, scope_mark + sizeof scope_mark - 1)) {
        p = static_cast<const char*>(memrchr(p-1, '/', end-(p-1)));
        ++p;
        const char session_mark[] = "session";
        if (starts_with(p, end, session_mark, session_mark + sizeof session_mark - 1))
            return make_pair(Service::SESSION, string());
    }
    return make_pair(Service::UNKNOWN, string());
}

extern char **environ;

static void execute(const vector<const char *> &args)
{
    assert(args.size() > 1);
    assert(args.back() == nullptr);
    cout << "    => Executing:";
    auto i = args.begin();
    auto e = args.end() - 1;
    for (; i != e; ++i) {
        cout << ' ' << *i;
    }
    cout << '\n';
    pid_t child_pid = 0;
    ixxx::posix::spawnp(&child_pid,
            args.front(),
            nullptr, nullptr,
            const_cast<char* const*>(args.data()), environ);
    siginfo_t info;
    ixxx::posix::waitid(P_PID, child_pid, &info, WEXITED);
    if (info.si_code == CLD_EXITED) {
        if (info.si_status)
            throw runtime_error("command exited with non-zero exit status");
    } else {
        throw runtime_error("command terminated by a signal");
    }
}

class Proc_Checker {
    public:
        Proc_Checker(const Args &args);
        int check();
        void report();
    private:
        void report_system();
        void report_users();

        const Args *args_{nullptr};
        uid_t my_uid {0};
        vector<const char *> cmd;

        set<string> services;
        map<uid_t, set<string>> user_services;
        bool auditd {false};
        bool systemd {false};
        pid_t systemd_pid {0};
        bool dbusd {false};
        map<uid_t, pid_t> user_systemd;
        set<uid_t> user_dbusd;
        map<uid_t, map<string, deque<pid_t>>> processes;

};
Proc_Checker::Proc_Checker(const Args &args)
    :
        args_(&args)
{
    my_uid = getuid();
}

int Proc_Checker::check()
{
    Proc_Reader reader;
    Proc_Reader::State state;
    while ((state = reader.next()) != Proc_Reader::DONE) {
        if (state == Proc_Reader::OK)
            continue;

        //cout << reader.pid() << ": " << reader.exe() << " -> restart!\n";
        auto x = get_service(reader.pid_c_str());
        //cout << "   => " << x.first << ' ' << x.second << '\n';

        switch (x.first) {
            case Service::YES:
                if (x.second == "auditd.service")
                    auditd = true;
                else if (x.second == "dbus.service") {
                    if (reader.uid() < 1000)
                        dbusd = true;
                    else {
                        const char *b = reader.exe();
                        const char *e = b + strlen(b);
                        const char dbus_mark[] = "/dbus-daemon";
                        if (ends_with(b, e, dbus_mark, dbus_mark + sizeof dbus_mark - 1))
                            user_dbusd.insert(reader.uid());
                        else {
                            processes[reader.uid()][reader.exe()].push_back(reader.pid());
                        }
                    }
                } else {
                    if (reader.uid() < 1000) {
                        services.insert(x.second);
                    } else
                        user_services[reader.uid()].insert(x.second);
                }
                break;
            case Service::SYSTEMD:
                if (reader.uid() < 1000) {
                    systemd = true;
                    systemd_pid = reader.pid();
                } else {
                    user_systemd.emplace(reader.uid(), reader.pid());
                }
                break;
            case Service::SESSION:
                processes[reader.uid()][reader.exe()].push_back(reader.pid());
                break;
            default:
                ;
        }
    }
    if (dbusd)
        return 10;
    if (!user_dbusd.empty() || !processes.empty())
        return 11;
    if (!services.empty() || !user_services.empty() || auditd
            || systemd || !user_systemd.empty())
        return 15;
    return 0;
}
void Proc_Checker::report()
{
    report_system();
    report_users();
}

void Proc_Checker::report_system()
{
    if (dbusd) {
        // cf. https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=805449
        cout << "\nYou have to restart the system (because dbus changed).\n\n";
    }
    cmd.clear();
    cmd.push_back("systemctl");
    cmd.push_back("restart");
    if (!services.empty()) {
        cout << "\nYou have to restart the following system services:\n\n";
        for (auto &service: services) {
            cout << "systemctl restart " << service;
            if (args_->display_managers.count(service))
                cout << "    # ATTENTION: a local user session might be terminated";
            cout << '\n';
            if (args_->restart && my_uid == 0) {
                if (args_->display_managers.count(service)) {
                    cout << "    => NOT restarting it automatically!\n";
                } else {
                    cmd.resize(2);
                    cmd.push_back(service.c_str());
                    cmd.push_back(nullptr);
                    execute(cmd);
                }
            }
        }
    }
    if (auditd) {
        // cf. https://bugzilla.redhat.com/show_bug.cgi?id=973697
        //     https://bugzilla.redhat.com/show_bug.cgi?id=1026648
        cout << "/usr/libexec/initscripts/legacy-actions/auditd/restart\n";
        if (args_->restart && my_uid == 0) {
            cmd.clear();
            cmd.push_back("/usr/libexec/initscripts/legacy-actions/auditd/restart");
            cmd.push_back(nullptr);
            execute(cmd);
        }
    }
    if (systemd) {
        cout << "systemctl daemon-reexec";
        if (args_->print_pid)
            cout << "    # " << systemd_pid;
        cout << '\n';
        if (args_->restart && my_uid == 0) {
            cmd.clear();
            cmd.push_back("systemctl");
            cmd.push_back("daemon-reexec");
            cmd.push_back(nullptr);
            execute(cmd);
        }
    }
}

void Proc_Checker::report_users()
{
    for (auto uid : user_dbusd) {
        cout << "\nYou have to logoff/login from/to session of user " << uid;
        if (uid == my_uid)
            cout << " (your user!)";
        cout << "\nbecause dbus changed.\n\n";
    }
    if (!user_services.empty()) {
        cout << "\nYou have to restart the following user services:\n\n";
        cmd.clear();
        cmd.push_back("systemctl");
        cmd.push_back("--user");
        cmd.push_back("restart");
        for (auto &x: user_services) {
            for (auto &service: x.second) {
                if (x.first != my_uid)
                    cout << "sudo -u '#" << x.first << "' ";
                cout << "systemctl --user restart " << service << '\n';
                if (args_->restart && my_uid == x.first) {
                    cmd.resize(3);
                    cmd.push_back(service.c_str());
                    cmd.push_back(nullptr);
                    execute(cmd);
                }
            }
        }
    }
    for (auto &x : user_systemd) {
        auto &uid = x.first; auto &pid = x.second;
        if (uid != my_uid)
            cout << "sudo -u '#" << uid << "' ";
        cout << "systemctl --user daemon-reexec";
        if (args_->print_pid)
            cout << "    # " << pid;
        cout << '\n';
        if (args_->restart && my_uid == uid) {
            cmd.resize(2);
            cmd.push_back("daemon-reexec");
            cmd.push_back(nullptr);
            execute(cmd);
        }
    }
    if (!processes.empty()) {
        cout << "\nThe following user processes must be restarted manually\n(or a session logoff/login might take care of them):\n\n";
        for (auto &x : processes) {
            for (auto &y : x.second) {
                cout << y.first << " (uid " << x.first << ") - pids:";
                for (auto pid : y.second) {
                    cout << ' ' << pid;
                }
                cout << '\n';
            }
        }
    }
}

static void check_output(const vector<const char*> &args, vector<char> &buf)
{
    assert(args.size() > 1);
    assert(args.back() == nullptr);

    int pipefd[2];
    ixxx::posix::pipe(pipefd);
    posix_spawn_file_actions_t as;
    ixxx::posix::spawn_file_actions_init(&as);
    unique_ptr<posix_spawn_file_actions_t,
        void(*)(posix_spawn_file_actions_t *)> asp(&as,
                    ixxx::posix::spawn_file_actions_destroy);
    ixxx::posix::spawn_file_actions_addclose(&as, 0);
    ixxx::posix::spawn_file_actions_addclose(&as, pipefd[0]);
    ixxx::posix::spawn_file_actions_adddup2(&as, pipefd[1], 1);

    pid_t child_pid = 0;
    ixxx::posix::spawnp(&child_pid,
            args.front(),
            &as, nullptr,
            const_cast<char* const*>(args.data()), environ);

    ixxx::posix::close(pipefd[1]);

    size_t n = 8*1024;
    size_t old_n = n;
    while (n == old_n) {
        size_t off = buf.size();
        buf.resize(off + old_n);
        n = ixxx::util::read_all(pipefd[0], buf.data() + off, old_n);
        buf.resize(off + n);
    }
    ixxx::posix::close(pipefd[0]);

    siginfo_t info;
    ixxx::posix::waitid(P_PID, child_pid, &info, WEXITED);
    if (info.si_code == CLD_EXITED) {
        if (info.si_status)
            throw runtime_error("command exited with non-zero exit status");
    } else {
        throw runtime_error("command terminated by a signal");
    }
}

static deque<string> get_path()
{
    deque<string> v;
    const char *path = ::getenv("PATH");
    const char *i = path;
    if (!i)
        return v;
    const char *end = i + strlen(path);
    const char *delim = find(i, end, ':');
    while (delim < end) {
        v.emplace_back(i, delim);
        i = delim + 1;
        delim = find(i, end, ':');
    }
    v.emplace_back(i, end);
    for (auto &x : v) {
        if (x.empty())
            x = ".";
    }
    return v;
}

static bool have_loginctl()
{
    try {
        ixxx::util::which(get_path(), "loginctl");
    } catch (range_error&) {
        return false;
    }
    return true;
}

static set<string> get_display_managers()
{
    set<string> services;

    if (!have_loginctl())
        return services;

    vector<const char *> cmd = { "loginctl", "list-sessions", "--no-legend", nullptr };
    vector<char> buf;
    check_output(cmd, buf);

    deque<string> ids;
    {
    const char *line = buf.data();
    const char *end = buf.data() + buf.size();
    const char *line_end = find(line, end, '\n');
    while (line_end < buf.data() + buf.size() ) {
        const char *x = find_if(line, line_end, [](auto c) { return c != ' '; });
        const char *y = find(x, line_end, ' ');
        ids.emplace_back(x, y);

        line = line_end+1;
        line_end = find(line, end, '\n');
    }
    }

    cmd.resize(1);
    cmd.push_back("show-session");
    for (auto &id : ids) {
        cmd.push_back(id.c_str());
    }
    cmd.push_back(nullptr);
    check_output(cmd, buf);

    {
    const char *line = buf.data();
    const char *end = buf.data() + buf.size();
    const char *line_end = find(line, end, '\n');

    //const char type_mark[] = "Type=";
    const char service_mark[] = "Service=";
    const char sshd_mark[] = "=sshd";
    const char gdm_mark[] = "gdm";
    while (line_end < buf.data() + buf.size() ) {
        if (starts_with(line, line_end,
                    service_mark, service_mark + sizeof service_mark - 1)
            && !ends_with(line, line_end, sshd_mark,
                sshd_mark + sizeof sshd_mark - 1)) {
            if (starts_with(line + sizeof service_mark - 1, line_end,
                        gdm_mark, gdm_mark + sizeof gdm_mark - 1))
                services.emplace("gdm");
            else
                services.emplace(line + sizeof service_mark - 1, line_end);
        }

        line = line_end+1;
        line_end = find(line, end, '\n');
    }
    }
    return services;
}


// rc: 10 => reboot necessary
// rc: 11 => session logout/login necessary
// rc: 15 => auto-restart possible
int main(int argc, char **argv)
{
    Args args;
    args.parse(argc, argv);
    if (args.check_dm)
        args.display_managers = get_display_managers();
    Proc_Checker pc(args);
    int rc = pc.check();
    pc.report();
    if (rc == 15 && args.restart) {
        pc = Proc_Checker(args);
        rc = pc.check();
    }
    return rc;
}
