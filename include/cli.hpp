#ifndef CLI_HPP
#define CLI_HPP

#include <functional>
#include <string>
#include <vector>
#include "vswitch_shmem.hpp"

/*
 * CliInterpreter - This class maintains a tree of valid serieses of CLI tokens and the functions
 * that should be called when they are inputted. When given a series of tokens and their literal
 * values through the interpret() function, the appropriate function will be called if the series
 * is valid. Otherwise, it will return an error code.
 */
class CliInterpreter {
public:
    enum token {
	ROOT, NL, EXIT, SHOW, MAC, ADDR_TBL, INTF, COUNT, NAME, UINT
    };

    CliInterpreter(VswitchShmem *shmem);
    int interpret(std::vector<token> tokens, std::vector<std::string> args);

private:
    /*
     * InterpreterTreeNode - A single node in the interpreter tree. Each node contains a
     * corresponding token, a function that should be executed if this is the final node in the
     * sequence (nullptr otherwise), and a vector of tokens which may come after the current one.
     */
    class InterpreterTreeNode {
    public:
	InterpreterTreeNode(token tkn);
	InterpreterTreeNode(token tkn, std::function<void(std::vector<std::string>)> func);

	token tkn;
	std::function<void(std::vector<std::string>)> func;
	std::vector<InterpreterTreeNode> children;
    };

    InterpreterTreeNode root;
    static VswitchShmem *shmem;

    void add_cmd(InterpreterTreeNode &node,
		 std::vector<token>::iterator cur,
		 std::vector<token>::iterator end,
		 std::function<void(std::vector<std::string>)> func);

    // CLI functions
    const static std::function<void(std::vector<std::string>)> show_mac_addrtbl;

    // Valid CLI commands
    const static std::vector<std::pair<std::vector<token>,std::function<void(std::vector<std::string>)>>> commands;
};

#endif // CLI_HPP
