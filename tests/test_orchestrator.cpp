/*
 * test_orchestrator.cpp - Entry point for the test orchestrator.
 *
 * For each test defined for the vswitch-testing container, this program starts an instance of
 * vswitch on the vswitch container and an instance of the test on the testing container. Once the
 * container program finishes executing, its result is recorded before ending the vswitch program
 * and initializing everything for the next test iteration. The final results are printed after all
 * tests have run.
 *
 * Note that this program should be run after executing the script init_test_env.sh to create the
 * necessary containers, and that it must be run as root since it runs docker commands to execute
 * the vswitch and vswitch_testing processes.
 */

#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <err.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "testing_utils.hpp"

std::vector<int> failed_tsts;
const std::vector<std::pair<std::string, std::string>> tests_and_args = {
    {"broadcast_test", ""},
    {"learning_test", ""},
    {"aging_test", "mac address-table aging-time 1\n"},
    {"mult_mac_test", "mac address-table aging-time 128\n"}
};

class Proc {
public:
    int write_fd = -1;
    pid_t pid = -1;
};

/*
 * create_piped_proc() - Creates a process whose standard output is replaced with the read end of
 * a new pipe. The write end of the pipe is accessible from the current running proccess through
 * Proc.write_fd. The new process's standard output and error may be redirected by passing a file
 * descriptor through new_out and new_err.
 */
Proc create_piped_proc(const char *args[],
		       bool create_pipe = true,
		       int new_out = -1,
		       int new_err = -1) {
    Proc proc;
    int pipe_fd[2];

    if(create_pipe) {
	if(pipe(pipe_fd) == -1) {
	    err(-1, "Call to pipe failed.");
	}
	proc.write_fd = pipe_fd[1];
    }

    switch(proc.pid = fork()) {
    case -1:
	err(-1, "Call to fork failed.");
    case 0:
	if(create_pipe) {
	    dup2(pipe_fd[0], STDIN_FILENO);
	    close(pipe_fd[1]);
	}

	if(new_out >= 0) {
	    dup2(new_out, STDOUT_FILENO);
	}
	if(new_err >= 0) {
	    dup2(new_err, STDERR_FILENO);
	}

	if(execvp(args[0], const_cast<char* const*>(args)) == -1) {
	    err(-1, "Call to execvp failed.");
	}
    }

    if(create_pipe) {
	close(pipe_fd[0]);
    }

    return proc;
}

/*
 * print_tst_rslts() - Called once after all tests have run. Prints box detailing the number of
 * tests which have passed and the tests which have failed, if any.
 */
void print_tst_rslts() {
    std::ostringstream oss;
    std::cout << std::endl << std::string(80, '=') << std::endl;
    oss
	<< "ǁ "
	<< (tests_and_args.size() - failed_tsts.size())
	<< "/"
	<< tests_and_args.size()
	<< " tests passed!";
    std::cout << std::left << std::setw(80) << oss.str() << "ǁ" << std::endl;
    std::cout << std::left << std::setw(80) << "ǁ" << "ǁ" << std::endl;

    if(failed_tsts.size() != 0) {
	std::cout
	    << std::left << std::setw(80) << "ǁ The following tests failed:"
	    << "ǁ"
	    << std::endl;

	for(int i : failed_tsts) {
	    oss.str("");
	    oss << "ǁ     [" << (i + 1) << "]: " << tests_and_args[i].first;
	    std::cout << std::left << std::setw(80) << oss.str()
		      << "ǁ" << std::endl;
	}
    }
    std::cout << std::string(80, '=') << std::endl;
}

int main() {
    Proc vswitch, vswitch_testing;
    int dev_null, cur_exit_status;
    int cur_test_indx = 0;

    const char *vswitch_args[] = {
	"docker", "exec", "-i", "vswitch", "vswitch/vswitch", NULL
    };

    dev_null = open("/dev/null", O_WRONLY);

    std::cout << std::endl << std::string(80, '=') << std::endl;
    for(auto [test, cli_args] : tests_and_args) {
	std::cout << "Running Test " << (cur_test_indx + 1) << " [" << test << "]..." << std::endl;

	const char *testing_args[] = {
	    "docker", "exec", "-i", "vswitch-testing", "vswitch/vswitch_testing", test.c_str(), NULL
	};

	vswitch = create_piped_proc(vswitch_args, true, dev_null, dev_null);
	write(vswitch.write_fd, cli_args.c_str(), strlen(cli_args.c_str()));

	// NOTE: Potential for race condition here. The switch may not be finished configuring itself
	// before the testing process is created and run.
	vswitch_testing = create_piped_proc(testing_args, false);
	waitpid(vswitch_testing.pid, &cur_exit_status, 0);

	if(WEXITSTATUS(cur_exit_status) != TestData::PASS) {
	    failed_tsts.push_back(cur_test_indx);
	}

	const char exit_str[] = "exit\n";
	write(vswitch.write_fd, exit_str, sizeof(exit_str));
	close(vswitch.write_fd);
	waitpid(vswitch.pid, NULL, 0);

	cur_test_indx++;
    }

    print_tst_rslts();

    close(dev_null);
    return 0;
}
