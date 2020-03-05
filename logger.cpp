/**
 * @file	logger.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "logger.hpp"

#include <vector>

namespace grida {

	void Logger::_puts(const std::string& text) {
        ::puts(text.c_str());
	}

	void Logger::printf(const char *format, ...) {
        std::vector<char> buffer;
        va_list ap;
        int len;

        buffer.resize(strlen(format) + 4096);

        va_start(ap, format);
        len = vsnprintf(buffer.data(), buffer.size() - 2, format, ap);
        va_end(ap);

        strcat_s(buffer.data(), buffer.size(), "\n");

        this->puts(buffer.data());
	}

} // namespace grida
