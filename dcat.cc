// dcat - a decompressing cat
//
// 2018, Georg Sauthoff <mail@gms.tf>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>
#include <map>
#include <vector>
#include <utility>
#include <algorithm>
#include <deque>
#include <iostream>
#include <stdexcept>

#include <string.h>
#include <stdlib.h>

#include <ixxx/posix.hh>
#include <ixxx/util.hh>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

struct Args {
    deque<const char*> filenames;
    bool read_from_stdin{false};

    Args() {}
    Args(int argc, char **argv)
    {
        bool look_for_option = true;
        for (int i = 1; i < argc; ++i) {
            if (argv[i][0] == '-' && !argv[i][1]) {
                read_from_stdin = true;
                continue;
            } else if (look_for_option) {
                if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                    help(cout, argv[0]);
                    exit(0);
                }
                if (argv[i][0] == '-') {
                    if (argv[i][1] == '-' && !argv[i][2]) {
                        look_for_option = false;
                        continue;
                    }
                    cerr << "Unknown option: " << argv[i] << '\n';
                    help(cerr, argv[0]);
                    exit(2);
                }
            }
            filenames.push_back(argv[i]);
        }
        if (read_from_stdin && !filenames.empty()) {
            cerr << "Can't mix - (stdin) with some filenames\n";
            exit(2);
        }
        if (filenames.empty())
            read_from_stdin = true;
    }
    void help(ostream &o, const char *argv0)
    {
        o << "Usage: " << argv0 << " [OPTION]... [FILE]...\n"
            "\n"
            "dcat - decompressing cat\n"
            "\n"
            "Detects compressed on decompresses them on-the-fly with\n"
            "the right helper. Reads from stdin when FILE is - or left out.\n"
            "\n"
            "Options:\n"
            "  -h, --help    This help screen\n"
            "\n";
    }

};

enum class Magic {
    NONE,
    GZIP,
    ZSTANDARD,
    LZ4,
    XZ,
    BZ2
};
static const map<Magic, const char*> magic2cat = {
    { Magic::NONE     , "cat"     },
    { Magic::GZIP     , "zcat"    },
    { Magic::ZSTANDARD, "zstdcat" },
    { Magic::LZ4      , "lz4cat"  },
    { Magic::XZ       , "xzcat"   },
    { Magic::BZ2      , "bzcat"   }
};
static const vector<pair<vector<unsigned char>, Magic>> bytes2magic = {
    { { 0x1f, 0x8b                               }, Magic::GZIP },
    { { 0x28, 0xb5, 0x2f, 0xfd                   }, Magic::ZSTANDARD },
    { { 0x04, 0x22, 0x4d, 0x18                   }, Magic::LZ4 },
    { { 0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00 }, Magic::XZ },
    { { 0x42, 0x5a, 0x68                         }, Magic::BZ2 }
};

static Magic detect_cat(const unsigned char *begin,
        const unsigned char *end)
{
    size_t n = end-begin;
    for (auto &x : bytes2magic) {
        if (x.first.size() <= n
                && equal(x.first.begin(), x.first.end(), begin)) {
            return x.second;
        }
    }
    return Magic::NONE;
}

static void exec_cat(Magic magic)
{
    const char *s = magic2cat.at(magic);
    char *v[2];
    v[0] = const_cast<char*>(s);
    v[1] = nullptr;
    ixxx::posix::execvp(s, v);
}

static void cat_file(const char *filename)
{
    int fd = ixxx::posix::open(filename, O_RDONLY);
    vector<unsigned char> v(8);
    ixxx::util::read_all(fd, v);
    Magic magic = detect_cat(&*v.begin(), &*v.end());
    ixxx::posix::lseek(fd, 0, SEEK_SET);
    ixxx::posix::dup2(fd, 0);
    exec_cat(magic);
}

static void cat_files(const deque<const char*> &filenames)
{
    for (auto filename : filenames) {
        int pid = ixxx::posix::fork();
        if (pid == 0) { // child
            cat_file(filename);
        } else { // parent
            siginfo_t info;
            ixxx::posix::waitid(P_PID, pid, &info, WEXITED);
            if (info.si_code == CLD_EXITED) {
                if (info.si_status)
                    throw runtime_error("decompress failed ("
                            + string(filename) + " => "
                            + to_string(info.si_status) + ")");
            } else {
                if (info.si_status == SIGPIPE) {
                    raise(SIGPIPE);
                } else {
                    throw runtime_error("decompress command terminated by a signal ("
                            + string(filename) + " => "
                            + to_string(info.si_status) + ")");
                }
            }
        }
    }
}

static void cat_stdin()
{
    int fd = 0;
    vector<unsigned char> v(8);
    ixxx::util::read_all(fd, v);
    Magic magic = detect_cat(&*v.begin(), &*v.end());

    int pipefd[2];
    ixxx::posix::pipe(pipefd);
    int pid = ixxx::posix::fork();
    if (pid == 0) { // child
        ixxx::posix::close(pipefd[1]);
        ixxx::posix::dup2(pipefd[0], 0);
        exec_cat(magic);
    } else { // parent
        ixxx::posix::close(pipefd[0]);
        ixxx::util::write_all(pipefd[1], v);

        size_t n = v.size();
        const size_t N = 128*1024;
        v.resize(N - n);
        ixxx::util::read_all(fd, v);
        ixxx::util::write_all(pipefd[1], v);
        v.resize(N);
        do {
            ixxx::util::read_all(fd, v);
            ixxx::util::write_all(pipefd[1], v);
        } while (v.size() == N);
        ixxx::posix::close(pipefd[1]);

        siginfo_t info;
        ixxx::posix::waitid(P_PID, pid, &info, WEXITED);
        if (info.si_code == CLD_EXITED) {
            if (info.si_status)
                throw runtime_error("stdin decompressor failed");
        } else {
            throw runtime_error("stdin decompressor terminated by a signal");
        }
    }

}

int main(int argc, char **argv)
{
    Args args(argc, argv);
    try {
        if (args.filenames.size() == 1)
            cat_file(args.filenames.front());
        else if (args.filenames.size() > 1)
            cat_files(args.filenames);
        else if (args.read_from_stdin)
            cat_stdin();
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << '\n';
        exit(1);
    }
    return 0;
}

