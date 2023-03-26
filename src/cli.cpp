#include <iostream>
#include "cli.hpp"

VswitchShmem *CliInterpreter::shmem = nullptr;

const std::function<void(std::vector<std::string>)>
CliInterpreter::show_mac_addrtbl = [](std::vector<std::string>) {
    shmem->mac_tbl.print_mactbl(std::cout);
};

const std::vector<std::pair<std::vector<CliInterpreter::token>,std::function<void(std::vector<std::string>)>>>
CliInterpreter::commands = {
    {{SHOW, MAC, ADDR_TBL}, show_mac_addrtbl}
};

CliInterpreter::CliInterpreter(VswitchShmem *shmem) : root(ROOT) {
    for(auto cmd_func : commands) {
	auto [cmd, func] = cmd_func;
	this->add_cmd(root, cmd.begin(), cmd.end(), func);
    }

    if(this->shmem == nullptr) {
	this->shmem = shmem;
    }
    return;
}

int CliInterpreter::interpret(std::vector<token> tokens, std::vector<std::string> args) {
    long unsigned int i, j;
    auto cur_node = &root;
    for(i = 0; i < tokens.size(); i++) {
	bool found = false;
	for(j = 0; j < cur_node->children.size(); j++) {
	    if(tokens[i] == cur_node->children[j].tkn) {
		found = true;
		cur_node = &cur_node->children[j];
		break;
	    }
	}

	if(found == false) {
	    return -1;
	}
    }

    if(cur_node->func == nullptr) {
	return -1;
    }

    cur_node->func(args);
    return 0;
}

void CliInterpreter::add_cmd(InterpreterTreeNode &node,
			     std::vector<token>::iterator cur,
			     std::vector<token>::iterator end,
			     std::function<void(std::vector<std::string>)> func) {
    if(cur == end) {
	node.func = func;
	return;
    }

    long unsigned int nxt_tkn_indx;
    for(nxt_tkn_indx = 0; nxt_tkn_indx < node.children.size(); nxt_tkn_indx++) {
	if(node.children[nxt_tkn_indx].tkn == *cur) {
	    break;
	}
    }

    if(nxt_tkn_indx == node.children.size()) {
	node.children.push_back(InterpreterTreeNode(*cur));
    }

    add_cmd(node.children[nxt_tkn_indx], ++cur, end, func);
    return;
}

CliInterpreter::InterpreterTreeNode::InterpreterTreeNode(token tkn) : tkn(tkn) {}

CliInterpreter::InterpreterTreeNode::InterpreterTreeNode(
    token tkn, std::function<void(std::vector<std::string>)> func)
    : tkn(tkn), func(func) {}
