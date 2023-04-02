/*
 * cli.cpp - Implementation of the CliInterpreter class, as well as its private
 * InterpreterTreeNode class.
 */

#include <iostream>
#include "cli.hpp"

// Aliases to replace some of the unpleasant types used frequently here.
using StrVec = std::vector<std::string>;
using TokenVec = std::vector<CliInterpreter::token>;
using CliFunc = std::function<void(std::vector<std::string>)>;

VswitchShmem *CliInterpreter::shmem = nullptr;

const CliFunc CliInterpreter::show_mac_addrtbl = [](StrVec) {
    shmem->mac_tbl.print_mactbl(std::cout);
};

const std::vector<std::pair<TokenVec, CliFunc>> CliInterpreter::commands = {
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

int CliInterpreter::interpret(TokenVec tokens, StrVec args) {
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
			     TokenVec::iterator cur,
			     TokenVec::iterator end,
			     CliFunc func) {
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

CliInterpreter::InterpreterTreeNode::InterpreterTreeNode(token tkn, CliFunc func)
    : tkn(tkn), func(func) {}
