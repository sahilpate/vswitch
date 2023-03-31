/*
 * vswitch_utils.hpp - Header for project utitlities.
 *
 * Contains miscellaneous declarations for functions used throughout the project.
 */

#ifndef VSWITCH_UTILS_HPP
#define VSWITCH_UTILS_HPP

#include <string>
#include <vector>
#include <PcapLiveDevice.h>

std::vector<pcpp::PcapLiveDevice *> get_intfs_prefixed_by(const std::string &prefix);

#endif // VSWITCH_UTILS_HPP
